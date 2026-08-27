#include "classicsetup/setup_mode_selection.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum classicsetup_setup_mode_selection_result
classicsetup_show_setup_mode_selection(
    enum classicsetup_setup_mode *setup_mode)
{
    enum classicsetup_setup_mode selected =
        setup_mode != NULL && *setup_mode == CLASSICSETUP_SETUP_ADVANCED
            ? CLASSICSETUP_SETUP_ADVANCED
            : CLASSICSETUP_SETUP_RECOMMENDED;

    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup");
        classicsetup_tui_add_centered(4, "Choose an installation experience.");
        if (selected == CLASSICSETUP_SETUP_RECOMMENDED) {
            attron(A_REVERSE | A_BOLD);
        }
        classicsetup_tui_add_centered(8, "Recommended installation");
        if (selected == CLASSICSETUP_SETUP_RECOMMENDED) {
            attroff(A_REVERSE | A_BOLD);
        }
        if (selected == CLASSICSETUP_SETUP_ADVANCED) {
            attron(A_REVERSE | A_BOLD);
        }
        classicsetup_tui_add_centered(10, "Advanced installation");
        if (selected == CLASSICSETUP_SETUP_ADVANCED) {
            attroff(A_REVERSE | A_BOLD);
        }
        classicsetup_tui_add_centered(
            13,
            selected == CLASSICSETUP_SETUP_RECOMMENDED
                ? "Automatic safe defaults for an empty test disk."
                : "Manual firmware, partition, and format choices.");
        attron(A_BOLD);
        classicsetup_tui_add_centered(
            LINES - 3,
            "UP/DOWN=Select    ENTER=Continue    Q=Quit");
        attroff(A_BOLD);
        refresh();

        key = getch();
        if (key == KEY_UP || key == KEY_DOWN) {
            selected = selected == CLASSICSETUP_SETUP_RECOMMENDED
                           ? CLASSICSETUP_SETUP_ADVANCED
                           : CLASSICSETUP_SETUP_RECOMMENDED;
        } else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            if (setup_mode != NULL) {
                *setup_mode = selected;
            }
            return CLASSICSETUP_SETUP_MODE_SELECTION_CONTINUE;
        } else if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_SETUP_MODE_SELECTION_QUIT;
        }
    }
}
