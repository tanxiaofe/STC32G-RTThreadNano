/* Actual console C code with a fake USB controller. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <rthw.h>
#include "usb_mock.h"
#include "../bsp/usb_console.h"
#include "../app/gui_console.h"
volatile BYTE UsbResetGeneration,DeviceState,UsbControlLines;
volatile BOOL UsbInBusy,UsbOutBusy;
volatile BYTE RxRptr,RxWptr,TxRptr,TxWptr;
BYTE RxBuffer[256],TxBuffer[256];
static struct rt_thread current;
static rt_tick_t tick_now;
static rt_base_t ea=1;
static int pending_bytes,packet_count,packets[2048],release_out,host_reads;
static char wire[8192];static unsigned int wire_size;
rt_base_t rt_hw_interrupt_disable(void){rt_base_t v=ea;ea=0;return v;}
void rt_hw_interrupt_enable(rt_base_t v){ea=v;}
rt_tick_t rt_tick_get(void){return tick_now;}
rt_uint8_t rt_interrupt_get_nest(void){return 0;}
rt_thread_t rt_thread_self(void){return &current;}
rt_err_t rt_thread_delay(rt_tick_t n){tick_now+=n;if(host_reads)UsbInBusy=0;return RT_EOK;}
rt_err_t rt_mutex_init(rt_mutex_t m,const char *n,rt_uint8_t f){(void)m;(void)n;(void)f;return RT_EOK;}
rt_err_t rt_mutex_take(rt_mutex_t m,rt_int32_t n){(void)m;(void)n;return RT_EOK;}
rt_err_t rt_mutex_release(rt_mutex_t m){(void)m;return RT_EOK;}
int rt_kprintf(const char *fmt,...){(void)fmt;return 0;}
void usb_init(void){DeviceState=DEVSTATE_DEFAULT;RxRptr=RxWptr=TxRptr=TxWptr=0;UsbInBusy=0;}
void usb_write_reg(BYTE addr,BYTE value)
{
 assert(!ea);
 if(addr==FIFO1){assert(wire_size<sizeof(wire));wire[wire_size++]=(char)value;pending_bytes++;}
 else if(addr==INCSR1&&value==INIPRDY){assert(packet_count<2048);packets[packet_count++]=pending_bytes;pending_bytes=0;}
 else if(addr==OUTCSR1&&value==0)release_out++;
}
static void empty_tx(void)
{
 int guard=1000;host_reads=1;
 while((TxRptr!=TxWptr||UsbInBusy)&&guard--){UsbInBusy=0;rt_hw_console_getchar();}
 assert(guard>0);host_reads=0;
}
int main(void)
{
 int i,packets_before;rt_tick_t start;char lcd_line[GUI_CONSOLE_COLS+1];
 gui_console_init();usb_console_init();rt_hw_console_output("no cable");assert(TxRptr==TxWptr);
 gui_console_line(0,lcd_line);assert(!strncmp(lcd_line,"no cable",8));
 DeviceState=DEVSTATE_CONFIGURED;UsbControlLines=1;
 assert(rt_hw_console_getchar()==3);
 TxRptr=TxWptr=0;UsbInBusy=0;for(i=0;i<64;i++)TxBuffer[TxWptr++]=(BYTE)i;
 packets_before=packet_count;rt_hw_console_getchar();assert(packets[packets_before]==64);
 UsbInBusy=0;rt_hw_console_getchar();assert(packets[packets_before+1]==0);
 UsbInBusy=0;
 RxRptr=250;RxWptr=253;RxBuffer[250]='x';RxBuffer[251]='y';RxBuffer[252]='z';UsbOutBusy=1;
 assert(rt_hw_console_getchar()=='x');assert(rt_hw_console_getchar()=='y');assert(rt_hw_console_getchar()=='z');
 assert(!UsbOutBusy&&release_out>0);
 RxRptr=254;RxWptr=1;RxBuffer[254]='\r';RxBuffer[255]='\n';RxBuffer[0]='a';
 assert(rt_hw_console_getchar()=='\r');assert(rt_hw_console_getchar()==-1);assert(rt_hw_console_getchar()=='a');
 TxRptr=0;TxWptr=255;UsbInBusy=1;start=tick_now;
 rt_hw_console_output("A");assert(TxWptr==255&&tick_now-start==200);
 gui_console_line(0,lcd_line);assert(!strncmp(lcd_line,"no cableA",9));
 host_reads=1;rt_hw_console_output("B");assert(tick_now-start<205);empty_tx();
 wire_size=0;TxRptr=TxWptr=0;UsbInBusy=0;rt_hw_console_output("hello\n");empty_tx();
 assert(wire_size==7&&wire[5]=='\r'&&wire[6]=='\n');
 UsbResetGeneration++;RxRptr=RxWptr=TxRptr=TxWptr=0;UsbInBusy=0;DeviceState=DEVSTATE_DEFAULT;UsbControlLines=0;
 assert(rt_hw_console_getchar()==-1);
 DeviceState=DEVSTATE_CONFIGURED;UsbControlLines=1;assert(rt_hw_console_getchar()==3);
 assert(ea==1);
 puts("PASS: CDC reconnect, CRLF, ring wrap, OUT flow control, full64/ZLP, TX backpressure/timeout, IRQ atomicity");
 return 0;
}
