/* SPDX-License-Identifier: Apache-2.0
 * Only the LCD thread calls these routines once the scheduler is running. */
#include "board.h"
#include "TFT480x320.h"
#include "ASCII5x7.h"
sbit TXT_RS=P4^5;
sbit TXT_WR=P4^2;
sbit TXT_CS=P4^3;
/* ASCII 字模放大两倍，含间隔占 12x16 像素；RGB565 通过 16 位并口送屏。 */
static void glyph(unsigned int x,unsigned int y,unsigned char ch,unsigned int color,unsigned char narrow_font)
{
    unsigned char row,repeat,col,bits;
    unsigned int c;
    if(ch<32||ch>126)ch='?';
    LCD_SetWindows(x,y,x+(narrow_font?5:11),y+15);
    TXT_RS=1; TXT_CS=0;
    for(row=0;row<8;row++)for(repeat=0;repeat<2;repeat++)
    {
        bits=ASCII5x7[(unsigned int)ch*8+row];
        for(col=0;col<6;col++)
        {
            c=(col<5&&(bits&(0x10>>col)))?color:0x0841;
            P6=(unsigned char)(c>>8); P2=(unsigned char)c;
            NOP(1); TXT_WR=0; NOP(1); TXT_WR=1; NOP(1);
            if(!narrow_font){NOP(1); TXT_WR=0; NOP(1); TXT_WR=1; NOP(1);}
        }
    }
    TXT_CS=1;
}
void lcd_text(unsigned int x,unsigned int y,const char *text,unsigned int color)
{
    if(y>Y_DOTS-16)return;
    while(*text&&x<=X_DOTS-12){glyph(x,y,(unsigned char)*text++,color,0);x+=12;}
}
/* 控制台使用 6x16 字符，每行可显示 76 列；普通标题保持 12x16。 */
void lcd_small_text(unsigned int x,unsigned int y,const char *text,unsigned int color)
{
    if(y>Y_DOTS-16)return;
    while(*text&&x<=X_DOTS-6){glyph(x,y,(unsigned char)*text++,color,1);x+=6;}
}
/* 固定显示十位数字覆盖旧字符，避免数字位数减少时残留。 */
void lcd_number(unsigned int x,unsigned int y,rt_uint32_t value,unsigned int color)
{
    char buffer[11];
    unsigned char i;
    buffer[10]=0;
    for(i=10;i>0;i--){buffer[i-1]=(char)('0'+value%10);value/=10;}
    lcd_text(x,y,buffer,color);
}
