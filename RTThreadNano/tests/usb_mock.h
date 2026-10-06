#ifndef STC_USB_HOST_MOCK_H
#define STC_USB_HOST_MOCK_H
#include <rtthread.h>
typedef unsigned char BYTE;
typedef unsigned int WORD;
typedef unsigned long DWORD;
typedef unsigned char BOOL;
#define xdata
#include "../bsp/usb/usb.h"
#define INDEX 14
#define INCSR1 17
#define OUTCSR1 20
#define FIFO1 33
#define INIPRDY 1
#define EP1IN_SIZE 64
extern volatile BYTE UsbControlLines;
#endif
