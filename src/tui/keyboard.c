#include "classicsetup/keyboard.h"

#include <ncurses.h>

#include "classicsetup/tui.h"

static const char *const keyboard_names[] = {
    "Korean 103/106-key Keyboard",
    "PC/AT 101-key Keyboard",
    "PC/AT 101-key Compatible Keyboard",
    "Other Keyboard"
};

static void draw_option(int row, const char *name, bool selected)
{
    int column = (COLS - 44) / 2;

    if (column < 2) {
        column = 2;
    }
    if (row < 0 || row >= LINES || column >= COLS) {
        return;
    }

    if (selected) {
        attron(A_REVERSE | A_BOLD);
    }

    mvaddnstr(row, column, name, COLS - column);

    if (selected) {
        attroff(A_REVERSE | A_BOLD);
    }
}

static void draw_keyboard_screen(int selected)
{
    int first_row = LINES / 2 - 3;
    int index;

    if (first_row < 4) {
        first_row = 4;
    }

    classicsetup_tui_begin_screen("ClassicSetup Setup");
    classicsetup_tui_add_centered(first_row - 2, "Choose the keyboard type for this setup.");

    for (index = 0; index < CLASSICSETUP_KEYBOARD_TYPE_COUNT; ++index) {
        draw_option(first_row + index, keyboard_names[index], index == selected);
    }

    attron(A_BOLD);
    classicsetup_tui_add_centered(
        LINES - 3,
        "UP/DOWN=Select    ENTER=Continue    B=Back    F3=Quit");
    attroff(A_BOLD);
    refresh();
}

enum classicsetup_keyboard_result classicsetup_show_keyboard(
    enum classicsetup_keyboard_type *keyboard_type)
{
    int selected = (int)*keyboard_type;

    if (selected < 0 || selected >= CLASSICSETUP_KEYBOARD_TYPE_COUNT) {
        selected = CLASSICSETUP_KEYBOARD_KOREAN_103_106;
    }

    for (;;) {
        int key;

        draw_keyboard_screen(selected);
        key = getch();

        if (key == KEY_UP && selected > 0) {
            --selected;
        } else if (key == KEY_DOWN && selected < CLASSICSETUP_KEYBOARD_TYPE_COUNT - 1) {
            ++selected;
        } else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            *keyboard_type = (enum classicsetup_keyboard_type)selected;
            return CLASSICSETUP_KEYBOARD_CONTINUE;
        } else if (key == 'b' || key == 'B') {
            return CLASSICSETUP_KEYBOARD_BACK;
        } else if (key == KEY_F(3)) {
            return CLASSICSETUP_KEYBOARD_QUIT;
        }
    }
}
