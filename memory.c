//
// Created by Max Wang on 2026/1/3.
//
#include "memory.h"

#include <string.h>

#include "mmio.h"
#include "panic.h"
#include "vm.h"

static inline int in_ram(VM *vm, vm_addr_t addr, size_t size) {
    return addr + size <= vm->memory_size;
}

static inline _Atomic uint32_t *atomic32_ptr_or_panic(VM *vm, vm_addr_t addr, const char *op_name) {
    if ((addr % _Alignof(_Atomic uint32_t)) != 0) {
        panic(panic_format("%s unaligned address: 0x%08x", op_name, addr), vm);
        return NULL;
    }
    if (!in_ram(vm, addr, sizeof(uint32_t))) {
        panic(panic_format("%s out of bounds: 0x%08x", op_name, addr), vm);
        return NULL;
    }
    if (find_mmio(vm, addr) != NULL) {
        panic(panic_format("%s does not support MMIO addr: 0x%08x", op_name, addr), vm);
        return NULL;
    }
    return (_Atomic uint32_t *)(void *)(&vm->memory[addr]);
}

static inline int fb_byte_index(VM *vm, vm_addr_t addr, size_t *out_index) {
    const size_t fb_base = FB_BASE(vm->memory_size);
    if (addr >= fb_base && addr < fb_base + FB_SIZE) {
        *out_index = (size_t)(addr - fb_base);
        return 1;
    }
    if (addr >= FB_LEGACY_BASE && addr < FB_LEGACY_BASE + FB_SIZE) {
        *out_index = (size_t)(addr - FB_LEGACY_BASE);
        return 1;
    }
    return 0;
}

#ifdef VM_MEMCHECK
static inline void memcheck_align(VM *vm, vm_addr_t addr, size_t align, const char *op) {
    if ((addr % align) != 0) {
        panic(panic_format("%s unaligned address: 0x%08x", op, addr), vm);
    }
}
#endif

uint8_t vm_read8(VM *vm, vm_addr_t addr) {
    vm_shared_lock(vm);
    size_t fb_index = 0;
    if (fb_byte_index(vm, addr, &fb_index)) {
        uint8_t v = ((uint8_t *) vm->fb)[fb_index];
        vm_shared_unlock(vm);
        return v;
    }
    if (!in_ram(vm, addr, 1)) {
        panic(panic_format("READ8 out of bounds: 0x%08x", addr), vm);
        vm_shared_unlock(vm);
        return 0;
    }
    uint8_t v = vm->memory[addr];
    vm_shared_unlock(vm);
    return v;
}

uint32_t vm_read32(VM *vm, vm_addr_t addr) {
    vm_shared_lock(vm);
#ifdef VM_MEMCHECK
    memcheck_align(vm, addr, 4, "READ32");
#endif
    MMIO_Device *dev = find_mmio(vm, addr);
    if (dev) {
        uint32_t v = vm_mmio_read32(vm, addr);
        vm_shared_unlock(vm);
        return v;
    }

    if (!in_ram(vm, addr, 4)) {
        panic(panic_format("READ32 out of bounds: 0x%08x", addr), vm);
        vm_shared_unlock(vm);
        return 0;
    }

    uint32_t v = 0;
    v |= vm->memory[addr + 0] << 0;
    v |= vm->memory[addr + 1] << 8;
    v |= vm->memory[addr + 2] << 16;
    v |= vm->memory[addr + 3] << 24;
    vm_shared_unlock(vm);
    return v;
}

uint64_t vm_read64(VM *vm, vm_addr_t addr) {
#ifdef VM_MEMCHECK
    memcheck_align(vm, addr, 8, "READ64");
#endif
    uint64_t lo = vm_read32(vm, addr);
    uint64_t hi = vm_read32(vm, addr + 4);
    return lo | (hi << 32);
}

uint32_t vm_atomic_load32_acquire(VM *vm, vm_addr_t addr) {
    _Atomic uint32_t *ptr = atomic32_ptr_or_panic(vm, addr, "LDAR");
    if (!ptr) {
        return 0;
    }
    return atomic_load_explicit(ptr, memory_order_acquire);
}

void vm_atomic_store32_release(VM *vm, vm_addr_t addr, uint32_t value) {
    _Atomic uint32_t *ptr = atomic32_ptr_or_panic(vm, addr, "STLR");
    if (!ptr) {
        return;
    }
    atomic_store_explicit(ptr, value, memory_order_release);
}

uint32_t vm_atomic_exchange32_seqcst(VM *vm, vm_addr_t addr, uint32_t value) {
    _Atomic uint32_t *ptr = atomic32_ptr_or_panic(vm, addr, "XCHG");
    if (!ptr) {
        return 0;
    }
    return atomic_exchange_explicit(ptr, value, memory_order_seq_cst);
}

uint32_t vm_atomic_fetch_add32_seqcst(VM *vm, vm_addr_t addr, uint32_t value) {
    _Atomic uint32_t *ptr = atomic32_ptr_or_panic(vm, addr, "XADD");
    if (!ptr) {
        return 0;
    }
    return atomic_fetch_add_explicit(ptr, value, memory_order_seq_cst);
}

uint32_t vm_atomic_compare_exchange32_seqcst(VM *vm,
                                             vm_addr_t addr,
                                             uint32_t expected,
                                             uint32_t desired,
                                             int *success) {
    _Atomic uint32_t *ptr = atomic32_ptr_or_panic(vm, addr, "CAS");
    if (!ptr) {
        if (success) {
            *success = 0;
        }
        return 0;
    }
    uint32_t observed = expected;
    int ok = atomic_compare_exchange_strong_explicit(ptr,
                                                     &observed,
                                                     desired,
                                                     memory_order_seq_cst,
                                                     memory_order_seq_cst);
    if (success) {
        *success = ok ? 1 : 0;
    }
    return observed;
}

void vm_write8(VM *vm, vm_addr_t addr, uint8_t value) {
    vm_shared_lock(vm);
    // intercept mmio request
    size_t fb_index = 0;
    if (fb_byte_index(vm, addr, &fb_index)) {
        ((uint8_t *) vm->fb)[fb_index] = value;
        vm_shared_unlock(vm);
        return;
    }

    if (!in_ram(vm, addr, 1)) {
        panic(panic_format("WRITE8 out of bounds: 0x%08x", addr), vm);
        vm_shared_unlock(vm);
        return;
    }

    vm->memory[addr] = value;
    vm_shared_unlock(vm);
}

void vm_write32(VM *vm, vm_addr_t addr, uint32_t value) {
    vm_shared_lock(vm);
#ifdef VM_MEMCHECK
    memcheck_align(vm, addr, 4, "WRITE32");
#endif
    MMIO_Device *dev = find_mmio(vm, addr);
    if (dev && dev->write32) {
        dev->write32(vm, addr, value);
        vm_shared_unlock(vm);
        return;
    }
    if (!in_ram(vm, addr, 4)) {
        panic(panic_format("WRITE32 out of bounds: 0x%08x", addr), vm);
        vm_shared_unlock(vm);
        return;
    }

    vm->memory[addr + 0] = (value >> 0) & 0xFF;
    vm->memory[addr + 1] = (value >> 8) & 0xFF;
    vm->memory[addr + 2] = (value >> 16) & 0xFF;
    vm->memory[addr + 3] = (value >> 24) & 0xFF;
    vm_shared_unlock(vm);
}

void vm_write64(VM *vm, vm_addr_t addr, uint64_t value) {
    vm_shared_lock(vm);
#ifdef VM_MEMCHECK
    memcheck_align(vm, addr, 8, "WRITE64");
#endif
    if (!in_ram(vm, addr, 8)) {
        panic(panic_format("WRITE64 out of bounds: 0x%08x", addr), vm);
        vm_shared_unlock(vm);
        return;
    }
    vm->memory[addr + 0] = (value >> 0) & 0xFF;
    vm->memory[addr + 1] = (value >> 8) & 0xFF;
    vm->memory[addr + 2] = (value >> 16) & 0xFF;
    vm->memory[addr + 3] = (value >> 24) & 0xFF;
    vm->memory[addr + 4] = (value >> 32) & 0xFF;
    vm->memory[addr + 5] = (value >> 40) & 0xFF;
    vm->memory[addr + 6] = (value >> 48) & 0xFF;
    vm->memory[addr + 7] = (value >> 56) & 0xFF;
    vm_shared_unlock(vm);
}
