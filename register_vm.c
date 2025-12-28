#include <stdio.h>
#include <stdlib.h>
#include "vm.h"

#define REG_COUNT 8
#define MEM_SIZE 1024
#define STACK_SIZE 256
#define FLAG_ZF 1

typedef struct {
    int regs[REG_COUNT];
    unsigned char *code;
    int ip;
    int code_size;
    unsigned int flags;

    int stack[STACK_SIZE];
    int sp;

    int memory[MEM_SIZE];
} VM;

void set_zf(VM *vm, int value) {
    if (value == 0) {
        vm->flags |= FLAG_ZF;
    } else {
        vm->flags &= ~FLAG_ZF;
    }
}

void vm_run(VM *vm) {
    while (vm->ip < vm->code_size) {
        unsigned char opcode = vm->code[vm->ip++];

        switch (opcode) {
            case OP_LOADI: {
                unsigned char r = NEXT_INSTRUCTION;
                int imm = NEXT_INSTRUCTION;
                vm->regs[r] = imm;
                set_zf(vm, vm->regs[r]);
                break;
            }

            case OP_ADD: {
                unsigned char rd = NEXT_INSTRUCTION;
                unsigned char r1 = NEXT_INSTRUCTION;
                unsigned char r2 = NEXT_INSTRUCTION;
                vm->regs[rd] = vm->regs[r1] + vm->regs[r2];
                set_zf(vm, vm->regs[rd]);
                break;
            }

            case OP_SUB: {
                unsigned char rd = NEXT_INSTRUCTION;
                unsigned char r1 = NEXT_INSTRUCTION;
                unsigned char r2 = NEXT_INSTRUCTION;
                vm->regs[rd] = vm->regs[r1] - vm->regs[r2];
                set_zf(vm, vm->regs[rd]);
                break;
            }
            case OP_MUL: {
                unsigned char rd = NEXT_INSTRUCTION;
                unsigned char r1 = NEXT_INSTRUCTION;
                unsigned char r2 = NEXT_INSTRUCTION;
                vm->regs[rd] = vm->regs[r1] * vm->regs[r2];
                set_zf(vm, vm->regs[rd]);
                break;
            }
            case OP_PRINT: {
                unsigned char r = NEXT_INSTRUCTION;
                printf("%d\n", vm->regs[r]);
                break;
            }
            case OP_HALT: {
                return;
            }
            case OP_JMP: {
                int addr = NEXT_INSTRUCTION;
                vm->ip = addr;
                break;
            }
            case OP_JZ: {
                int addr = NEXT_INSTRUCTION;
                if (vm->flags & FLAG_ZF) {
                    vm->ip = addr;
                }
                break;
            }
            case OP_PUSH: {
                unsigned char r = NEXT_INSTRUCTION;
                vm->stack[--vm->sp] = vm->regs[r];
                break;
            }
            case OP_POP: {
                unsigned char r = NEXT_INSTRUCTION;
                vm->regs[r] = vm->stack[vm->sp++];
                break;
            }
            case OP_CALL: {
                int addr = NEXT_INSTRUCTION;
                vm->stack[--vm->sp] = vm->ip;
                vm->ip = addr;
                break;
            }
            case OP_RET: {
                vm->ip = vm->stack[vm->sp++];
                break;
            }
            case OP_LOAD: {
                unsigned char r = NEXT_INSTRUCTION;
                unsigned char addr = NEXT_INSTRUCTION;
                vm->regs[r] = vm->memory[addr];
                set_zf(vm, vm->regs[r]);
                break;
            }
            case OP_STORE: {
                unsigned char r = NEXT_INSTRUCTION;
                unsigned char addr = NEXT_INSTRUCTION;
                vm->memory[addr] = vm->regs[r];
                break;
            }
            case OP_LOAD_IND: {
                unsigned char r_dest = NEXT_INSTRUCTION;
                unsigned char r_addr = NEXT_INSTRUCTION;
                vm->regs[r_dest] = vm->memory[vm->regs[r_addr]];
                set_zf(vm, vm->regs[r_dest]);
                break;
            }
            case OP_STORE_IND: {
                unsigned char r_src = NEXT_INSTRUCTION;
                unsigned char r_addr = NEXT_INSTRUCTION;
                vm->memory[vm->regs[r_addr]] = vm->regs[r_src];
                break;
            }
            default: {
                printf("Unknown opcode %d\n", opcode);
                return;
            }
        }
    }
}

void vm_dump(VM *vm, int mem_preview) {
    printf("VM dump:\n");
    printf("Registers:\n");
    for (int i = 0; i < REG_COUNT; i++) {
        printf("r%d = %d\n", i, vm->regs[i]);
    }

    printf("Stack (top -> bottom):\n");
    for (int i = vm->sp; i < STACK_SIZE; i++) {
        printf("[%d] = %d\n", i, vm->stack[i]);
    }
    if (vm->sp == STACK_SIZE) {
        printf("<empty>\n");
    }
    printf("Memory (first %d cells):\n", mem_preview);
    for (int i = 0; i < mem_preview && i < MEM_SIZE; i++) {
        printf("[%d] = %d\n", i, vm->memory[i]);
    }
    printf("IP = %d\n", vm->ip);
    printf("ZF = %d\n", (vm->flags & FLAG_ZF) != 0);
}

int main() {
    //r0 = 0
    //r1 = 0
    //r2 = 5
    //loop_start:
    //r3 = r2 -r1
    //if r3 == 0 jmp end
    //r4 = memory[r1]
    //r0 += r4
    //r1 += 1
    //jmp loop_start
    //end:
    //print r0,
    //halt
    unsigned char program[] = {
        OP_LOADI, 0, 0,
        OP_LOADI, 1, 0,
        OP_LOADI, 2, 5,
        // loop_start (byte 9)
        OP_SUB, 3, 2, 1,
        OP_JZ, 31,
        OP_LOAD_IND, 4, 1,
        OP_ADD, 0, 0, 4,
        OP_LOADI, 5, 1,
        OP_ADD, 1, 1, 5,
        OP_JMP, 9,
        // end (byte 30)
        OP_PRINT, 0,
        OP_HALT
    };

    VM vm = {0};
    vm.code = program;
    vm.code_size = sizeof(program);
    vm.ip = 0;
    vm.flags = 0;
    vm.sp = STACK_SIZE;

    printf("Loaded VM. \n code length: %d\n Stack size: %d\n Memory Size: %d\n Memory Head: %p\n", vm.code_size,
           STACK_SIZE, MEM_SIZE, &vm.memory[0]);

    for (int i = 0; i < 5; i++) {
        vm.memory[i] = i + 1;
    }

    vm_run(&vm);
    vm_dump(&vm, 0);
    return 0;
}
