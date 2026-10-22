/* Copyright (c) 2026 Tunuino. Proteus v0.7, STM32F427, 8 MHz HSE.
 * Experimental bench target: no engine output or sensor initialization. */
#include "stm32f4xx.h"
#include "board.h"
#include "tusb.h"

uint32_t SystemCoreClock = 16000000u;
volatile uint32_t tunuino_bench_fault;
static volatile uint32_t ticks;

static void fatal(uint32_t reason) {
    tunuino_bench_fault = reason;
    __disable_irq();
    for (;;) __WFI();
}

/* Called by ST startup BEFORE data/bss initialization. No global state used. */
void SystemInit(void) {
    __disable_irq();
    SysTick->CTRL = 0;
    for (unsigned i = 0; i < 8; ++i) {
        NVIC->ICER[i] = UINT32_MAX;
        NVIC->ICPR[i] = UINT32_MAX;
    }
    SCB->VTOR = 0x08000000u;
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY)) {}
    RCC->CFGR &= ~RCC_CFGR_SW;
    while ((RCC->CFGR & RCC_CFGR_SWS) != 0) {}
    RCC->CR &= ~(RCC_CR_PLLON | RCC_CR_HSEON | RCC_CR_CSSON);
    while (RCC->CR & RCC_CR_PLLRDY) {}
    while (RCC->CR & RCC_CR_HSERDY) {}
    RCC->CR &= ~RCC_CR_HSEBYP;
    RCC->CIR = 0;
    __DSB(); __ISB();
}

void board_init(void) {
    /* 8 MHz / 8 * 192 / 4 = 48 MHz SYSCLK and USB. APB1 = 24 MHz. */
    FLASH->ACR = FLASH_ACR_LATENCY_3WS | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    RCC->CR |= RCC_CR_HSEON;
    uint32_t limit = 10000000u;
    while (!(RCC->CR & RCC_CR_HSERDY) && --limit) {}
    if (!limit) fatal(1);
    RCC->PLLCFGR = 8u | (192u << 6) | (1u << 16) | RCC_PLLCFGR_PLLSRC_HSE | (4u << 24);
    RCC->CR |= RCC_CR_PLLON;
    limit = 10000000u;
    while (!(RCC->CR & RCC_CR_PLLRDY) && --limit) {}
    if (!limit) fatal(2);
    RCC->CFGR = RCC_CFGR_PPRE1_DIV2 | RCC_CFGR_SW_PLL;
    limit = 10000000u;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL && --limit) {}
    if (!limit) fatal(3);
    SystemCoreClock = 48000000u;
    /* PA11/PA12, AF10, native USB OTG FS; no other GPIO is configured. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    (void)RCC->AHB1ENR;
    GPIOA->MODER = (GPIOA->MODER & ~((3u << 22) | (3u << 24))) | (2u << 22) | (2u << 24);
    GPIOA->OTYPER &= ~((1u << 11) | (1u << 12));
    GPIOA->OSPEEDR |= (3u << 22) | (3u << 24);
    GPIOA->PUPDR &= ~((3u << 22) | (3u << 24));
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~((15u << 12) | (15u << 16))) | (10u << 12) | (10u << 16);
    RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;
    (void)RCC->AHB2ENR;
    RCC->AHB2RSTR |= RCC_AHB2RSTR_OTGFSRST;
    RCC->AHB2RSTR &= ~RCC_AHB2RSTR_OTGFSRST;
    board_usb_vbus_init();
    NVIC_SetPriority(OTG_FS_IRQn,2);
    if (SysTick_Config(SystemCoreClock / 1000u)) fatal(4);
    __enable_irq();
}

void SysTick_Handler(void) { ++ticks; }
void OTG_FS_IRQHandler(void) { tud_int_handler(0); }
uint32_t tusb_time_millis_api(void) { return ticks; }
uint64_t board_millis(void) {
    /* Main-loop owner only; called regularly, including while USB is idle. */
    static uint32_t previous;
    static uint64_t epoch;
    uint32_t now = ticks;
    if (now < previous) epoch += UINT64_C(1) << 32;
    previous = now;
    return epoch | now;
}
void board_uid(uint8_t uid[12]) {
    const volatile uint8_t *source = (const volatile uint8_t *)0x1FFF7A10u;
    for (unsigned i = 0; i < 12; ++i) uid[i] = source[i];
}
/* No constructors or semihosting are used. ST startup calls newlib init arrays. */
void _init(void) {}
void _fini(void) {}
