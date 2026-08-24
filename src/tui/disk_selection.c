#include "classicsetup/disk_selection.h"

#include <ncurses.h>
#include <stdio.h>

#include "classicsetup/tui.h"

static void format_size(
    unsigned long long size_bytes,
    char *text,
    size_t text_size)
{
    const double bytes = (double)size_bytes;

    if (size_bytes >= 1000000000000ULL) {
        snprintf(text, text_size, "%.1f TB", bytes / 1000000000000.0);
    } else if (size_bytes >= 1000000000ULL) {
        snprintf(text, text_size, "%.1f GB", bytes / 1000000000.0);
    } else if (size_bytes >= 1000000ULL) {
        snprintf(text, text_size, "%.1f MB", bytes / 1000000.0);
    } else {
        snprintf(text, text_size, "%llu bytes", size_bytes);
    }
}

static void draw_at(int row, const char *text, bool selected)
{
    if (row < 0 || row >= LINES || COLS <= 2) {
        return;
    }

    if (selected) {
        attron(A_REVERSE | A_BOLD);
    }
    mvaddnstr(row, 2, text, COLS - 2);
    if (selected) {
        attroff(A_REVERSE | A_BOLD);
    }
}

static void draw_disk_screen(
    const struct classicsetup_disk_info *disks,
    size_t disk_count,
    bool scan_failed,
    size_t selected)
{
    enum { LIST_TOP = 5 };
    char line[384];
    char size_text[32];
    int detail_row = LINES - 7;
    int visible_rows;
    size_t first = 0;
    size_t index;

    classicsetup_tui_begin_screen("ClassicSetup - Disk Selection");
    classicsetup_tui_add_centered(3, "Choose a disk for the next setup step.");

    if (disk_count == 0) {
        classicsetup_tui_add_centered(
            LINES / 2,
            scan_failed ? "Disk information could not be read."
                        : "No installable disks were found.");
    } else {
        if (detail_row < LIST_TOP + 2) {
            detail_row = LIST_TOP + 2;
        }
        visible_rows = detail_row - LIST_TOP - 1;
        if (visible_rows < 1) {
            visible_rows = 1;
        }

        if (selected >= (size_t)visible_rows) {
            first = selected - (size_t)visible_rows + 1;
        }

        for (index = first;
             index < disk_count && index < first + (size_t)visible_rows;
             ++index) {
            format_size(disks[index].size_bytes, size_text, sizeof(size_text));
            snprintf(
                line,
                sizeof(line),
                "%c Disk %zu  %s  %s",
                index == selected ? '>' : ' ',
                index,
                disks[index].device_path,
                size_text);
            draw_at(LIST_TOP + (int)(index - first), line, index == selected);
        }

        format_size(disks[selected].size_bytes, size_text, sizeof(size_text));
        snprintf(line, sizeof(line), "Model: %s", disks[selected].model);
        draw_at(detail_row, line, false);
        snprintf(line, sizeof(line), "Device: %s", disks[selected].device_path);
        draw_at(detail_row + 1, line, false);
        snprintf(line, sizeof(line), "Capacity: %s", size_text);
        draw_at(detail_row + 2, line, false);
    }

    attron(A_BOLD);
    classicsetup_tui_add_centered(
        LINES - 3,
        disk_count > 0
            ? "UP/DOWN=Select    ENTER=Continue    B=Back    F3=Quit"
            : "B=Back    F3=Quit");
    attroff(A_BOLD);
    refresh();
}

enum classicsetup_disk_selection_result classicsetup_show_disk_selection(
    const struct classicsetup_disk_info *disks,
    size_t disk_count,
    bool scan_failed,
    size_t *selected_index)
{
    size_t selected = 0;

    if (disk_count > 0 && *selected_index < disk_count) {
        selected = *selected_index;
    }

    for (;;) {
        int key;

        draw_disk_screen(disks, disk_count, scan_failed, selected);
        key = getch();

        if (key == KEY_UP && selected > 0) {
            --selected;
        } else if (key == KEY_DOWN && selected + 1 < disk_count) {
            ++selected;
        } else if (disk_count > 0 &&
                   (key == '\n' || key == '\r' || key == KEY_ENTER)) {
            *selected_index = selected;
            return CLASSICSETUP_DISK_SELECTION_CONTINUE;
        } else if (key == 'b' || key == 'B') {
            return CLASSICSETUP_DISK_SELECTION_BACK;
        } else if (key == KEY_F(3)) {
            return CLASSICSETUP_DISK_SELECTION_QUIT;
        }
    }
}
