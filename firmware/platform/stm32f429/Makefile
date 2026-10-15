# SPDX-License-Identifier: MIT
TOOLCHAIN ?= arm-none-eabi-
CC := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE := $(TOOLCHAIN)size
# Default standalone image; set only for a verified installed bootloader.
APP_FLASH_ORIGIN ?= 0x08000000
CFLAGS := -mcpu=cortex-m4 -mthumb -DSTM32F429xx -std=c11 -Os -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -Werror
LDFLAGS := -mcpu=cortex-m4 -mthumb -nostdlib -Wl,--gc-sections -Wl,-Map=build/tunuino.map -T linker.ld -Wl,--defsym=APP_FLASH_ORIGIN=$(APP_FLASH_ORIGIN)
include ../../console/console.mk
SOURCES := startup.c main.c board.c startup_contract.c mcu_clock.c mcu_tick.c usart_console.c can1.c wideband_can.c \
  ../../core/tune_maps.c ../../core/tune_validation.c ../../core/tune_store.c \
  ../../core/tune_wire.c ../../core/sensor_state.c $(TUNUINO_CONSOLE_SOURCES)
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))
.PHONY: all clean
all: build/tunuino.elf build/tunuino.bin
build:
	mkdir -p build
build/%.o: %.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
build/tunuino.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -lgcc -o $@
	$(SIZE) $@
build/tunuino.bin: build/tunuino.elf
	$(OBJCOPY) -O binary $< $@
clean:
	rm -rf build
