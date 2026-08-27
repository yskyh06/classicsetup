#ifndef CLASSICSETUP_TUI_H
#define CLASSICSETUP_TUI_H

#include <stdbool.h>

bool classicsetup_tui_init(void);
void classicsetup_tui_shutdown(void);

int classicsetup_tui_canvas_width(void);
int classicsetup_tui_canvas_height(void);
int classicsetup_tui_compact_list_height(
    int item_count,
    int maximum_rows);

void classicsetup_tui_begin_screen(const char *title);
void classicsetup_tui_add_centered(int row, const char *text);
void classicsetup_tui_add_text(int row, int column, const char *text);
void classicsetup_tui_draw_frame(int top, int left, int bottom, int right);
void classicsetup_tui_draw_list_row(
    int row,
    int left,
    int width,
    const char *text,
    bool selected);
void classicsetup_tui_draw_footer(const char *text);
void classicsetup_tui_draw_warning(int row, const char *text);
void classicsetup_tui_draw_bullet(int row, const char *text);
void classicsetup_tui_draw_metadata(
    int row,
    const char *label,
    const char *value);

#endif
