# SPDX-License-Identifier: LicenseRef-Tunuino-Proprietary-1.0
# Board-specific configuration in the reference firmware board.mk location.
BOARD_DIR ?= config/boards/tunuino
IS_STM32F429 = yes
# USART2: PD5 TX, PD6 RX. CAN1: PD0 RX, PD1 TX.
# Hardware output and peripheral initialization remain disabled until verified.
DDEFS += -DFIRMWARE_ID=\"tunuino\"
DDEFS += -DSTM32_UART_USE_USART2=TRUE
