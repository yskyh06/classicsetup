#include "classicsetup/format_selection.h"

#include <ncurses.h>

#include "classicsetup/tui.h"

static void draw_option(int row, const char *text, bool selected)
{
    int column = (COLS - 48) / 2;

    if (column < 2) {
        column = 2;
    }
    if (row < 0 || row >= LINES || column >= COLS) {
        return;
    }
    if (selected) {
        attron(A_REVERSE | A_BOLD);
    }
    mvaddnstr(row, column, text, COLS - column);
    if (selected) {
        attroff(A_REVERSE | A_BOLD);
    }
}

static void draw_format_screen(enum classicsetup_format_mode selected)
{
    classicsetup_tui_begin_screen("ClassicSetup - Format Partition");
    classicsetup_tui_add_centered(
        LINES / 2 - 6,
        "The selected Windows partition needs a format plan.");
    classicsetup_tui_add_centered(
        LINES / 2 - 4,
        "Use the UP and DOWN ARROW keys to select a method,");
    classicsetup_tui_add_centered(
        LINES / 2 - 3,
        "and then press ENTER to continue.");
    classicsetup_tui_add_centered(
        LINES / 2 - 1,
        "Press ESC to select a different partition.");

    draw_option(
        LINES / 2 + 2,
        "Format the partition using NTFS (Quick)",
        selected == CLASSICSETUP_FORMAT_QUICK);
    draw_option(
        LINES / 2 + 3,
        "Format the partition using NTFS",
        selected == CLASSICSETUP_FORMAT_FULL);

    attron(A_BOLD);
    classicsetup_tui_add_centered(
        LINES - 3,
        "UP/DOWN=Select    ENTER=Continue    ESC=Cancel    F3=Quit");
    attroff(A_BOLD);
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
        } else if (key == 27) {
            return CLASSICSETUP_FORMAT_SELECTION_CANCEL;
        } else if (key == KEY_F(3)) {
            return CLASSICSETUP_FORMAT_SELECTION_QUIT;
        }
    }
}
