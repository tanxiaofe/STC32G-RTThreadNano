#ifndef RT_STC_BOARD_H
#define RT_STC_BOARD_H
#include <rtthread.h>
void board_init(void);
void board_tick_start(void);
rt_uint8_t board_keys(void);
void lcd_text(unsigned int x,unsigned int y,const char *text,unsigned int color);
void lcd_number(unsigned int x,unsigned int y,rt_uint32_t value,unsigned int color);
#endif
