# lamp-vm

## Compile

The project's Cmake file is currently forced to compile in aarch64 mode since I am using a Macbook, some changes must be
done if you want to compile it by yourself.

## Features

Assembler could be found at (lampvm-assembler)[https://github.com/glowingstone124/lampvm-toolchain]

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
