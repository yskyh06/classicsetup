#include "classicsetup/quit.h"

#include <ncurses.h>

#include "classicsetup/tui.h"

enum classicsetup_quit_confirmation_result classicsetup_confirm_quit(void)
{
    classicsetup_tui_begin_screen("ClassicSetup");
    classicsetup_tui_add_centered(
        LINES / 2 - 2,
        "Setup has not been completed.");
    classicsetup_tui_add_centered(
        LINES / 2,
        "If you quit now, Windows will not be installed.");
    attron(A_BOLD);
    classicsetup_tui_add_centered(
        LINES - 3,
        "F3=Quit Setup    ESC=Continue Setup");
    attroff(A_BOLD);
    refresh();

    for (;;) {
        int key = getch();

        if (key == KEY_F(3)) {
            return CLASSICSETUP_QUIT_CONFIRM;
        }
        if (key == 27) {
            return CLASSICSETUP_QUIT_CONTINUE_SETUP;
        }
    }
}
