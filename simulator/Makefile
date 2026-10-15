# SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
# Native validation harness. Full ECU simulation is not implemented yet.
BUILD_DIR ?= ../build/simulator
.PHONY: all test clean
all: test
test:
	cmake -S .. -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR) --parallel 2
	ctest --test-dir $(BUILD_DIR) --output-on-failure
clean:
	cmake -E rm -rf $(BUILD_DIR)
