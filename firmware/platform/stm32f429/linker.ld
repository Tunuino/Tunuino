/* SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0 */
/* Tunuino STM32F429ZIT6: 2 MiB flash, 256 KiB SRAM including CCM.
 * Initial firmware uses only the contiguous 128 KiB SRAM region. */
ENTRY(Reset_Handler)
/* Override via --defsym=APP_FLASH_ORIGIN=... only after bootloader layout verification. */
APP_FLASH_ORIGIN = DEFINED(APP_FLASH_ORIGIN) ? APP_FLASH_ORIGIN : 0x08000000;
ASSERT(APP_FLASH_ORIGIN >= 0x08000000 && APP_FLASH_ORIGIN < 0x08200000 &&
       (APP_FLASH_ORIGIN % 0x4000) == 0, "Invalid application flash origin")
MEMORY {
  FLASH (rx)  : ORIGIN = APP_FLASH_ORIGIN, LENGTH = 0x08200000 - APP_FLASH_ORIGIN
  RAM (rwx)   : ORIGIN = 0x20000000, LENGTH = 128K
}
_estack = ORIGIN(RAM) + LENGTH(RAM);
SECTIONS {
  .isr_vector : { . = ALIGN(512); KEEP(*(.isr_vector)) } > FLASH
  .text : { *(.text*) *(.rodata*) . = ALIGN(4); } > FLASH
  .ARM.exidx : { *(.ARM.exidx*) } > FLASH
  .data : {
    . = ALIGN(4);
    _sdata = .;
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } > RAM AT > FLASH
  _sidata = LOADADDR(.data);
  .bss (NOLOAD) : {
    . = ALIGN(4);
    _sbss = .;
    *(.bss*) *(COMMON)
    . = ALIGN(4);
    _ebss = .;
  } > RAM
  /DISCARD/ : { *(.comment*) *(.note*) }
}
ASSERT(_estack > _ebss, "RAM overflow")
