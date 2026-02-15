#include "../include/kernel/console_fb.h"
#include "../include/kernel/platform.h"
#include "../include/kernel/printk.h"
#include "../include/kernel/types.h"

static volatile uint32_t g_klog_level = KERNEL_LOG_LEVEL_DEFAULT;

static const char *klog_level_name(uint32_t level) {
    switch (level) {
        case KLOG_LEVEL_ERROR:
            return "ERR";
        case KLOG_LEVEL_WARN:
            return "WRN";
        case KLOG_LEVEL_INFO:
            return "INF";
        case KLOG_LEVEL_DEBUG:
            return "DBG";
        default:
            return "LOG";
    }
}

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

uint32_t klog_should_emit(uint32_t level) {
    if (level == 0u) {
        return 0u;
    }
    return (level <= g_klog_level) ? 1u : 0u;
}

void klog_set_level(uint32_t level) {
    if (level < KLOG_LEVEL_ERROR) {
        level = KLOG_LEVEL_ERROR;
    } else if (level > KLOG_LEVEL_DEBUG) {
        level = KLOG_LEVEL_DEBUG;
    }
    g_klog_level = level;
}

uint32_t klog_get_level(void) {
    return g_klog_level;
}

void klog_prefix(uint32_t level, const char *tag) {
    if (!klog_should_emit(level)) {
        return;
    }
    kputc((uint32_t)'[');
    kputs(klog_level_name(level));
    kputc((uint32_t)']');
    kputc((uint32_t)'[');
    if (tag && tag[0] != '\0') {
        kputs(tag);
    } else {
        kputs("kernel");
    }
    kputc((uint32_t)']');
    kputc((uint32_t)' ');
}

void klog_line(uint32_t level, const char *tag, const char *msg) {
    if (!klog_should_emit(level)) {
        return;
    }
    klog_prefix(level, tag);
    if (msg) {
        kputs(msg);
    }
    kputc((uint32_t)'\n');
}
