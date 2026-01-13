//
// Created by Max Wang on 2025/12/30.
//
#pragma once
#include "vm.h"
#ifndef VM_INTERRUPT_H
#define VM_INTERRUPT_H
void vm_handle_interrupts(VM *vm);
void init_ivt(VM *vm);
void register_isr(VM *vm, uint32_t int_no, uint64_t isr_ip);
void trigger_interrupt(VM *vm, uint32_t int_no);

enum {
    INT_KEYBOARD = 0x00,
    INT_DIVIDE_BY_ZERO = 0x01,
} INT_ALIAS;
#endif