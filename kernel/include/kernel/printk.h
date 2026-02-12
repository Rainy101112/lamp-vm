#ifndef LAMP_KERNEL_PRINTK_H
#define LAMP_KERNEL_PRINTK_H

#include "kernel/types.h"

void kputc(uint32_t c);
void kputs(const char *s);
void kprintf(const char *s);
void kprint_u32(uint32_t v);
void kprint_hex32(uint32_t v);

#endif
