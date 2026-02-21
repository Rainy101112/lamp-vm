#include "../include/kernel/platform.h"
#include "../include/kernel/sched.h"

#define SCHED_TICK_PERIOD_US 50000u
#define SCHED_QUANTUM_TICKS 4u

typedef struct sched_ofile {
    uint32_t used;
    uint32_t refs;
    uint32_t type;
    uint32_t status_flags;
} sched_ofile_t;

typedef struct sched_fdent {
    uint32_t used;
    uint32_t ofile_idx;
    uint32_t fd_flags;
} sched_fdent_t;

enum {
    SCHED_OFILE_TYPE_NONE = 0u,
    SCHED_OFILE_TYPE_STDIN = 1u,
    SCHED_OFILE_TYPE_STDOUT = 2u,
    SCHED_OFILE_TYPE_STDERR = 3u
};

#define SCHED_FD_OFILE_INVALID ((uint32_t)~0u)

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
    sched_ofile_t ofiles[SCHED_MAX_FDS];
    sched_fdent_t fdtab[SCHED_MAX_FDS];
} sched_task_slot_t;

static inline void timer_program_period_us(uint32_t period_us) {
    *(volatile uint32_t *)(uintptr_t)TIMER_MMIO_BASE = period_us;
}

static volatile unsigned int g_ticks;
static volatile unsigned int g_need_resched;
static sched_task_slot_t g_tasks[SCHED_MAX_TASKS];
static uint32_t g_current_idx;
static uint32_t g_next_tid;

static void sched_fd_table_clear(sched_task_slot_t *slot);
static void sched_fd_table_init_stdio(sched_task_slot_t *slot);
static int sched_slot_close_fd(sched_task_slot_t *slot, int32_t fd);
static void sched_slot_close_all_fds(sched_task_slot_t *slot);

static inline uint32_t sched_tick_now(void) {
    return (uint32_t)g_ticks;
}

static void sched_clear_task(sched_task_slot_t *slot) {
    uint32_t i;
    volatile uint8_t *bytes;
    if (!slot) {
        return;
    }
    bytes = (volatile uint8_t *)(uintptr_t)slot;
    for (i = 0u; i < (uint32_t)sizeof(*slot); i++) {
        bytes[i] = 0u;
    }
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

static int sched_alloc_slot(void) {
    for (uint32_t i = 1u; i < SCHED_MAX_TASKS; i++) {
        sched_task_slot_t *slot = &g_tasks[i];
        if (!slot->used) {
            return (int)i;
        }
    }
    return -1;
}

static uint32_t sched_idx_runnable(uint32_t idx) {
    sched_task_slot_t *slot;
    if (idx == 0u) {
        return 0u;
    }
    if (idx >= SCHED_MAX_TASKS) {
        return 0u;
    }
    slot = &g_tasks[idx];
    if (!slot->used) {
        return 0u;
    }
    return slot->pub.state == SCHED_TASK_RUNNABLE;
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
    sched_task_slot_t *slot;
    if (g_current_idx >= SCHED_MAX_TASKS) {
        return 0;
    }
    slot = &g_tasks[g_current_idx];
    if (!slot->used) {
        return 0;
    }
    return slot;
}

static void sched_fd_table_clear(sched_task_slot_t *slot) {
    uint32_t i;
    if (!slot) {
        return;
    }
    for (i = 0u; i < SCHED_MAX_FDS; i++) {
        slot->ofiles[i].used = 0u;
        slot->ofiles[i].refs = 0u;
        slot->ofiles[i].type = SCHED_OFILE_TYPE_NONE;
        slot->ofiles[i].status_flags = 0u;
        slot->fdtab[i].used = 0u;
        slot->fdtab[i].ofile_idx = SCHED_FD_OFILE_INVALID;
        slot->fdtab[i].fd_flags = 0u;
    }
}

static void sched_fd_table_init_stdio(sched_task_slot_t *slot) {
    if (!slot || SCHED_MAX_FDS < 3u) {
        return;
    }
    sched_fd_table_clear(slot);

    slot->ofiles[0].used = 1u;
    slot->ofiles[0].refs = 1u;
    slot->ofiles[0].type = SCHED_OFILE_TYPE_STDIN;
    slot->ofiles[0].status_flags = SCHED_FD_O_RDONLY;
    slot->fdtab[0].used = 1u;
    slot->fdtab[0].ofile_idx = 0u;

    slot->ofiles[1].used = 1u;
    slot->ofiles[1].refs = 1u;
    slot->ofiles[1].type = SCHED_OFILE_TYPE_STDOUT;
    slot->ofiles[1].status_flags = SCHED_FD_O_WRONLY;
    slot->fdtab[1].used = 1u;
    slot->fdtab[1].ofile_idx = 1u;

    slot->ofiles[2].used = 1u;
    slot->ofiles[2].refs = 1u;
    slot->ofiles[2].type = SCHED_OFILE_TYPE_STDERR;
    slot->ofiles[2].status_flags = SCHED_FD_O_WRONLY;
    slot->fdtab[2].used = 1u;
    slot->fdtab[2].ofile_idx = 2u;
}

static sched_ofile_t *sched_slot_ofile_by_fd(sched_task_slot_t *slot, int32_t fd, sched_fdent_t **fdent_out) {
    uint32_t of_idx;
    sched_fdent_t *fdent;
    if (!slot || fd < 0 || (uint32_t)fd >= SCHED_MAX_FDS) {
        return 0;
    }
    fdent = &slot->fdtab[(uint32_t)fd];
    if (!fdent->used) {
        return 0;
    }
    of_idx = fdent->ofile_idx;
    if (of_idx >= SCHED_MAX_FDS) {
        return 0;
    }
    if (!slot->ofiles[of_idx].used || slot->ofiles[of_idx].refs == 0u) {
        return 0;
    }
    if (fdent_out) {
        *fdent_out = fdent;
    }
    return &slot->ofiles[of_idx];
}

static int sched_slot_find_free_fd(const sched_task_slot_t *slot, uint32_t start) {
    uint32_t i;
    if (!slot || start >= SCHED_MAX_FDS) {
        return -1;
    }
    for (i = start; i < SCHED_MAX_FDS; i++) {
        if (!slot->fdtab[i].used) {
            return (int)i;
        }
    }
    return -1;
}

static int sched_slot_close_fd(sched_task_slot_t *slot, int32_t fd) {
    sched_fdent_t *fdent;
    sched_ofile_t *of;
    if (!slot) {
        return SCHED_FD_EBADF;
    }
    of = sched_slot_ofile_by_fd(slot, fd, &fdent);
    if (!of) {
        return SCHED_FD_EBADF;
    }
    fdent->used = 0u;
    fdent->ofile_idx = SCHED_FD_OFILE_INVALID;
    fdent->fd_flags = 0u;
    if (of->refs != 0u) {
        of->refs--;
    }
    if (of->refs == 0u) {
        of->used = 0u;
        of->type = SCHED_OFILE_TYPE_NONE;
        of->status_flags = 0u;
    }
    return SCHED_FD_OK;
}

static void sched_slot_close_all_fds(sched_task_slot_t *slot) {
    uint32_t i;
    if (!slot) {
        return;
    }
    for (i = 0u; i < SCHED_MAX_FDS; i++) {
        if (slot->fdtab[i].used) {
            (void)sched_slot_close_fd(slot, (int32_t)i);
        }
    }
}

static sched_task_slot_t *sched_find_by_tid(uint32_t tid) {
    if (tid == 0u) {
        return &g_tasks[0];
    }
    for (uint32_t i = 1u; i < SCHED_MAX_TASKS; i++) {
        sched_task_slot_t *slot = &g_tasks[i];
        if (!slot->used) {
            continue;
        }
        if (slot->pub.tid == tid) {
            return slot;
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
    sched_fd_table_init_stdio(slot);
    g_need_resched = 1u;
    return (int)slot->pub.tid;
}

void sched_init(void) {
    sched_task_slot_t *root;
    for (uint32_t i = 0u; i < SCHED_MAX_TASKS; i++) {
        sched_clear_task(&g_tasks[i]);
    }

    g_ticks = 0u;
    g_need_resched = 0u;
    g_current_idx = 0u;
    g_next_tid = 1u;

    root = &g_tasks[0];
    root->used = 1u;
    root->is_idle = 1u;
    root->entry = sched_idle_task;
    root->name = "idle";
    root->parent_tid = -1;
    root->exit_code = 0u;
    sched_waitq_init(&root->child_waitq);
    root->pub.tid = 0u;
    root->pub.state = SCHED_TASK_RUNNABLE;
    root->pub.wake_tick = 0u;
    root->pub.run_ticks = 0u;
    root->pub.arg = 0;
    sched_fd_table_init_stdio(root);

    timer_program_period_us(SCHED_TICK_PERIOD_US);
}

uint32_t sched_tick_period_us(void) {
    return SCHED_TICK_PERIOD_US;
}

void schedule_tick(void) {
    g_ticks++;
    sched_try_wake_sleepers((uint32_t)g_ticks);

    if (g_current_idx < SCHED_MAX_TASKS) {
        sched_task_slot_t *slot = &g_tasks[g_current_idx];
        if (slot->used && !slot->is_idle && slot->pub.state == SCHED_TASK_RUNNING) {
            slot->quantum_used++;
            if (slot->quantum_used >= SCHED_QUANTUM_TICKS) {
                g_need_resched = 1u;
            }
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
    sched_slot_close_all_fds(slot);
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

int sched_fd_close(int32_t fd) {
    return sched_slot_close_fd(sched_current_slot(), fd);
}

int sched_fd_dup(int32_t oldfd) {
    int newfd;
    sched_fdent_t *oldfdent;
    sched_ofile_t *of;
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot) {
        return SCHED_FD_EBADF;
    }
    of = sched_slot_ofile_by_fd(slot, oldfd, &oldfdent);
    if (!of) {
        return SCHED_FD_EBADF;
    }
    newfd = sched_slot_find_free_fd(slot, 0u);
    if (newfd < 0) {
        return SCHED_FD_EMFILE;
    }
    slot->fdtab[(uint32_t)newfd].used = 1u;
    slot->fdtab[(uint32_t)newfd].ofile_idx = oldfdent->ofile_idx;
    slot->fdtab[(uint32_t)newfd].fd_flags = 0u;
    of->refs++;
    return newfd;
}

int sched_fd_dup2(int32_t oldfd, int32_t newfd) {
    sched_fdent_t *oldfdent;
    sched_ofile_t *of;
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot) {
        return SCHED_FD_EBADF;
    }
    if (newfd < 0 || (uint32_t)newfd >= SCHED_MAX_FDS) {
        return SCHED_FD_EINVAL;
    }
    of = sched_slot_ofile_by_fd(slot, oldfd, &oldfdent);
    if (!of) {
        return SCHED_FD_EBADF;
    }
    if (oldfd == newfd) {
        return newfd;
    }
    if (slot->fdtab[(uint32_t)newfd].used) {
        (void)sched_slot_close_fd(slot, newfd);
    }
    slot->fdtab[(uint32_t)newfd].used = 1u;
    slot->fdtab[(uint32_t)newfd].ofile_idx = oldfdent->ofile_idx;
    slot->fdtab[(uint32_t)newfd].fd_flags = 0u;
    of->refs++;
    return newfd;
}

int sched_fd_fcntl_getfl(int32_t fd, uint32_t *out_flags) {
    sched_ofile_t *of = sched_slot_ofile_by_fd(sched_current_slot(), fd, 0);
    if (!out_flags) {
        return SCHED_FD_EINVAL;
    }
    if (!of) {
        return SCHED_FD_EBADF;
    }
    *out_flags = of->status_flags;
    return SCHED_FD_OK;
}

int sched_fd_fcntl_getfd(int32_t fd, uint32_t *out_flags) {
    sched_fdent_t *fdent = 0;
    if (!out_flags) {
        return SCHED_FD_EINVAL;
    }
    if (!sched_slot_ofile_by_fd(sched_current_slot(), fd, &fdent)) {
        return SCHED_FD_EBADF;
    }
    *out_flags = fdent->fd_flags;
    return SCHED_FD_OK;
}

int sched_fd_fcntl_setfd(int32_t fd, uint32_t flags) {
    sched_fdent_t *fdent = 0;
    if (!sched_slot_ofile_by_fd(sched_current_slot(), fd, &fdent)) {
        return SCHED_FD_EBADF;
    }
    fdent->fd_flags = flags & SCHED_FD_CLOEXEC;
    return SCHED_FD_OK;
}

int sched_fd_fcntl_setfl(int32_t fd, uint32_t flags) {
    sched_ofile_t *of = sched_slot_ofile_by_fd(sched_current_slot(), fd, 0);
    if (!of) {
        return SCHED_FD_EBADF;
    }
    of->status_flags = (of->status_flags & ~SCHED_FD_O_NONBLOCK) | (flags & SCHED_FD_O_NONBLOCK);
    return SCHED_FD_OK;
}

uint32_t sched_fd_can_read(int32_t fd) {
    sched_ofile_t *of = sched_slot_ofile_by_fd(sched_current_slot(), fd, 0);
    if (!of) {
        return 0u;
    }
    return (of->type == SCHED_OFILE_TYPE_STDIN) ? 1u : 0u;
}

uint32_t sched_fd_can_write(int32_t fd) {
    sched_ofile_t *of = sched_slot_ofile_by_fd(sched_current_slot(), fd, 0);
    if (!of) {
        return 0u;
    }
    return (of->type == SCHED_OFILE_TYPE_STDOUT || of->type == SCHED_OFILE_TYPE_STDERR) ? 1u : 0u;
}

uint32_t sched_fd_is_nonblock(int32_t fd) {
    sched_ofile_t *of = sched_slot_ofile_by_fd(sched_current_slot(), fd, 0);
    if (!of) {
        return 0u;
    }
    return ((of->status_flags & SCHED_FD_O_NONBLOCK) != 0u) ? 1u : 0u;
}

uint32_t sched_fd_is_open(int32_t fd) {
    sched_task_slot_t *slot = sched_current_slot();
    if (!slot || fd < 0 || (uint32_t)fd >= SCHED_MAX_FDS) {
        return 0u;
    }
    return slot->fdtab[(uint32_t)fd].used ? 1u : 0u;
}

uint32_t sched_fd_is_stdin(int32_t fd) {
    sched_ofile_t *of = sched_slot_ofile_by_fd(sched_current_slot(), fd, 0);
    if (!of) {
        return 0u;
    }
    return (of->type == SCHED_OFILE_TYPE_STDIN) ? 1u : 0u;
}

uint32_t sched_fd_is_tty(int32_t fd) {
    sched_ofile_t *of = sched_slot_ofile_by_fd(sched_current_slot(), fd, 0);
    if (!of) {
        return 0u;
    }
    return (of->type == SCHED_OFILE_TYPE_STDIN ||
            of->type == SCHED_OFILE_TYPE_STDOUT ||
            of->type == SCHED_OFILE_TYPE_STDERR) ? 1u : 0u;
}

void sched_pump_once(void) {
    uint32_t prev_idx = g_current_idx;
    uint32_t next = sched_pick_next_idx();
    sched_task_slot_t *slot;
    if (next >= SCHED_MAX_TASKS) {
        return;
    }
    slot = &g_tasks[next];
    if (!slot->used || !slot->entry || slot->pub.state != SCHED_TASK_RUNNABLE) {
        return;
    }
    g_current_idx = next;
    slot->pub.state = SCHED_TASK_RUNNING;
    slot->pub.run_ticks++;
    slot->entry(&slot->pub, slot->pub.arg);
    if (slot->pub.state == SCHED_TASK_RUNNING) {
        slot->pub.state = SCHED_TASK_RUNNABLE;
    }
    g_current_idx = prev_idx;
}

void sched_block_until_runnable(void) {
    sched_task_slot_t *self = sched_current_slot();
    if (!self || self->is_idle) {
        return;
    }
    while (self->used && (self->pub.state == SCHED_TASK_BLOCKED || self->pub.state == SCHED_TASK_SLEEPING)) {
        sched_pump_once();
        __asm__ __volatile__("" ::: "memory");
    }
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
