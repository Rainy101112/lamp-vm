# BIOS Build Guide

This directory contains the LampVM BIOS source (`bios.c`).

> This project may split into a separate repository in the future.

## Prerequisites

This project needs a custom LLVM toolchain to compile.

## Output Format A: Legacy Flat `boot.bin` (VM `--bin` format)

This format is:

- 24-byte little-endian header (`TEXT_BASE/TEXT_SIZE/DATA_BASE/DATA_SIZE/BSS_BASE/BSS_SIZE`)
- followed by text bytes
- followed by data bytes

### Build commands

```bash
$LAMP_CLANG --target=lamp-unknown-unknown -c bios.c -o bios.o

# One ld command directly emits boot.bin in legacy flat format.
$LAMP_LD -T boot_flat.ld bios.o -o boot.bin
```

### Run with VM

```bash
./build/vm --bin bios/boot.bin --smp 1
```

## Output Format B: ELF (`bios.elf`)

Useful for inspection/debugging of symbols/sections.

```bash
$LAMP_CLANG --target=lamp-unknown-unknown -c bios.c -o bios.o
$LAMP_LD bios.o -e _start -o bios.elf
```

To inspect text asm only:

```bash
$LAMP_CLANG --target=lamp-unknown-unknown -S bios.c -o bios.s
```

## Boot Contract Reminder

- BIOS itself is loaded by VM from `--bin` file.
- BIOS loads kernel ELF from `disk.img` at `LBA 1` (see `bios.c`, `docs/bios.md`).

