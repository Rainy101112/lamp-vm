#include "../include/kernel/irq.h"
#include "../include/kernel/trap.h"
#include "../include/kernel/types.h"

static volatile uint32_t g_irq_counts[256];
static volatile uint32_t g_fault_div0;

/*
 * VM contract: r31 carries interrupt vector number on ISR entry.
 * Real ISR stubs should preserve context, then call trap_dispatch(irq).
 */
void irq_common_entry(uint32_t irq_no) {
    trap_dispatch(irq_no);
}

void irq_default(uint32_t irq_no) {
    if (irq_no < 256u) {
        g_irq_counts[irq_no]++;
    }
}

void irq_divide_by_zero(uint32_t irq_no) {
    (void)irq_no;
    g_fault_div0 = 1u;
    for (;;) {
        __asm__ __volatile__("" ::: "memory");
    }
}

void irq_disk_complete(uint32_t irq_no) {
    if (irq_no < 256u) {
        g_irq_counts[irq_no]++;
    }
}

void irq_serial(uint32_t irq_no) {
    if (irq_no < 256u) {
        g_irq_counts[irq_no]++;
    }
}

void irq_keyboard(uint32_t irq_no) {
    if (irq_no < 256u) {
        g_irq_counts[irq_no]++;
    }
}
