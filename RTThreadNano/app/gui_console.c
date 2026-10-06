/* SPDX-License-Identifier: Apache-2.0 */
#include <rthw.h>
#include "gui_console.h"
/* 保留最近 14 行 MSH 输出；环形滚动不复制整个屏幕缓冲。 */
static char RT_DATA_STORAGE lines[GUI_CONSOLE_ROWS][GUI_CONSOLE_COLS];
static unsigned char top,row,column,escape;
static unsigned int parameter;
static void clear_line(unsigned char physical)
{unsigned char i;for(i=0;i<GUI_CONSOLE_COLS;i++)lines[physical][i]=' ';}
static unsigned char physical_row(void){return (top+row)%GUI_CONSOLE_ROWS;}
static void clear_all(void)
{unsigned char i;for(i=0;i<GUI_CONSOLE_ROWS;i++)clear_line(i);top=row=column=0;}
void gui_console_init(void)
{rt_base_t level=rt_hw_interrupt_disable();clear_all();escape=0;parameter=0;rt_hw_interrupt_enable(level);}
static void newline(void)
{
    column=0;
    if(row+1<GUI_CONSOLE_ROWS)row++;
    else top=(top+1)%GUI_CONSOLE_ROWS;
    clear_line(physical_row());
}
static void character(unsigned char c)
{
    unsigned char i;
    if(escape==1)
    {escape=c=='['?2:0;parameter=0;return;}
    if(escape==2)
    {
        if(c>='0'&&c<='9')
        {if(parameter<1000)parameter=parameter*10+c-'0';return;}
        if(c==';')return;
        if(c=='J'&&parameter==2)clear_all();
        else if(c=='H'||c=='f')row=column=0;
        else if(c=='K')for(i=column;i<GUI_CONSOLE_COLS;i++)lines[physical_row()][i]=' ';
        else if(c=='D')column=parameter>=column?0:column-(parameter?parameter:1);
        else if(c=='C')
        {parameter=column+(parameter?parameter:1);column=parameter>=GUI_CONSOLE_COLS?GUI_CONSOLE_COLS-1:parameter;}
        escape=0;return;
    }
    if(c==27){escape=1;return;}
    if(c=='\r'){column=0;return;}
    if(c=='\n'){newline();return;}
    if(c=='\b'){if(column)column--;return;}
    if(c=='\t')
    {do{character(' ');}while(column%4);return;}
    if(c<32||c==127)return;
    /* Delayed wrapping avoids a second blank line when a full row ends with LF. */
    if(column==GUI_CONSOLE_COLS)newline();
    lines[physical_row()][column++]=c<=126?(char)c:'?';
}
/* 每个字符使用短中断临界区；LCD 读取时不会遇到半更新的索引。 */
void gui_console_feed(const char *text)
{
    rt_base_t level;
    while(*text)
    {
        level=rt_hw_interrupt_disable();character((unsigned char)*text++);
        rt_hw_interrupt_enable(level);
    }
}
void gui_console_line(unsigned char logical,char *out)
{
    rt_base_t level;unsigned char i,physical;
    if(logical>=GUI_CONSOLE_ROWS){out[0]=0;return;}
    level=rt_hw_interrupt_disable();physical=(top+logical)%GUI_CONSOLE_ROWS;
    for(i=0;i<GUI_CONSOLE_COLS;i++)out[i]=lines[physical][i];
    out[GUI_CONSOLE_COLS]=0;rt_hw_interrupt_enable(level);
}
