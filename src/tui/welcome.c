#include "classicsetup/welcome.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum classicsetup_welcome_result classicsetup_show_welcome(void)
{
    int key;

    classicsetup_tui_begin_screen("ClassicSetup Setup");

    classicsetup_tui_add_text(3, 3, "Welcome to ClassicSetup.");
    classicsetup_tui_add_text(
        5,
        3,
        "This program prepares a computer for Windows installation.");
    classicsetup_tui_draw_bullet(8, "To continue, press ENTER.");
    classicsetup_tui_draw_bullet(10, "To quit Setup, press Q.");
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
