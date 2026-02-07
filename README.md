# lamp-vm

## Compile

The project's Cmake file is currently forced to compile in aarch64 mode since I am using a Macbook, some changes must be
done if you want to compile it by yourself.

## Features

Assembler could be found at (lampvm-assembler)[https://github.com/glowingstone124/lampvm-toolchain]

## Run

```bash
./vm --bin boot.bin --smp 1
```

Arguments:
- `--bin <file>`: program binary path (default: `boot.bin`)
- `--smp <cores>`: CPU worker thread count in `[1, 64]` (default: `1`)
- `--selftest`: run built-in SMP/atomic self-tests and exit

SMP note:
- `--smp > 1` enables an experimental mode with multiple CPU worker threads.
- Current implementation uses per-core architectural state (registers/IP/flags/stacks/interrupt context).
- Cores share memory/MMIO/IO and device model.

### SMP Memory Model
> Old programs built for single core which don't operate with hard coded stack addr *should* run correctly.
> For max compatibility, please set --smp 1.
- Shared memory model is **sequentially consistent (SC)** at VM level.
- All VM memory/MMIO/IO accesses are serialized through a shared VM lock.
- Per-core architectural state (`regs/ip/flags/stacks`) is private to each core and not shared.
- Interrupt pending bits are per-core atomic flags; `IPI` delivers to a specific target core.

### SMP Stack Layout

- Stacks are stored in VM memory addresses (not host-side arrays).
- For `--smp 1`, legacy stack addresses are kept:
  - `CALL_STACK_BASE`, `DATA_STACK_BASE`, `ISR_STACK_BASE`
- For `--smp > 1`, each core gets a dedicated stack region near the top of RAM.
- VM validates that program/data/bss do not overlap the SMP stack pool at startup.

### BSP/AP Model

- `CPU0` is the BSP and starts executing immediately.
- `CPU1..N-1` are APs and remain parked until BSP starts them.
- AP startup is controlled by instruction `STARTAP`.
- Runtime core identity is exposed by instruction `CPUID`.

### Atomic Instructions

Recommended synchronization path is ISA instructions (not I/O ports):

- `CAS`, `XADD`, `XCHG` (atomic RMW)
- `LDAR`, `STLR` (acquire/release)
- `FENCE` (full fence)
- `PAUSE` (spin-wait hint)
- `IPI` (targeted inter-processor interrupt)

For all atomic memory instructions, unaligned addresses trigger VM panic.

## Program Binary Format

The VM expects a single program binary with a 24-byte header followed by text and data:

- Header: 6 little-endian `u32` values
  1. `TEXT_BASE`
  2. `TEXT_SIZE` (bytes)
  3. `DATA_BASE`
  4. `DATA_SIZE` (bytes)
  5. `BSS_BASE`
  6. `BSS_SIZE` (bytes)
- Text section: instruction stream, `u64` little-endian
- Data section: raw bytes

This matches the output from the toolchain.

### Panic

VM will panic if a bad instruction was executed, and a debug message will be print.

### Debug (optional, compile-time)

Enable with CMake:

```bash
cmake -DVM_DEBUG=ON ..
```

When enabled, the VM includes:
- Instruction statistics (per-opcode counts).
- Memory alignment checks for 32/64-bit reads and writes.
- Interactive single-step and breakpoints.

Runtime controls (only when built with `VM_DEBUG=ON`):
- `VM_DEBUG_STEP=1` or `VM_STEP=1` starts in single-step mode.
- `VM_DEBUG_PAUSE=1` pauses immediately at start.
- `VM_BREAKPOINTS=0x201C,0x2024` sets breakpoints (comma/space-separated, hex or decimal).

Interactive debugger commands:
- `s` step, `c` continue
- `r` registers
- `m <addr> <len>` memory dump
- `b <addr>` add breakpoint, `d <addr>` remove breakpoint, `l` list breakpoints
- `q` quit VM

### Current Memory Mapping

| Type        | Start Addr        | End Addr    | Size    | Usage                            |
|-------------|-------------------|-------------|---------|----------------------------------|
| IVT         | 0x000000          | 0x0007FF    | 2048 B  | IVT, 8B each                     |
| CALL_STACK  | 0x000800          | 0x000FFF    | 2048 B  | Call Stack                       |
| DATA_STACK  | 0x001000          | 0x0017FF    | 2048 B  | Data Stack                       |
| ISR_STACK   | 0x001800          | 0x001FFF    | 2048 B  | Interrupt Stack (saved context)  |
| Timedate    | 0x002000          | 0x00201B    | 28 B    | Time (control + 3×u64)           |
| PROGRAM     | 0x00201C          | FB start    | ~4 MB   | Program                          |
| FrameBuffer | MEM_END - FB_SIZE | MEM_END - 1 | depends | FrameBuffer                      |


...

### Interrupt Tables(IVT) Mapping

In default, LampVM supports 256 interrupt ids. This vector starts at memory address 0x0, since memory is actually a
segment on heap space.

Keyboard Input now has the highest priority, it's located on 0x00. Edit 0x00 first in your program to configure the
address handling Keyboard Interrupt.

## Roadmap

Write Wiki

Implements a standard VGA-like screen.

Implements a virtual hard disk.

Create a assembler and make a subset of C.

## Instructions

### Instructions Mapping

Please see [ISA](docs/isa.md)
