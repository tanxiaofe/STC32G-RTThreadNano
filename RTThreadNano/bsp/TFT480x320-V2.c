/* Original panel initialization and timings retained; unused font/DMA code removed. */

#include	"TFT480x320.h"

u16	foreground,background;

sbit LCD_RS  = P4^5;
sbit LCD_WR  = P4^2;
sbit LCD_CS  = P4^3;
sbit LCD_RST = P4^1;
sfr	 LCD_BUS_H = 0xE8;	
sfr	 LCD_BUS_L = 0xA0;	

#define LCD_CS_SET()		LCD_CS = 1
#define LCD_CS_CLR()		LCD_CS = 0

#define LCD_RST_SET()		LCD_RST = 1
#define LCD_RST_CLR()		LCD_RST = 0

#define LCD_RS_SET()		LCD_RS = 1
#define LCD_RS_CLR()		LCD_RS = 0

#define LCD_WR_SET()		LCD_WR = 1
#define LCD_WR_CLR()		LCD_WR = 0

#define LCD_RD_SET()		LCD_RD = 1
#define LCD_RD_CLR()		LCD_RD = 0

#define LCD_WriteWord(n) 	LCD_BUS_H = (u8)(n>>8); LCD_BUS_L = (u8)n; NOP(1);	LCD_WR_CLR(); NOP(1);	LCD_WR_SET();	NOP(1);	

#define	LCD_WR_PULSE()		NOP(1);	LCD_WR_CLR(); NOP(1);	LCD_WR_SET()

void LCD_delay(u16 ms)
{
    u16 i;
	do{
	
	
	
		i = MAIN_Fosc / 6000;	
		while(--i)	;
    }while(--ms);
}

void LCD_RegWriteComd(u8 cmd)
{
	LCD_RS_CLR();			
	LCD_CS_CLR();
	LCD_BUS_H = 0;
	LCD_BUS_L = cmd;
	LCD_WR_PULSE();
	LCD_RS_SET();			
	LCD_CS_SET();
}

void LCD_RegWriteData(u8 dat)
{

	LCD_CS_CLR();
	LCD_BUS_H = 0;
	LCD_BUS_L = dat;
	LCD_WR_PULSE();
	LCD_CS_SET();
}

void LCD_Init(void)
{
	P6n_standard(0xff);
	P2n_standard(0xff);
	P4n_standard(0x2e);

	background = 0x00;
	foreground = 0xffff;

	LCD_RS_SET();
	LCD_CS_SET();
	LCD_BUS_H = 0xFF;
	LCD_BUS_L = 0xFF;
	LCD_WR_SET();

	LCD_RST_SET();
	LCD_delay(150);
	LCD_RST_CLR();
	LCD_delay(150);
	LCD_RST_SET();
	LCD_delay(150);

	
	LCD_RegWriteComd(0x11);
	LCD_delay(20);
	LCD_RegWriteComd(0xD0);
	LCD_RegWriteData(0x07);
	LCD_RegWriteData(0x42);
	LCD_RegWriteData(0x18);
	
	LCD_RegWriteComd(0xD1);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(0x07);
	LCD_RegWriteData(0x10);
	
	LCD_RegWriteComd(0xD2);
	LCD_RegWriteData(0x01);
	LCD_RegWriteData(0x02);
	
	LCD_RegWriteComd(0xC0);
	LCD_RegWriteData(0x10);
	LCD_RegWriteData(0x3B);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(0x02);
	LCD_RegWriteData(0x11);
	
	LCD_RegWriteComd(0xC5);
	LCD_RegWriteData(0x03);
	
	LCD_RegWriteComd(0xC8);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(0x32);
	LCD_RegWriteData(0x36);
	LCD_RegWriteData(0x45);
	LCD_RegWriteData(0x06);
	LCD_RegWriteData(0x16);
	LCD_RegWriteData(0x37);
	LCD_RegWriteData(0x75);
	LCD_RegWriteData(0x77);
	LCD_RegWriteData(0x54);
	LCD_RegWriteData(0x0C);
	LCD_RegWriteData(0x00);
	
	LCD_RegWriteComd(0x36);
	#if (X_DOTS == 320)
		LCD_RegWriteData(0x0A);
	#endif
	#if (X_DOTS == 480)
		LCD_RegWriteData(0x28);
	#endif
	
	LCD_RegWriteComd(0x3A);
	LCD_RegWriteData(0x55);
	
	LCD_RegWriteComd(0x2A);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(0x00);

	LCD_RegWriteData(((X_DOTS-1)>>8));
	LCD_RegWriteData(((X_DOTS-1)&0xff));
	
	LCD_RegWriteComd(0x2B);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(0x00);

	LCD_RegWriteData(((Y_DOTS-1)>>8));
	LCD_RegWriteData(((Y_DOTS-1)&0xff));

	LCD_delay(120);

	LCD_RegWriteComd(0x29);
	LCD_RegWriteComd(0x2c);

	
	SetView_V();		
	LCD_delay(1);
	LCD_Fill_XY(0,0,X_DOTS,Y_DOTS,0x0000);

}

void LCD_SetWindows(u16 xStar, u16 yStar,u16 xEnd,u16 yEnd)
{
	LCD_RegWriteComd(0x2a);	
	LCD_RegWriteData((u8)(xStar>>8));
	LCD_RegWriteData((u8)(xStar &0xff));		
	LCD_RegWriteData((u8)(xEnd>>8));
	LCD_RegWriteData((u8)(xEnd &0xff));

	LCD_RegWriteComd(0x2b);	
	LCD_RegWriteData((u8)(yStar>>8));
	LCD_RegWriteData((u8)(yStar &0xff));		
	LCD_RegWriteData((u8)(yEnd>>8));
	LCD_RegWriteData((u8)(yEnd &0xff)); 

	LCD_RegWriteComd(0x2c);	
}

void LCD_Fill_XY(u16 x0,u16 y0,u16 x,u16 y,u16 color)
{
	u8 i;

	LCD_SetWindows(x0,y0,x0+x-1,y0+y-1);	
	LCD_RS = 1;	LCD_CS_CLR();
	LCD_BUS_H = (u8)(color>>8);
	LCD_BUS_L = (u8)color;

	i = (u8)(((u32)x *(u32)y) & 7);	
	while(i != 0)
	{
		LCD_WR_PULSE();
		i--;
	}
	x = (u16)(((u32)x *(u32)y) /8);	
	while(x != 0)	
	{
		LCD_WR_PULSE();	
		LCD_WR_PULSE();
		LCD_WR_PULSE();
		LCD_WR_PULSE();
		LCD_WR_PULSE();
		LCD_WR_PULSE();
		LCD_WR_PULSE();
		LCD_WR_PULSE();
		x--;
	}
	LCD_CS_SET();

}

void	SetView_V(void)		
{
	LCD_RegWriteComd(0x36);

	LCD_RegWriteData(0x09);		

	LCD_RegWriteComd(0x2A);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(((320-1)>>8));
	LCD_RegWriteData(((320-1)&0xff));

	LCD_RegWriteComd(0x2B);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(0x00);
	LCD_RegWriteData(((480-1)>>8));
	LCD_RegWriteData(((480-1)&0xff));
}
