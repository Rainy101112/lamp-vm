#include "../include/kernel/console_fb.h"
#include "../include/kernel/printk.h"
#include "../include/kernel/types.h"

void kputc(uint32_t c) {
    console_fb_putc(c);
}

void kputs(const char *s) {
    console_fb_puts(s);
}

void kprintf(const char *s) {
    kputs(s);
}

void kprint_u32(uint32_t v) {
    /* Backend currently has unstable lowering for unsigned div/cmp patterns. */
    kprint_hex32(v);
}

void kprint_hex32(uint32_t v) {
    static const char hexdig[] = "0123456789ABCDEF";
    kputc((uint32_t)'0');
    kputc((uint32_t)'x');
    for (int shift = 28; shift >= 0; shift -= 4) {
        uint32_t nib = (v >> (uint32_t)shift) & 0xFu;
        kputc((uint32_t)(uint8_t)hexdig[nib]);
    }
}
