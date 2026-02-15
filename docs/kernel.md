# Kernel Scaffold

> This project may split into a separate repository in the future.

This document describes the current kernel scaffold and bring-up flow, aligned with `bios.md`.

## Goal

Establish a stable bring-up baseline before implementing policy:

1. `kernel_entry` early init
2. trap/IRQ table ownership
3. timer-driven tick hook
4. simple scheduler core (runqueue + sleep/wakeup + wait queue)
5. SMP stubs and spinlock primitives

## Layout

- `include/kernel/platform.h`: platform constants (interrupt numbers, memory constants)
- `include/kernel/types.h`: private freestanding C11 integer/pointer type aliases
- `include/kernel/kernel.h`: kernel boot sequence API
- `include/kernel/console.h`: console ring-buffer and IO API
- `include/kernel/init_task.h`: built-in init task bootstrap
- `include/kernel/printk.h`: console print + leveled kernel log API
- `include/kernel/trap.h`: trap/IRQ entry and dispatch API
- `include/kernel/irq.h`: IRQ handlers API
- `include/kernel/sched.h`: scheduler API
- `include/kernel/syscall.h`: syscall numbers and dispatcher API
- `include/kernel/smp.h`: BSP/AP interfaces
- `include/kernel/vm_info.h`: BootInfo metadata from BIOS handoff
- `include/kernel/spinlock.h`: lock primitive API
- `src/entry.c`: `kernel_entry` and top-level init sequence
- `src/console.c`: console core (`rx` ring buffer, wait queue, read/write path, no implicit rx echo)
- `src/init_task.c`: kernel init task (`init$` command loop and runtime controls)
- `src/trap.c`: trap dispatch skeleton
- `src/irq.c`: IRQ handlers skeleton
- `src/sched.c`: task scheduler core (single-core, cooperative step model)
- `src/syscall.c`: syscall dispatcher and ABI mailbox publish
- `src/smp.c`: SMP bootstrap skeleton
- `src/vm_info.c`: BootInfo decode/log helper
- `src/spinlock.c`: spinlock skeleton (to be wired to atomic ISA ops)

## Bring-up Order

1. Implement serial/console output in `irq.c` or dedicated console module.
2. Implement timer interrupt registration in `trap_init()`.
3. Implement `schedule_tick()` and task context switch path.
4. Enable SMP via `smp_init_bsp()` after single-core path is stable.

## Scheduler Status

Current `sched` is intentionally a transition design for POSIX bring-up:

- fixed task table (`SCHED_MAX_TASKS`)
- round-robin runnable selection with tick quantum
- task states: `RUNNABLE/RUNNING/SLEEPING/BLOCKED/ZOMBIE`
- blocking primitives: `sched_sleep_ticks()` and wait-queue wakeup

Current limitations:

- execution model is cooperative "task step" callbacks, not full register-context switch yet
- this is enough to validate timeout/blocking semantics and syscall-facing scheduler APIs before low-level context-switch code lands

## Syscall ABI (Current)

- interrupt vector: `IRQ_SYSCALL = 0x80`
- input registers at trap entry: `r0=nr`, `r1..r6=arg0..arg5`
- initial syscalls: `getpid`, `yield`, `sleep_ticks`, `exit`, `waitpid`, `nanosleep`, `read`, `write`, `poll`, `select`, `tty_getmode`, `tty_setmode`
- return publishing: fixed mailbox at `SYSCALL_ABI_ADDR (0x002FE000)`

Note:

- VM currently restores caller registers on `IRET`, so direct register return is not yet available.
- The dispatcher writes `ret/errno` and the last call snapshot into the syscall mailbox.

## Logging (Current)

- unified log prefix format: `[LVL][tag] message`
- levels: `ERR`, `WRN`, `INF`, `DBG`
- default level is controlled by `KERNEL_LOG_LEVEL_DEFAULT` in `include/kernel/platform.h`
- panic path keeps forced error-level output and clears console with panic colors

## Console Behavior (Current)

- default tty local mode: `ECHO|ICANON|ISIG`
- RX path normalizes `\r` to `\n`
- RX path handles backspace/delete (`0x08`/`0x7F`) by deleting the latest unread non-newline byte
- when `ISIG` is enabled, `^C` clears current input fragment and terminates line
- RX path counts complete lines and exposes dropped-byte stats for diagnostics
- input bytes are queued through tty line discipline (echo is controlled by tty mode bits)
- serial IRQ handler drains all pending RX bytes in one interrupt

## Read/Write Semantics (Current)

- `read(fd=0)` and `write(fd=1|2)` support larger user buffers via internal chunk loops
- `read` preserves short-read behavior:
  - blocks only for the first chunk when in blocking mode
  - once partial data is copied, subsequent chunks are polled nonblocking
- nonblocking `read` returns `-1/EAGAIN` when no data is available

## Poll/Select/TTY (Current)

- `poll` ABI: `arg0=pollfd*`, `arg1=nfds`, `arg2=timeout_ms`
- `select` ABI: `arg0=nfds`, `arg1=read_mask*`, `arg2=write_mask*`, `arg3=except_mask*`, `arg4=timeout_ms`
- fd model (current): `0=stdin`, `1=stdout`, `2=stderr`
- `tty_getmode(fd)` and `tty_setmode(fd, lflag)` expose tty local mode bits
- blocking `poll/select` currently follow transition semantics: task is parked/slept then syscall returns `-1/EAGAIN`, caller retries

## Contract With BIOS

BIOS behavior is fixed by `bios.md`:

- BIOS loads ELF PT_LOAD segments and jumps to `e_entry`.
- BIOS publishes BootInfo at fixed address before handoff.
- Kernel must reinitialize IVT/trap policy on entry.
- Kernel must not assume register state beyond control transfer.
- Kernel headers avoid libc integer headers and use `kernel/types.h`.
- Kernel build-time `KERNEL_MEM_SIZE` should match BIOS/VM memory-size contract for direct-mapped assumptions (`FB_BASE`, pointer-range checks).
- Kernel validates BootInfo `mem_bytes` at boot and warns on mismatch.

## Build To Load Pipeline

This is the practical end-to-end flow used by the current BIOS:

1. Build `kernel.elf`
2. Write `kernel.elf` into `disk.img` at `LBA 1`
3. Build BIOS `boot.bin`
4. Run the VM with BIOS as `--bin`

### 1) Build kernel ELF

Current recommended flags for backend bring-up:

- `-O0` (some files may still hit ISel issues at higher optimization)
- `-ffreestanding -fno-builtin -fno-stack-protector`

- This project needs a custom LLVM toolchain.
```bash
mkdir -p build-kernel

for f in kernel/src/*.c; do
  $LAMP_CLANG --target=lamp-unknown-unknown \
    -ffreestanding -fno-builtin -fno-stack-protector -O0 \
    -Ikernel/include -c "$f" -o "build-kernel/$(basename "$f" .c).o"
done

$LAMP_LD -T kernel/linker.ld -e kernel_entry build-kernel/*.o -o build-kernel/kernel.elf
```

### 2) Write kernel ELF to `disk.img` at LBA 1

BIOS reads kernel from `LBA 1` (offset `512` bytes).

```bash
test -f disk.img || truncate -s 512M disk.img
dd if=build-kernel/kernel.elf of=disk.img bs=512 seek=1 conv=notrunc
```

### 3) Build BIOS boot image

```bash
$LAMP_CLANG --target=lamp-unknown-unknown -c bios/bios.c -o bios/bios.o
$LAMP_LD -T bios/boot_flat.ld bios/bios.o -o bios/boot.bin
```

### 4) Boot VM

```bash
./build/vm --bin bios/boot.bin --smp 1
```
