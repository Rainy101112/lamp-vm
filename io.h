#ifndef VM_IO_H
#define VM_IO_H
void accept_io(VM *vm, int addr, int value);

enum IO_TABLE {
    SCREEN = 0x01,
    SCREEN_ATTRIBUTE = 0x02,
    KEYBOARD = 0x03,
    DISK_CMD = 0x10,
    DISK_LBA = 0x11,
    DISK_MEM = 0x12,
    DISK_COUNT = 0x13,
    DISK_STATUS = 0x14,
};
#endif