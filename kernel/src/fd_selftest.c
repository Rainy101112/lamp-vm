#include "../include/kernel/console.h"
#include "../include/kernel/fd_selftest.h"
#include "../include/kernel/platform.h"
#include "../include/kernel/printk.h"
#include "../include/kernel/sched.h"
#include "../include/kernel/syscall.h"
#include "../include/kernel/types.h"

enum {
    ABI_OFF_LAST_NR = 0x08u,
    ABI_OFF_RET = 0x24u,
    ABI_OFF_ERRNO = 0x28u,
    ABI_OFF_TICK = 0x2Cu
};

enum {
    TEST_ERRNO_EAGAIN = 11u,
    TEST_ERRNO_EBADF = 9u,
    TEST_ERRNO_ENOENT = 2u,
    TEST_ERRNO_ENOTSOCK = 88u,
    TEST_ERRNO_EOPNOTSUPP = 95u,
    TEST_ERRNO_EAFNOSUPPORT = 97u,
    TEST_ERRNO_ENOTCONN = 107u
};

typedef struct fdtest_pollfd32 {
    int32_t fd;
    int16_t events;
    int16_t revents;
} fdtest_pollfd32_t;

static uint8_t g_fdtest_buf[64];
static fdtest_pollfd32_t g_fdtest_pfd;
static uint32_t g_fdtest_sel_read;
static const char g_fdtest_path_dev_null[] = "/dev/null";
static const char g_fdtest_path_dev_zero[] = "/dev/zero";
static const char g_fdtest_path_dev_tty[] = "/dev/tty";
static const char g_fdtest_path_missing[] = "/dev/missing";

static inline uint32_t abi_read32(uint32_t off) {
    return *(volatile uint32_t *)(uintptr_t)(SYSCALL_ABI_ADDR + off);
}

static inline uint32_t ptr32(const void *p) {
    return (uint32_t)(uintptr_t)p;
}

#define FDTEST_SYSCALL(nr_, a0_, a1_, a2_, a3_, a4_, a5_, ret_lval_, err_lval_)       \
    do {                                                                                \
        syscall_regs_t __regs;                                                          \
        (__regs).nr = (nr_);                                                            \
        (__regs).arg0 = (a0_);                                                          \
        (__regs).arg1 = (a1_);                                                          \
        (__regs).arg2 = (a2_);                                                          \
        (__regs).arg3 = (a3_);                                                          \
        (__regs).arg4 = (a4_);                                                          \
        (__regs).arg5 = (a5_);                                                          \
        (void)syscall_dispatch(&__regs);                                                \
        (ret_lval_) = abi_read32(ABI_OFF_RET);                                          \
        (err_lval_) = (((int32_t)(ret_lval_)) == -1) ? abi_read32(ABI_OFF_ERRNO) : 0u; \
    } while (0)
static void fdtest_drain_stdin(void) {
    uint8_t buf[32];
    while (console_read(buf, (uint32_t)sizeof(buf), 1u) > 0) {
    }
}

static void fdtest_fail(const char *msg, uint32_t got, uint32_t want, uint32_t *fails) {
    if (fails) {
        (*fails)++;
    }
    klog_prefix(KLOG_LEVEL_ERROR, "fdtest");
    kputs(msg);
    kputs(" got=");
    kprint_hex32(got);
    kputs(" want=");
    kprint_hex32(want);
    kputc((uint32_t)'\n');
}

void fd_selftest_run(void) {
    uint32_t fails = 0u;
    uint32_t ret = 0u;
    uint32_t err = 0u;
    uint32_t dupfd;
    uint32_t fd;
    uint32_t i;
    uint32_t old_fl0 = 0u;
    fdtest_pollfd32_t *pfd = &g_fdtest_pfd;
    volatile uint32_t *sel_read = &g_fdtest_sel_read;

    KLOGI("fdtest", "start v8");
    fdtest_drain_stdin();

    FDTEST_SYSCALL(SYS_FCNTL, 0u, SYS_FCNTL_F_GETFL, 0u, 0u, 0u, 0u, old_fl0, err);
    if (err != 0u) {
        fdtest_fail("fcntl(F_GETFL,0)", err, 0u, &fails);
        return;
    }

    FDTEST_SYSCALL(SYS_DUP, 0u, 0u, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || (int32_t)ret < 0) {
        fdtest_fail("dup(stdin) errno", err, 0u, &fails);
        fdtest_fail("dup(stdin) ret", ret, 0u, &fails);
        klog_prefix(KLOG_LEVEL_ERROR, "fdtest");
        kputs("dup(stdin) abi last_nr=");
        kprint_hex32(abi_read32(ABI_OFF_LAST_NR));
        kputs(" abi errno=");
        kprint_hex32(abi_read32(ABI_OFF_ERRNO));
        kputs(" abi ret=");
        kprint_hex32(abi_read32(ABI_OFF_RET));
        kputs(" abi tick=");
        kprint_hex32(abi_read32(ABI_OFF_TICK));
        kputc((uint32_t)'\n');
        if ((int32_t)ret >= 0) {
            (void)sched_fd_close((int32_t)ret);
        }
        klog_prefix(KLOG_LEVEL_ERROR, "fdtest");
        kputs("dup(stdin) fd0_open=");
        kprint_hex32(sched_fd_is_open(0));
        kputc((uint32_t)'\n');
        return;
    }
    dupfd = ret;
    if (dupfd < 3u) {
        fdtest_fail("dup(stdin) fd range", dupfd, 3u, &fails);
    }

    FDTEST_SYSCALL(SYS_FCNTL, dupfd, SYS_FCNTL_F_GETFD, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || ret != 0u) {
        fdtest_fail("fcntl(F_GETFD) initial", ret, 0u, &fails);
    }

    FDTEST_SYSCALL(SYS_FCNTL, dupfd, SYS_FCNTL_F_SETFD, SYS_FD_CLOEXEC, 0u, 0u, 0u, ret, err);
    if (err != 0u || ret != 0u) {
        fdtest_fail("fcntl(F_SETFD,FD_CLOEXEC)", err, 0u, &fails);
    }

    FDTEST_SYSCALL(SYS_FCNTL, dupfd, SYS_FCNTL_F_GETFD, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || (ret & SYS_FD_CLOEXEC) == 0u) {
        fdtest_fail("fcntl(F_GETFD) cloexec", ret, SYS_FD_CLOEXEC, &fails);
    }

    FDTEST_SYSCALL(SYS_FCNTL, dupfd, SYS_FCNTL_F_SETFL, SYS_O_NONBLOCK, 0u, 0u, 0u, ret, err);
    if (err != 0u || ret != 0u) {
        fdtest_fail("fcntl(F_SETFL,O_NONBLOCK)", err, 0u, &fails);
    }

    FDTEST_SYSCALL(SYS_FCNTL, 0u, SYS_FCNTL_F_GETFL, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || (ret & SYS_O_NONBLOCK) == 0u) {
        fdtest_fail("fcntl(F_GETFL,stdin) shared", ret, SYS_O_NONBLOCK, &fails);
    }

    FDTEST_SYSCALL(SYS_READ, dupfd, ptr32(g_fdtest_buf), 1u, 0u, 0u, 0u, ret, err);
    if ((int32_t)ret == -1) {
        if (err != TEST_ERRNO_EAGAIN) {
            fdtest_fail("read(nonblock) errno", err, TEST_ERRNO_EAGAIN, &fails);
        }
    } else {
        if (err != 0u) {
            fdtest_fail("read(nonblock) err", err, 0u, &fails);
        }
        if (ret > 1u) {
            fdtest_fail("read(nonblock) ret", ret, 1u, &fails);
        }
    }

    pfd->fd = (int32_t)dupfd;
    pfd->events = (int16_t)SYS_POLLIN;
    pfd->revents = 0;
    FDTEST_SYSCALL(SYS_POLL, ptr32(pfd), 1u, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u) {
        fdtest_fail("poll(stdin,timeout=0) errno", err, 0u, &fails);
    }
    if (ret > 1u) {
        fdtest_fail("poll(stdin,timeout=0) ret", ret, 1u, &fails);
    } else if (ret == 0u) {
        if (pfd->revents != 0) {
            fdtest_fail("poll revents empty", (uint32_t)(uint16_t)pfd->revents, 0u, &fails);
        }
    } else {
        if (((uint16_t)pfd->revents & (uint16_t)SYS_POLLIN) == 0u) {
            fdtest_fail("poll revents ready", (uint32_t)(uint16_t)pfd->revents, SYS_POLLIN, &fails);
        }
    }

    pfd->fd = -1;
    pfd->events = (int16_t)SYS_POLLIN;
    pfd->revents = (int16_t)0x7FFF;
    FDTEST_SYSCALL(SYS_POLL, ptr32(pfd), 1u, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || ret != 0u) {
        fdtest_fail("poll(fd<0) ret/errno", err, 0u, &fails);
        fdtest_fail("poll(fd<0) ret", ret, 0u, &fails);
    }
    if (pfd->revents != 0) {
        fdtest_fail("poll(fd<0) revents", (uint32_t)(uint16_t)pfd->revents, 0u, &fails);
    }

    if (dupfd >= 32u) {
        fdtest_fail("dupfd out of select range", dupfd, 31u, &fails);
    }
    *sel_read = (dupfd < 32u) ? (1u << dupfd) : 0u;
    FDTEST_SYSCALL(SYS_SELECT, dupfd + 1u, ptr32((const void *)sel_read), 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u) {
        fdtest_fail("select(stdin,timeout=0) errno", err, 0u, &fails);
    }
    if (ret > 1u) {
        fdtest_fail("select(stdin,timeout=0) ret", ret, 1u, &fails);
    } else if (ret == 0u) {
        if (*sel_read != 0u) {
            fdtest_fail("select readmask empty", *sel_read, 0u, &fails);
        }
    } else if (dupfd < 32u) {
        if (((*sel_read) & (1u << dupfd)) == 0u) {
            fdtest_fail("select readmask ready", *sel_read, 1u << dupfd, &fails);
        }
    }

    FDTEST_SYSCALL(SYS_CLOSE, dupfd, 0u, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || ret != 0u) {
        fdtest_fail("close(dupfd)", err, 0u, &fails);
    }

    FDTEST_SYSCALL(SYS_CLOSE, dupfd, 0u, 0u, 0u, 0u, 0u, ret, err);
    if ((int32_t)ret != -1 || err != TEST_ERRNO_EBADF) {
        fdtest_fail("close(dupfd) second", err, TEST_ERRNO_EBADF, &fails);
    }

    FDTEST_SYSCALL(SYS_FCNTL, 0u, SYS_FCNTL_F_SETFL, old_fl0, 0u, 0u, 0u, ret, err);
    if (err != 0u || ret != 0u) {
        fdtest_fail("restore stdin flags", err, 0u, &fails);
    }

    FDTEST_SYSCALL(SYS_OPEN, ptr32(g_fdtest_path_dev_null), SYS_O_RDWR, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || (int32_t)ret < 0) {
        fdtest_fail("open(/dev/null)", err, 0u, &fails);
    } else {
        fd = ret;
        FDTEST_SYSCALL(SYS_READ, fd, ptr32(g_fdtest_buf), 8u, 0u, 0u, 0u, ret, err);
        if (err != 0u || ret != 0u) {
            fdtest_fail("read(/dev/null)", err, 0u, &fails);
            fdtest_fail("read(/dev/null) ret", ret, 0u, &fails);
        }
        FDTEST_SYSCALL(SYS_WRITE, fd, ptr32("x"), 1u, 0u, 0u, 0u, ret, err);
        if (err != 0u || ret != 1u) {
            fdtest_fail("write(/dev/null)", err, 0u, &fails);
            fdtest_fail("write(/dev/null) ret", ret, 1u, &fails);
        }
        FDTEST_SYSCALL(SYS_CLOSE, fd, 0u, 0u, 0u, 0u, 0u, ret, err);
        if (err != 0u || ret != 0u) {
            fdtest_fail("close(/dev/null)", err, 0u, &fails);
        }
    }

    for (i = 0u; i < 8u; i++) {
        g_fdtest_buf[i] = 0xA5u;
    }
    FDTEST_SYSCALL(SYS_OPEN, ptr32(g_fdtest_path_dev_zero), SYS_O_RDONLY, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || (int32_t)ret < 0) {
        fdtest_fail("open(/dev/zero)", err, 0u, &fails);
    } else {
        fd = ret;
        FDTEST_SYSCALL(SYS_READ, fd, ptr32(g_fdtest_buf), 8u, 0u, 0u, 0u, ret, err);
        if (err != 0u || ret != 8u) {
            fdtest_fail("read(/dev/zero)", err, 0u, &fails);
            fdtest_fail("read(/dev/zero) ret", ret, 8u, &fails);
        }
        for (i = 0u; i < 8u; i++) {
            if (g_fdtest_buf[i] != 0u) {
                fdtest_fail("read(/dev/zero) byte", g_fdtest_buf[i], 0u, &fails);
                break;
            }
        }
        FDTEST_SYSCALL(SYS_CLOSE, fd, 0u, 0u, 0u, 0u, 0u, ret, err);
        if (err != 0u || ret != 0u) {
            fdtest_fail("close(/dev/zero)", err, 0u, &fails);
        }
    }

    FDTEST_SYSCALL(SYS_OPEN, ptr32(g_fdtest_path_dev_tty), SYS_O_RDWR, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || (int32_t)ret < 0) {
        fdtest_fail("open(/dev/tty)", err, 0u, &fails);
    } else {
        fd = ret;
        FDTEST_SYSCALL(SYS_CLOSE, fd, 0u, 0u, 0u, 0u, 0u, ret, err);
        if (err != 0u || ret != 0u) {
            fdtest_fail("close(/dev/tty)", err, 0u, &fails);
        }
    }

    FDTEST_SYSCALL(SYS_OPEN, ptr32(g_fdtest_path_missing), SYS_O_RDONLY, 0u, 0u, 0u, 0u, ret, err);
    if ((int32_t)ret != -1 || err != TEST_ERRNO_ENOENT) {
        fdtest_fail("open(/dev/missing) errno", err, TEST_ERRNO_ENOENT, &fails);
    }

    FDTEST_SYSCALL(SYS_SOCKET, 0xFFFFu, 1u, 0u, 0u, 0u, 0u, ret, err);
    if ((int32_t)ret != -1 || err != TEST_ERRNO_EAFNOSUPPORT) {
        fdtest_fail("socket(bad domain)", err, TEST_ERRNO_EAFNOSUPPORT, &fails);
    }

    FDTEST_SYSCALL(SYS_SOCKET, 2u, 1u, 0u, 0u, 0u, 0u, ret, err);
    if (err != 0u || (int32_t)ret < 0) {
        fdtest_fail("socket(AF_INET)", err, 0u, &fails);
    } else {
        fd = ret;
        FDTEST_SYSCALL(SYS_BIND, fd, 0u, 0u, 0u, 0u, 0u, ret, err);
        if ((int32_t)ret != -1 || err != TEST_ERRNO_EOPNOTSUPP) {
            fdtest_fail("bind(socket)", err, TEST_ERRNO_EOPNOTSUPP, &fails);
        }

        FDTEST_SYSCALL(SYS_SEND, fd, ptr32("x"), 1u, 0u, 0u, 0u, ret, err);
        if ((int32_t)ret != -1 || err != TEST_ERRNO_ENOTCONN) {
            fdtest_fail("send(socket)", err, TEST_ERRNO_ENOTCONN, &fails);
        }

        FDTEST_SYSCALL(SYS_RECV, fd, ptr32(g_fdtest_buf), 1u, 0u, 0u, 0u, ret, err);
        if ((int32_t)ret != -1 || err != TEST_ERRNO_ENOTCONN) {
            fdtest_fail("recv(socket)", err, TEST_ERRNO_ENOTCONN, &fails);
        }

        FDTEST_SYSCALL(SYS_CLOSE, fd, 0u, 0u, 0u, 0u, 0u, ret, err);
        if (err != 0u || ret != 0u) {
            fdtest_fail("close(socket)", err, 0u, &fails);
        }
    }

    FDTEST_SYSCALL(SYS_BIND, 0u, 0u, 0u, 0u, 0u, 0u, ret, err);
    if ((int32_t)ret != -1 || err != TEST_ERRNO_ENOTSOCK) {
        fdtest_fail("bind(non-socket)", err, TEST_ERRNO_ENOTSOCK, &fails);
    }

    if (fails == 0u) {
        KLOGI("fdtest", "pass");
    } else {
        klog_prefix(KLOG_LEVEL_ERROR, "fdtest");
        kputs("fail count=");
        kprint_hex32(fails);
        kputc((uint32_t)'\n');
    }
}
