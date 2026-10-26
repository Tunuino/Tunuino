/* Copyright (c) 2026 Tunuino. Host-only GPIO/GPT register model. */
#ifndef TUNUINO_OUTPUT_TEST_HAL_H
#define TUNUINO_OUTPUT_TEST_HAL_H
#include "ch.h"
typedef struct {uint32_t value,configured;} gpio_model;
typedef gpio_model *ioportid_t;
extern gpio_model output_gpio[5];
#define GPIOB (&output_gpio[0])
#define GPIOC (&output_gpio[1])
#define GPIOD (&output_gpio[2])
#define GPIOE (&output_gpio[3])
#define GPIOG (&output_gpio[4])
typedef struct {uint32_t CNT;} timer_model;
extern timer_model output_clock;
#define TIM2 (&output_clock)
typedef struct {uint32_t AHB1ENR;} rcc_model;
extern rcc_model output_rcc;
#define RCC (&output_rcc)
#define RCC_AHB1ENR_GPIOBEN 1u
#define RCC_AHB1ENR_GPIOCEN 2u
#define RCC_AHB1ENR_GPIODEN 4u
#define RCC_AHB1ENR_GPIOEEN 8u
#define RCC_AHB1ENR_GPIOGEN 16u
#define PAL_MODE_OUTPUT_PUSHPULL 1u
void palSetPad(ioportid_t port,unsigned pad);
void palClearPad(ioportid_t port,unsigned pad);
void palSetPadMode(ioportid_t port,unsigned pad,unsigned mode);
enum {GPT_READY=2,GPT_ONESHOT=4};
typedef struct GPTDriver {unsigned state;} GPTDriver;
typedef struct {unsigned frequency;void (*callback)(GPTDriver *);unsigned cr2,dier;} GPTConfig;
extern GPTDriver GPTD5;
msg_t gptStart(GPTDriver *driver,const GPTConfig *config);
void gptStopTimerI(GPTDriver *driver);
void gptStartOneShotI(GPTDriver *driver,uint32_t delay);
#endif
