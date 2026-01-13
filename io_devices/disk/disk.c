//
// Created by Max Wang on 2025/12/30.
//
#include "../../vm.h"
#include "disk.h"

#include <stdlib.h>

#include "../../panic.h"

#include <unistd.h>

#include "../../memory.h"

#include "../../interrupt.h"

void disk_init(VM *vm, const char *path) {
    printf("cwd: %s\n", getcwd(NULL, 0));

    vm->disk.fp = fopen(path, "r+b");
    if (!vm->disk.fp) {
        printf("Disk image not found, creating new disk: %s\n", path);
        vm->disk.fp = fopen(path, "w+b");
        if (!vm->disk.fp) {
            perror("fopen");
            panic("Cannot create disk image", vm);
            return;
        }
        uint8_t *buf = calloc(1, DISK_SIZE);
        if (!buf) {
            panic("Failed to allocate memory for disk init", vm);
            return;
        }
        fwrite(buf, 1, DISK_SIZE, vm->disk.fp);
        fflush(vm->disk.fp);
        free(buf);
        fclose(vm->disk.fp);
        vm->disk.fp = fopen(path, "r+b");
        printf("Disk image created: %s, size %d bytes\n", path, DISK_SIZE);
    }
    vm->disk.lba = 0;
    vm->disk.mem_addr = 0;
    vm->disk.count = 0;
    vm->disk.status = 0;
}

void disk_read(VM *vm) {
    size_t addr = vm->disk.mem_addr;
    fseek(vm->disk.fp, vm->disk.lba * DISK_SECTOR_SIZE, SEEK_SET);

    uint8_t buf[DISK_SIZE];
    fread(buf, 1, DISK_SECTOR_SIZE * vm->disk.count, vm->disk.fp);

    for (size_t i = 0; i < vm->disk.count * DISK_SECTOR_SIZE; i++) {
        vm_write8(vm, addr + i, buf[i]);
    }
}

void disk_write(const VM *vm) {
    size_t addr = vm->disk.mem_addr;
    fseek(vm->disk.fp, vm->disk.lba * DISK_SECTOR_SIZE, SEEK_SET);

    uint8_t buf[DISK_SECTOR_SIZE * vm->disk.count];
    for (size_t i = 0; i < vm->disk.count * DISK_SECTOR_SIZE; i++) {
        buf[i] = vm_read8(vm, addr + i);
    }

    fwrite(buf, 1, vm->disk.count * DISK_SECTOR_SIZE, vm->disk.fp);
    fflush(vm->disk.fp);
    int fd = fileno(vm->disk.fp);
    if (fd >= 0) fsync(fd);
}

void disk_cmd(VM *vm, const int value) {
    if (vm->disk.status == DISK_STATUS_BUSY)
        return;

    vm->disk.pending_cmd = value;
    vm->disk.status = DISK_STATUS_BUSY;
}
void disk_tick(VM *vm) {
    if (vm->disk.status != DISK_STATUS_BUSY)
        return;

    switch (vm->disk.pending_cmd) {
        case DISK_CMD_READ:
            fseek(vm->disk.fp, vm->disk.lba * DISK_SECTOR_SIZE, SEEK_SET);
            fread(&vm->memory[vm->disk.mem_addr], DISK_SECTOR_SIZE, vm->disk.count, vm->disk.fp);
            break;

        case DISK_CMD_WRITE:
            fseek(vm->disk.fp, vm->disk.lba * DISK_SECTOR_SIZE, SEEK_SET);
            fwrite(&vm->memory[vm->disk.mem_addr], DISK_SECTOR_SIZE, vm->disk.count, vm->disk.fp);
            fflush(vm->disk.fp);
            fsync(fileno(vm->disk.fp));
            break;
    }

    vm->disk.status = DISK_STATUS_FREE;
    vm->disk.pending_cmd = 0;

    trigger_interrupt(vm, INT_DISK_COMPLETE);
}

