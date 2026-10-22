/* Host-only ChibiOS model. Never included in hardware builds. */
#ifndef TUNUINO_TEST_CH_H
#define TUNUINO_TEST_CH_H
#include <stdint.h>
#include <stdbool.h>
typedef uint32_t eventflags_t;
typedef struct { unsigned unused; } event_listener_t;
#define EVENT_MASK(n) (1u << (n))
#define TIME_MS2I(ms) (ms)
#define MSG_OK 0
void chSysInit(void);
void chSysLock(void);
void chSysUnlock(void);
void chThdSleepMilliseconds(uint32_t ms);
void chEvtRegisterMaskWithFlags(void *source, event_listener_t *listener, unsigned mask, eventflags_t flags);
eventflags_t chEvtGetAndClearFlags(event_listener_t *listener);
#endif
