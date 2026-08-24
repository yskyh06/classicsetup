#ifndef CLASSICSETUP_TUI_H
#define CLASSICSETUP_TUI_H

#include <stdbool.h>

bool classicsetup_tui_init(void);
void classicsetup_tui_shutdown(void);

void classicsetup_tui_begin_screen(const char *title);
void classicsetup_tui_add_centered(int row, const char *text);

#endif
