/* Host-only ChibiOS model. Never included in hardware builds. */
#ifndef TUNUINO_TEST_CH_H
#define TUNUINO_TEST_CH_H
#include <stdint.h>
#include <stdbool.h>
typedef uint32_t eventflags_t;
typedef struct { unsigned unused; } event_listener_t;
typedef uint32_t systime_t;
typedef uint32_t sysinterval_t;
typedef struct virtual_timer virtual_timer_t;
typedef void (*vtfunc_t)(virtual_timer_t *, void *);
struct virtual_timer { bool armed; systime_t due; vtfunc_t callback; void *argument; };
#define EVENT_MASK(n) (1u << (n))
#define TIME_MS2I(ms) (ms)
#define MSG_OK 0
void chSysInit(void);
void chSysLock(void);
void chSysUnlock(void);
void chSysLockFromISR(void);
void chSysUnlockFromISR(void);
void chVTObjectInit(virtual_timer_t *timer);
void chVTSet(virtual_timer_t *timer, sysinterval_t delay, vtfunc_t callback, void *argument);
void chVTSetI(virtual_timer_t *timer, sysinterval_t delay, vtfunc_t callback, void *argument);
systime_t chVTGetSystemTimeX(void);
void chThdSleepMilliseconds(uint32_t ms);
void chEvtRegisterMaskWithFlags(void *source, event_listener_t *listener, unsigned mask, eventflags_t flags);
eventflags_t chEvtGetAndClearFlags(event_listener_t *listener);
#endif
