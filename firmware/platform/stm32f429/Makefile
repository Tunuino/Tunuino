# SPDX-License-Identifier: MIT
TOOLCHAIN ?= arm-none-eabi-
CC := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE := $(TOOLCHAIN)size
CFLAGS := -mcpu=cortex-m4 -mthumb -DSTM32F429xx -std=c11 -Os -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -Werror
LDFLAGS := -mcpu=cortex-m4 -mthumb -nostdlib -Wl,--gc-sections -Wl,-Map=build/tunuino.map -T linker.ld
SOURCES := startup.c main.c board.c startup_contract.c mcu_clock.c mcu_tick.c usart_console.c \
  ../../core/tune_maps.c ../../core/tune_validation.c ../../core/tune_store.c \
  ../../core/tune_wire.c ../../core/console_protocol.c ../../core/console_tune_read.c \
  ../../core/console_tx_queue.c ../../core/console_tune_request.c \
  ../../core/console_tune_rx.c ../../core/console_uart_pump.c ../../core/console_tune_link.c \
  ../../core/console_tx.c
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))
.PHONY: all clean
all: build/tunuino.elf build/tunuino.bin
build:
	mkdir -p build
build/%.o: %.c | build
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
build/tunuino.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@
build/tunuino.bin: build/tunuino.elf
	$(OBJCOPY) -O binary $< $@
clean:
	rm -rf build
