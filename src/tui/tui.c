#include "classicsetup/tui.h"

#include <ncurses.h>
#include <stdio.h>
#include <string.h>

enum {
    CLASSICSETUP_CANVAS_WIDTH = 80,
    CLASSICSETUP_CANVAS_HEIGHT = 25,
    CLASSICSETUP_COLOR_SCREEN = 1,
    CLASSICSETUP_COLOR_SELECTED = 2,
    CLASSICSETUP_COLOR_WARNING = 3,
    CLASSICSETUP_COLOR_FOOTER = 4
};

static int canvas_width(void)
{
    return COLS < CLASSICSETUP_CANVAS_WIDTH ? COLS : CLASSICSETUP_CANVAS_WIDTH;
}

static int canvas_height(void)
{
    return LINES < CLASSICSETUP_CANVAS_HEIGHT ? LINES
                                             : CLASSICSETUP_CANVAS_HEIGHT;
}

static int canvas_left(void)
{
    return (COLS - canvas_width()) / 2;
}

static int canvas_top(void)
{
    return (LINES - canvas_height()) / 2;
}

int classicsetup_tui_canvas_width(void)
{
    return canvas_width();
}

int classicsetup_tui_canvas_height(void)
{
    return canvas_height();
}

int classicsetup_tui_compact_list_height(
    int item_count,
    int maximum_rows)
{
    if (item_count < 1) {
        return 1;
    }
    if (maximum_rows < 1) {
        return 1;
    }
    return item_count < maximum_rows ? item_count : maximum_rows;
}

bool classicsetup_tui_init(void)
{
    if (initscr() == NULL) {
        return false;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(CLASSICSETUP_COLOR_SCREEN, COLOR_WHITE, COLOR_BLUE);
        init_pair(CLASSICSETUP_COLOR_SELECTED, COLOR_BLUE, COLOR_WHITE);
        init_pair(CLASSICSETUP_COLOR_WARNING, COLOR_YELLOW, COLOR_BLUE);
        init_pair(CLASSICSETUP_COLOR_FOOTER, COLOR_BLACK, COLOR_WHITE);
        bkgd(COLOR_PAIR(CLASSICSETUP_COLOR_SCREEN));
    }

    return true;
}

void classicsetup_tui_shutdown(void)
{
    endwin();
}

void classicsetup_tui_begin_screen(const char *title)
{
    int left = canvas_left();
    int top = canvas_top();
    int width = canvas_width();

    erase();
    attron(A_BOLD);

    if (title != NULL && canvas_height() > 1 && width > 4) {
        mvaddnstr(top, left + 1, title, width - 2);
        mvhline(top + 1, left + 1, ACS_HLINE, width - 2);
    }

    attroff(A_BOLD);
}

void classicsetup_tui_add_text(int row, int column, const char *text)
{
    int height = canvas_height();
    int width = canvas_width();

    if (text == NULL || row < 0 || row >= height || column < 0 ||
        column >= width - 1) {
        return;
    }
    mvaddnstr(
        canvas_top() + row,
        canvas_left() + column,
        text,
        width - column - 1);
}

void classicsetup_tui_draw_frame(int top, int left, int bottom, int right)
{
    if (top < 0 || left < 0 || bottom <= top || right <= left ||
        bottom >= canvas_height() - 1 || right >= canvas_width()) {
        return;
    }
    top += canvas_top();
    bottom += canvas_top();
    left += canvas_left();
    right += canvas_left();
    mvaddch(top, left, ACS_ULCORNER);
    mvhline(top, left + 1, ACS_HLINE, right - left - 1);
    mvaddch(top, right, ACS_URCORNER);
    mvvline(top + 1, left, ACS_VLINE, bottom - top - 1);
    mvvline(top + 1, right, ACS_VLINE, bottom - top - 1);
    mvaddch(bottom, left, ACS_LLCORNER);
    mvhline(bottom, left + 1, ACS_HLINE, right - left - 1);
    mvaddch(bottom, right, ACS_LRCORNER);
}

void classicsetup_tui_draw_list_row(
    int row,
    int left,
    int width,
    const char *text,
    bool selected)
{
    int usable;

    int canvas_row;
    int canvas_column;

    if (text == NULL || row < 0 || row >= canvas_height() || left < 0 ||
        left >= canvas_width() - 1 || width <= 0) {
        return;
    }
    usable = width < canvas_width() - left - 1
                 ? width
                 : canvas_width() - left - 1;
    canvas_row = canvas_top() + row;
    canvas_column = canvas_left() + left;
    if (selected) {
        attron(COLOR_PAIR(CLASSICSETUP_COLOR_SELECTED) | A_BOLD);
    }
    mvhline(canvas_row, canvas_column, ' ', usable);
    mvaddnstr(
        canvas_row,
        canvas_column + (usable > 2 ? 1 : 0),
        text,
        usable - 1);
    if (selected) {
        attroff(COLOR_PAIR(CLASSICSETUP_COLOR_SELECTED) | A_BOLD);
    }
}

void classicsetup_tui_draw_footer(const char *text)
{
    int row = canvas_top() + canvas_height() - 1;
    int left = canvas_left();
    int width = canvas_width();

    if (text == NULL || row < 0 || width <= 1) {
        return;
    }
    attron(COLOR_PAIR(CLASSICSETUP_COLOR_FOOTER) | A_BOLD);
    mvhline(row, left, ' ', width);
    mvaddnstr(row, left + 1, text, width - 2);
    attroff(COLOR_PAIR(CLASSICSETUP_COLOR_FOOTER) | A_BOLD);
}

void classicsetup_tui_draw_warning(int row, const char *text)
{
    attron(COLOR_PAIR(CLASSICSETUP_COLOR_WARNING) | A_BOLD);
    classicsetup_tui_add_text(row, 4, text);
    attroff(COLOR_PAIR(CLASSICSETUP_COLOR_WARNING) | A_BOLD);
}

void classicsetup_tui_draw_bullet(int row, const char *text)
{
    classicsetup_tui_add_text(row, 5, "*  ");
    classicsetup_tui_add_text(row, 8, text);
}

void classicsetup_tui_draw_metadata(
    int row,
    const char *label,
    const char *value)
{
    char line[256];

    if (label == NULL || value == NULL) {
        return;
    }
    snprintf(line, sizeof(line), "%-10s %s", label, value);
    classicsetup_tui_add_text(row, 4, line);
}

void classicsetup_tui_add_centered(int row, const char *text)
{
    int text_length = (int)strlen(text);
    int width = canvas_width();
    int column = (width - text_length) / 2;
    int maximum_length;

    if (row < 0 || row >= canvas_height() || width <= 1) {
        return;
    }
    if (column < 0) {
        column = 0;
    }
    maximum_length = width - column - 1;
    if (maximum_length > 0) {
        mvaddnstr(
            canvas_top() + row,
            canvas_left() + column,
            text,
            maximum_length);
    }
}
