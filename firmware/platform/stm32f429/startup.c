// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Tunuino
#include <stdint.h>
extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
int main(void);
void Reset_Handler(void);
void Default_Handler(void);
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void MemManage_Handler(void) __attribute__((weak, alias("Default_Handler")));
void BusFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void UsageFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void DebugMon_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
typedef void (*handler_t)(void);
__attribute__((used, section(".isr_vector")))
const handler_t vector_table[16 + 82] = {
    [0] = (handler_t)&_estack,
    [1] = Reset_Handler,
    [2] = NMI_Handler,
    [3] = HardFault_Handler,
    [4] = MemManage_Handler,
    [5] = BusFault_Handler,
    [6] = UsageFault_Handler,
    [11] = SVC_Handler,
    [12] = DebugMon_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [16 ... 97] = Default_Handler
};
void Reset_Handler(void) {
    uint32_t *dst;
    const uint32_t *src = &_sidata;
    for (dst = &_sdata; dst < &_edata;) *dst++ = *src++;
    for (dst = &_sbss; dst < &_ebss;) *dst++ = 0u;
    // Relocate interrupt vectors for applications started by a bootloader.
    // VTOR is available on Cortex-M4; vector table is aligned by linker.
    (*(volatile uint32_t *)0xE000ED08u) = (uint32_t)(uintptr_t)vector_table;
    __asm volatile ("dsb\n\tisb" ::: "memory");
    (void)main();
    Default_Handler();
}
void Default_Handler(void) {
    for (;;) __asm volatile ("wfi");
}

// Minimal freestanding memory primitive required by protocol initialization.
void *memset(void *destination, int value, __SIZE_TYPE__ count) {
    volatile unsigned char *out = (volatile unsigned char *)destination;
    for (__SIZE_TYPE__ i = 0; i < count; ++i) out[i] = (unsigned char)value;
    return destination;
}
