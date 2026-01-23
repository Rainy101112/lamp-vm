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

// SCREEN / SCREEN_ATTRIBUTE / KEYBOARD are repurposed as a basic serial device:
// SCREEN: TX write-only, KEYBOARD: RX read-only, SCREEN_ATTRIBUTE: status/control.
#define SERIAL_STATUS_RX_READY 0x01
#define SERIAL_STATUS_TX_READY 0x02
#define SERIAL_CTRL_RX_INT_ENABLE 0x01
#endif
