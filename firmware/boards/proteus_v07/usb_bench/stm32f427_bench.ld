/* Copyright (c) 2026 Tunuino. Experimental direct-flash bench image.
 * Address 0x08000000 replaces the resident application/flash bootloader.
 * No option-byte changes. Use only after backing up the existing installation. */
ENTRY(Reset_Handler)
MEMORY {
  FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 128K
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
  .isr_vector : { . = ALIGN(512); KEEP(*(.isr_vector)) } > FLASH
  .text : {
    *(.text*) *(.rodata*)
    KEEP(*(.init)) KEEP(*(.fini))
    . = ALIGN(4);
  } > FLASH
  .ARM.extab : { *(.ARM.extab*) } > FLASH
  .ARM.exidx : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
  .preinit_array : {
    PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } > FLASH
  .init_array : {
    PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } > FLASH
  .fini_array : {
    PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } > FLASH
  _sidata = LOADADDR(.data);
  .data : { . = ALIGN(4); _sdata = .; *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
  .bss (NOLOAD) : { . = ALIGN(8); _sbss = .; *(.bss*) *(COMMON) . = ALIGN(8); _ebss = .; } > RAM
  _end = .;
  ASSERT(_ebss <= _estack - 16384, "Insufficient RAM for reserved 16 KiB stack")
  /DISCARD/ : { *(.note.GNU-stack) }
}
