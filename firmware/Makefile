# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Tunuino
# Firmware-level build entry point, following the repository module boundary.
# The STM32F429 is currently the only supported firmware target.
BOARD ?= stm32f429
SUPPORTED_BOARDS := stm32f429
ifeq ($(filter $(BOARD),$(SUPPORTED_BOARDS)),)
$(error Unsupported BOARD '$(BOARD)'; supported: $(SUPPORTED_BOARDS))
endif
.PHONY: all clean
all:
	$(MAKE) -C platform/$(BOARD) all
clean:
	$(MAKE) -C platform/$(BOARD) clean
