#include "classicsetup/after_format.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum classicsetup_after_format_result classicsetup_show_after_format(
    int filesystems_applied)
{
    classicsetup_tui_begin_screen("ClassicSetup - Storage Step Complete");
    classicsetup_tui_draw_wrapped_text(
        4,
        4,
        classicsetup_tui_canvas_width() - 8,
        filesystems_applied
            ? "The partition layout and filesystems were applied and verified."
            : "The filesystem apply step has not completed.");
    classicsetup_tui_draw_wrapped_text(
        6,
        4,
        classicsetup_tui_canvas_width() - 8,
        "Windows image application is not implemented yet.");
    classicsetup_tui_draw_wrapped_text(
        7,
        4,
        classicsetup_tui_canvas_width() - 8,
        "Later installation steps are not implemented yet.");
    classicsetup_tui_draw_footer("ENTER=Finish    B=Back    Q=Quit");
    refresh();

    for (;;) {
        int key = getch();

        if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            return CLASSICSETUP_AFTER_FORMAT_FINISH;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_AFTER_FORMAT_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_AFTER_FORMAT_QUIT;
        }
    }
}
