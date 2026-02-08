#include "../include/kernel/kernel.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/smp.h"
#include "../include/kernel/trap.h"
#include "../include/kernel/types.h"

static volatile uint32_t g_kernel_booted;

static void kernel_early_init(void) {
    g_kernel_booted = 0u;
}

static void kernel_late_init(void) {
    g_kernel_booted = 1u;
}

void kernel_entry(void) {
    kernel_early_init();

    /* Kernel owns IVT policy after BIOS handoff. */
    trap_init();

    /* Keep single-core path first, then expand to SMP. */
    smp_init_bsp();

    sched_init();
    kernel_late_init();
    sched_run();

    for (;;) {
        __asm__ __volatile__("" ::: "memory");
    }
}
