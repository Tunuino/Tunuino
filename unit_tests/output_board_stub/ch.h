/* Copyright (c) 2026 Tunuino. Host-only interrupt lock model. */
#ifndef TUNUINO_OUTPUT_TEST_CH_H
#define TUNUINO_OUTPUT_TEST_CH_H
#include <stdbool.h>
#include <stdint.h>
typedef int msg_t;
#define MSG_OK 0
static inline void chSysLock(void){}
static inline void chSysUnlock(void){}
static inline void chSysLockFromISR(void){}
static inline void chSysUnlockFromISR(void){}
#endif
