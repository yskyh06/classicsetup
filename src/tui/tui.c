#include "classicsetup/tui.h"

#include <ncurses.h>
#include <ctype.h>
#include <string.h>

enum {
    CLASSICSETUP_CANVAS_WIDTH = 80,
    CLASSICSETUP_CANVAS_HEIGHT = 25,
    CLASSICSETUP_COLOR_SCREEN = 1,
    CLASSICSETUP_COLOR_SELECTED = 2,
    CLASSICSETUP_COLOR_WARNING = 3,
    CLASSICSETUP_COLOR_FOOTER = 4
};

static int canvas_width(void)
{
    int width;

    classicsetup_tui_canvas_dimensions_for_terminal(
        COLS,
        LINES,
        &width,
        NULL);
    return width;
}

static int canvas_height(void)
{
    int height;

    classicsetup_tui_canvas_dimensions_for_terminal(
        COLS,
        LINES,
        NULL,
        &height);
    return height;
}

enum classicsetup_tui_layout_profile classicsetup_tui_select_layout_profile(
    int terminal_columns,
    int terminal_lines)
{
    if (terminal_columns < 80 || terminal_lines < 25) {
        return CLASSICSETUP_TUI_LAYOUT_COMPACT;
    }
    if (terminal_columns >= 120 && terminal_lines >= 35) {
        return CLASSICSETUP_TUI_LAYOUT_LARGE;
    }
    if (terminal_columns >= 100 && terminal_lines >= 30) {
        return CLASSICSETUP_TUI_LAYOUT_MEDIUM;
    }
    return CLASSICSETUP_TUI_LAYOUT_CLASSIC;
}

void classicsetup_tui_canvas_dimensions_for_terminal(
    int terminal_columns,
    int terminal_lines,
    int *canvas_columns,
    int *canvas_lines)
{
    enum classicsetup_tui_layout_profile profile =
        classicsetup_tui_select_layout_profile(
            terminal_columns,
            terminal_lines);
    int width = terminal_columns > 0 ? terminal_columns : 1;
    int height = terminal_lines > 0 ? terminal_lines : 1;

    if (profile == CLASSICSETUP_TUI_LAYOUT_CLASSIC) {
        width = CLASSICSETUP_CANVAS_WIDTH;
        height = CLASSICSETUP_CANVAS_HEIGHT;
    } else if (profile == CLASSICSETUP_TUI_LAYOUT_MEDIUM) {
        width = 100;
        height = 30;
    } else if (profile == CLASSICSETUP_TUI_LAYOUT_LARGE) {
        width = terminal_columns < 120 ? terminal_columns : 120;
        height = terminal_lines < 40 ? terminal_lines : 40;
    }
    if (canvas_columns != NULL) {
        *canvas_columns = width;
    }
    if (canvas_lines != NULL) {
        *canvas_lines = height;
    }
}

static int canvas_left(void)
{
    return (COLS - canvas_width()) / 2;
}

static int canvas_top(void)
{
    return (LINES - canvas_height()) / 2;
}

int classicsetup_tui_canvas_width(void)
{
    return canvas_width();
}

int classicsetup_tui_canvas_height(void)
{
    return canvas_height();
}

int classicsetup_tui_compact_list_height(
    int item_count,
    int maximum_rows)
{
    if (item_count < 1) {
        return 1;
    }
    if (maximum_rows < 1) {
        return 1;
    }
    return item_count < maximum_rows ? item_count : maximum_rows;
}

bool classicsetup_tui_init(void)
{
    if (initscr() == NULL) {
        return false;
    }

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if (has_colors()) {
        start_color();
        init_pair(CLASSICSETUP_COLOR_SCREEN, COLOR_WHITE, COLOR_BLUE);
        init_pair(CLASSICSETUP_COLOR_SELECTED, COLOR_BLUE, COLOR_WHITE);
        init_pair(CLASSICSETUP_COLOR_WARNING, COLOR_YELLOW, COLOR_BLUE);
        init_pair(CLASSICSETUP_COLOR_FOOTER, COLOR_BLACK, COLOR_WHITE);
        bkgd(COLOR_PAIR(CLASSICSETUP_COLOR_SCREEN));
    }

    return true;
}

void classicsetup_tui_shutdown(void)
{
    endwin();
}

void classicsetup_tui_begin_screen(const char *title)
{
    int left = canvas_left();
    int top = canvas_top();
    int width = canvas_width();

    erase();
    attron(A_BOLD);

    if (title != NULL && canvas_height() > 1 && width > 4) {
        mvaddnstr(top, left + 1, title, width - 2);
        mvhline(top + 1, left + 1, ACS_HLINE, width - 2);
    }

    attroff(A_BOLD);
}

void classicsetup_tui_add_text(int row, int column, const char *text)
{
    int height = canvas_height();
    int width = canvas_width();

    if (text == NULL || row < 0 || row >= height || column < 0 ||
        column >= width - 1) {
        return;
    }
    mvaddnstr(
        canvas_top() + row,
        canvas_left() + column,
        text,
        width - column - 1);
}

void classicsetup_tui_draw_frame(int top, int left, int bottom, int right)
{
    if (top < 0 || left < 0 || bottom <= top || right <= left ||
        bottom >= canvas_height() - 1 || right >= canvas_width()) {
        return;
    }
    top += canvas_top();
    bottom += canvas_top();
    left += canvas_left();
    right += canvas_left();
    mvaddch(top, left, ACS_ULCORNER);
    mvhline(top, left + 1, ACS_HLINE, right - left - 1);
    mvaddch(top, right, ACS_URCORNER);
    mvvline(top + 1, left, ACS_VLINE, bottom - top - 1);
    mvvline(top + 1, right, ACS_VLINE, bottom - top - 1);
    mvaddch(bottom, left, ACS_LLCORNER);
    mvhline(bottom, left + 1, ACS_HLINE, right - left - 1);
    mvaddch(bottom, right, ACS_LRCORNER);
}

void classicsetup_tui_draw_list_row(
    int row,
    int left,
    int width,
    const char *text,
    bool selected)
{
    int usable;

    int canvas_row;
    int canvas_column;

    if (text == NULL || row < 0 || row >= canvas_height() || left < 0 ||
        left >= canvas_width() - 1 || width <= 0) {
        return;
    }
    usable = width < canvas_width() - left - 1
                 ? width
                 : canvas_width() - left - 1;
    canvas_row = canvas_top() + row;
    canvas_column = canvas_left() + left;
    if (selected) {
        attron(COLOR_PAIR(CLASSICSETUP_COLOR_SELECTED) | A_BOLD);
    }
    mvhline(canvas_row, canvas_column, ' ', usable);
    mvaddnstr(
        canvas_row,
        canvas_column + (usable > 2 ? 1 : 0),
        text,
        usable - 1);
    if (selected) {
        attroff(COLOR_PAIR(CLASSICSETUP_COLOR_SELECTED) | A_BOLD);
    }
}

void classicsetup_tui_draw_footer(const char *text)
{
    int row = canvas_top() + canvas_height() - 1;
    int left = canvas_left();
    int width = canvas_width();

    if (text == NULL || row < 0 || width <= 1) {
        return;
    }
    attron(COLOR_PAIR(CLASSICSETUP_COLOR_FOOTER) | A_BOLD);
    mvhline(row, left, ' ', width);
    mvaddnstr(row, left + 1, text, width - 2);
    attroff(COLOR_PAIR(CLASSICSETUP_COLOR_FOOTER) | A_BOLD);
}

void classicsetup_tui_draw_warning(int row, const char *text)
{
    attron(COLOR_PAIR(CLASSICSETUP_COLOR_WARNING) | A_BOLD);
    classicsetup_tui_draw_wrapped_text(
        row,
        4,
        canvas_width() - 8,
        text);
    attroff(COLOR_PAIR(CLASSICSETUP_COLOR_WARNING) | A_BOLD);
}

static int append_output_character(
    char character,
    char *output,
    size_t output_size,
    size_t *output_length)
{
    if (*output_length + 1 >= output_size) {
        return -1;
    }
    output[(*output_length)++] = character;
    output[*output_length] = '\0';
    return 0;
}

int classicsetup_tui_wrap_text(
    const char *text,
    int width,
    char *output,
    size_t output_size)
{
    const char *cursor = text;
    size_t output_length = 0;
    int line_length = 0;
    int line_count;

    if (text == NULL || width < 1 || output == NULL || output_size == 0) {
        return -1;
    }
    output[0] = '\0';
    if (*text == '\0') {
        return 0;
    }
    line_count = 1;
    while (*cursor != '\0') {
        const char *word;
        size_t word_length;

        if (*cursor == '\n') {
            if (append_output_character(
                    '\n', output, output_size, &output_length) != 0) {
                return -1;
            }
            ++cursor;
            line_length = 0;
            ++line_count;
            continue;
        }
        if (isspace((unsigned char)*cursor)) {
            ++cursor;
            continue;
        }
        word = cursor;
        while (*cursor != '\0' && *cursor != '\n' &&
               !isspace((unsigned char)*cursor)) {
            ++cursor;
        }
        word_length = (size_t)(cursor - word);
        while (word_length > 0) {
            size_t available;
            size_t take;
            size_t index;

            if (line_length > 0) {
                if (line_length >= width ||
                    (size_t)(width - line_length - 1) < word_length) {
                    if (append_output_character(
                            '\n', output, output_size, &output_length) != 0) {
                        return -1;
                    }
                    line_length = 0;
                    ++line_count;
                    continue;
                }
                if (append_output_character(
                        ' ', output, output_size, &output_length) != 0) {
                    return -1;
                }
                ++line_length;
            }
            available = (size_t)(width - line_length);
            take = word_length < available ? word_length : available;
            for (index = 0; index < take; ++index) {
                if (append_output_character(
                        word[index], output, output_size, &output_length) != 0) {
                    return -1;
                }
            }
            word += take;
            word_length -= take;
            line_length += (int)take;
            if (word_length > 0) {
                if (append_output_character(
                        '\n', output, output_size, &output_length) != 0) {
                    return -1;
                }
                line_length = 0;
                ++line_count;
            }
        }
    }
    return line_count;
}

int classicsetup_tui_draw_wrapped_text(
    int row,
    int column,
    int width,
    const char *text)
{
    char wrapped[8192];
    const char *line;
    int line_count;
    int index;

    line_count = classicsetup_tui_wrap_text(
        text,
        width,
        wrapped,
        sizeof(wrapped));
    if (line_count < 0) {
        classicsetup_tui_add_text(row, column, text);
        return 1;
    }
    line = wrapped;
    for (index = 0; index < line_count; ++index) {
        const char *end = strchr(line, '\n');
        char current[256];
        size_t length = end == NULL ? strlen(line) : (size_t)(end - line);

        if (length >= sizeof(current)) {
            length = sizeof(current) - 1;
        }
        memcpy(current, line, length);
        current[length] = '\0';
        classicsetup_tui_add_text(row + index, column, current);
        line = end == NULL ? line + length : end + 1;
    }
    return line_count;
}

void classicsetup_tui_draw_bullet(int row, const char *text)
{
    classicsetup_tui_add_text(row, 5, "*  ");
    classicsetup_tui_draw_wrapped_text(
        row,
        8,
        canvas_width() - 11,
        text);
}

void classicsetup_tui_draw_metadata(
    int row,
    const char *label,
    const char *value)
{
    if (label == NULL || value == NULL) {
        return;
    }
    classicsetup_tui_add_text(row, 4, label);
    classicsetup_tui_draw_wrapped_text(
        row,
        15,
        canvas_width() - 18,
        value);
}

void classicsetup_tui_add_centered(int row, const char *text)
{
    int text_length = (int)strlen(text);
    int width = canvas_width();
    int column = (width - text_length) / 2;
    int maximum_length;

    if (row < 0 || row >= canvas_height() || width <= 1) {
        return;
    }
    if (column < 0) {
        column = 0;
    }
    maximum_length = width - column - 1;
    if (maximum_length > 0) {
        mvaddnstr(
            canvas_top() + row,
            canvas_left() + column,
            text,
            maximum_length);
    }
}
