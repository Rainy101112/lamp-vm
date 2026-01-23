#include "stack.h"
#include "vm.h"
#include "interrupt.h"
#include "memory.h"
#include "panic.h"

// Created by Max Wang on 2025/12/30.

#define ISR_ARG_REG 31

static inline void isr_push_u32(VM *vm, uint32_t v) {
    isr_push(vm, (uint64_t)v);
}

static inline uint32_t isr_pop_u32(VM *vm) {
    return (uint32_t)isr_pop(vm);
}

void vm_enter_interrupt(VM *vm, uint32_t int_no) {
    if (int_no >= IVT_SIZE)
        return;

    if (vm->in_interrupt)
        return;

    const uint64_t isr_ip = vm_read64(vm, IVT_BASE + int_no * 8);
    if (isr_ip == UINT64_MAX)
        return;

    vm->regs[ISR_ARG_REG] = int_no;

    isr_push(vm, (uint64_t)vm->ip);
    isr_push(vm, (uint64_t)vm->flags);

    for (uint32_t i = 0; i < REG_COUNT; i++) {
        isr_push_u32(vm, vm->regs[i]);
    }

    vm->ip = (size_t)isr_ip;
    vm->in_interrupt = 1;
}

void vm_iret(VM *vm) {
    if (!vm->in_interrupt)
        return;

    for (int i = (int)REG_COUNT - 1; i >= 0; i--) {
        vm->regs[i] = isr_pop_u32(vm);
    }

    vm->flags = (unsigned int)isr_pop(vm);

    vm->ip = (size_t)isr_pop(vm);

    vm->in_interrupt = 0;
}

void vm_handle_interrupts(VM *vm) {
    if (vm->in_interrupt)
        return;

    for (uint32_t i = 0; i < IVT_SIZE; i++) {
        if (!vm->interrupt_flags[i])
            continue;

        vm->interrupt_flags[i] = 0;
        vm_enter_interrupt(vm, i);
        break;
    }
}

void init_ivt(VM *vm) {
    for (uint32_t i = 0; i < IVT_SIZE; i++) {
        vm_write64(vm, IVT_BASE + i * 8, UINT64_MAX);
        vm->interrupt_flags[i] = 0;
    }
    vm->in_interrupt = 0;
}

void register_isr(VM *vm, uint32_t int_no, uint64_t isr_ip) {
    if (int_no >= IVT_SIZE) {
        panic(panic_format("Invalid interrupt number %u\n", int_no), vm);
        return;
    }
    vm_write64(vm, IVT_BASE + int_no * 8, isr_ip);
}

void trigger_interrupt(VM *vm, uint32_t int_no) {
    if (int_no >= IVT_SIZE)
        return;
    vm->interrupt_flags[int_no] = 1;
}
