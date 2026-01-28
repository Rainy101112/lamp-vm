
//
// Created by Max Wang on 2025/12/28.
//

#ifndef VM_VM_H
#define VM_VM_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <sys/_pthread/_pthread_cond_t.h>
#include <sys/_pthread/_pthread_mutex_t.h>
#include <sys/_pthread/_pthread_t.h>

static inline uint64_t INST(uint8_t op, uint8_t rd, uint8_t rs1, uint8_t rs2, uint32_t imm) {
    return ((uint64_t)op << 56 | (uint64_t)rd << 48 | (uint64_t)rs1 << 40 | (uint64_t)rs2 << 32) |
        imm;
}
typedef struct VM VM;
#ifdef VM_DEBUG
typedef struct VM_Debug VM_Debug;
#endif
#define MAX_MMIO_DEVICES 16

#define FB_WIDTH 640
#define FB_HEIGHT 480
#define FB_BPP 4
#define FB_SIZE (FB_WIDTH * FB_HEIGHT * FB_BPP)

#define IO_SIZE 256

#define REG_COUNT 32
#define DUMP_MEM_SEEK_LEN 16

#define IVT_SIZE 256
#define IVT_ENTRY_SIZE 8
#define CALL_STACK_SIZE 256
#define DATA_STACK_SIZE 256
#define ISR_STACK_SIZE 256

#define TIME_REALTIME_OFFSET   0
#define TIME_MONOTONIC_OFFSET  8
#define TIME_BOOTTIME_OFFSET   16

#define IVT_BASE 0x0000
#define CALL_STACK_BASE (IVT_BASE + IVT_SIZE * IVT_ENTRY_SIZE)
#define DATA_STACK_BASE (CALL_STACK_BASE + CALL_STACK_SIZE * 8)
#define ISR_STACK_BASE (DATA_STACK_BASE + DATA_STACK_SIZE * 8)
#define TIME_BASE (ISR_STACK_BASE + ISR_STACK_SIZE * 8)
#define PROGRAM_BASE (TIME_BASE + 28)

#define FB_BASE(addr_space_size) (addr_space_size)
typedef uint32_t vm_addr_t;

typedef struct {
    FILE *fp;

    uint32_t lba;
    uint32_t mem_addr;
    uint32_t count;

    pthread_t worker_thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond_var;

    uint8_t status;
    int pending_cmd;
    int current_cmd;
    bool thread_running;
    bool op_complete;
} Disk;
typedef uint32_t (*mmio_read32_fn)(VM *vm, uint32_t addr);
typedef void (*mmio_write32_fn)(VM *vm, uint32_t addr, uint32_t val);
typedef struct {
    uint32_t start;
    uint32_t end;
    mmio_read32_fn read32;
    mmio_write32_fn write32;
} MMIO_Device;
struct VM{
    uint32_t regs[REG_COUNT];
    uint64_t *code;
    size_t ip;
    size_t execution_times;
    int halted;
    int panic;
    unsigned int flags;

    int data_stack[DATA_STACK_SIZE];
    int dsp;

    int call_stack[CALL_STACK_SIZE];
    int csp;

    int isp;
    uint8_t *memory;
    size_t memory_size;

    /*
     * framebuffer is mapped after main memory:
     * [fb_base, fb_base + FB_SIZE)
     */
    uint32_t *fb;

    int io[IO_SIZE];

    Disk disk;

    int interrupt_flags[IVT_SIZE];
    int in_interrupt;

    uint64_t start_realtime_ns;
    uint64_t start_monotonic_ns;
    uint64_t latched_realtime;
    uint64_t latched_monotonic;
    uint64_t latched_boottime;

    int suspend_count;

    MMIO_Device *mmio_devices[MAX_MMIO_DEVICES];
    int mmio_count;

#ifdef VM_DEBUG
    VM_Debug *debug;
#endif
};
enum {
    OP_ADD = 1,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_HALT,
    OP_JMP,
    OP_JZ,
    OP_PUSH,
    OP_POP,
    OP_CALL,
    OP_RET,
    OP_LOAD,
    OP_LOAD32,
    OP_LOADX32,
    OP_STORE,
    OP_STORE32,
    OP_STOREX32,
    OP_CMP,
    OP_CMPI,
    OP_MOV,
    OP_MOVI,
    OP_MEMSET,
    OP_MEMCPY,
    OP_IN,
    OP_OUT,
    OP_INT,
    OP_IRET,
    OP_MOD,
    OP_AND,
    OP_OR,
    OP_XOR,
    OP_NOT,
    OP_SHL,
    OP_SHR,
    OP_SAR,
    OP_JNZ,
    OP_JG,
    OP_JGE,
    OP_JL,
    OP_JLE,
    OP_JC,
    OP_JNC,
    OP_FADD,
    OP_FSUB,
    OP_FMUL,
    OP_FDIV,
    OP_FNEG,
    OP_FABS,
    OP_FSQRT,
    OP_FCMP,
    OP_ITOF,
    OP_FTOI,
    OP_FLOAD32,
    OP_FSTORE32
};

void vm_dump(const VM *vm, int mem_preview);

static inline uint64_t host_unix_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
static inline uint64_t host_monotonic_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}
#endif
