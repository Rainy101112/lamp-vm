#ifndef LAMP_KERNEL_CONSOLE_H
#define LAMP_KERNEL_CONSOLE_H

#include "kernel/types.h"

enum {
    CONSOLE_IO_OK = 0,
    CONSOLE_IO_BLOCKED = -1
};

void console_init(void);
void console_rx_feed(uint8_t c);
uint32_t console_rx_dropped(void);
uint32_t console_rx_lines(void);

int console_read(uint8_t *dst, uint32_t len, uint32_t nonblock);
uint32_t console_write(const uint8_t *src, uint32_t len);

#endif
