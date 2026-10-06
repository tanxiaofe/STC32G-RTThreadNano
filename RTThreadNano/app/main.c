/* SPDX-License-Identifier: Apache-2.0 */
#include <rthw.h>
#include "cpuport.h"
#include "board.h"
#include "TFT480x320.h"
#include "shell.h"
#include "usb_console.h"
#include "demo.h"
#include "gui.h"
#include "gui_console.h"
/* 静态控制块放 XDATA；线程硬件栈必须放 EDATA。
 * 心跳线程使用 rt_thread_create，栈由 port/dynamic_stack.c 的专用池分配。
 */
static struct rt_thread RT_DATA_STORAGE key_thread,lcd_thread;
static rt_thread_t heartbeat_thread;
static unsigned char RT_STACK_STORAGE key_stack[384];
static unsigned char RT_STACK_STORAGE lcd_stack[640];
static unsigned char RT_DATA_STORAGE heap_area[2048];
static struct rt_event RT_DATA_STORAGE ui_event;
static struct rt_messagequeue RT_DATA_STORAGE beat_queue;
static struct rt_mutex RT_DATA_STORAGE counter_lock;
static struct rt_timer RT_DATA_STORAGE soft_timer;
/* 每条消息是 4 字节心跳计数，外加队列链接指针，容量 4。 */
static rt_uint32_t RT_DATA_STORAGE queue_pool[8];
#define EVENT_BEAT 1UL
#define EVENT_KEY 2UL
#define EVENT_TIMER 4UL
static volatile rt_uint8_t stop_requested,soft_enabled=1;
static volatile rt_uint32_t soft_hits,mq_sent,mq_received,mq_dropped,last_message;
static volatile rt_uint32_t beats,key_presses,frames;
static volatile rt_uint8_t last_key;
static volatile rt_uint16_t period=50;

/* 动态线程的入口必须为 void entry(void *parameter)。
 * demo_dynamic_start 使用 rt_thread_create 创建；通过队列传送样本、事件通知 LCD。
 * period 单位为 10 ms；delay 阻塞当前线程，其他任务可以继续运行。
 */
static void heartbeat_thread_entry(void *parameter)
{
    RT_UNUSED(parameter);
    for(;;){rt_uint32_t sample;rt_uint16_t wait_ticks;
        /* stop 在没有持有互斥量时退出，由 idle 回收动态控制块和 EDATA 栈。 */
        if(stop_requested)return;
        rt_mutex_take(&counter_lock,RT_WAITING_FOREVER);
        sample=++beats;wait_ticks=period;
        rt_mutex_release(&counter_lock);
        if(rt_mq_send(&beat_queue,&sample,sizeof(sample))==RT_EOK)mq_sent++;
        else mq_dropped++;
        rt_event_send(&ui_event,EVENT_BEAT);
        rt_thread_delay(wait_ticks);
    }
}
/* 每 10 ms 扫描，连续三次稳定后接受；只处理按下边沿，长按不重复计数。 */
static void keys(void *parameter)
{
    rt_base_t level;
    rt_uint8_t raw,previous=0,stable=0,held=0,pressed;
    RT_UNUSED(parameter);
    for(;;)
    {
        raw=board_keys();
        if(raw!=previous){previous=raw;stable=0;}
        else if(stable<3&&++stable==3)
        {
            pressed=raw&~held;held=raw;
            if(pressed)
            {
                rt_mutex_take(&counter_lock,RT_WAITING_FOREVER);
                level=rt_hw_interrupt_disable();
                key_presses++;last_key=pressed;
                gui_keys(pressed);
                if((pressed&1)&&period>10)period-=10;
                if((pressed&2)&&period<100)period+=10;
                if(pressed&16){beats=0;key_presses=0;}
                rt_hw_interrupt_enable(level);
                rt_mutex_release(&counter_lock);
                rt_event_send(&ui_event,EVENT_KEY);
            }
        }
        rt_thread_delay(1);
    }
}
/* 横屏 GUI 由 LCD 线程独占；MSH 页面只重画内容变化的文本行。 */
static void display(void *parameter)
{
    rt_uint32_t events,message;
    RT_UNUSED(parameter);
    for(;;)
    {
        rt_event_recv(&ui_event,EVENT_BEAT|EVENT_KEY|EVENT_TIMER,
                      RT_EVENT_FLAG_OR|RT_EVENT_FLAG_CLEAR,10,&events);
        while(rt_mq_recv(&beat_queue,&message,sizeof(message),0)==RT_EOK)
        {last_message=message;mq_received++;}
        if(!rt_port_dynamic_stack_guard() ||
           key_stack[sizeof(key_stack)-1]!='#' || lcd_stack[sizeof(lcd_stack)-1]!='#')
            rt_port_panic(6);
        rt_mutex_take(&counter_lock,RT_WAITING_FOREVER);
        frames++;
        rt_mutex_release(&counter_lock);
        gui_render();
        rt_thread_delay(10);
    }
}
/* 真正的软件定时器回调：运行在 timer 线程；只更新计数和发送事件。 */
static void soft_timer_callback(void *parameter)
{RT_UNUSED(parameter);soft_hits++;rt_event_send(&ui_event,EVENT_TIMER);}
static void heartbeat_cleanup(struct rt_thread *thread)
{rt_base_t level=rt_hw_interrupt_disable();if(heartbeat_thread==thread)heartbeat_thread=RT_NULL;rt_hw_interrupt_enable(level);}
rt_err_t demo_dynamic_start(void)
{
    rt_thread_t thread;
    if(heartbeat_thread||rt_port_dynamic_stack_busy())return -RT_ERROR;
    stop_requested=0;
    thread=rt_thread_create("beat",heartbeat_thread_entry,RT_NULL,512,1,5);
    if(thread==RT_NULL)return -RT_ENOMEM;
    thread->cleanup=heartbeat_cleanup;heartbeat_thread=thread;
    if(rt_thread_startup(thread)!=RT_EOK){rt_thread_delete(thread);return -RT_ERROR;}
    return RT_EOK;
}
void demo_dynamic_stop(void){stop_requested=1;}
void demo_component_stats(rt_uint32_t *values)
{
    rt_base_t level=rt_hw_interrupt_disable();
    values[0]=soft_hits;values[1]=mq_sent;values[2]=mq_received;values[3]=mq_dropped;
    values[4]=last_message;values[5]=beat_queue.entry;values[6]=soft_enabled;
    values[7]=heartbeat_thread!=RT_NULL;rt_hw_interrupt_enable(level);
}
rt_err_t demo_soft_timer(rt_uint16_t milliseconds)
{
    rt_tick_t ticks;
    if(milliseconds==0){soft_enabled=0;rt_timer_stop(&soft_timer);return RT_EOK;}
    if(milliseconds<10||milliseconds>1000||milliseconds%10)return -RT_EINVAL;
    rt_timer_stop(&soft_timer);ticks=milliseconds/10;
    if(rt_timer_control(&soft_timer,RT_TIMER_CTRL_SET_TIME,&ticks)!=RT_EOK)return -RT_ERROR;
    soft_enabled=1;return rt_timer_start(&soft_timer);
}
static void require(rt_err_t result)
{
    if(result!=RT_EOK)rt_port_panic(4);
}
/* 先初始化内核和线程，再启动 USB 与节拍，最后启动调度器；此前保持中断关闭。 */
void main(void)
{
    board_init();
    gui_console_init();
    rt_port_object_init();
    rt_system_timer_init();
    rt_system_scheduler_init();
    /* XDATA 普通堆只分配控制块和 IPC 数据；硬件栈由独立 EDATA 池提供。 */
    rt_system_heap_init(heap_area,heap_area+sizeof(heap_area));
    rt_system_timer_thread_init();
    require(rt_event_init(&ui_event,"ui",RT_IPC_FLAG_PRIO));
    require(rt_mutex_init(&counter_lock,"counts",RT_IPC_FLAG_PRIO));
    require(rt_mq_init(&beat_queue,"beatq",queue_pool,sizeof(rt_uint32_t),sizeof(queue_pool),RT_IPC_FLAG_PRIO));
    rt_timer_init(&soft_timer,"sample",soft_timer_callback,RT_NULL,20,
                  RT_TIMER_FLAG_PERIODIC|RT_TIMER_FLAG_SOFT_TIMER);
    require(rt_timer_start(&soft_timer));
    /* 动态创建示例：控制块来自 XDATA 堆，512 字节栈来自 EDATA 专用池。
     * 创建可能因资源不足返回 RT_NULL，必须先判断再 startup。
     */
    require(demo_dynamic_start());

    /* 示例 2：创建并启动按键线程，优先级 2，入口 keys，每 tick 扫描一次。 */
    require(rt_thread_init(&key_thread, "key", keys, RT_NULL,
                           key_stack, sizeof(key_stack), 2, 5));
    require(rt_thread_startup(&key_thread));

    /* 示例 3：创建并启动 LCD 线程，优先级 4；只有该线程操作 LCD。 */
    require(rt_thread_init(&lcd_thread, "lcd", display, RT_NULL,
                           lcd_stack, sizeof(lcd_stack), 4, 5));
    require(rt_thread_startup(&lcd_thread));

    rt_thread_idle_init();
    require(finsh_system_init());
    usb_console_init();
    board_tick_start();
    /* 步骤 5：启动调度器；最高优先级的就绪线程先运行。
     * 此后 main 不再轮询；入口函数中的循环分别属于各自线程。
     */
    rt_system_scheduler_start();
    rt_port_panic(5);
}

/* 临界区内一次读取统计快照，防止多字节计数被中断更新。 */
void demo_stats(rt_uint32_t *beat,rt_uint32_t *key,rt_uint32_t *frame,rt_uint16_t *milliseconds)
{
    rt_base_t level;rt_mutex_take(&counter_lock,RT_WAITING_FOREVER);
    level=rt_hw_interrupt_disable();
    *beat=beats;*key=key_presses;*frame=frames;*milliseconds=period*10;
    rt_hw_interrupt_enable(level);rt_mutex_release(&counter_lock);
}
void demo_beat_period(rt_uint16_t milliseconds)
{
    rt_base_t level=rt_hw_interrupt_disable();
    period=milliseconds/10;
    rt_hw_interrupt_enable(level);
}
/* 只清零演示统计，不复位 MCU。 */
void demo_reset(void)
{
    rt_base_t level;rt_mutex_take(&counter_lock,RT_WAITING_FOREVER);
    level=rt_hw_interrupt_disable();
    beats=key_presses=frames=0;
    rt_hw_interrupt_enable(level);rt_mutex_release(&counter_lock);
}
