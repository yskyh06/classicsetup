#include "classicsetup/welcome.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum classicsetup_welcome_result classicsetup_show_welcome(void)
{
    int key;

    classicsetup_tui_begin_screen("ClassicSetup Setup");

    classicsetup_tui_add_text(5, 4, "Welcome to ClassicSetup.");
    classicsetup_tui_add_text(
        7,
        4,
        "This program prepares a computer for Windows installation.");
    classicsetup_tui_add_text(9, 4, "Press ENTER to continue.");
    classicsetup_tui_draw_frame(12, 3, LINES - 4, COLS - 4);
    classicsetup_tui_draw_footer("ENTER=Continue    Q=Quit");
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
