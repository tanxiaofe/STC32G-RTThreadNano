/* SPDX-License-Identifier: Apache-2.0 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <rthw.h>
#include "../app/gui.h"
#include "../app/gui_console.h"
static rt_base_t irq_enabled=1;
static int fills,texts,small_texts,numbers;
static char snapshot[GUI_CONSOLE_COLS+1];
rt_base_t rt_hw_interrupt_disable(void){rt_base_t previous=irq_enabled;irq_enabled=0;return previous;}
void rt_hw_interrupt_enable(rt_base_t level){irq_enabled=level;}
void *rt_memcpy(void *out,const void *in,rt_ubase_t size){return memcpy(out,in,size);}
void *rt_memset(void *out,int value,rt_ubase_t size){return memset(out,value,size);}
rt_int32_t rt_memcmp(const void *a,const void *b,rt_size_t size){return memcmp(a,b,size);}
rt_tick_t rt_tick_get(void){return 123;}
void rt_memory_info(rt_size_t *total,rt_size_t *used,rt_size_t *maximum){*total=2000;*used=100;*maximum=200;}
void demo_component_stats(rt_uint32_t *v){int i;for(i=0;i<8;i++)v[i]=i+1;}
void demo_stats(rt_uint32_t *b,rt_uint32_t *k,rt_uint32_t *f,rt_uint16_t *ms){*b=12;*k=3;*f=10;*ms=500;}
void LCD_Fill_XY(unsigned int x,unsigned int y,unsigned int w,unsigned int h,unsigned int color)
{(void)color;assert(w&&h&&x+w<=480&&y+h<=320);fills++;}
static void bounds(unsigned int x,unsigned int y,const char *text,unsigned int width)
{assert(irq_enabled&&x+strlen(text)*width<=480&&y+16<=320);}
void lcd_text(unsigned int x,unsigned int y,const char *text,unsigned int color)
{(void)color;bounds(x,y,text,12);texts++;}
void lcd_small_text(unsigned int x,unsigned int y,const char *text,unsigned int color)
{(void)color;bounds(x,y,text,6);small_texts++;}
void lcd_number(unsigned int x,unsigned int y,rt_uint32_t value,unsigned int color)
{(void)value;(void)color;assert(irq_enabled&&x+120<=480&&y+16<=320);numbers++;}
int main(void)
{
    int i,before;char message[100];
    gui_console_init();gui_console_feed("msh > echo hello\r\nhello\nmsh > ");
    gui_console_line(0,snapshot);assert(!strncmp(snapshot,"msh > echo hello",16));
    gui_console_line(1,snapshot);assert(!strncmp(snapshot,"hello",5));
    gui_console_line(2,snapshot);assert(!strncmp(snapshot,"msh > ",6));
    gui_console_feed("a\bB");gui_console_line(2,snapshot);assert(snapshot[6]=='B');
    gui_console_feed("\033[2J\033[Habc\033[2DXY\033[K");
    gui_console_line(0,snapshot);assert(!strncmp(snapshot,"aXY ",4));
    gui_console_feed("\033[H\tT");gui_console_line(0,snapshot);assert(snapshot[4]=='T');
    gui_console_init();
    memset(message,'x',GUI_CONSOLE_COLS);message[GUI_CONSOLE_COLS]=0;
    gui_console_feed(message);gui_console_feed("\nnext");
    gui_console_line(1,snapshot);assert(!strncmp(snapshot,"next",4));
    gui_console_init();
    for(i=0;i<20;i++){sprintf(message,"line%02d\n",i);gui_console_feed(message);}
    gui_console_line(0,snapshot);assert(!strncmp(snapshot,"line07",6));
    gui_console_line(12,snapshot);assert(!strncmp(snapshot,"line19",6));
    gui_console_line(13,snapshot);assert(snapshot[0]==' ');
    gui_set_page(GUI_STATUS);gui_render();assert(numbers==5);
    before=numbers;gui_render();assert(numbers==before); /* unchanged widgets */
    gui_keys(8);assert(gui_page()==GUI_IPC);gui_render();assert(numbers==before+6);
    gui_keys(32);assert(gui_page()==GUI_MSH);gui_render();
    before=small_texts;gui_render();assert(small_texts==before); /* no full redraw */
    gui_console_feed("NEW");gui_render();assert(small_texts==before+1);
    gui_keys(4);assert(gui_page()==GUI_IPC);
    gui_keys(4);assert(gui_page()==GUI_STATUS);
    gui_keys(4);assert(gui_page()==GUI_MSH);
    gui_keys(8);assert(gui_page()==GUI_STATUS);
    gui_set_page(9);assert(gui_page()==GUI_STATUS);
    assert(fills==6&&texts>0&&irq_enabled);
    puts("PASS: MSH ring scrolling, CRLF/backspace/ANSI/wrap/tab, landscape widget bounds, page keys and changed-line-only redraw");return 0;
}
