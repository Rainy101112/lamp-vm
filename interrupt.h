#pragma once
#ifndef VM_INTERRUPT_H
#define VM_INTERRUPT_H

#include <stdint.h>
#include "vm.h"

void vm_handle_interrupts(VM *vm);
void init_ivt(VM *vm);
void register_isr(VM *vm, uint32_t int_no, uint64_t isr_ip);
void trigger_interrupt(VM *vm, uint32_t int_no);
void trigger_interrupt_target(VM *vm, int core_id, uint32_t int_no);

void vm_enter_interrupt(VM *vm, uint32_t int_no);
void vm_iret(VM *vm);

typedef enum InterruptNo {
    INT_KEYBOARD        = 0x00,
    INT_DIVIDE_BY_ZERO  = 0x01,
    INT_DISK_COMPLETE   = 0x02,
    INT_SERIAL          = 0x03
} InterruptNo;

#endif // VM_INTERRUPT_H
