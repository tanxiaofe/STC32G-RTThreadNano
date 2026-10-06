/* SPDX-License-Identifier: Apache-2.0 */
#ifndef STC_RT_GUI_H
#define STC_RT_GUI_H
#define GUI_STATUS 0
#define GUI_IPC 1
#define GUI_MSH 2
void gui_keys(unsigned char pressed);
void gui_set_page(unsigned char page);
unsigned char gui_page(void);
/* Only the LCD thread invokes rendering. */
void gui_render(void);
#endif
