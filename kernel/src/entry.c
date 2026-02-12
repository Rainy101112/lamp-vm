#include "../include/kernel/console_fb.h"
#include "../include/kernel/kernel.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/smp.h"
#include "../include/kernel/trap.h"
#include "../include/kernel/types.h"

static volatile uint32_t g_kernel_booted;
static char g_msg_boot[] = "LAMP KERNEL V0.01 BUILD 2 12 2026\n";
static char g_msg_trap[] = "TRAP: READY\n";
static char g_msg_smp[] = "SMP: BSP ONLINE\n";
static char g_msg_sched[] = "SCHED: START\n";

static void kernel_early_init(void) {
    g_kernel_booted = 0u;
}

static void kernel_late_init(void) {
    g_kernel_booted = 1u;
}

void kernel_entry(void) {
    kernel_early_init();
    console_fb_init();
    console_fb_puts(g_msg_boot);

    /* Kernel owns IVT policy after BIOS handoff. */
    trap_init();
    console_fb_puts(g_msg_trap);

    /* Keep single-core path first, then expand to SMP. */
    smp_init_bsp();
    console_fb_puts(g_msg_smp);

    sched_init();
    kernel_late_init();
    console_fb_puts(g_msg_sched);
    sched_run();

    for (;;) {
        __asm__ __volatile__("" ::: "memory");
    }
}
