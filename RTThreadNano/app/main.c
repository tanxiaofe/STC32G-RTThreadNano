/* SPDX-License-Identifier: Apache-2.0 */
#include <rthw.h>
#include "cpuport.h"
#include "board.h"
#include "TFT480x320.h"
#include "shell.h"
#include "usb_console.h"
/* 控制块使用 XDATA，硬件栈使用 EDATA；所有线程静态分配，不依赖动态堆。 */
static struct rt_thread RT_DATA_STORAGE heartbeat_thread,key_thread,lcd_thread;
static unsigned char edata heartbeat_stack[512];
static unsigned char edata key_stack[512];
static unsigned char edata lcd_stack[768];
static struct rt_semaphore ui_event;
static volatile rt_uint32_t beats,key_presses,frames;
static volatile rt_uint8_t last_key;
static volatile rt_uint16_t period=50;

/* 心跳通过信号量通知显示线程；period 单位为 10 ms 系统节拍。 */
static void heartbeat(void *parameter)
{
    RT_UNUSED(parameter);
    for(;;){beats++;rt_sem_release(&ui_event);rt_thread_delay(period);}
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
                level=rt_hw_interrupt_disable();
                key_presses++;last_key=pressed;
                if((pressed&1)&&period>10)period-=10;
                if((pressed&2)&&period<100)period+=10;
                if(pressed&16){beats=0;key_presses=0;}
                rt_hw_interrupt_enable(level);
                rt_sem_release(&ui_event);
            }
        }
        rt_thread_delay(1);
    }
}
/* 只有显示线程操作 LCD，避免线程交错写屏幕窗口；刷新上限为每秒十次。 */
static void display(void *parameter)
{
    rt_base_t level;
    rt_uint32_t beat_snapshot,key_snapshot;
    rt_uint8_t key_snapshot8;
    RT_UNUSED(parameter);
    LCD_Fill_XY(0,0,320,480,0x0841);
    lcd_text(16,20,"RT-THREAD NANO 4.1.1",0x07ff);
    lcd_text(16,48,"STC32G12K128 / C251",0xffff);
    lcd_text(16,90,"PREEMPTIVE / 100HZ",0xffe0);
    lcd_text(16,130,"TICK",0xffff);
    lcd_text(16,180,"HEARTBEAT",0x07e0);
    lcd_text(16,230,"KEY EVENTS",0xffff);
    lcd_text(16,280,"DISPLAY UPDATES",0xffff);
    lcd_text(16,340,"UP/DOWN: BEAT SPEED",0x07ff);
    lcd_text(16,368,"CENTER: RESET COUNTS",0x07ff);
    lcd_text(16,414,"STATIC THREADS + IPC",0x7bef);
    lcd_text(16,442,"USB CDC / FINSH MSH",0x7bef);
    for(;;)
    {
        rt_sem_take(&ui_event,10);
/* 检查向上增长的栈尾填充值，发现部分溢出时停机。 */
        if(heartbeat_stack[sizeof(heartbeat_stack)-1]!='#' ||
           key_stack[sizeof(key_stack)-1]!='#' || lcd_stack[sizeof(lcd_stack)-1]!='#')
            rt_port_panic(6);
        level=rt_hw_interrupt_disable();
        beat_snapshot=beats;key_snapshot=key_presses;key_snapshot8=last_key;
        rt_hw_interrupt_enable(level);
        lcd_number(16,152,rt_tick_get(),0xffe0);
        lcd_number(16,202,beat_snapshot,0x07e0);
        lcd_number(16,252,key_snapshot,0xffff);
        lcd_number(160,324,key_snapshot8,0x07ff);
        lcd_number(16,302,++frames,0xffff);
        /* At most 10 redraws/s, leaving time for idle and lower priority tasks. */
        rt_thread_delay(10);
    }
}
static void require(rt_err_t result)
{
    if(result!=RT_EOK)rt_port_panic(4);
}
/* 先初始化内核和线程，再启动 USB 与节拍，最后启动调度器；此前保持中断关闭。 */
void main(void)
{
    board_init();
    rt_port_object_init();
    rt_system_timer_init();
    rt_system_scheduler_init();
    require(rt_sem_init(&ui_event,"ui",0,RT_IPC_FLAG_PRIO));
/* 优先级数字越小越高：心跳 1、按键 2、LCD 4；各线程时间片为 5 tick。 */
    require(rt_thread_init(&heartbeat_thread,"beat",heartbeat,RT_NULL,
                          heartbeat_stack,sizeof(heartbeat_stack),1,5));
    require(rt_thread_init(&key_thread,"key",keys,RT_NULL,
                          key_stack,sizeof(key_stack),2,5));
    require(rt_thread_init(&lcd_thread,"lcd",display,RT_NULL,
                          lcd_stack,sizeof(lcd_stack),4,5));
    require(rt_thread_startup(&heartbeat_thread));
    require(rt_thread_startup(&key_thread));
    require(rt_thread_startup(&lcd_thread));
    rt_thread_idle_init();
    require(finsh_system_init());
    usb_console_init();
    board_tick_start();
    rt_system_scheduler_start();
    rt_port_panic(5);
}

/* 临界区内一次读取统计快照，防止多字节计数被中断更新。 */
void demo_stats(rt_uint32_t *beat,rt_uint32_t *key,rt_uint32_t *frame,rt_uint16_t *milliseconds)
{
    rt_base_t level=rt_hw_interrupt_disable();
    *beat=beats;*key=key_presses;*frame=frames;*milliseconds=period*10;
    rt_hw_interrupt_enable(level);
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
    rt_base_t level=rt_hw_interrupt_disable();
    beats=key_presses=frames=0;
    rt_hw_interrupt_enable(level);
}
