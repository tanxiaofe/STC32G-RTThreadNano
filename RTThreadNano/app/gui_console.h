/* SPDX-License-Identifier: Apache-2.0 */
#ifndef STC_GUI_CONSOLE_H
#define STC_GUI_CONSOLE_H
#define GUI_CONSOLE_COLS 76
#define GUI_CONSOLE_ROWS 14
void gui_console_init(void);
void gui_console_feed(const char *text);
/* Caller provides COLS+1 bytes. Snapshot one logical row under a short IRQ lock. */
void gui_console_line(unsigned char row,char *out);
#endif
