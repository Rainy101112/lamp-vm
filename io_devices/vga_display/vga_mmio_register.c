//
// Created by Max Wang on 2026/1/18.
//

#include "vga_mmio_register.h"

uint32_t fb_read32(VM *vm, uint32_t addr) {
    size_t fb_base = FB_BASE(vm->memory_size);
    size_t pixel_index = (addr - fb_base) / 4;
    return vm->fb[pixel_index];
}

void fb_write32(VM *vm, uint32_t addr, uint32_t value) {
    size_t fb_base = FB_BASE(vm->memory_size);
    size_t pixel_index = (addr - fb_base) / 4;
    //printf("Writing to MMIO ID %d\n", vm->mmio_count);
    vm->fb[pixel_index] = value;
}

void register_fb_mmio(VM *vm) {
    static MMIO_Device fb_dev;
    fb_dev.start = FB_BASE(vm->memory_size);
    fb_dev.end = FB_BASE(vm->memory_size) + FB_SIZE - 1;
    fb_dev.read32 = fb_read32;
    fb_dev.write32 = fb_write32;
    vm->mmio_devices[vm->mmio_count++] = &fb_dev;
    printf("Registered VM Screen to MMIO ID %d\n", vm->mmio_count);
}