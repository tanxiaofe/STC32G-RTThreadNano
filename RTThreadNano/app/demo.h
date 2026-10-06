#ifndef STC_RT_DEMO_H
#define STC_RT_DEMO_H
#include <rtthread.h>
void demo_stats(rt_uint32_t *beat,rt_uint32_t *key,rt_uint32_t *frame,rt_uint16_t *milliseconds);
void demo_beat_period(rt_uint16_t milliseconds);
void demo_reset(void);
#endif
