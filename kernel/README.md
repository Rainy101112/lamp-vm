# Kernel Scaffold

This directory provides a minimal kernel-side scaffold aligned with `docs/bios.md`.

## Goal

Establish a stable bring-up skeleton before implementing policy:

1. `kernel_entry` early init
2. trap/IRQ table ownership
3. timer-driven tick hook
4. simple scheduler loop
5. SMP stubs and spinlock primitives

## Layout

- `include/kernel/platform.h`: platform constants (interrupt numbers, memory constants)
- `include/kernel/types.h`: private freestanding C11 integer/pointer type aliases
- `include/kernel/kernel.h`: kernel boot sequence API
- `include/kernel/trap.h`: trap/IRQ entry and dispatch API
- `include/kernel/irq.h`: IRQ handlers API
- `include/kernel/sched.h`: scheduler API
- `include/kernel/smp.h`: BSP/AP interfaces
- `include/kernel/spinlock.h`: lock primitive API
- `src/entry.c`: `kernel_entry` and top-level init sequence
- `src/trap.c`: trap dispatch skeleton
- `src/irq.c`: IRQ handlers skeleton
- `src/sched.c`: minimal scheduler loop
- `src/smp.c`: SMP bootstrap skeleton
- `src/spinlock.c`: spinlock skeleton (to be wired to atomic ISA ops)

## Bring-up Order

1. Implement serial/console output in `irq.c` or dedicated console module.
2. Implement timer ISR registration in `trap_init()`.
3. Implement `schedule_tick()` and task context switch path.
4. Enable SMP via `smp_init_bsp()` after single-core path is stable.

## Contract With BIOS

BIOS behavior is fixed by `docs/bios.md`:

- BIOS loads ELF PT_LOAD segments and jumps to `e_entry`.
- Kernel must reinitialize IVT/trap policy on entry.
- Kernel must not assume register state beyond control transfer.
- Kernel headers avoid libc integer headers and use `kernel/types.h`.
