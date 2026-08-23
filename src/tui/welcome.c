#include "classicsetup/welcome.h"

#include <ncurses.h>
#include <string.h>

static void add_centered(int row, const char *text)
{
    int column = (COLS - (int)strlen(text)) / 2;

    if (row >= 0 && row < LINES && column >= 0) {
        mvaddstr(row, column, text);
    }
}

enum classicsetup_welcome_result classicsetup_show_welcome(void)
{
    enum classicsetup_welcome_result result = CLASSICSETUP_WELCOME_QUIT;
    int key;

    if (initscr() == NULL) {
        return result;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_WHITE, COLOR_BLUE);
        bkgd(COLOR_PAIR(1));
    }

    erase();
    attron(A_BOLD);
    mvaddstr(1, 2, "ClassicSetup Setup");
    attroff(A_BOLD);

    add_centered(LINES / 2 - 2, "Welcome to ClassicSetup");
    add_centered(LINES / 2, "This setup assistant is ready to begin.");
    add_centered(LINES / 2 + 1, "Press ENTER to continue or F3 to quit.");

    attron(A_BOLD);
    add_centered(LINES - 3, "ENTER=Continue    F3=Quit");
    attroff(A_BOLD);
    refresh();

    while ((key = getch()) != KEY_F(3)) {
        if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            result = CLASSICSETUP_WELCOME_CONTINUE;
            break;
        }
    }

    endwin();
    return result;
}
