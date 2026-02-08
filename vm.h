
//
// Created by Max Wang on 2025/12/28.
//
#pragma once
#ifndef VM_VM_H
#define VM_VM_H
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <pthread.h>
#include <stdatomic.h>

static inline uint64_t INST(uint8_t op, uint8_t rd, uint8_t rs1, uint8_t rs2, uint32_t imm) {
    return ((uint64_t)op << 56 | (uint64_t)rd << 48 | (uint64_t)rs1 << 40 | (uint64_t)rs2 << 32) |
        imm;
}
typedef struct VM VM;
typedef struct VCPU VCPU;
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
#define FB_LEGACY_BASE 0x00620000u
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
struct VCPU {
    uint32_t regs[REG_COUNT];
    size_t ip;
    size_t last_ip;
    unsigned int flags;
    int dsp;
    int csp;
    int isp;
    int in_interrupt;
    uint64_t execution_times;
    int core_id;
    vm_addr_t call_stack_base;
    vm_addr_t data_stack_base;
    vm_addr_t isr_stack_base;
    int is_bsp;
};

extern _Thread_local VCPU *vm_tls_vcpu;

static inline VCPU *vm_current_cpu(VM *vm);

struct VM{
    int halted;
    int panic;
    uint8_t *memory;
    size_t memory_size;

    /*
     * framebuffer is mapped after main memory:
     * [fb_base, fb_base + FB_SIZE)
     */
    uint32_t *fb;

    int io[IO_SIZE];

    Disk disk;

    atomic_int *interrupt_flags;

    uint64_t start_realtime_ns;
    uint64_t start_monotonic_ns;
    uint64_t latched_realtime;
    uint64_t latched_monotonic;
    uint64_t latched_boottime;

    int suspend_count;

    MMIO_Device *mmio_devices[MAX_MMIO_DEVICES];
    int mmio_count;

    /*
     * SMP runtime configuration and state.
     */
    int smp_cores;
    VCPU *cpus;
    atomic_bool *core_released;
    atomic_uint_fast64_t total_execution_times;
    pthread_mutex_t shared_lock;
    vm_addr_t stack_pool_base;
    size_t stack_pool_size;

#ifdef VM_DEBUG
    VM_Debug *debug;
#endif
};

static inline VCPU *vm_current_cpu(VM *vm) {
    if (vm_tls_vcpu)
        return vm_tls_vcpu;
    if (!vm || !vm->cpus)
        return NULL;
    return &vm->cpus[0];
}

static inline void vm_shared_lock(VM *vm) {
    if (vm)
        pthread_mutex_lock(&vm->shared_lock);
}

static inline void vm_shared_unlock(VM *vm) {
    if (vm)
        pthread_mutex_unlock(&vm->shared_lock);
}
enum {
    OP_ADD = 0x01,
    OP_SUB = 0x02,
    OP_MUL = 0x03,
    OP_DIV = 0x04,
    OP_HALT = 0x05,
    OP_JMP = 0x06,
    OP_JZ = 0x07,
    OP_PUSH = 0x08,
    OP_POP = 0x09,
    OP_CALL = 0x0A,
    OP_RET = 0x0B,
    OP_LOAD = 0x0C,
    OP_LOAD32 = 0x0D,
    OP_LOADX32 = 0x0E,
    OP_STORE = 0x0F,
    OP_STORE32 = 0x10,
    OP_STOREX32 = 0x11,
    OP_CMP = 0x12,
    OP_CMPI = 0x13,
    OP_MOV = 0x14,
    OP_MOVI = 0x15,
    OP_MEMSET = 0x16,
    OP_MEMCPY = 0x17,
    OP_IN = 0x18,
    OP_OUT = 0x19,
    OP_INT = 0x1A,
    OP_IRET = 0x1B,
    OP_MOD = 0x1C,
    OP_AND = 0x1D,
    OP_OR = 0x1E,
    OP_XOR = 0x1F,
    OP_NOT = 0x20,
    OP_SHL = 0x21,
    OP_SHR = 0x22,
    OP_SAR = 0x23,
    OP_JNZ = 0x24,
    OP_JG = 0x25,
    OP_JGE = 0x26,
    OP_JL = 0x27,
    OP_JLE = 0x28,
    OP_JC = 0x29,
    OP_JNC = 0x2A,
    OP_FADD = 0x2B,
    OP_FSUB = 0x2C,
    OP_FMUL = 0x2D,
    OP_FDIV = 0x2E,
    OP_FNEG = 0x2F,
    OP_FABS = 0x30,
    OP_FSQRT = 0x31,
    OP_FCMP = 0x32,
    OP_ITOF = 0x33,
    OP_FTOI = 0x34,
    OP_FLOAD32 = 0x35,
    OP_FSTORE32 = 0x36,
    OP_INC = 0x37,
    OP_ADDI = 0x38,
    OP_SUBI = 0x39,
    OP_ANDI = 0x3A,
    OP_ORI = 0x3B,
    OP_XORI = 0x3C,
    OP_SHLI = 0x3D,
    OP_SHRI = 0x3E,
    OP_CAS = 0x3F,
    OP_XADD = 0x40,
    OP_XCHG = 0x41,
    OP_LDAR = 0x42,
    OP_STLR = 0x43,
    OP_FENCE = 0x44,
    OP_PAUSE = 0x45,
    OP_STARTAP = 0x46,
    OP_IPI = 0x47,
    OP_CPUID = 0x48,
    OP_CALLR = 0x49,
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
