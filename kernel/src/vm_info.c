#include "../include/kernel/platform.h"
#include "../include/kernel/printk.h"
#include "../include/kernel/vm_info.h"

static inline uint32_t vm_read32(uint32_t addr) {
    return *(volatile uint32_t *)(uintptr_t)addr;
}

int vm_info_load_boot(boot_info_t *out) {
    if (!out) {
        return 0;
    }

    out->magic = vm_read32(BOOTINFO_ADDR + 0x00u);
    out->version = vm_read32(BOOTINFO_ADDR + 0x04u);
    out->size = vm_read32(BOOTINFO_ADDR + 0x08u);
    for (uint32_t i = 0; i < 4u; i++) {
        out->vendor_words[i] = vm_read32(BOOTINFO_ADDR + 0x0Cu + i * 4u);
    }
    out->mem_bytes_lo = vm_read32(BOOTINFO_ADDR + 0x1Cu);
    out->mem_bytes_hi = vm_read32(BOOTINFO_ADDR + 0x20u);
    out->disk_bytes_lo = vm_read32(BOOTINFO_ADDR + 0x24u);
    out->disk_bytes_hi = vm_read32(BOOTINFO_ADDR + 0x28u);
    out->smp_cores = vm_read32(BOOTINFO_ADDR + 0x2Cu);

    if (out->magic != BOOTINFO_MAGIC || out->version != BOOTINFO_VERSION) {
        return 0;
    }
    return 1;
}

static void vm_info_print_vendor(const uint32_t vendor_words[4]) {
    int printed = 0;
    for (uint32_t wi = 0; wi < 4u; wi++) {
        uint32_t w = vendor_words[wi];
        for (uint32_t bi = 0; bi < 4u; bi++) {
            uint32_t c = (w >> (bi * 8u)) & 0xFFu;
            if (c == 0u) {
                return;
            }
            kputc(c);
            printed = 1;
        }
    }
    if (!printed) {
        kprintf("unknown");
    }
}

void vm_info_log_boot(void) {
    boot_info_t info;
    if (!vm_info_load_boot(&info)) {
        kprintf("VMINFO: BOOTINFO MISSING\n");
        return;
    }

    kprintf("VMINFO: VENDOR=");
    vm_info_print_vendor(info.vendor_words);
    kprintf(" MEM_LO=");
    kprint_hex32(info.mem_bytes_lo);
    kprintf(" MEM_HI=");
    kprint_hex32(info.mem_bytes_hi);
    kprintf(" DISK_LO=");
    kprint_hex32(info.disk_bytes_lo);
    kprintf(" DISK_HI=");
    kprint_hex32(info.disk_bytes_hi);
    kprintf(" SMP=");
    kprint_hex32(info.smp_cores);
    kprintf("\n");
}
