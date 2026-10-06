#ifndef STC_RT_CPU_PORT_H
#define STC_RT_CPU_PORT_H
#include <rtthread.h>
#define RT_PORT_FRAME_SIZE 46
void rt_port_yield(void);
void rt_port_tick(void);
void rt_port_object_init(void);
void rt_port_panic(unsigned char reason);
extern volatile unsigned char rt_port_fault;
#endif
