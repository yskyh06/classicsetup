#include "classicsetup/quit.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum classicsetup_quit_confirmation_result classicsetup_confirm_quit(void)
{
    classicsetup_tui_begin_screen("ClassicSetup");
    classicsetup_tui_draw_warning(4, "Setup has not been completed.");
    classicsetup_tui_add_text(
        6,
        4,
        "If you quit now, Windows will not be installed.");
    classicsetup_tui_add_text(8, 4, "Press Q to quit Setup.");
    classicsetup_tui_add_text(9, 4, "Press ESC to continue Setup.");
    classicsetup_tui_draw_footer("Q=Quit Setup    ESC=Continue Setup");
    refresh();

    for (;;) {
        int key = getch();

        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_QUIT_CONFIRM;
        }
        if (key == 27) {
            return CLASSICSETUP_QUIT_CONTINUE_SETUP;
        }
    }
}
