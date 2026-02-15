#ifndef LAMP_KERNEL_SYSCALL_H
#define LAMP_KERNEL_SYSCALL_H

#include "kernel/types.h"

enum {
    SYS_GETPID = 0u,
    SYS_YIELD = 1u,
    SYS_SLEEP_TICKS = 2u,
    SYS_EXIT = 3u,
    SYS_WAITPID = 4u,
    SYS_NANOSLEEP = 5u,
    SYS_READ = 6u,
    SYS_WRITE = 7u
};

enum {
    SYS_WAITPID_WNOHANG = 1u,
    SYS_IO_NONBLOCK = 1u
};

typedef struct syscall_regs {
    uint32_t nr;
    uint32_t arg0;
    uint32_t arg1;
    uint32_t arg2;
    uint32_t arg3;
    uint32_t arg4;
    uint32_t arg5;
} syscall_regs_t;

void syscall_init(void);
uint32_t syscall_dispatch(const syscall_regs_t *regs);
void syscall_dispatch_from_irq_regs(uint32_t r0,
                                    uint32_t r1,
                                    uint32_t r2,
                                    uint32_t r3,
                                    uint32_t r4,
                                    uint32_t r5,
                                    uint32_t r6);

#endif
