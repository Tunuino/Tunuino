# SPDX-License-Identifier: MIT
# Tunuino firmware console sources.
# Paths are relative to firmware/console/.
TUNUINO_CONSOLE_SOURCES := \
  console_protocol.c ../core/console_tune_read.c \
  console_tx_queue.c ../core/console_tune_request.c \
  ../core/console_tune_rx.c console_uart_pump.c \
  ../core/console_tune_link.c ../core/console_tx.c
