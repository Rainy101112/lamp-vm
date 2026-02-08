ENTRY(kernel_entry)

SECTIONS
{
  /* Keep kernel image in low memory; BIOS jumps to ELF e_entry. */
  . = 0x00100000;

  .text ALIGN(0x1000) : {
    *(.text .text.*)
  }

  .rodata ALIGN(0x1000) : {
    *(.rodata .rodata.*)
  }

  .data ALIGN(0x1000) : {
    *(.data .data.*)
  }

  .bss ALIGN(0x1000) : {
    __bss_start = .;
    *(.bss .bss.*)
    *(COMMON)
    __bss_end = .;
  }

  __kernel_end = .;
}
