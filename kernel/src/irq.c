#include "../include/kernel/console_fb.h"
#include "../include/kernel/irq.h"
#include "../include/kernel/panic.h"
#include "../include/kernel/platform.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/trap.h"
#include "../include/kernel/types.h"

volatile uint32_t g_irq_stub_no;
static volatile trap_frame_t g_last_trap_frame;
static volatile uint32_t g_irq_counts[KERNEL_IVT_SIZE];
static volatile uint32_t g_fault_div0;

static volatile uint8_t g_rx_byte;
static volatile uint32_t g_rx_pending;
static volatile uint32_t g_rx_dropped;

static inline uint32_t io_in32(uint32_t addr) {
    uint32_t v;
    __asm__ volatile ("in %0, %1" : "=r"(v) : "r"(addr));
    return v;
}

static inline void io_out32(uint32_t addr, uint32_t value) {
    __asm__ volatile ("out %0, %1" :: "r"(value), "r"(addr));
}

static void serial_drain_rx(void) {
    uint32_t status = io_in32(IO_SERIAL_STATUS);
    if ((status & SERIAL_STATUS_RX_READY) == 0u) {
        return;
    }
    uint32_t v = io_in32(IO_SERIAL_RX);
    if (g_rx_pending != 0u) {
        g_rx_dropped++;
        return;
    }
    g_rx_byte = (uint8_t)(v & 0xFFu);
    g_rx_pending = 1u;
}

void irq_common_entry(uint32_t irq_no) {
    g_last_trap_frame.irq_no = irq_no;
    g_last_trap_frame.dispatch_count++;
    g_last_trap_frame.tick_snapshot = sched_ticks();
    trap_dispatch(irq_no);
}

void irq_common_entry_from_stub(void) {
    irq_common_entry(g_irq_stub_no);
}

void irq_input_init(void) {
    g_rx_byte = 0u;
    g_rx_pending = 0u;
    g_rx_dropped = 0u;
    io_out32(IO_SERIAL_STATUS, SERIAL_CTRL_RX_INT_ENABLE);
}

void irq_poll_input_echo(void) {
    if (g_rx_pending == 0u) {
        return;
    }

    uint8_t c = g_rx_byte;
    g_rx_pending = 0u;
    if (c == (uint8_t)'\r' || c == (uint8_t)'\n') {
        console_fb_putc((uint32_t)'\n');
    } else if (c == (uint8_t)'\t') {
        console_fb_putc((uint32_t)'\t');
    } else if (c >= (uint8_t)' ' && c <= (uint8_t)'~') {
        console_fb_putc((uint32_t)c);
    } else {
        // Ignore non-printable control bytes to avoid spurious cursor advance/scroll.
    }
}

uint32_t irq_input_dropped(void) {
    return g_rx_dropped;
}

const trap_frame_t *irq_last_trap_frame(void) {
    return (const trap_frame_t *)&g_last_trap_frame;
}

void irq_default(uint32_t irq_no) {
    if (irq_no < KERNEL_IVT_SIZE) {
        g_irq_counts[irq_no]++;
    }
}

void irq_divide_by_zero(uint32_t irq_no) {
    (void)irq_no;
    g_fault_div0 = 1u;
    kpanic("divide by zero");
}

void irq_disk_complete(uint32_t irq_no) {
    if (irq_no < KERNEL_IVT_SIZE) {
        g_irq_counts[irq_no]++;
    }
}

void irq_serial(uint32_t irq_no) {
    if (irq_no < KERNEL_IVT_SIZE) {
        g_irq_counts[irq_no]++;
    }
    serial_drain_rx();
}

void irq_keyboard(uint32_t irq_no) {
    if (irq_no < KERNEL_IVT_SIZE) {
        g_irq_counts[irq_no]++;
    }
    serial_drain_rx();
}

void irq_timer(uint32_t irq_no) {
    if (irq_no < KERNEL_IVT_SIZE) {
        g_irq_counts[irq_no]++;
    }
    schedule_tick();
}

__asm__(
    ".text\n"
    ".globl irq_stub_entry\n"
    "irq_stub_entry:\n"
    "  mov r2, r31\n"
    "  movi r30, 0x003FF000\n"
    "  mov r31, r30\n"
    "  movi r0, g_irq_stub_no\n"
    "  store32 r2, r0, 0\n"
    "  call irq_common_entry_from_stub\n"
    "  iret\n"
);
