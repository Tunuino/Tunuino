# SPDX-License-Identifier: MIT
TOOLCHAIN ?= arm-none-eabi-
CC := $(TOOLCHAIN)gcc
OBJCOPY := $(TOOLCHAIN)objcopy
SIZE := $(TOOLCHAIN)size
CFLAGS := -mcpu=cortex-m4 -mthumb -std=c11 -Os -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra -Werror
LDFLAGS := -mcpu=cortex-m4 -mthumb -nostdlib -Wl,--gc-sections -Wl,-Map=build/tunuino.map -T linker.ld
SOURCES := startup.c main.c board.c startup_contract.c mcu_clock.c mcu_tick.c
OBJECTS := $(patsubst %.c,build/%.o,$(SOURCES))
.PHONY: all clean
all: build/tunuino.elf build/tunuino.bin
build:
	mkdir -p build
build/%.o: %.c | build
	$(CC) $(CFLAGS) -c $< -o $@
build/tunuino.elf: $(OBJECTS) linker.ld
	$(CC) $(CFLAGS) $(OBJECTS) $(LDFLAGS) -o $@
	$(SIZE) $@
build/tunuino.bin: build/tunuino.elf
	$(OBJCOPY) -O binary $< $@
clean:
	rm -rf build
