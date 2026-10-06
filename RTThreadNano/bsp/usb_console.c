/* SPDX-License-Identifier: Apache-2.0
 * Uses STC MCU Limited's native USB CDC demo, with RT-Thread adaptation.
 * USB registers are shared with USB ISR: short EA critical sections guard access.
 */
#include <rtthread.h>
#include <rthw.h>
#ifdef RT_HOST_TEST
#include "../tests/usb_mock.h"
#else
#include "usb/stc.h"
#include "usb/usb.h"
#include "usb/usb_req_class.h"
#endif
#include "usb_console.h"
#include "../app/gui_console.h"
static struct rt_mutex RT_DATA_STORAGE console_mutex;
static BYTE zlp_needed,seen_reset,connected,previous_cr,last_dtr;
/* 推进 USB 端点传输；关闭 EA 的短临界区保护共享 FIFO。接收空间恢复后释放 OUT 端点。 */
static void pump(void)
{
    BYTE count;
    rt_base_t level=rt_hw_interrupt_disable();
    if(seen_reset!=UsbResetGeneration)
    {seen_reset=UsbResetGeneration;zlp_needed=0;connected=0;previous_cr=0;}
    if(DeviceState==DEVSTATE_CONFIGURED)
    {
        if(UsbOutBusy && (BYTE)(RxWptr-RxRptr)<128)
        {UsbOutBusy=0;usb_write_reg(INDEX,1);usb_write_reg(OUTCSR1,0);}
        if(!UsbInBusy && TxRptr!=TxWptr)
        {
            UsbInBusy=1;usb_write_reg(INDEX,1);count=0;
            while(TxRptr!=TxWptr && count<EP1IN_SIZE)
            {usb_write_reg(FIFO1,TxBuffer[TxRptr++]);count++;}
/* 满 64 字节包后补零长度包，通知主机本次传输结束。 */
            zlp_needed=(count==EP1IN_SIZE);
            usb_write_reg(INCSR1,INIPRDY);
        }
        else if(!UsbInBusy && zlp_needed)
        {zlp_needed=0;UsbInBusy=1;usb_write_reg(INDEX,1);usb_write_reg(INCSR1,INIPRDY);}
    }
    rt_hw_interrupt_enable(level);
}
void usb_console_init(void)
{
    rt_mutex_init(&console_mutex,"usbcout",RT_IPC_FLAG_PRIO);
    usb_init();seen_reset=UsbResetGeneration;
}
/* 互斥锁保证多个线程的输出不交错；中断上下文不允许等待互斥锁。 */
void rt_console_lock(void)
{
    if(rt_thread_self() && !rt_interrupt_get_nest())
        rt_mutex_take(&console_mutex,RT_WAITING_FOREVER);
}
void rt_console_unlock(void)
{
    if(rt_thread_self() && !rt_interrupt_get_nest())rt_mutex_release(&console_mutex);
}
/* 队列满时延时让出 CPU，最多等待 200 tick；有符号差值兼容节拍回绕。 */
static int put_byte(BYTE c)
{
    rt_tick_t deadline=rt_tick_get()+200;
    rt_base_t level;
    for(;;)
    {
        pump();
        if(DeviceState!=DEVSTATE_CONFIGURED)return 0;
        level=rt_hw_interrupt_disable();
        if(DeviceState==DEVSTATE_CONFIGURED && (BYTE)(TxWptr-TxRptr)<255)
        {TxBuffer[TxWptr++]=c;rt_hw_interrupt_enable(level);return 1;}
        rt_hw_interrupt_enable(level);
        if((rt_int32_t)(rt_tick_get()-deadline)>=0)return 0;
        rt_thread_delay(1);
    }
}
void rt_hw_console_output(const char *str)
{
    /* No blocking or FIFO access in interrupt context. */
    if(!rt_thread_self() || rt_interrupt_get_nest())return;
    /* 捕获完整输出，即使 USB 未连接或发送超时，LCD 也保留 MSH 内容。 */
    gui_console_feed(str);
    while(*str)
    {
        if(*str=='\n'&&!put_byte('\r'))break;
        if(!put_byte((BYTE)*str++))break;
    }
    pump();
}
/* DTR 或首个数据触发会话；Ctrl-C 清除旧输入，CRLF 合并为一次命令提交。 */
char rt_hw_console_getchar(void)
{
    BYTE c;
    rt_base_t level;
    pump();
    if(DeviceState!=DEVSTATE_CONFIGURED)
    {connected=0;rt_thread_delay(1);return (char)-1;}
    if(last_dtr && !(UsbControlLines&1))connected=0;
    last_dtr=UsbControlLines&1;
    if(!connected && ((UsbControlLines&1) || RxRptr!=RxWptr))
    {
        connected=1;
        rt_kprintf("\nSTC32G USB CDC / RT-Thread Nano\nType help for commands.\n");
        return 3; /* Ctrl-C discards stale input and refreshes the MSH prompt. */
    }
    level=rt_hw_interrupt_disable();
    if(RxRptr==RxWptr){rt_hw_interrupt_enable(level);rt_thread_delay(1);return (char)-1;}
    c=RxBuffer[RxRptr++];rt_hw_interrupt_enable(level);
    /* Collapse CRLF so Windows terminals produce exactly one command. */
    if(c=='\n'&&previous_cr){previous_cr=0;return (char)-1;}
    previous_cr=(c=='\r');
    return (char)c;
}
