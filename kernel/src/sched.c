#include "../include/kernel/irq.h"
#include "../include/kernel/platform.h"
#include "../include/kernel/sched.h"

#define SCHED_TICK_PERIOD_US 50000u

static inline void timer_program_period_us(uint32_t period_us) {
    *(volatile uint32_t *)(uintptr_t)TIMER_MMIO_BASE = period_us;
}

static volatile unsigned int g_ticks;
static volatile unsigned int g_need_resched;

static void sched_idle(void) {
    __asm__ __volatile__("" ::: "memory");
}

void sched_init(void) {
    g_ticks = 0;
    g_need_resched = 0;
    timer_program_period_us(SCHED_TICK_PERIOD_US);
}

void schedule_tick(void) {
    g_ticks++;

    /*
     * Keep a simple fixed-time-slice reschedule signal for the bootstrap path.
     * Real scheduler will replace this with per-task accounting.
     */
    if ((g_ticks & 0x7u) == 0u) {
        g_need_resched = 1;
    }
}

unsigned int sched_ticks(void) {
    return g_ticks;
}

void sched_run(void) {
    for (;;) {
        irq_poll_input_echo();
        if (g_need_resched) {
            g_need_resched = 0;
            /*
             * Placeholder for future context switch:
             * next = pick_next_task();
             * switch_to(next);
             */
        }
        sched_idle();
    }
}
