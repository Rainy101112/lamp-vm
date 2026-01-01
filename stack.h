//
// Created by Max Wang on 2025/12/28.
//

#ifndef VM_STACK_H
#define VM_STACK_H
#define DATA_PUSH(vm,val) (vm->memory[DATA_STACK_BASE + --vm->dsp] = val)
#define DATA_POP(vm) (vm->memory[DATA_STACK_BASE + vm->dsp++])
#define CALL_PUSH(vm,val) (vm->memory[CALL_STACK_BASE + --vm->csp] = val)
#define CALL_POP(vm) (vm->memory[CALL_STACK_BASE + vm->csp++])

#endif //VM_STACK_H
