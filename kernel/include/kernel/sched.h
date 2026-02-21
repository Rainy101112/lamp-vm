#ifndef LAMP_KERNEL_SCHED_H
#define LAMP_KERNEL_SCHED_H

#include "types.h"

typedef struct sched_task sched_task_t;
typedef struct sched_waitq sched_waitq_t;
typedef void (*sched_task_entry_t)(sched_task_t *task, void *arg);

enum {
    SCHED_MAX_TASKS = 16u,
    SCHED_NAME_MAX = 16u,
    SCHED_MAX_FDS = 32u
};

enum {
    SCHED_WAITPID_ANY = -1,
    SCHED_WAITPID_WNOHANG = 1u,
    SCHED_WAITPID_NO_CHILD = -1,
    SCHED_WAITPID_BLOCKED = -2
};

enum {
    SCHED_TASK_UNUSED = 0u,
    SCHED_TASK_RUNNABLE = 1u,
    SCHED_TASK_RUNNING = 2u,
    SCHED_TASK_SLEEPING = 3u,
    SCHED_TASK_BLOCKED = 4u,
    SCHED_TASK_ZOMBIE = 5u
};

enum {
    SCHED_FD_O_ACCMODE = 0x00000003u,
    SCHED_FD_O_RDONLY = 0x00000000u,
    SCHED_FD_O_WRONLY = 0x00000001u,
    SCHED_FD_O_RDWR = 0x00000002u,
    SCHED_FD_O_NONBLOCK = 0x00000800u
};

enum {
    SCHED_FD_CLOEXEC = 0x00000001u
};

enum {
    SCHED_FD_OK = 0,
    SCHED_FD_EBADF = -1,
    SCHED_FD_EMFILE = -2,
    SCHED_FD_EINVAL = -3
};

struct sched_task {
    uint32_t tid;
    uint32_t state;
    uint32_t wake_tick;
    uint32_t run_ticks;
    void *arg;
};

struct sched_waitq {
    uint32_t bits[(SCHED_MAX_TASKS + 31u) / 32u];
};

void sched_init(void);
int sched_spawn(const char *name, sched_task_entry_t entry, void *arg);
void sched_exit(void);
void sched_exit_code(uint32_t code);
void sched_yield(void);
void sched_sleep_ticks(uint32_t ticks);
int sched_current_tid(void);
uint32_t sched_tick_period_us(void);
int sched_waitpid(int32_t pid, uint32_t options, uint32_t *status_out);

int sched_fd_close(int32_t fd);
int sched_fd_dup(int32_t oldfd);
int sched_fd_dup2(int32_t oldfd, int32_t newfd);
int sched_fd_fcntl_getfd(int32_t fd, uint32_t *out_flags);
int sched_fd_fcntl_setfd(int32_t fd, uint32_t flags);
int sched_fd_fcntl_getfl(int32_t fd, uint32_t *out_flags);
int sched_fd_fcntl_setfl(int32_t fd, uint32_t flags);
uint32_t sched_fd_can_read(int32_t fd);
uint32_t sched_fd_can_write(int32_t fd);
uint32_t sched_fd_is_nonblock(int32_t fd);
uint32_t sched_fd_is_open(int32_t fd);
uint32_t sched_fd_is_stdin(int32_t fd);
uint32_t sched_fd_is_tty(int32_t fd);

void sched_waitq_init(sched_waitq_t *q);
void sched_waitq_sleep(sched_waitq_t *q, uint32_t timeout_ticks);
void sched_waitq_wake_one(sched_waitq_t *q);
void sched_waitq_wake_all(sched_waitq_t *q);
void sched_block_until_runnable(void);
void sched_pump_once(void);

void sched_run(void);
void schedule_tick(void);
unsigned int sched_ticks(void);

#endif
