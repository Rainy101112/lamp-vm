#include "stack.h"
#include "vm.h"
#include "interrupt.h"
#include "memory.h"
#include "panic.h"

// Created by Max Wang on 2025/12/30.

#define ISR_ARG_REG 31

static inline size_t irq_word_index(int core_id, uint32_t int_no) {
    return (size_t)core_id * (size_t)IRQ_BITMAP_WORDS + (size_t)(int_no >> 6);
}

static inline uint64_t irq_bit_mask(uint32_t int_no) {
    return 1ULL << (int_no & 63);
}

static inline void isr_push_u32(VM *vm, uint32_t v) {
    isr_push(vm, (uint64_t)v);
}

static inline uint32_t isr_pop_u32(VM *vm) {
    return (uint32_t)isr_pop(vm);
}

void vm_enter_interrupt(VM *vm, uint32_t int_no) {
    VCPU *cpu = vm_current_cpu(vm);
    if (!cpu)
        return;
    if (int_no >= IVT_SIZE)
        return;

    if (cpu->in_interrupt)
        return;

    const uint64_t isr_ip = vm_read64(vm, IVT_BASE + int_no * 8);
    if (isr_ip == UINT64_MAX)
        return;

    cpu->regs[ISR_ARG_REG] = int_no;

    isr_push(vm, (uint64_t)cpu->ip);
    isr_push(vm, (uint64_t)cpu->flags);

    for (uint32_t i = 0; i < REG_COUNT; i++) {
        isr_push_u32(vm, cpu->regs[i]);
    }

    cpu->ip = (size_t)(vm_addr_t)isr_ip;
    cpu->in_interrupt = 1;
}

void vm_iret(VM *vm) {
    VCPU *cpu = vm_current_cpu(vm);
    if (!cpu)
        return;
    if (!cpu->in_interrupt)
        return;

    for (int i = (int)REG_COUNT - 1; i >= 0; i--) {
        cpu->regs[i] = isr_pop_u32(vm);
    }

    cpu->flags = (unsigned int)isr_pop(vm);

    cpu->ip = (size_t)(vm_addr_t)isr_pop(vm);

    cpu->in_interrupt = 0;
}

void vm_handle_interrupts(VM *vm) {
    VCPU *cpu = vm_current_cpu(vm);
    if (!cpu)
        return;
    if (cpu->in_interrupt)
        return;

    const int core_id = cpu->core_id;
    const size_t base = (size_t)core_id * (size_t)IRQ_BITMAP_WORDS;

    // O(IRQ_BITMAP_WORDS) fast path: only 4 words when IVT_SIZE is 256.
    for (uint32_t w = 0; w < IRQ_BITMAP_WORDS; w++) {
        uint_fast64_t word = atomic_load(&vm->interrupt_bitmap[base + w]);
        if (word == 0)
            continue;

        const int bit = __builtin_ctzll((unsigned long long)word);
        const uint64_t mask = 1ULL << (uint32_t)bit;

        while (1) {
            uint_fast64_t expected = word;
            if ((expected & mask) == 0) {
                break;
            }
            const uint_fast64_t desired = expected & (uint_fast64_t)(~mask);
            if (atomic_compare_exchange_weak(&vm->interrupt_bitmap[base + w], &expected, desired)) {
                const uint32_t int_no = (uint32_t)(w * 64u + (uint32_t)bit);
                vm_enter_interrupt(vm, int_no);
                return;
            }
            word = atomic_load(&vm->interrupt_bitmap[base + w]);
            if (word == 0)
                break;
        }
    }
}

void init_ivt(VM *vm) {
    for (uint32_t i = 0; i < IVT_SIZE; i++) {
        vm_write64(vm, IVT_BASE + i * 8, UINT64_MAX);
    }
    for (int c = 0; c < vm->smp_cores; c++) {
        for (uint32_t w = 0; w < IRQ_BITMAP_WORDS; w++) {
            atomic_store(&vm->interrupt_bitmap[(size_t)c * (size_t)IRQ_BITMAP_WORDS + w], 0);
        }
    }
    for (int c = 0; c < vm->smp_cores; c++) {
        vm->cpus[c].in_interrupt = 0;
    }
}

void register_isr(VM *vm, uint32_t int_no, uint64_t isr_ip) {
    if (int_no >= IVT_SIZE) {
        panic(panic_format("Invalid interrupt number %u\n", int_no), vm);
        return;
    }
    vm_write64(vm, IVT_BASE + int_no * 8, (uint64_t)(vm_addr_t)isr_ip);
}

void trigger_interrupt(VM *vm, uint32_t int_no) {
    trigger_interrupt_target(vm, 0, int_no);
}

void trigger_interrupt_target(VM *vm, int core_id, uint32_t int_no) {
    if (!vm)
        return;
    if (core_id < 0 || core_id >= vm->smp_cores)
        return;
    if (int_no >= IVT_SIZE)
        return;

    const size_t idx = irq_word_index(core_id, int_no);
    const uint64_t mask = irq_bit_mask(int_no);
    atomic_fetch_or(&vm->interrupt_bitmap[idx], (uint_fast64_t)mask);
}
