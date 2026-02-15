#include "../include/kernel/console.h"
#include "../include/kernel/platform.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/syscall.h"

/*
 * Current VM interrupt model restores caller registers on IRET.
 * So syscall return values are published via a fixed ABI mailbox in RAM.
 * Register return ABI can be added after low-level context plumbing changes.
 */
enum {
    SYSCALL_ABI_OFF_MAGIC = 0x00u,
    SYSCALL_ABI_OFF_VERSION = 0x04u,
    SYSCALL_ABI_OFF_LAST_NR = 0x08u,
    SYSCALL_ABI_OFF_ARG0 = 0x0Cu,
    SYSCALL_ABI_OFF_ARG1 = 0x10u,
    SYSCALL_ABI_OFF_ARG2 = 0x14u,
    SYSCALL_ABI_OFF_ARG3 = 0x18u,
    SYSCALL_ABI_OFF_ARG4 = 0x1Cu,
    SYSCALL_ABI_OFF_ARG5 = 0x20u,
    SYSCALL_ABI_OFF_RET = 0x24u,
    SYSCALL_ABI_OFF_ERRNO = 0x28u,
    SYSCALL_ABI_OFF_TICK = 0x2Cu
};

enum {
    ERRNO_OK = 0u,
    ERRNO_EINTR = 4u,
    ERRNO_EBADF = 9u,
    ERRNO_ECHILD = 10u,
    ERRNO_EAGAIN = 11u,
    ERRNO_EFAULT = 14u,
    ERRNO_EINVAL = 22u,
    ERRNO_ENOSYS = 38u
};

typedef struct syscall_timespec32 {
    int32_t tv_sec;
    int32_t tv_nsec;
} syscall_timespec32_t;

enum {
    NS_PER_SEC = 1000000000u,
    US_PER_SEC = 1000000u,
    NS_PER_US = 1000u
};

static inline void abi_write32(uint32_t off, uint32_t v) {
    *(volatile uint32_t *)(uintptr_t)(SYSCALL_ABI_ADDR + off) = v;
}

static inline uint32_t abi_ptr_range_ok(uint32_t addr, uint32_t len) {
    if (addr == 0u || len == 0u) {
        return 0u;
    }
    if (addr >= KERNEL_MEM_SIZE) {
        return 0u;
    }
    return (len <= (KERNEL_MEM_SIZE - addr)) ? 1u : 0u;
}

static inline uint32_t abi_user_write32(uint32_t addr, uint32_t v) {
    if (!abi_ptr_range_ok(addr, 4u)) {
        return 0u;
    }
    *(volatile uint32_t *)(uintptr_t)addr = v;
    return 1u;
}

static inline uint32_t abi_user_write_bytes(uint32_t addr, const uint8_t *src, uint32_t len) {
    uint32_t i;
    if (!src || len == 0u) {
        return 1u;
    }
    if (!abi_ptr_range_ok(addr, len)) {
        return 0u;
    }
    for (i = 0u; i < len; i++) {
        *(volatile uint8_t *)(uintptr_t)(addr + i) = src[i];
    }
    return 1u;
}

static inline uint32_t abi_user_read_bytes(uint32_t addr, uint8_t *dst, uint32_t len) {
    uint32_t i;
    if (!dst || len == 0u) {
        return 1u;
    }
    if (!abi_ptr_range_ok(addr, len)) {
        return 0u;
    }
    for (i = 0u; i < len; i++) {
        dst[i] = *(volatile uint8_t *)(uintptr_t)(addr + i);
    }
    return 1u;
}

static inline uint32_t abi_user_read_timespec(uint32_t addr, syscall_timespec32_t *out) {
    if (!out || !abi_ptr_range_ok(addr, 8u)) {
        return 0u;
    }
    out->tv_sec = *(volatile int32_t *)(uintptr_t)(addr + 0u);
    out->tv_nsec = *(volatile int32_t *)(uintptr_t)(addr + 4u);
    return 1u;
}

static inline uint32_t abi_user_write_timespec_zero(uint32_t addr) {
    if (addr == 0u) {
        return 1u;
    }
    if (!abi_ptr_range_ok(addr, 8u)) {
        return 0u;
    }
    *(volatile uint32_t *)(uintptr_t)(addr + 0u) = 0u;
    *(volatile uint32_t *)(uintptr_t)(addr + 4u) = 0u;
    return 1u;
}

static inline uint32_t div_ceil_u32(uint32_t numer, uint32_t denom) {
    if (denom == 0u) {
        return 0xFFFFFFFFu;
    }
    return (numer / denom) + ((numer % denom) ? 1u : 0u);
}

static inline void syscall_publish_result(const syscall_regs_t *regs, uint32_t ret, uint32_t err) {
    abi_write32(SYSCALL_ABI_OFF_MAGIC, SYSCALL_ABI_MAGIC);
    abi_write32(SYSCALL_ABI_OFF_VERSION, SYSCALL_ABI_VERSION);
    abi_write32(SYSCALL_ABI_OFF_LAST_NR, regs ? regs->nr : 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG0, regs ? regs->arg0 : 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG1, regs ? regs->arg1 : 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG2, regs ? regs->arg2 : 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG3, regs ? regs->arg3 : 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG4, regs ? regs->arg4 : 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG5, regs ? regs->arg5 : 0u);
    abi_write32(SYSCALL_ABI_OFF_RET, ret);
    abi_write32(SYSCALL_ABI_OFF_ERRNO, err);
    abi_write32(SYSCALL_ABI_OFF_TICK, sched_ticks());
}

void syscall_init(void) {
    abi_write32(SYSCALL_ABI_OFF_MAGIC, SYSCALL_ABI_MAGIC);
    abi_write32(SYSCALL_ABI_OFF_VERSION, SYSCALL_ABI_VERSION);
    abi_write32(SYSCALL_ABI_OFF_LAST_NR, 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG0, 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG1, 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG2, 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG3, 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG4, 0u);
    abi_write32(SYSCALL_ABI_OFF_ARG5, 0u);
    abi_write32(SYSCALL_ABI_OFF_RET, 0u);
    abi_write32(SYSCALL_ABI_OFF_ERRNO, 0u);
    abi_write32(SYSCALL_ABI_OFF_TICK, 0u);
}

uint32_t syscall_dispatch(const syscall_regs_t *regs) {
    uint32_t ret = 0u;
    uint32_t err = ERRNO_OK;
    if (!regs) {
        syscall_publish_result(0, (uint32_t)-1, ERRNO_ENOSYS);
        return (uint32_t)-1;
    }

    switch (regs->nr) {
        case SYS_GETPID:
            ret = (uint32_t)sched_current_tid();
            break;
        case SYS_YIELD:
            sched_yield();
            ret = 0u;
            break;
        case SYS_SLEEP_TICKS:
            sched_sleep_ticks(regs->arg0);
            ret = 0u;
            break;
        case SYS_EXIT:
            sched_exit_code(regs->arg0);
            ret = 0u;
            break;
        case SYS_WAITPID: {
            int32_t pid = (int32_t)regs->arg0;
            uint32_t status_addr = regs->arg1;
            uint32_t options = regs->arg2;
            uint32_t status = 0u;
            int rc;

            if (pid == 0 || pid < SCHED_WAITPID_ANY) {
                err = ERRNO_EINVAL;
                ret = (uint32_t)-1;
                break;
            }
            if ((options & ~SYS_WAITPID_WNOHANG) != 0u) {
                err = ERRNO_EINVAL;
                ret = (uint32_t)-1;
                break;
            }

            rc = sched_waitpid(pid,
                               (options & SYS_WAITPID_WNOHANG) ? SCHED_WAITPID_WNOHANG : 0u,
                               &status);
            if (rc > 0) {
                if (status_addr != 0u && !abi_user_write32(status_addr, status)) {
                    err = ERRNO_EFAULT;
                    ret = (uint32_t)-1;
                    break;
                }
                ret = (uint32_t)rc;
                break;
            }
            if (rc == 0) {
                ret = 0u;
                break;
            }
            if (rc == SCHED_WAITPID_NO_CHILD) {
                err = ERRNO_ECHILD;
                ret = (uint32_t)-1;
                break;
            }
            if (rc == SCHED_WAITPID_BLOCKED) {
                /*
                 * Transitional behavior:
                 * caller is parked on child wait-queue; runtime should retry syscall.
                 */
                err = ERRNO_EAGAIN;
                ret = (uint32_t)-1;
                break;
            }
            err = ERRNO_ECHILD;
            ret = (uint32_t)-1;
            break;
        }
        case SYS_NANOSLEEP: {
            syscall_timespec32_t req;
            uint32_t req_addr = regs->arg0;
            uint32_t rem_addr = regs->arg1;
            uint32_t total_us;
            uint32_t tick_us;
            uint32_t req_ns_us;
            uint32_t ticks;

            if (!abi_user_read_timespec(req_addr, &req)) {
                err = ERRNO_EFAULT;
                ret = (uint32_t)-1;
                break;
            }
            if (req.tv_sec < 0 || req.tv_nsec < 0 || req.tv_nsec >= (int32_t)NS_PER_SEC) {
                err = ERRNO_EINVAL;
                ret = (uint32_t)-1;
                break;
            }

            if (!abi_user_write_timespec_zero(rem_addr)) {
                err = ERRNO_EFAULT;
                ret = (uint32_t)-1;
                break;
            }

            if (req.tv_sec == 0 && req.tv_nsec == 0) {
                ret = 0u;
                break;
            }

            tick_us = sched_tick_period_us();
            if (tick_us == 0u) {
                err = ERRNO_EINTR;
                ret = (uint32_t)-1;
                break;
            }

            /*
             * Convert to microseconds using ceil for sub-us nanos.
             * Keep the whole path in 32-bit math to avoid __udivdi3 dependency.
             */
            if ((uint32_t)req.tv_sec > (0xFFFFFFFFu / US_PER_SEC)) {
                total_us = 0xFFFFFFFFu;
            } else {
                total_us = (uint32_t)req.tv_sec * US_PER_SEC;
                req_ns_us = div_ceil_u32((uint32_t)req.tv_nsec, NS_PER_US);
                if (total_us > 0xFFFFFFFFu - req_ns_us) {
                    total_us = 0xFFFFFFFFu;
                } else {
                    total_us += req_ns_us;
                }
            }

            ticks = div_ceil_u32(total_us, tick_us);
            if (ticks == 0u) {
                ticks = 1u;
            }
            sched_sleep_ticks(ticks);
            ret = 0u;
            break;
        }
        case SYS_READ: {
            uint32_t fd = regs->arg0;
            uint32_t buf_addr = regs->arg1;
            uint32_t len = regs->arg2;
            uint32_t flags = regs->arg3;
            uint8_t byte_buf[64];
            uint32_t copied = 0u;
            uint32_t nonblock = (flags & SYS_IO_NONBLOCK) ? 1u : 0u;

            if (fd != 0u) {
                err = ERRNO_EBADF;
                ret = (uint32_t)-1;
                break;
            }
            if (len == 0u) {
                ret = 0u;
                break;
            }
            if (buf_addr == 0u || !abi_ptr_range_ok(buf_addr, len)) {
                err = ERRNO_EFAULT;
                ret = (uint32_t)-1;
                break;
            }

            while (copied < len) {
                uint32_t remain = len - copied;
                uint32_t want = (remain > (uint32_t)sizeof(byte_buf)) ? (uint32_t)sizeof(byte_buf) : remain;
                /*
                 * If some bytes are already copied, do not block for the next chunk.
                 * This preserves short-read behavior instead of potentially sleeping.
                 */
                int n = console_read(byte_buf, want, (copied != 0u) ? 1u : nonblock);
                if (n == CONSOLE_IO_BLOCKED) {
                    if (copied == 0u) {
                        err = ERRNO_EAGAIN;
                        ret = (uint32_t)-1;
                    } else {
                        ret = copied;
                    }
                    break;
                }
                if (n == 0) {
                    if (copied != 0u) {
                        ret = copied;
                    } else if (nonblock) {
                        err = ERRNO_EAGAIN;
                        ret = (uint32_t)-1;
                    } else {
                        ret = 0u;
                    }
                    break;
                }
                if (!abi_user_write_bytes(buf_addr + copied, byte_buf, (uint32_t)n)) {
                    err = ERRNO_EFAULT;
                    ret = (uint32_t)-1;
                    break;
                }
                copied += (uint32_t)n;
                ret = copied;
                if ((uint32_t)n < want) {
                    break;
                }
            }
            break;
        }
        case SYS_WRITE: {
            uint32_t fd = regs->arg0;
            uint32_t buf_addr = regs->arg1;
            uint32_t len = regs->arg2;
            uint8_t byte_buf[128];
            uint32_t total = 0u;

            if (fd != 1u && fd != 2u) {
                err = ERRNO_EBADF;
                ret = (uint32_t)-1;
                break;
            }
            if (len == 0u) {
                ret = 0u;
                break;
            }
            if (buf_addr == 0u || !abi_ptr_range_ok(buf_addr, len)) {
                err = ERRNO_EFAULT;
                ret = (uint32_t)-1;
                break;
            }

            while (total < len) {
                uint32_t remain = len - total;
                uint32_t want = (remain > (uint32_t)sizeof(byte_buf)) ? (uint32_t)sizeof(byte_buf) : remain;
                uint32_t written;

                if (!abi_user_read_bytes(buf_addr + total, byte_buf, want)) {
                    err = ERRNO_EFAULT;
                    ret = (uint32_t)-1;
                    break;
                }
                written = console_write(byte_buf, want);
                total += written;
                ret = total;
                if (written < want) {
                    break;
                }
            }
            break;
        }
        default:
            err = ERRNO_ENOSYS;
            ret = (uint32_t)-1;
            break;
    }

    syscall_publish_result(regs, ret, err);
    return ret;
}

void syscall_dispatch_from_irq_regs(uint32_t r0,
                                    uint32_t r1,
                                    uint32_t r2,
                                    uint32_t r3,
                                    uint32_t r4,
                                    uint32_t r5,
                                    uint32_t r6) {
    syscall_regs_t regs;
    regs.nr = r0;
    regs.arg0 = r1;
    regs.arg1 = r2;
    regs.arg2 = r3;
    regs.arg3 = r4;
    regs.arg4 = r5;
    regs.arg5 = r6;
    (void)syscall_dispatch(&regs);
}
