#ifndef STC_RT_DEMO_H
#define STC_RT_DEMO_H
#include <rtthread.h>
void demo_stats(rt_uint32_t *beat,rt_uint32_t *key,rt_uint32_t *frame,rt_uint16_t *milliseconds);
void demo_beat_period(rt_uint16_t milliseconds);
void demo_reset(void);
void demo_component_stats(rt_uint32_t *values);
rt_err_t demo_dynamic_start(void);
void demo_dynamic_stop(void);
rt_err_t demo_soft_timer(rt_uint16_t milliseconds);
#endif
