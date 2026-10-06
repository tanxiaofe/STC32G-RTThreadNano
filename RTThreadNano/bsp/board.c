/* SPDX-License-Identifier: Apache-2.0 */
#include "board.h"
#include "STC32G.h"
#include "config.h"
#include "TFT480x320.h"
/* 初始化 GPIO 与 LCD；启动阶段关闭 EA，首个线程恢复时才开放中断。 */
void board_init(void)
{
    EA=0; WTST=0; CKCON=0; P_SW2|=0x80;
    P0M1=0xff; P0M0=0; P1M1=0xff; P1M0=0;
    P2M1=0xff; P2M0=0; P3M1=0xff; P3M0=0;
    P32=1; P3M1&=~4;
    P4M1=0xff; P4M0=0; P5M1=0x1f; P5M0=0;
    P6M1=0xff; P6M0=0; P7=0xff; P7M1=P7M0=0;
    LCD_Init();
}
/* 12T 自动重装：35 MHz / 12 / 100 能装入 16 位；1T 下会超出计数范围。 */
void board_tick_start(void)
{
    unsigned int reload;
    /* Timer0 mode0 auto-reload, 12T: 35 MHz/12/100 fits 16 bits.
     * 1T would exceed 65535 at 100 Hz. EA stays off until scheduler starts. */
    AUXR&=~0x80; TMOD&=0xf0;
    reload=(unsigned int)(65536UL-MAIN_Fosc/12UL/RT_TICK_PER_SECOND);
    TH0=(unsigned char)(reload>>8); TL0=(unsigned char)reload;
    TF0=0; ET0=1; TR0=1;
}
/* 按键低电平有效，返回位 0..5 依次为上、下、左、右、中、P3.2。 */
rt_uint8_t board_keys(void)
{
    rt_uint8_t value=0;
    if(!P74)value|=1; if(!P73)value|=2;
    if(!P72)value|=4; if(!P71)value|=8;
    if(!P70)value|=16; if(!P32)value|=32;
    return value;
}
