#include "../include/kernel/console.h"
#include "../include/kernel/console_fb.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/types.h"

enum {
    CONSOLE_RX_CAP = 256u,
    CONSOLE_RX_MASK = CONSOLE_RX_CAP - 1u
};

static volatile uint32_t g_rx_head;
static volatile uint32_t g_rx_tail;
static volatile uint32_t g_rx_dropped;
static volatile uint32_t g_rx_lines;
static uint8_t g_rx_buf[CONSOLE_RX_CAP];
static sched_waitq_t g_rx_waitq;

void console_init(void) {
    g_rx_head = 0u;
    g_rx_tail = 0u;
    g_rx_dropped = 0u;
    g_rx_lines = 0u;
    sched_waitq_init(&g_rx_waitq);
}

void console_rx_feed(uint8_t c) {
    if (c == (uint8_t)'\r') {
        c = (uint8_t)'\n';
    }
    if (c == (uint8_t)'\b' || c == 0x7Fu) {
        uint32_t head = g_rx_head;
        if (head != g_rx_tail) {
            uint32_t prev = (head - 1u) & CONSOLE_RX_MASK;
            if (g_rx_buf[prev] != (uint8_t)'\n') {
                g_rx_head = prev;
            }
        }
        return;
    }
    if (c == 0u) {
        return;
    }

    uint32_t head = g_rx_head;
    uint32_t next = (head + 1u) & CONSOLE_RX_MASK;
    if (next == g_rx_tail) {
        g_rx_dropped++;
        return;
    }

    g_rx_buf[head] = c;
    g_rx_head = next;
    if (c == (uint8_t)'\n') {
        g_rx_lines++;
    }
    sched_waitq_wake_one(&g_rx_waitq);
}

uint32_t console_rx_dropped(void) {
    return g_rx_dropped;
}

uint32_t console_rx_lines(void) {
    return g_rx_lines;
}

int console_read(uint8_t *dst, uint32_t len, uint32_t nonblock) {
    uint32_t n = 0u;
    if (!dst || len == 0u) {
        return 0;
    }

    while (n < len) {
        uint32_t tail = g_rx_tail;
        if (tail == g_rx_head) {
            break;
        }
        {
            uint8_t c = g_rx_buf[tail];
            dst[n++] = c;
            if (c == (uint8_t)'\n' && g_rx_lines != 0u) {
                g_rx_lines--;
            }
        }
        g_rx_tail = (tail + 1u) & CONSOLE_RX_MASK;
    }

    if (n != 0u) {
        return (int)n;
    }
    if (nonblock) {
        return 0;
    }

    sched_waitq_sleep(&g_rx_waitq, 0u);
    return CONSOLE_IO_BLOCKED;
}

uint32_t console_write(const uint8_t *src, uint32_t len) {
    uint32_t n = 0u;
    if (!src || len == 0u) {
        return 0u;
    }
    for (n = 0u; n < len; n++) {
        console_fb_putc((uint32_t)src[n]);
    }
    return n;
}
