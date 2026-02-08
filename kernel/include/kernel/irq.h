#ifndef LAMP_KERNEL_IRQ_H
#define LAMP_KERNEL_IRQ_H

#include "kernel/types.h"

void irq_default(uint32_t irq_no);
void irq_divide_by_zero(uint32_t irq_no);
void irq_disk_complete(uint32_t irq_no);
void irq_serial(uint32_t irq_no);
void irq_keyboard(uint32_t irq_no);

void irq_common_entry(uint32_t irq_no);

#endif
