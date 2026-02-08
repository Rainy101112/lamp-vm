#include "../include/kernel/sched.h"

static volatile unsigned int g_ticks;
static volatile unsigned int g_need_resched;

static void sched_idle(void) {
    __asm__ __volatile__("" ::: "memory");
}

void sched_init(void) {
    g_ticks = 0;
    g_need_resched = 0;
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
