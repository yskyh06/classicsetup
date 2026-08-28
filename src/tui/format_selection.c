#include "classicsetup/format_selection.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

static void draw_option(int row, const char *text, bool selected)
{
    int width = classicsetup_tui_canvas_width();
    int column = (width - 58) / 2;

    if (column < 2) {
        column = 2;
    }
    if (row < 0 || row >= classicsetup_tui_canvas_height() ||
        column >= width) {
        return;
    }
    classicsetup_tui_draw_list_row(row, column, 58, text, selected);
}

static void draw_format_screen(enum classicsetup_format_mode selected)
{
    classicsetup_tui_begin_screen("ClassicSetup - Format Partition");
    classicsetup_tui_draw_wrapped_text(
        3,
        3,
        classicsetup_tui_canvas_width() - 6,
        "The selected Windows partition needs a format plan.");
    classicsetup_tui_draw_wrapped_text(
        5,
        3,
        classicsetup_tui_canvas_width() - 6,
        "Use the UP and DOWN ARROW keys to select a method,");
    classicsetup_tui_draw_wrapped_text(
        6,
        3,
        classicsetup_tui_canvas_width() - 6,
        "and then press ENTER to continue.");
    classicsetup_tui_draw_wrapped_text(
        8,
        3,
        classicsetup_tui_canvas_width() - 6,
        "Press B to select a different partition.");

    classicsetup_tui_draw_frame(
        10,
        3,
        13,
        classicsetup_tui_canvas_width() - 4);

    draw_option(
        11,
        "Format the partition using NTFS (Quick)",
        selected == CLASSICSETUP_FORMAT_QUICK);
    draw_option(
        12,
        "Format the partition using NTFS",
        selected == CLASSICSETUP_FORMAT_FULL);

    classicsetup_tui_draw_footer(
        "UP/DOWN=Select    ENTER=Continue    B=Back    Q=Quit");
    refresh();
}

enum classicsetup_format_selection_result classicsetup_show_format_selection(
    enum classicsetup_format_mode *format_mode)
{
    enum classicsetup_format_mode selected =
        classicsetup_default_format_mode();

    if (format_mode != NULL && *format_mode == CLASSICSETUP_FORMAT_FULL) {
        selected = CLASSICSETUP_FORMAT_FULL;
    }

    for (;;) {
        int key;

        draw_format_screen(selected);
        key = getch();
        if (key == KEY_UP) {
            selected = classicsetup_format_move_option(selected, -1);
        } else if (key == KEY_DOWN) {
            selected = classicsetup_format_move_option(selected, 1);
        } else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            if (format_mode != NULL) {
                *format_mode = selected;
            }
            return CLASSICSETUP_FORMAT_SELECTION_CONTINUE;
        } else if (key == 'b' || key == 'B') {
            return CLASSICSETUP_FORMAT_SELECTION_BACK;
        } else if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_FORMAT_SELECTION_QUIT;
        }
    }
}
