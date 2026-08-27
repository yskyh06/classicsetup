#include "classicsetup/tui.h"

#include <ncurses.h>
#include <string.h>

enum {
    CLASSICSETUP_COLOR_SCREEN = 1,
    CLASSICSETUP_COLOR_SELECTED = 2,
    CLASSICSETUP_COLOR_WARNING = 3,
    CLASSICSETUP_COLOR_FOOTER = 4
};

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
    erase();
    attron(A_BOLD);

    if (LINES > 1 && COLS > 4) {
        mvaddnstr(1, 2, title, COLS - 2);
        mvhline(2, 2, ACS_HLINE, COLS - 4);
    }

    attroff(A_BOLD);
}

void classicsetup_tui_add_text(int row, int column, const char *text)
{
    if (text == NULL || row < 0 || row >= LINES || column < 0 ||
        column >= COLS - 1) {
        return;
    }
    mvaddnstr(row, column, text, COLS - column - 1);
}

void classicsetup_tui_draw_frame(int top, int left, int bottom, int right)
{
    if (top < 0 || left < 0 || bottom <= top || right <= left ||
        bottom >= LINES - 1 || right >= COLS) {
        return;
    }
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

    if (text == NULL || row < 0 || row >= LINES || left < 0 ||
        left >= COLS - 1 || width <= 0) {
        return;
    }
    usable = width < COLS - left - 1 ? width : COLS - left - 1;
    if (selected) {
        attron(COLOR_PAIR(CLASSICSETUP_COLOR_SELECTED) | A_BOLD);
    }
    mvhline(row, left, ' ', usable);
    mvaddnstr(row, left + (usable > 2 ? 1 : 0), text, usable - 1);
    if (selected) {
        attroff(COLOR_PAIR(CLASSICSETUP_COLOR_SELECTED) | A_BOLD);
    }
}

void classicsetup_tui_draw_footer(const char *text)
{
    int row = LINES - 1;

    if (text == NULL || row < 0 || COLS <= 1) {
        return;
    }
    attron(COLOR_PAIR(CLASSICSETUP_COLOR_FOOTER) | A_BOLD);
    mvhline(row, 0, ' ', COLS);
    mvaddnstr(row, 1, text, COLS - 2);
    attroff(COLOR_PAIR(CLASSICSETUP_COLOR_FOOTER) | A_BOLD);
}

void classicsetup_tui_draw_warning(int row, const char *text)
{
    attron(COLOR_PAIR(CLASSICSETUP_COLOR_WARNING) | A_BOLD);
    classicsetup_tui_add_text(row, 4, text);
    attroff(COLOR_PAIR(CLASSICSETUP_COLOR_WARNING) | A_BOLD);
}

void classicsetup_tui_add_centered(int row, const char *text)
{
    int text_length = (int)strlen(text);
    int column = (COLS - text_length) / 2;
    int maximum_length;

    if (row < 0 || row >= LINES || COLS <= 1) {
        return;
    }
    if (column < 0) {
        column = 0;
    }
    maximum_length = COLS - column - 1;
    if (maximum_length > 0) {
        mvaddnstr(row, column, text, maximum_length);
    }
}
