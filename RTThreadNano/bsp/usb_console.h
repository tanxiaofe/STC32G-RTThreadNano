#ifndef STC_USB_CONSOLE_H
#define STC_USB_CONSOLE_H
void usb_console_init(void);
void rt_console_lock(void);
void rt_console_unlock(void);
char rt_hw_console_getchar(void);
#endif
