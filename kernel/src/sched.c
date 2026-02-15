#include "../include/kernel/irq.h"
#include "../include/kernel/platform.h"
#include "../include/kernel/sched.h"

#define SCHED_TICK_PERIOD_US 50000u
#define SCHED_QUANTUM_TICKS 4u

/*
 * Scheduler currently executes tasks as short callback steps.
 * This keeps blocking/time semantics testable before ISA-level context switch lands.
 */
typedef struct sched_task_slot {
    sched_task_t pub;
    uint32_t used;
    uint32_t is_idle;
    uint32_t quantum_used;
    int32_t parent_tid;
    uint32_t exit_code;
    sched_task_entry_t entry;
    const char *name;
    sched_waitq_t *waitq;
    sched_waitq_t child_waitq;
} sched_task_slot_t;

static inline void timer_program_period_us(uint32_t period_us) {
    *(volatile uint32_t *)(uintptr_t)TIMER_MMIO_BASE = period_us;
}

static volatile unsigned int g_ticks;
static volatile unsigned int g_need_resched;
static sched_task_slot_t g_tasks[SCHED_MAX_TASKS];
static uint32_t g_current_idx;
static uint32_t g_next_tid;

static inline uint32_t sched_tick_now(void) {
    return (uint32_t)g_ticks;
}

static void sched_clear_task(sched_task_slot_t *slot) {
    if (!slot) {
        return;
    }
    slot->pub.tid = 0u;
    slot->pub.state = SCHED_TASK_UNUSED;
    slot->pub.wake_tick = 0u;
    slot->pub.run_ticks = 0u;
    slot->pub.arg = 0;
    slot->used = 0u;
    slot->is_idle = 0u;
    slot->quantum_used = 0u;
    slot->parent_tid = -1;
    slot->exit_code = 0u;
    slot->entry = 0;
    slot->name = 0;
    slot->waitq = 0;
    sched_waitq_init(&slot->child_waitq);
}

static inline void waitq_set_bit(sched_waitq_t *q, uint32_t idx) {
    q->bits[idx / 32u] |= (1u << (idx % 32u));
}

static inline void waitq_clear_bit(sched_waitq_t *q, uint32_t idx) {
    q->bits[idx / 32u] &= ~(1u << (idx % 32u));
}

static inline uint32_t waitq_test_bit(const sched_waitq_t *q, uint32_t idx) {
    return (q->bits[idx / 32u] >> (idx % 32u)) & 1u;
}

static uint32_t sched_slot_index(const sched_task_slot_t *slot) {
    if (!slot) {
        return 0u;
    }
    return (uint32_t)(slot - &g_tasks[0]);
}

static void sched_detach_waitq(sched_task_slot_t *slot) {
    uint32_t idx;
    if (!slot || !slot->waitq) {
        return;
    }
    idx = sched_slot_index(slot);
    if (idx < SCHED_MAX_TASKS) {
        waitq_clear_bit(slot->waitq, idx);
    }
    slot->waitq = 0;
}

static void sched_idle_task(sched_task_t *task, void *arg) {
    (void)task;
    (void)arg;
    __asm__ __volatile__("" ::: "memory");
}

static void sched_input_task(sched_task_t *task, void *arg) {
    (void)task;
    (void)arg;
    irq_poll_input_echo();
    sched_yield();
}

static int sched_alloc_slot(void) {
    for (uint32_t i = 0u; i < SCHED_MAX_TASKS; i++) {
        if (!g_tasks[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t sched_idx_runnable(uint32_t idx) {
    if (idx >= SCHED_MAX_TASKS) {
        return 0u;
    }
    if (!g_tasks[idx].used) {
        return 0u;
    }
    return g_tasks[idx].pub.state == SCHED_TASK_RUNNABLE;
}

static uint32_t sched_pick_next_idx(void) {
    for (uint32_t off = 1u; off <= SCHED_MAX_TASKS; off++) {
        uint32_t idx = (g_current_idx + off) % SCHED_MAX_TASKS;
        if (sched_idx_runnable(idx)) {
            return idx;
        }
    }
    return 0u;
}

static sched_task_slot_t *sched_current_slot(void) {
    if (g_current_idx >= SCHED_MAX_TASKS) {
        return 0;
    }
    if (!g_tasks[g_current_idx].used) {
        return 0;
    }
    return &g_tasks[g_current_idx];
}

static sched_task_slot_t *sched_find_by_tid(uint32_t tid) {
    if (tid == 0u) {
        return &g_tasks[0];
    }
    for (uint32_t i = 1u; i < SCHED_MAX_TASKS; i++) {
        if (!g_tasks[i].used) {
            continue;
        }
        if (g_tasks[i].pub.tid == tid) {
            return &g_tasks[i];
        }
    }
    return 0;
}

static uint32_t sched_task_is_child_of(const sched_task_slot_t *slot, int32_t parent_tid, int32_t pid) {
    if (!slot || !slot->used || slot->is_idle) {
        return 0u;
    }
    if (slot->parent_tid != parent_tid) {
        return 0u;
    }
    if (pid == SCHED_WAITPID_ANY) {
        return 1u;
    }
    return (slot->pub.tid == (uint32_t)pid) ? 1u : 0u;
}

static int sched_try_reap_child(int32_t parent_tid, int32_t pid, uint32_t *status_out) {
    uint32_t has_child = 0u;
    for (uint32_t i = 1u; i < SCHED_MAX_TASKS; i++) {
        sched_task_slot_t *slot = &g_tasks[i];
        if (!sched_task_is_child_of(slot, parent_tid, pid)) {
            continue;
        }
        has_child = 1u;
        if (slot->pub.state != SCHED_TASK_ZOMBIE) {
            continue;
        }
        if (status_out) {
            *status_out = (slot->exit_code & 0xFFu) << 8;
        }
        {
            const int child_tid = (int)slot->pub.tid;
            sched_clear_task(slot);
            return child_tid;
        }
    }
    if (!has_child) {
        return SCHED_WAITPID_NO_CHILD;
    }
    return 0;
}

static void sched_try_wake_sleepers(uint32_t now_tick) {
    for (uint32_t i = 0u; i < SCHED_MAX_TASKS; i++) {
        sched_task_slot_t *slot = &g_tasks[i];
        if (!slot->used) {
            continue;
        }
        if (slot->pub.state != SCHED_TASK_SLEEPING && slot->pub.state != SCHED_TASK_BLOCKED) {
            continue;
        }
        if (slot->pub.wake_tick == 0u || slot->pub.wake_tick > now_tick) {
            continue;
        }
        sched_detach_waitq(slot);
        slot->pub.wake_tick = 0u;
        slot->pub.state = SCHED_TASK_RUNNABLE;
        slot->quantum_used = 0u;
        g_need_resched = 1u;
    }
}

int sched_spawn(const char *name, sched_task_entry_t entry, void *arg) {
    int slot_idx;
    sched_task_slot_t *slot;
    sched_task_slot_t *parent_slot;
    if (!entry) {
        return -1;
    }

    slot_idx = sched_alloc_slot();
    if (slot_idx < 0) {
        return -1;
    }

    slot = &g_tasks[(uint32_t)slot_idx];
    slot->used = 1u;
    slot->is_idle = 0u;
    slot->entry = entry;
    slot->name = name;
    slot->waitq = 0;
    sched_waitq_init(&slot->child_waitq);
    slot->quantum_used = 0u;
    slot->exit_code = 0u;
    parent_slot = sched_current_slot();
    slot->parent_tid = parent_slot ? (int32_t)parent_slot->pub.tid : 0;
    slot->pub.tid = g_next_tid++;
    if (g_next_tid == 0u) {
        g_next_tid = 1u;
    }
    slot->pub.state = SCHED_TASK_RUNNABLE;
    slot->pub.wake_tick = 0u;
    slot->pub.run_ticks = 0u;
    slot->pub.arg = arg;
    return (int)slot->pub.tid;
}

void sched_init(void) {
    for (uint32_t i = 0u; i < SCHED_MAX_TASKS; i++) {
        sched_clear_task(&g_tasks[i]);
    }

    g_ticks = 0u;
    g_need_resched = 0u;
    g_current_idx = 0u;
    g_next_tid = 1u;

    g_tasks[0].used = 1u;
    g_tasks[0].is_idle = 1u;
    g_tasks[0].entry = sched_idle_task;
    g_tasks[0].name = "idle";
    g_tasks[0].parent_tid = -1;
    g_tasks[0].exit_code = 0u;
    sched_waitq_init(&g_tasks[0].child_waitq);
    g_tasks[0].pub.tid = 0u;
    g_tasks[0].pub.state = SCHED_TASK_RUNNABLE;
    g_tasks[0].pub.wake_tick = 0u;
    g_tasks[0].pub.run_ticks = 0u;
    g_tasks[0].pub.arg = 0;

    (void)sched_spawn("input", sched_input_task, 0);
    timer_program_period_us(SCHED_TICK_PERIOD_US);
}

uint32_t sched_tick_period_us(void) {
    return SCHED_TICK_PERIOD_US;
}

void schedule_tick(void) {
    g_ticks++;
    sched_try_wake_sleepers((uint32_t)g_ticks);

    if (g_tasks[g_current_idx].used && !g_tasks[g_current_idx].is_idle &&
        g_tasks[g_current_idx].pub.state == SCHED_TASK_RUNNING) {
        g_tasks[g_current_idx].quantum_used++;
        if (g_tasks[g_current_idx].quantum_used >= SCHED_QUANTUM_TICKS) {
            g_need_resched = 1u;
        }
    }
}

unsigned int sched_ticks(void) {
    return g_ticks;
}

int sched_current_tid(void) {
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot) {
        return -1;
    }
    return (int)slot->pub.tid;
}

void sched_yield(void) {
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot) {
        return;
    }
    if (slot->pub.state == SCHED_TASK_RUNNING) {
        slot->pub.state = SCHED_TASK_RUNNABLE;
    }
    slot->quantum_used = 0u;
    g_need_resched = 1u;
}

void sched_exit_code(uint32_t code) {
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot || slot->is_idle) {
        return;
    }
    sched_detach_waitq(slot);
    slot->exit_code = code;
    slot->pub.state = SCHED_TASK_ZOMBIE;
    slot->pub.wake_tick = 0u;
    slot->quantum_used = 0u;
    if (slot->parent_tid >= 0) {
        sched_task_slot_t *parent = sched_find_by_tid((uint32_t)slot->parent_tid);
        if (parent) {
            sched_waitq_wake_all(&parent->child_waitq);
        }
    }
    g_need_resched = 1u;
}

void sched_exit(void) {
    sched_exit_code(0u);
}

void sched_sleep_ticks(uint32_t ticks) {
    uint32_t now;
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot) {
        return;
    }
    if (slot->is_idle) {
        return;
    }

    now = sched_tick_now();
    slot->pub.wake_tick = now + (ticks ? ticks : 1u);
    slot->pub.state = SCHED_TASK_SLEEPING;
    slot->quantum_used = 0u;
    g_need_resched = 1u;
}

void sched_waitq_init(sched_waitq_t *q) {
    if (!q) {
        return;
    }
    for (uint32_t i = 0u; i < (SCHED_MAX_TASKS + 31u) / 32u; i++) {
        q->bits[i] = 0u;
    }
}

void sched_waitq_sleep(sched_waitq_t *q, uint32_t timeout_ticks) {
    uint32_t now;
    uint32_t slot_idx;
    sched_task_slot_t *slot = sched_current_slot();
    if (!q || !slot || slot->is_idle) {
        return;
    }

    now = sched_tick_now();
    slot_idx = sched_slot_index(slot);
    if (slot_idx >= SCHED_MAX_TASKS) {
        return;
    }

    sched_detach_waitq(slot);
    slot->waitq = q;
    waitq_set_bit(q, slot_idx);
    slot->pub.state = SCHED_TASK_BLOCKED;
    slot->pub.wake_tick = timeout_ticks ? (now + timeout_ticks) : 0u;
    slot->quantum_used = 0u;
    g_need_resched = 1u;
}

static void sched_wake_slot(sched_task_slot_t *slot) {
    if (!slot || !slot->used || slot->is_idle) {
        return;
    }
    if (slot->pub.state != SCHED_TASK_BLOCKED && slot->pub.state != SCHED_TASK_SLEEPING) {
        return;
    }
    sched_detach_waitq(slot);
    slot->pub.wake_tick = 0u;
    slot->pub.state = SCHED_TASK_RUNNABLE;
    slot->quantum_used = 0u;
    g_need_resched = 1u;
}

void sched_waitq_wake_one(sched_waitq_t *q) {
    if (!q) {
        return;
    }
    for (uint32_t i = 1u; i < SCHED_MAX_TASKS; i++) {
        if (!waitq_test_bit(q, i)) {
            continue;
        }
        sched_wake_slot(&g_tasks[i]);
        return;
    }
}

void sched_waitq_wake_all(sched_waitq_t *q) {
    if (!q) {
        return;
    }
    for (uint32_t i = 1u; i < SCHED_MAX_TASKS; i++) {
        if (!waitq_test_bit(q, i)) {
            continue;
        }
        sched_wake_slot(&g_tasks[i]);
    }
}

int sched_waitpid(int32_t pid, uint32_t options, uint32_t *status_out) {
    int rc;
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot || slot->is_idle) {
        return SCHED_WAITPID_NO_CHILD;
    }

    rc = sched_try_reap_child((int32_t)slot->pub.tid, pid, status_out);
    if (rc != 0) {
        return rc;
    }

    if (options & SCHED_WAITPID_WNOHANG) {
        return 0;
    }

    sched_waitq_sleep(&slot->child_waitq, 0u);
    return SCHED_WAITPID_BLOCKED;
}

void sched_run(void) {
    for (;;) {
        sched_task_slot_t *slot;
        if (g_need_resched || !sched_idx_runnable(g_current_idx)) {
            g_need_resched = 0u;
            g_current_idx = sched_pick_next_idx();
        }

        slot = &g_tasks[g_current_idx];
        if (!slot->used || !slot->entry || slot->pub.state != SCHED_TASK_RUNNABLE) {
            g_need_resched = 1u;
            continue;
        }

        slot->pub.state = SCHED_TASK_RUNNING;
        slot->pub.run_ticks++;
        slot->entry(&slot->pub, slot->pub.arg);
        if (slot->pub.state == SCHED_TASK_RUNNING) {
            slot->pub.state = SCHED_TASK_RUNNABLE;
        }
    }
}
