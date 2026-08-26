#include "classicsetup/welcome.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum classicsetup_welcome_result classicsetup_show_welcome(void)
{
    int key;

    classicsetup_tui_begin_screen("ClassicSetup Setup");

    classicsetup_tui_add_centered(LINES / 2 - 2, "Welcome to ClassicSetup");
    classicsetup_tui_add_centered(LINES / 2, "This setup assistant is ready to begin.");
    classicsetup_tui_add_centered(LINES / 2 + 1, "Press ENTER to continue or Q to quit.");

    attron(A_BOLD);
    classicsetup_tui_add_centered(LINES - 3, "ENTER=Continue    Q=Quit");
    attroff(A_BOLD);
    refresh();

    for (;;) {
        key = getch();

        if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            return CLASSICSETUP_WELCOME_CONTINUE;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_WELCOME_QUIT;
        }
    }
}
