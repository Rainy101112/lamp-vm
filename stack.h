//
// Created by Max Wang on 2025/12/28.
//

#ifndef VM_STACK_H
#define VM_STACK_H
#include "memory.h"
#include "panic.h"

static inline void data_push(VM *vm, uint32_t val) {
    if (vm->dsp == 0) {
        panic("Data stack overflow", vm);
        return;
    }
    vm->dsp--;
    vm_write32(vm, DATA_STACK_BASE + (vm->dsp * 4), val);
}

static inline uint32_t data_pop(VM *vm) {
    if (vm->dsp >= DATA_STACK_SIZE) {
        panic("Data stack underflow", vm);
        return 0;
    }
    uint32_t val = vm_read32(vm, DATA_STACK_BASE + (vm->dsp * 4));
    vm->dsp++;
    return val;
}

static inline void call_push(VM *vm, uint64_t val) {
    if (vm->csp == 0) {
        panic("Call stack overflow", vm);
        return;
    }
    vm->csp--;
    vm_write64(vm, CALL_STACK_BASE + vm->csp * 8, val);
}

static inline uint64_t call_pop(VM *vm) {
    if (vm->csp >= CALL_STACK_SIZE) {
        panic("Call stack underflow", vm);
        return 0;
    }
    uint64_t val = vm_read64(vm, CALL_STACK_BASE + vm->csp * 8);
    vm->csp++;
    return val;
}
static inline void isr_push(VM *vm, uint64_t val) {
    if (vm->isp == 0) {
        panic("Interrupt stack overflow", vm);
        return;
    }
    vm->isp--;
    vm_write64(vm, ISR_STACK_BASE + (uint32_t)vm->isp * 8, val);
}

static inline uint64_t isr_pop(VM *vm) {
    if (vm->isp >= ISR_STACK_SIZE) {
        panic("Interrupt stack underflow", vm);
        return 0;
    }
    uint64_t val = vm_read64(vm, ISR_STACK_BASE + (uint32_t)vm->isp * 8);
    vm->isp++;
    return val;
}

#endif // VM_STACK_H
