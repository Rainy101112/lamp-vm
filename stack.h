//
// Created by Max Wang on 2025/12/28.
//

#ifndef VM_STACK_H
#define VM_STACK_H
#define DATA_PUSH(vm, v) do { \
    if ((vm)->dsp <= 0) { \
        printf("Data stack overflow\n"); \
        return; \
    } \
    (vm)->data_stack[--(vm)->dsp] = (v); \
} while (0)

#define CALL_PUSH(vm, v) do { \
    if ((vm)->csp <= 0) { \
        printf("Call stack overflow\n"); \
        return; \
    } \
    (vm)->call_stack[--(vm)->csp] = (v); \
} while (0)

#define DATA_POP(vm)( \
    ((vm)->dsp >= DATA_STACK_SIZE) ? \
    (printf("Data stack underflow\n"), 0): \
    (vm)->data_stack[(vm)->dsp++] \
)

#define CALL_POP(vm)( \
    ((vm)->csp >= CALL_STACK_SIZE) ? \
    (printf("Call stack underflow\n"), 0): \
    (vm)->call_stack[(vm)->csp++] \
)
#endif //VM_STACK_H
