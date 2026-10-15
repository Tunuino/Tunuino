# SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
# Console sources are resolved relative to the invoking Makefile directory.
# firmware/platform/stm32f429/Makefile includes this file.
TUNUINO_CONSOLE_SOURCES := \
  ../../console/console_protocol.c ../../core/console_tune_read.c \
  ../../console/console_tx_queue.c ../../core/console_tune_request.c \
  ../../core/console_tune_rx.c ../../console/console_uart_pump.c \
  ../../core/console_tune_link.c ../../core/console_tx.c
