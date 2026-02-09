#include "stack.h"
#include "vm.h"
#include "interrupt.h"
#include "memory.h"
#include "panic.h"

// Created by Max Wang on 2025/12/30.

#define ISR_ARG_REG 31

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static inline size_t irq_index(const VM *vm, int core_id, uint32_t int_no) {
    return (size_t)core_id * (size_t)IVT_SIZE + (size_t)int_no;
}
#pragma GCC diagnostic pop

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

    cpu->ip = (size_t)isr_ip;
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

    cpu->ip = (size_t)isr_pop(vm);

    cpu->in_interrupt = 0;
}

void vm_handle_interrupts(VM *vm) {
    VCPU *cpu = vm_current_cpu(vm);
    if (!cpu)
        return;
    if (cpu->in_interrupt)
        return;

    const int core_id = cpu->core_id;
    for (uint32_t i = 0; i < IVT_SIZE; i++) {
        atomic_int *slot = &vm->interrupt_flags[irq_index(vm, core_id, i)];
        if (atomic_load(slot) == 0)
            continue;

        if (atomic_exchange(slot, 0) == 0)
            continue;
        vm_enter_interrupt(vm, i);
        break;
    }
}

void init_ivt(VM *vm) {
    for (uint32_t i = 0; i < IVT_SIZE; i++) {
        vm_write64(vm, IVT_BASE + i * 8, UINT64_MAX);
    }
    for (int c = 0; c < vm->smp_cores; c++) {
        for (uint32_t i = 0; i < IVT_SIZE; i++) {
            atomic_store(&vm->interrupt_flags[irq_index(vm, c, i)], 0);
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
    vm_write64(vm, IVT_BASE + int_no * 8, isr_ip);
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
    atomic_store(&vm->interrupt_flags[irq_index(vm, core_id, int_no)], 1);
}
