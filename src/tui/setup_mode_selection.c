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
        classicsetup_tui_draw_wrapped_text(
            4,
            4,
            classicsetup_tui_canvas_width() - 8,
            "Choose how you want to install Windows.");
        classicsetup_tui_draw_wrapped_text(
            5,
            4,
            classicsetup_tui_canvas_width() - 8,
            "Use the UP and DOWN ARROW keys to select an option.");
        classicsetup_tui_draw_frame(
            6,
            3,
            13,
            classicsetup_tui_canvas_width() - 4);
        classicsetup_tui_draw_list_row(
            7,
            5,
            classicsetup_tui_canvas_width() - 10,
            "Recommended installation",
            selected == CLASSICSETUP_SETUP_RECOMMENDED);
        classicsetup_tui_draw_wrapped_text(
            8,
            8,
            classicsetup_tui_canvas_width() - 12,
            "Automatic settings and a simplified installation experience.");
        classicsetup_tui_draw_list_row(
            10,
            5,
            classicsetup_tui_canvas_width() - 10,
            "Advanced installation",
            selected == CLASSICSETUP_SETUP_ADVANCED);
        classicsetup_tui_draw_wrapped_text(
            11,
            8,
            classicsetup_tui_canvas_width() - 12,
            "Manually configure firmware, disks, partitions, and formatting.");
        classicsetup_tui_draw_footer(
            "UP/DOWN=Select    ENTER=Continue    Q=Quit");
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
