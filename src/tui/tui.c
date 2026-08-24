#include "classicsetup/tui.h"

#include <ncurses.h>
#include <string.h>

enum {
    CLASSICSETUP_COLOR_SCREEN = 1
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

    if (LINES > 1 && COLS > 2) {
        mvaddnstr(1, 2, title, COLS - 2);
    }

    attroff(A_BOLD);
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
