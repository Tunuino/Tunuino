# SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
# Copyright (c) 2026 Tunuino
# Repository-level entry points; hardware outputs remain disabled.
# Native tests require CMake and a host C compiler.
# STM32 builds additionally require arm-none-eabi-gcc and binutils.
.PHONY: all test simulator firmware console tools clean
all: test

test:
	$(MAKE) -C simulator test

simulator:
	$(MAKE) -C simulator test

firmware:
	$(MAKE) -C firmware all

console:
	./gradlew :branding:build :logging:build :trigger-ui:build --no-daemon

tools:
	./gradlew :version:build :bin2header:build --no-daemon

clean:
	$(MAKE) -C simulator clean
	$(MAKE) -C firmware clean
