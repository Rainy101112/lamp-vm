//
// Created by Max Wang on 2026/1/18.
//

#ifndef VM_MMIO_H
#define VM_MMIO_H
#include "vm.h"

static inline MMIO_Device *find_mmio(VM *vm, uint32_t addr) {
    //most MMIO traffic repeatedly touches a small range.
    MMIO_Device *cached = vm->mmio_cache_dev;
    if (cached && addr >= vm->mmio_cache_start && addr <= vm->mmio_cache_end) {
        return cached;
    }

    for (int i = 0; i < vm->mmio_count; i++) {
        MMIO_Device *dev = vm->mmio_devices[i];
        if (addr >= dev->start && addr <= dev->end) {
            vm->mmio_cache_dev = dev;
            vm->mmio_cache_start = dev->start;
            vm->mmio_cache_end = dev->end;
            return dev;
        }
    }

    vm->mmio_cache_dev = NULL;
    return NULL;
}


uint32_t vm_mmio_read32(VM* vm, uint32_t addr);
void vm_mmio_write32(VM* vm, uint32_t addr, uint32_t val);
#endif //VM_MMIO_H
