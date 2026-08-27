#include "classicsetup/disk_selection.h"

#include <ncurses.h>
#include <stdio.h>

#include "classicsetup/keymap.h"
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
    int width = classicsetup_tui_canvas_width();

    if (row < 0 || row >= classicsetup_tui_canvas_height() || width <= 2) {
        return;
    }

    classicsetup_tui_draw_list_row(row, 4, width - 8, text, selected);
}

static void draw_disk_screen(
    const struct classicsetup_disk_info *disks,
    size_t disk_count,
    bool scan_failed,
    size_t selected)
{
    enum { LIST_TOP = 6, MAX_VISIBLE_ROWS = 7 };
    char line[384];
    char size_text[32];
    int detail_row;
    int visible_rows = classicsetup_tui_compact_list_height(
        (int)disk_count,
        MAX_VISIBLE_ROWS);
    int frame_bottom = LIST_TOP + visible_rows;
    size_t first = 0;
    size_t index;

    classicsetup_tui_begin_screen("ClassicSetup - Disk Selection");
    classicsetup_tui_add_text(3, 3, "The following list shows the available disks.");
    classicsetup_tui_add_text(
        4,
        3,
        "Use the UP and DOWN ARROW keys to select a disk.");
    classicsetup_tui_draw_frame(
        LIST_TOP - 1,
        2,
        frame_bottom,
        classicsetup_tui_canvas_width() - 3);
    detail_row = frame_bottom + 2;

    if (disk_count == 0) {
        classicsetup_tui_add_text(
            8,
            4,
            scan_failed ? "Disk information could not be read."
                        : "No installable disks were found.");
    } else {
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
                "Disk %zu   %-18s %12s",
                index,
                disks[index].device_path,
                size_text);
            draw_at(LIST_TOP + (int)(index - first), line, index == selected);
        }

        format_size(disks[selected].size_bytes, size_text, sizeof(size_text));
        classicsetup_tui_draw_metadata(
            detail_row,
            "Model:",
            disks[selected].model);
        classicsetup_tui_draw_metadata(
            detail_row + 1,
            "Device:",
            disks[selected].device_path);
        classicsetup_tui_draw_metadata(
            detail_row + 2,
            "Capacity:",
            size_text);
    }

    classicsetup_tui_draw_footer(
        disk_count > 0
            ? "UP/DOWN=Select    ENTER=Continue    B=Back    Q=Quit"
            : "B=Back    Q=Quit");
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
        } else if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_DISK_SELECTION_QUIT;
        }
    }
}
