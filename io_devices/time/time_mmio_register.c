//
// Created by Max Wang on 2026/1/18.
//

#include "time_mmio_register.h"
#include <stdlib.h>

uint32_t time_read32(VM *vm, uint32_t addr) {
    uint32_t offset = addr - TIME_BASE;
    switch (offset) {
        case 0x00: //Control
            return 1;
        case 0x04: //Realtime low
            vm->latched_realtime = host_unix_time_ns();
            return (uint32_t) (vm->latched_realtime & 0xFFFFFFFF);
        case 0x08: //Realtime high
            return (uint32_t) (vm->latched_realtime >> 32);
        case 0x0C: //Monotonic low
            vm->latched_monotonic = host_monotonic_time_ns();
            return (uint32_t) (vm->latched_monotonic & 0xFFFFFFFF);
        case 0x10: //Monotonic high
            return (uint32_t) (vm->latched_monotonic >> 32);
        case 0x14: //Boot low
            vm->latched_boottime = host_monotonic_time_ns() - vm->start_monotonic_ns;
            return (uint32_t) (vm->latched_boottime & 0xFFFFFFFF);
        case 0x18: //Boot high
            return (uint32_t) (vm->latched_boottime >> 32);
        default:
            printf("Unknown MMIO Register Offset: 0x%08x\n", offset);
            return 0;

    }
}

void time_write32(VM *vm, uint32_t addr, uint32_t value) {
    fprintf(stderr, "Attempted to write to read-only TIME MMIO at 0x%08x\n", addr);
    vm->halted = 1;
}

void register_time_mmio(VM *vm) {
    static MMIO_Device time_dev;
    time_dev.start = TIME_BASE;
    time_dev.end = TIME_BASE + 23;
    time_dev.read32 = time_read32;
    time_dev.write32 = time_write32;

    if (vm->mmio_count < MAX_MMIO_DEVICES) {
        vm->mmio_devices[vm->mmio_count++] = &time_dev;
        printf("Registered VM Timer to MMIO ID %d\n", vm->mmio_count);
    }
}
