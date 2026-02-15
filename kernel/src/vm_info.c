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
    if (out->magic != BOOTINFO_MAGIC || out->version != BOOTINFO_VERSION || out->size < BOOTINFO_SIZE) {
        return 0;
    }

    for (uint32_t i = 0; i < 4u; i++) {
        out->vendor_words[i] = vm_read32(BOOTINFO_ADDR + 0x0Cu + i * 4u);
    }
    out->mem_bytes_lo = vm_read32(BOOTINFO_ADDR + 0x1Cu);
    out->mem_bytes_hi = vm_read32(BOOTINFO_ADDR + 0x20u);
    out->disk_bytes_lo = vm_read32(BOOTINFO_ADDR + 0x24u);
    out->disk_bytes_hi = vm_read32(BOOTINFO_ADDR + 0x28u);
    out->smp_cores = vm_read32(BOOTINFO_ADDR + 0x2Cu);
    out->layout_version = vm_read32(BOOTINFO_ADDR + 0x30u);
    out->arch_id = vm_read32(BOOTINFO_ADDR + 0x34u);
    out->endian = vm_read32(BOOTINFO_ADDR + 0x38u);
    out->phys_addr_bits = vm_read32(BOOTINFO_ADDR + 0x3Cu);
    out->page_size = vm_read32(BOOTINFO_ADDR + 0x40u);
    out->timer_freq_hz = vm_read32(BOOTINFO_ADDR + 0x44u);
    out->features = vm_read32(BOOTINFO_ADDR + 0x48u);
    out->fb_width = vm_read32(BOOTINFO_ADDR + 0x4Cu);
    out->fb_height = vm_read32(BOOTINFO_ADDR + 0x50u);
    out->fb_bpp = vm_read32(BOOTINFO_ADDR + 0x54u);
    out->fb_stride_bytes = vm_read32(BOOTINFO_ADDR + 0x58u);
    out->boot_realtime_ns_lo = vm_read32(BOOTINFO_ADDR + 0x5Cu);
    out->boot_realtime_ns_hi = vm_read32(BOOTINFO_ADDR + 0x60u);
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

static void vm_info_log_features(uint32_t features) {
    int first = 1;
    kprintf("VMINFO: FEAT=");
    kprint_hex32(features);
    kprintf(" [");
    if (features & BOOTINFO_FEATURE_TIME_MMIO) {
        if (!first) kputc((uint32_t)' ');
        kprintf("TIME");
        first = 0;
    }
    if (features & BOOTINFO_FEATURE_FB_MMIO) {
        if (!first) kputc((uint32_t)' ');
        kprintf("FB");
        first = 0;
    }
    if (features & BOOTINFO_FEATURE_DISK_IO) {
        if (!first) kputc((uint32_t)' ');
        kprintf("DISK");
        first = 0;
    }
    if (features & BOOTINFO_FEATURE_SMP) {
        if (!first) kputc((uint32_t)' ');
        kprintf("SMP");
        first = 0;
    }
    if (features & BOOTINFO_FEATURE_TIMER_IRQ) {
        if (!first) kputc((uint32_t)' ');
        kprintf("TIMER_IRQ");
        first = 0;
    }
    if (first) {
        kprintf("none");
    }
    kprintf("]\n");
}

void vm_info_log_boot(void) {
    boot_info_t info;
    if (!vm_info_load_boot(&info)) {
        kprintf("VMINFO: BOOTINFO MISSING\n");
        return;
    }

    kprintf("VMINFO: VENDOR=");
    vm_info_print_vendor(info.vendor_words);
    kprintf("\n");
    kprintf(" MEM_LO=");
    kprint_hex32(info.mem_bytes_lo);
    kprintf("\n");
    kprintf(" MEM_HI=");
    kprint_hex32(info.mem_bytes_hi);
    kprintf("\n");
    kprintf(" DISK_LO=");
    kprint_hex32(info.disk_bytes_lo);
    kprintf("\n");
    kprintf(" DISK_HI=");
    kprint_hex32(info.disk_bytes_hi);
    kprintf("\n");
    kprintf(" SMP=");
    kprint_hex32(info.smp_cores);
    kprintf("\n");

    kprintf("VMINFO: LAYOUT=");
    kprint_hex32(info.layout_version);
    kprintf("\n");
    kprintf(" ARCH=");
    kprint_hex32(info.arch_id);
    kprintf("\n");
    kprintf(" ENDIAN=");
    kprint_hex32(info.endian);
    kprintf("\n");
    kprintf(" PADDR_BITS=");
    kprint_hex32(info.phys_addr_bits);
    kprintf("\n");
    kprintf(" PAGE=");
    kprint_hex32(info.page_size);
    kprintf("\n");
    kprintf(" TIMER_HZ=");
    kprint_hex32(info.timer_freq_hz);
    kprintf("\n");
    kprintf("\n");

    kprintf("VMINFO: FB_W=");
    kprint_hex32(info.fb_width);
    kprintf("\n");
    kprintf(" FB_H=");
    kprint_hex32(info.fb_height);
    kprintf("\n");
    kprintf(" FB_BPP=");
    kprint_hex32(info.fb_bpp);
    kprintf("\n");
    kprintf(" FB_STRIDE=");
    kprint_hex32(info.fb_stride_bytes);
    kprintf("\n");
    kprintf(" BOOT_RT_LO=");
    kprint_hex32(info.boot_realtime_ns_lo);
    kprintf("\n");
    kprintf(" BOOT_RT_HI=");
    kprint_hex32(info.boot_realtime_ns_hi);
    kprintf("\n");

    vm_info_log_features(info.features);

    if (info.layout_version != SYSINFO_LAYOUT_VERSION) {
        kprintf("VMINFO: WARN SYSINFO LAYOUT MISMATCH\n");
    }
    if (info.arch_id != BOOTINFO_ARCH_LAMP32 || info.endian != BOOTINFO_ENDIAN_LITTLE) {
        kprintf("VMINFO: WARN ARCH/ENDIAN MISMATCH\n");
    }
    if (info.mem_bytes_hi != 0u || info.mem_bytes_lo != KERNEL_MEM_SIZE) {
        kprintf("VMINFO: WARN MEM_SIZE CONTRACT MISMATCH EXPECT=");
        kprint_hex32(KERNEL_MEM_SIZE);
        kprintf(" GOT_LO=");
        kprint_hex32(info.mem_bytes_lo);
        kprintf(" GOT_HI=");
        kprint_hex32(info.mem_bytes_hi);
        kprintf("\n");
    }
}
