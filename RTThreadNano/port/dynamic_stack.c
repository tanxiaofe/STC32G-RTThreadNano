/* SPDX-License-Identifier: Apache-2.0 */
#include <rthw.h>
#include "cpuport.h"
/* 硬件栈必须在 EDATA；一个可回收动态栈槽，控制块从 XDATA 堆申请。 */
static rt_uint8_t RT_STACK_STORAGE dynamic_stack_pool[RT_PORT_DYNAMIC_STACK_SIZE];
static rt_uint8_t busy;
static rt_size_t allocated_size;
void *rt_thread_stack_alloc(rt_size_t size)
{
    rt_base_t level;void *result=RT_NULL;
    if(size<128||size>sizeof(dynamic_stack_pool))return RT_NULL;
    level=rt_hw_interrupt_disable();
    if(!busy){busy=1;allocated_size=size;result=dynamic_stack_pool;}
    rt_hw_interrupt_enable(level);return result;
}
void rt_thread_stack_free(void *stack)
{
    rt_base_t level=rt_hw_interrupt_disable();
    /* 错误地址或重复释放不能交出正在使用的栈槽。 */
    if(stack==dynamic_stack_pool&&busy){busy=0;allocated_size=0;}
    rt_hw_interrupt_enable(level);
}
unsigned char rt_port_dynamic_stack_busy(void)
{
    rt_base_t level=rt_hw_interrupt_disable();unsigned char value=busy;
    rt_hw_interrupt_enable(level);return value;
}
unsigned char rt_port_dynamic_stack_guard(void)
{
    rt_base_t level=rt_hw_interrupt_disable();
    unsigned char valid=!busy||dynamic_stack_pool[allocated_size-1]=='#';
    rt_hw_interrupt_enable(level);return valid;
}
