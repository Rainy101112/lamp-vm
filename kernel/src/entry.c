#include "../include/kernel/console_fb.h"
#include "../include/kernel/irq.h"
#include "../include/kernel/kernel.h"
#include "../include/kernel/printk.h"
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
    console_fb_init();
    kprintf("LAMP KERNEL V0.03 IRQ+IO+LOG RX-LATCH\n");

    /* Kernel owns IVT policy after BIOS handoff. */
    trap_init();
    irq_input_init();
    kprintf("TRAP: READY, INPUT IRQ ENABLED\n");

    /* Keep single-core path first, then expand to SMP. */
    smp_init_bsp();
    kprintf("SMP: BSP ONLINE\n");

    sched_init();
    kernel_late_init();
    kprintf("SCHED: START (TYPE TO ECHO)\n");
    sched_run();

    for (;;) {
        __asm__ __volatile__("" ::: "memory");
    }
}
