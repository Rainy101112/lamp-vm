#include "../include/kernel/trap.h"
#include "../include/kernel/irq.h"
#include "../include/kernel/platform.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/types.h"

static trap_handler_t g_trap_table[KERNEL_IVT_SIZE];
static volatile uint32_t g_trap_ready;
static volatile uint32_t g_irq_mask[KERNEL_IVT_SIZE / 32u];

void trap_register(uint32_t irq_no, trap_handler_t handler) {
    if (irq_no >= KERNEL_IVT_SIZE) {
        return;
    }
    g_trap_table[irq_no] = handler ? handler : irq_default;
}

void trap_init(void) {
    for (uint32_t i = 0; i < KERNEL_IVT_SIZE; i++) {
        g_trap_table[i] = irq_default;
    }
    for (uint32_t i = 0; i < (KERNEL_IVT_SIZE / 32u); i++) {
        g_irq_mask[i] = 0u;
    }

    trap_register(IRQ_DIVIDE_BY_ZERO, irq_divide_by_zero);
    trap_register(IRQ_DISK_COMPLETE, irq_disk_complete);
    trap_register(IRQ_SERIAL, irq_serial);
    trap_register(IRQ_KEYBOARD, irq_keyboard);

    g_irq_mask[IRQ_DIVIDE_BY_ZERO / 32u] |= (1u << (IRQ_DIVIDE_BY_ZERO % 32u));
    g_irq_mask[IRQ_DISK_COMPLETE / 32u] |= (1u << (IRQ_DISK_COMPLETE % 32u));
    g_irq_mask[IRQ_SERIAL / 32u] |= (1u << (IRQ_SERIAL % 32u));
    g_irq_mask[IRQ_KEYBOARD / 32u] |= (1u << (IRQ_KEYBOARD % 32u));

    g_trap_ready = 1u;
}

void trap_dispatch(uint32_t irq_no) {
    if (!g_trap_ready || irq_no >= KERNEL_IVT_SIZE) {
        return;
    }

    if ((g_irq_mask[irq_no / 32u] & (1u << (irq_no % 32u))) == 0u) {
        return;
    }

    g_trap_table[irq_no](irq_no);
    schedule_tick();
}
