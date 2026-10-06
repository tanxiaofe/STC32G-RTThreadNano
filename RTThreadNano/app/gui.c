/* SPDX-License-Identifier: Apache-2.0 */
#include "gui.h"
#include "gui_console.h"
#include "demo.h"
#include "board.h"
#ifndef RT_HOST_TEST
#include "TFT480x320.h"
#else
void LCD_Fill_XY(unsigned int x,unsigned int y,unsigned int width,unsigned int height,unsigned int color);
#endif
#define BG 0x0841
#define TEXT 0xffff
#define ACCENT 0x07ff
/* 数值快照与文本缓存放 XDATA；只有 LCD 线程可以直接绘图。 */
static volatile unsigned char page;
static unsigned char drawn_page=255;
static char RT_DATA_STORAGE rendered[GUI_CONSOLE_ROWS][GUI_CONSOLE_COLS+1];
static char RT_DATA_STORAGE line[GUI_CONSOLE_COLS+1];
static rt_uint32_t RT_DATA_STORAGE shown[12],ipc[8];
static rt_uint32_t RT_DATA_STORAGE beats,keys,frames;
static rt_uint16_t RT_DATA_STORAGE milliseconds;
static rt_size_t RT_DATA_STORAGE heap_total,heap_used,heap_max;
void gui_set_page(unsigned char value){if(value<=GUI_MSH)page=value;}
unsigned char gui_page(void){return page;}
/* 与 3Dcube 一致的按键位：P7.2 左、P7.1 右、P3.2 下一页。 */
void gui_keys(unsigned char pressed)
{
    if(pressed&4)page=page? page-1:GUI_MSH;
    else if(pressed&(8|32))page=(page+1)%3;
}
static void number(unsigned char slot,unsigned int x,unsigned int y,rt_uint32_t value,unsigned int color)
{if(shown[slot]!=value){shown[slot]=value;lcd_number(x,y,value,color);}}
static void page_frame(unsigned char value)
{
    unsigned char i;
    LCD_Fill_XY(0,0,480,320,BG);
    lcd_text(12,10,"RT-THREAD NANO / STC32G",TEXT);
    lcd_text(24,40,"STATUS",value==GUI_STATUS?ACCENT:0x7bef);
    lcd_text(192,40,"IPC",value==GUI_IPC?ACCENT:0x7bef);
    lcd_text(360,40,"MSH",value==GUI_MSH?ACCENT:0x7bef);
    LCD_Fill_XY(12+value*156,62,144,2,ACCENT);
    lcd_small_text(12,302,"L/R: PAGE  P3.2: NEXT  U/D: BEAT  CENTER: RESET",0x7bef);
    for(i=0;i<12;i++)shown[i]=0xffffffffUL;
    if(value==GUI_STATUS)
    {
        lcd_small_text(16,82,"100Hz PREEMPTIVE | USB CDC | DYNAMIC THREAD + IPC",ACCENT);
        lcd_text(16,120,"TICK",TEXT);lcd_text(260,120,"KEYS",TEXT);
        lcd_text(16,180,"HEARTBEAT",TEXT);lcd_text(260,180,"PERIOD MS",TEXT);
        lcd_text(16,240,"LCD UPDATES",TEXT);lcd_text(260,240,"DYNAMIC",TEXT);
    }
    else if(value==GUI_IPC)
    {
        lcd_small_text(16,76,"EVENT OR|CLEAR + COUNTS MUTEX + 4-MESSAGE QUEUE",ACCENT);
        lcd_text(16,102,"SOFT TIMER HITS",TEXT);lcd_text(16,134,"QUEUE SENT",TEXT);
        lcd_text(16,166,"QUEUE RECEIVED",TEXT);lcd_text(16,198,"QUEUE DROPPED",TEXT);
        lcd_text(16,230,"LAST SAMPLE",TEXT);lcd_text(16,262,"HEAP USED",TEXT);
    }
    else rt_memset(rendered,0,sizeof(rendered));
}
/* 切页重画框架；数值和控制台只更新变化的区域。 */
void gui_render(void)
{
    unsigned char value=page,i;
    if(drawn_page!=value){page_frame(value);drawn_page=value;}
    if(value==GUI_MSH)
    {
        for(i=0;i<GUI_CONSOLE_ROWS;i++)
        {
            gui_console_line(i,line);
            if(rt_memcmp(rendered[i],line,sizeof(line)))
            {lcd_small_text(12,74+i*16,line,TEXT);rt_memcpy(rendered[i],line,sizeof(line));}
        }
        return;
    }
    demo_component_stats(ipc);
    if(value==GUI_STATUS)
    {
        demo_stats(&beats,&keys,&frames,&milliseconds);
        number(0,16,144,rt_tick_get(),0xffe0);number(1,260,144,keys,TEXT);
        number(2,16,204,beats,0x07e0);number(3,260,204,milliseconds,TEXT);
        number(4,16,264,frames,TEXT);
        if(shown[5]!=ipc[7]){shown[5]=ipc[7];lcd_text(260,264,ipc[7]?"RUNNING ":"STOPPED ",ipc[7]?0x07e0:0xffe0);}
    }
    else
    {
        rt_memory_info(&heap_total,&heap_used,&heap_max);
        number(0,300,102,ipc[0],0xffe0);number(1,300,134,ipc[1],0x07e0);
        number(2,300,166,ipc[2],0x07e0);number(3,300,198,ipc[3],0xf800);
        number(4,300,230,ipc[4],TEXT);number(5,300,262,heap_used,TEXT);
    }
}
