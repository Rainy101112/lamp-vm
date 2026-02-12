//
// Created by Max Wang on 2025/12/28.
//
#ifndef VM_FETCH_H
#define VM_FETCH_H

#include <string.h>

#include "panic.h"
#include "vm.h"

#define FETCH64(vm, op, rd, rs1, rs2, imm)                                                         \
    do {                                                                                           \
        VCPU *cpu = vm_current_cpu((vm));                                                          \
        if (!cpu) {                                                                                \
            panic("No active CPU context\n", (vm));                                                \
            return;                                                                                \
        }                                                                                          \
        const vm_addr_t ip = (vm_addr_t)cpu->ip;                                                   \
        if ((size_t)ip + 8u > (vm)->memory_size) {                                                 \
            panic("IP out of bounds\n", (vm));                                                     \
            return;                                                                                \
        }                                                                                          \
        cpu->last_ip = ip;                                                                         \
        /* Fast path: instruction fetch is always from normal RAM (not MMIO). */                  \
        uint64_t inst = 0;                                                                         \
        memcpy(&inst, &((vm)->memory[ip]), sizeof(uint64_t));                                      \
        cpu->ip = (size_t)(ip + 8u);                                                               \
        op = (inst >> 56) & 0xFF;                                                                  \
        rd = (inst >> 48) & 0xFF;                                                                  \
        rs1 = (inst >> 40) & 0xFF;                                                                 \
        rs2 = (inst >> 32) & 0xFF;                                                                 \
        imm = (int32_t)(inst & 0xFFFFFFFFu);                                                       \
    } while (0)

#endif // VM_FETCH_H
