#include "../include/kernel/console.h"
#include "../include/kernel/console_fb.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/types.h"

enum {
    CONSOLE_RX_CAP = 256u,
    CONSOLE_RX_MASK = CONSOLE_RX_CAP - 1u,
    TTY_LFLAG_SUPPORTED = TTY_LFLAG_ECHO | TTY_LFLAG_ICANON | TTY_LFLAG_ISIG,
    TTY_CC_VINTR = 0x03u,
    TTY_CC_VEOF = 0x04u,
    TTY_CC_VERASE_BS = 0x08u,
    TTY_CC_VERASE_DEL = 0x7Fu,
    TTY_CC_VKILL = 0x15u
};

static volatile uint32_t g_rx_head;
static volatile uint32_t g_rx_tail;
static volatile uint32_t g_rx_dropped;
static volatile uint32_t g_rx_lines;
static volatile uint32_t g_rx_eofs;
static volatile uint32_t g_tty_lflag;
static uint8_t g_rx_buf[CONSOLE_RX_CAP];
static sched_waitq_t g_rx_waitq;

static void console_echo_data_char(uint8_t c) {
    if ((g_tty_lflag & TTY_LFLAG_ECHO) == 0u) {
        return;
    }
    if (c == (uint8_t)'\a' ||
        c == (uint8_t)'\n' || c == (uint8_t)'\r' ||
        c == (uint8_t)'\t' || c == (uint8_t)'\v' || c == (uint8_t)'\f' ||
        (c >= (uint8_t)' ' && c <= (uint8_t)'~')) {
        console_fb_putc((uint32_t)c);
    }
}

static void console_echo_backspace(void) {
    if ((g_tty_lflag & TTY_LFLAG_ECHO) == 0u) {
        return;
    }
    console_fb_putc((uint32_t)'\b');
}

static void console_echo_intr(void) {
    if ((g_tty_lflag & TTY_LFLAG_ECHO) == 0u) {
        return;
    }
    console_fb_putc((uint32_t)'^');
    console_fb_putc((uint32_t)'C');
    console_fb_putc((uint32_t)'\n');
}

static uint32_t console_rx_pop_last_editable(void) {
    uint32_t head = g_rx_head;
    if (head == g_rx_tail) {
        return 0u;
    }
    {
        uint32_t prev = (head - 1u) & CONSOLE_RX_MASK;
        if (g_rx_buf[prev] == (uint8_t)'\n' || g_rx_buf[prev] == (uint8_t)TTY_CC_VEOF) {
            return 0u;
        }
        g_rx_head = prev;
    }
    return 1u;
}

static void console_rx_enqueue_char(uint8_t c, uint32_t do_echo) {
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
    } else if (c == (uint8_t)TTY_CC_VEOF) {
        g_rx_eofs++;
    }
    if (do_echo) {
        console_echo_data_char(c);
    }
    sched_waitq_wake_one(&g_rx_waitq);
}

void console_init(void) {
    g_rx_head = 0u;
    g_rx_tail = 0u;
    g_rx_dropped = 0u;
    g_rx_lines = 0u;
    g_rx_eofs = 0u;
    g_tty_lflag = TTY_LFLAG_ECHO | TTY_LFLAG_ICANON | TTY_LFLAG_ISIG;
    sched_waitq_init(&g_rx_waitq);
}

void console_rx_feed(uint8_t c) {
    uint32_t canonical = ((g_tty_lflag & TTY_LFLAG_ICANON) != 0u) ? 1u : 0u;
    if (c == (uint8_t)'\r') {
        c = (uint8_t)'\n';
    }
    if (c == (uint8_t)TTY_CC_VINTR && (g_tty_lflag & TTY_LFLAG_ISIG) != 0u) {
        while (console_rx_pop_last_editable()) {
            console_echo_backspace();
        }
        console_echo_intr();
        console_rx_enqueue_char((uint8_t)'\n', 0u);
        return;
    }
    if (canonical && c == (uint8_t)TTY_CC_VKILL) {
        while (console_rx_pop_last_editable()) {
            console_echo_backspace();
        }
        return;
    }
    if (canonical &&
        (c == (uint8_t)TTY_CC_VERASE_BS || c == (uint8_t)TTY_CC_VERASE_DEL)) {
        if (console_rx_pop_last_editable()) {
            console_echo_backspace();
        }
        return;
    }
    if (canonical && c == (uint8_t)TTY_CC_VEOF) {
        console_rx_enqueue_char(c, 0u);
        return;
    }
    if (c == 0u) {
        return;
    }

    console_rx_enqueue_char(c, 1u);
}

uint32_t console_rx_dropped(void) {
    return g_rx_dropped;
}

uint32_t console_rx_lines(void) {
    return g_rx_lines;
}

uint32_t console_can_read(void) {
    if ((g_tty_lflag & TTY_LFLAG_ICANON) != 0u) {
        return (g_rx_lines != 0u || g_rx_eofs != 0u) ? 1u : 0u;
    }
    return (g_rx_head != g_rx_tail) ? 1u : 0u;
}

int console_wait_readable(uint32_t timeout_ticks, uint32_t nonblock) {
    if (console_can_read()) {
        return CONSOLE_IO_OK;
    }
    if (nonblock) {
        return 0;
    }
    sched_waitq_sleep(&g_rx_waitq, timeout_ticks);
    return CONSOLE_IO_BLOCKED;
}

uint32_t console_tty_get_lflag(void) {
    return g_tty_lflag;
}

uint32_t console_tty_set_lflag(uint32_t lflag) {
    g_tty_lflag = lflag & TTY_LFLAG_SUPPORTED;
    return g_tty_lflag;
}

int console_read(uint8_t *dst, uint32_t len, uint32_t nonblock) {
    uint32_t n = 0u;
    uint32_t canonical;
    uint32_t saw_eof = 0u;
    if (!dst || len == 0u) {
        return 0;
    }

    canonical = (g_tty_lflag & TTY_LFLAG_ICANON) ? 1u : 0u;
    if (canonical && g_rx_lines == 0u && g_rx_eofs == 0u) {
        if (nonblock) {
            return 0;
        }
        sched_waitq_sleep(&g_rx_waitq, 0u);
        return CONSOLE_IO_BLOCKED;
    }

    while (n < len) {
        uint32_t tail = g_rx_tail;
        if (tail == g_rx_head) {
            break;
        }
        {
            uint8_t c = g_rx_buf[tail];
            if (canonical && c == (uint8_t)TTY_CC_VEOF) {
                if (g_rx_eofs != 0u) {
                    g_rx_eofs--;
                }
                g_rx_tail = (tail + 1u) & CONSOLE_RX_MASK;
                saw_eof = 1u;
                break;
            }
            dst[n++] = c;
            if (c == (uint8_t)'\n' && g_rx_lines != 0u) {
                g_rx_lines--;
                if (canonical) {
                    g_rx_tail = (tail + 1u) & CONSOLE_RX_MASK;
                    break;
                }
            }
        }
        g_rx_tail = (tail + 1u) & CONSOLE_RX_MASK;
    }

    if (n != 0u) {
        return (int)n;
    }
    if (canonical && saw_eof) {
        return 0;
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
