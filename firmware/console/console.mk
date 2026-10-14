# SPDX-License-Identifier: MIT
# Tunuino ECU console module, following the firmware/console/console.mk layout.
# Existing source files are referenced directly until migrated with their tests.
TUNUINO_CONSOLE_SOURCES := \
  ../../core/console_protocol.c ../../core/console_tune_read.c \
  ../../console/console_tx_queue.c ../../core/console_tune_request.c \
  ../../core/console_tune_rx.c ../../core/console_uart_pump.c \
  ../../core/console_tune_link.c ../../core/console_tx.c
