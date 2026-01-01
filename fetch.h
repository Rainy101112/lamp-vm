//
// Created by Max Wang on 2025/12/28.
//
#ifndef VM_FETCH_H
#define VM_FETCH_H
#define FETCH64(vm, op, rd, rs1, rs2, imm)            \
do {                                                  \
if ((vm)->ip + 8 > (vm)->memory_size) {           \
panic("IP out of bounds\n", vm);              \
return;                                       \
}                                                 \
uint64_t inst = *(uint64_t*)((vm)->memory + (vm)->ip); \
(vm)->ip += 8;                                    \
op  = (inst >> 56) & 0xFF;                         \
rd  = (inst >> 48) & 0xFF;                         \
rs1 = (inst >> 40) & 0xFF;                         \
rs2 = (inst >> 32) & 0xFF;                         \
imm = (int32_t)(inst & 0xFFFFFFFF);               \
} while (0)

#endif //VM_FETCH_H
