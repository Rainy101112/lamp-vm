//
// Created by Max Wang on 2025/12/28.
//
#ifndef VM_FETCH_H
#define VM_FETCH_H
#include <memory.h>
#define FETCH64(vm, op, rd, rs1, rs2, imm)                                                         \
    do {                                                                                           \
        VCPU *cpu = vm_current_cpu((vm));                                                          \
        if (!cpu) {                                                                                 \
            panic("No active CPU context\n", (vm));                                                \
            return;                                                                                \
        }                                                                                          \
        if (cpu->ip + 8 > (vm)->memory_size) {                                                     \
            panic("IP out of bounds\n", vm);                                                       \
            return;                                                                                \
        }                                                                                          \
        cpu->last_ip = cpu->ip;                                                                    \
        uint64_t inst = vm_read64((vm), (vm_addr_t)cpu->ip);                                       \
        cpu->ip += 8;                                                                              \
        op = (inst >> 56) & 0xFF;                                                                  \
        rd = (inst >> 48) & 0xFF;                                                                  \
        rs1 = (inst >> 40) & 0xFF;                                                                 \
        rs2 = (inst >> 32) & 0xFF;                                                                 \
        imm = (int32_t)(inst & 0xFFFFFFFF);                                                        \
    } while (0)

#endif // VM_FETCH_H
