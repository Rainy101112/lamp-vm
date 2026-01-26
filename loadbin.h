//
// Created by Max Wang on 2025/12/29.
//

#ifndef VM_LOADBIN_H
#define VM_LOADBIN_H
#include <stdint.h>
#include <stdlib.h>

typedef struct {
    uint32_t text_base;
    uint32_t text_size;
    uint32_t data_base;
    uint32_t data_size;
    uint32_t bss_base;
    uint32_t bss_size;
} ProgramLayout;

uint64_t *load_program(const char *filename, size_t *out_size);
uint8_t *load_data(const char *filename, size_t *out_size);
int load_layout(const char *filename, ProgramLayout *out_layout);

#endif // VM_LOADBIN_H
