#include "classicsetup/install_mode_selection.h"

#include <ncurses.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

static const char *const mode_names[] = {
    "UEFI with GPT",
    "Legacy BIOS with MBR"
};

static void draw_option(int row, const char *text, int selected)
{
    int width = classicsetup_tui_canvas_width();
    int column = (width - 46) / 2;

    if (column < 2) {
        column = 2;
    }
    if (row < 0 || row >= classicsetup_tui_canvas_height() ||
        column >= width) {
        return;
    }
    classicsetup_tui_draw_list_row(row, column, 46, text, selected != 0);
}

static void draw_screen(enum classicsetup_install_mode selected)
{
    int first_row = 6;
    int index;

    if (first_row < 5) {
        first_row = 5;
    }
    classicsetup_tui_begin_screen("ClassicSetup - Installation Mode");
    classicsetup_tui_draw_wrapped_text(
        first_row - 3,
        4,
        classicsetup_tui_canvas_width() - 8,
        "Select the firmware and partitioning mode.");
    classicsetup_tui_draw_frame(
        first_row - 1,
        3,
        first_row + 3,
        classicsetup_tui_canvas_width() - 4);
    for (index = 0; index < CLASSICSETUP_INSTALL_MODE_COUNT; ++index) {
        draw_option(
            first_row + index,
            mode_names[index],
            index == (int)selected);
    }
    classicsetup_tui_draw_footer(
        "UP/DOWN=Select    ENTER=Continue    B=Back    Q=Quit");
    refresh();
}

enum classicsetup_install_mode_selection_result
classicsetup_show_install_mode_selection(
    enum classicsetup_install_mode *install_mode)
{
    enum classicsetup_install_mode selected;

    if (install_mode == NULL || *install_mode < 0 ||
        *install_mode >= CLASSICSETUP_INSTALL_MODE_COUNT) {
        selected = classicsetup_default_install_mode();
    } else {
        selected = *install_mode;
    }

    for (;;) {
        int key;

        draw_screen(selected);
        key = getch();
        if (key == KEY_UP && selected > CLASSICSETUP_INSTALL_UEFI_GPT) {
            selected = (enum classicsetup_install_mode)(selected - 1);
        } else if (key == KEY_DOWN &&
                   selected + 1 < CLASSICSETUP_INSTALL_MODE_COUNT) {
            selected = (enum classicsetup_install_mode)(selected + 1);
        } else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            if (install_mode != NULL) {
                *install_mode = selected;
            }
            return CLASSICSETUP_INSTALL_MODE_SELECTION_CONTINUE;
        } else if (key == 'b' || key == 'B') {
            return CLASSICSETUP_INSTALL_MODE_SELECTION_BACK;
        } else if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_INSTALL_MODE_SELECTION_QUIT;
        }
    }
}
