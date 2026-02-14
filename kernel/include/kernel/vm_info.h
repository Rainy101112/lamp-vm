#ifndef LAMP_KERNEL_VM_INFO_H
#define LAMP_KERNEL_VM_INFO_H

#include "kernel/types.h"

typedef struct {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t vendor_words[4];
    uint32_t mem_bytes_lo;
    uint32_t mem_bytes_hi;
    uint32_t disk_bytes_lo;
    uint32_t disk_bytes_hi;
    uint32_t smp_cores;
} boot_info_t;

int vm_info_load_boot(boot_info_t *out);
void vm_info_log_boot(void);

#endif
