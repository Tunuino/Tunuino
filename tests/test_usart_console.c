// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include "../firmware/platform/stm32f407/usart_console.h"
#include <assert.h>
int main(void) {
    tunuino_usart_config_t cfg = tunuino_usart_make_config(16000000u, 115200u);
    assert(cfg.valid);
    assert(cfg.brr == 139u);
    cfg = tunuino_usart_make_config(42000000u, 115200u);
    assert(cfg.valid && cfg.brr == 365u);
    assert(!tunuino_usart_make_config(0u, 115200u).valid);
    assert(!tunuino_usart_make_config(16000000u, 0u).valid);
    assert(!tunuino_usart_make_config(16000000u, 2000000u).valid);
    return 0;
}
