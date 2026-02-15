#include "../include/kernel/console_fb.h"
#include "../include/kernel/irq.h"
#include "../include/kernel/panic.h"
#include "../include/kernel/printk.h"
#include "../include/kernel/types.h"

static void panic_halt(void) {
    for (;;) {
        __asm__ __volatile__("" ::: "memory");
    }
}

static void kpanic_emit_prefix(void) {
    console_fb_set_colors(0x00FFFFFFu, 0x00800000u);
    console_fb_clear();
    KLOGE("panic", "kernel panic");
}

void kpanic(const char *msg) {
    const trap_frame_t *tf = irq_last_trap_frame();
    kpanic_emit_prefix();
    klog_prefix(KLOG_LEVEL_ERROR, "panic");
    kputs("reason=");
    if (msg) {
        kputs(msg);
    } else {
        kputs("<null>");
    }
    kputs("\n");
    if (tf) {
        klog_prefix(KLOG_LEVEL_ERROR, "panic");
        kputs("last_irq=");
        kprint_u32(tf->irq_no);
        kputs(" dispatch_count=");
        kprint_u32(tf->dispatch_count);
        kputs(" ticks=");
        kprint_u32(tf->tick_snapshot);
        kputs("\n");
    }
    klog_prefix(KLOG_LEVEL_ERROR, "panic");
    kputs("rx_dropped=");
    kprint_u32(irq_input_dropped());
    kputs("\n");
    panic_halt();
}
