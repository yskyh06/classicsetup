#include "classicsetup/partition_selection.h"

#include <ctype.h>
#include <errno.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

enum modal_result {
    MODAL_ACCEPT,
    MODAL_CANCEL,
    MODAL_QUIT
};

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

    if (row < 0 || row >= classicsetup_tui_canvas_height() || width <= 4) {
        return;
    }

    classicsetup_tui_draw_list_row(row, 4, width - 8, text, selected);
}

static const char *state_tag(enum classicsetup_plan_item_state state)
{
    switch (state) {
    case CLASSICSETUP_PLAN_EXISTING:
        return "[EXISTING]";
    case CLASSICSETUP_PLAN_NEW:
        return "[NEW]";
    case CLASSICSETUP_PLAN_DELETED:
        return "[DELETE]";
    case CLASSICSETUP_PLAN_UNALLOCATED:
        return "";
    }

    return "[UNKNOWN]";
}

static const char *item_display_name(
    const struct classicsetup_plan_item *item,
    char *text,
    size_t text_size)
{
    switch (item->role) {
    case CLASSICSETUP_PARTITION_ROLE_EFI:
        return "EFI System Partition";
    case CLASSICSETUP_PARTITION_ROLE_MSR:
        return "Microsoft Reserved";
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        return "Windows Partition";
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        return "Recovery Partition";
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
        return "System Reserved Partition";
    case CLASSICSETUP_PARTITION_ROLE_NONE:
        return "Unallocated Space";
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
        if (item->number > 0) {
            snprintf(text, text_size, "Partition %u", item->number);
        } else {
            snprintf(text, text_size, "New Partition");
        }
        return text;
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        break;
    }

    return "Unknown Item";
}

static void draw_plan_item(
    int row,
    const struct classicsetup_plan_item *item,
    bool selected)
{
    char line[384];
    char name[80];
    char size_text[32];
    const char *display_name = item_display_name(item, name, sizeof(name));

    format_size(item->size_bytes, size_text, sizeof(size_text));
    snprintf(
        line,
        sizeof(line),
        "%c %-30s %12s  %s",
        selected ? '>' : ' ',
        display_name,
        size_text,
        state_tag(item->state));
    draw_at(row, line, selected);
}

struct partition_screen_layout {
    int instruction_row;
    int disk_row;
    int list_top;
    int list_end;
    int detail_row;
};

static struct partition_screen_layout calculate_screen_layout(
    int rows,
    size_t item_count)
{
    int visible_rows = classicsetup_tui_compact_list_height(
        (int)item_count,
        8);
    struct partition_screen_layout layout = {
        .instruction_row = 3,
        .disk_row = 6,
        .list_top = 8,
        .list_end = 8 + visible_rows,
        .detail_row = 10 + visible_rows
    };

    if (rows < 18) {
        layout.instruction_row = -1;
        layout.disk_row = 3;
        layout.list_top = 5;
        visible_rows = classicsetup_tui_compact_list_height(
            (int)item_count,
            rows - 8);
        layout.list_end = layout.list_top + visible_rows;
        layout.detail_row = -1;
    }
    return layout;
}

static void draw_partition_footer(
    const struct partition_screen_layout *layout,
    bool has_windows_layout)
{
    (void)layout;
    classicsetup_tui_draw_footer(
        has_windows_layout
            ? "ENTER=Install  C=Create  D=Delete  U=Undo Layout  B=Back  Q=Quit"
            : "ENTER=Install  C=Create  D=Delete  B=Back  Q=Quit");
}

static void draw_partition_screen(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_plan *plan,
    enum classicsetup_install_mode install_mode,
    bool scan_failed,
    size_t selected)
{
    struct partition_screen_layout layout = calculate_screen_layout(
        classicsetup_tui_canvas_height(),
        plan->item_count);
    char line[320];
    char size_text[32];
    int visible_rows = layout.list_end - layout.list_top;
    size_t first = 0;
    size_t item;

    classicsetup_tui_begin_screen("ClassicSetup - Partition Selection");
    format_size(disk->size_bytes, size_text, sizeof(size_text));
    if (layout.instruction_row >= 0) {
        classicsetup_tui_draw_wrapped_text(
            layout.instruction_row,
            3,
            classicsetup_tui_canvas_width() - 6,
            "The list shows existing partitions and unallocated disk space.");
        classicsetup_tui_draw_wrapped_text(
            layout.instruction_row + 1,
            3,
            classicsetup_tui_canvas_width() - 6,
            "Use the UP and DOWN ARROW keys to select an item.");
    }
    snprintf(
        line,
        sizeof(line),
        "Disk 0  %s  %s  %s  [%s]",
        disk->device_path,
        size_text,
        disk->model[0] != '\0' ? disk->model : "Unknown model",
        install_mode == CLASSICSETUP_INSTALL_BIOS_MBR
            ? "Legacy BIOS/MBR"
            : "UEFI/GPT");
    draw_at(layout.disk_row, line, false);
    if (layout.list_end > layout.list_top &&
        classicsetup_tui_canvas_width() > 12) {
        classicsetup_tui_draw_frame(
            layout.list_top - 1,
            2,
            layout.list_end,
            classicsetup_tui_canvas_width() - 3);
    }

    if (plan->item_count == 0) {
        if (classicsetup_tui_canvas_height() >= 6) {
            classicsetup_tui_add_text(
                9,
                4,
                scan_failed ? "Partition information could not be read."
                            : "No selectable partition space was found.");
        }
    } else if (visible_rows > 0) {
        if (selected >= (size_t)visible_rows) {
            first = selected - (size_t)visible_rows + 1;
        }

        for (item = first;
             item < plan->item_count &&
             item < first + (size_t)visible_rows;
             ++item) {
            draw_plan_item(
                layout.list_top + (int)(item - first),
                &plan->items[item],
                item == selected);
        }
    }

    if (layout.detail_row >= 0 && selected < plan->item_count) {
        char selected_name[80];
        const struct classicsetup_plan_item *selected_item =
            &plan->items[selected];

        snprintf(
            line,
            sizeof(line),
            "Selected: %s  Start sector: %llu",
            item_display_name(
                selected_item,
                selected_name,
                sizeof(selected_name)),
            selected_item->start_sector);
        draw_at(layout.detail_row, line, false);
    }

    attron(A_BOLD);
    if (plan->item_count > 0) {
        draw_partition_footer(
            &layout,
            classicsetup_plan_has_windows_layout_for_mode(
                plan,
                install_mode));
    } else {
        classicsetup_tui_draw_footer("B=Back  Q=Quit");
    }
    attroff(A_BOLD);
    refresh();
}

static enum modal_result prompt_create_size(
    const struct classicsetup_plan_item *space,
    unsigned long long *size_mb)
{
    char input[21];
    char line[96];
    char available[32];
    char message[96] = "";
    unsigned long long maximum_mb =
        classicsetup_plan_max_size_mb(space->sector_count);
    size_t length;
    bool replace_on_digit = true;

    format_size(space->size_bytes, available, sizeof(available));
    snprintf(input, sizeof(input), "%llu", maximum_mb);
    length = strlen(input);

    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Create Partition");
        classicsetup_tui_draw_wrapped_text(
            4,
            4,
            classicsetup_tui_canvas_width() - 8,
            "Create a planned partition in the selected free space.");
        snprintf(
            line,
            sizeof(line),
            "Maximum size: %llu MB (%s available)",
            maximum_mb,
            available);
        classicsetup_tui_add_text(6, 4, line);
        classicsetup_tui_add_text(8, 4, "Partition size in MB:");
        snprintf(line, sizeof(line), "[ %s ]", input);
        classicsetup_tui_add_text(9, 6, line);
        classicsetup_tui_draw_warning(11, message);
        classicsetup_tui_draw_footer(
            "ENTER=Create    BACKSPACE=Edit    ESC=Cancel    Q=Quit");
        refresh();

        key = getch();
        if (key >= '0' && key <= '9') {
            if (replace_on_digit) {
                length = 0;
                input[0] = '\0';
                replace_on_digit = false;
            }
            if (length + 1 < sizeof(input)) {
                input[length++] = (char)key;
                input[length] = '\0';
                message[0] = '\0';
            } else {
                snprintf(message, sizeof(message), "The number is too long.");
            }
        } else if (key == KEY_BACKSPACE || key == KEY_DC ||
                   key == 127 || key == '\b') {
            replace_on_digit = false;
            if (length > 0) {
                input[--length] = '\0';
            }
            message[0] = '\0';
        } else if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            char *end;
            unsigned long long parsed;
            unsigned long long sectors;

            errno = 0;
            parsed = strtoull(input, &end, 10);
            if (length == 0 || errno != 0 || end == input || *end != '\0') {
                snprintf(message, sizeof(message), "Enter a valid number.");
            } else if (classicsetup_plan_size_mb_to_sectors(
                           parsed,
                           space->sector_count,
                           &sectors) != 0) {
                snprintf(
                    message,
                    sizeof(message),
                    "Size must be greater than zero and fit the free space.");
            } else {
                *size_mb = parsed;
                return MODAL_ACCEPT;
            }
        } else if (key == 27) {
            return MODAL_CANCEL;
        } else if (classicsetup_key_is_quit(key)) {
            return MODAL_QUIT;
        } else if (isprint((unsigned char)key)) {
            snprintf(message, sizeof(message), "Digits only.");
        }
    }
}

static enum modal_result show_windows_layout_error(void)
{
    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Windows Layout");
        classicsetup_tui_draw_wrapped_text(
            4,
            4,
            classicsetup_tui_canvas_width() - 8,
            "The selected space is too small or invalid for the current policy.");
        classicsetup_tui_add_text(
            6,
            4,
            "The planned layout was not changed.");
        classicsetup_tui_draw_footer(
            "ENTER=Return    ESC=Return    Q=Quit");
        refresh();

        key = getch();
        if (key == '\n' || key == '\r' || key == KEY_ENTER || key == 27) {
            return MODAL_CANCEL;
        }
        if (classicsetup_key_is_quit(key)) {
            return MODAL_QUIT;
        }
    }
}

static enum modal_result confirm_delete(
    const struct classicsetup_plan_item *item)
{
    char line[320];
    char size_text[32];

    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Delete Partition");
        classicsetup_tui_draw_wrapped_text(
            4,
            4,
            classicsetup_tui_canvas_width() - 8,
            item->state == CLASSICSETUP_PLAN_NEW
                ? "Remove this new partition from the plan?"
                : "Mark this existing partition for deletion?");
        if (item->state == CLASSICSETUP_PLAN_EXISTING) {
            snprintf(line, sizeof(line), "Device: %s", item->device_path);
            classicsetup_tui_add_text(6, 4, line);
        }
        format_size(item->size_bytes, size_text, sizeof(size_text));
        snprintf(line, sizeof(line), "Size: %s", size_text);
        classicsetup_tui_add_text(7, 4, line);
        attron(A_BOLD);
        classicsetup_tui_add_text(9, 4, "Press D again to confirm.");
        attroff(A_BOLD);
        classicsetup_tui_draw_footer(
            "D=Delete    ESC=Cancel    Q=Quit");
        refresh();

        key = getch();
        if (key == 'd' || key == 'D') {
            return MODAL_ACCEPT;
        }
        if (key == 27) {
            return MODAL_CANCEL;
        }
        if (classicsetup_key_is_quit(key)) {
            return MODAL_QUIT;
        }
    }
}

static enum modal_result confirm_undo_windows_layout(void)
{
    for (;;) {
        int key;

        classicsetup_tui_begin_screen(
            "ClassicSetup - Undo Windows Layout");
        classicsetup_tui_draw_wrapped_text(
            4,
            4,
            classicsetup_tui_canvas_width() - 8,
            "The automatically created Windows partitions will be removed");
        classicsetup_tui_add_text(
            5,
            4,
            "from the planned layout.");
        classicsetup_tui_add_text(
            7,
            4,
            "No changes have been written to disk.");
        attron(A_BOLD);
        classicsetup_tui_add_text(
            9,
            4,
            "Press U again to confirm.");
        attroff(A_BOLD);
        classicsetup_tui_draw_footer(
            "U=Confirm    ESC=Cancel    Q=Quit");
        refresh();

        key = getch();
        if (key == 'u' || key == 'U') {
            return MODAL_ACCEPT;
        }
        if (key == 27) {
            return MODAL_CANCEL;
        }
        if (classicsetup_key_is_quit(key)) {
            return MODAL_QUIT;
        }
    }
}

static size_t find_unallocated_at(
    const struct classicsetup_partition_plan *plan,
    unsigned long long sector)
{
    size_t index;

    for (index = 0; index < plan->item_count; ++index) {
        const struct classicsetup_plan_item *item = &plan->items[index];

        if (item->state == CLASSICSETUP_PLAN_UNALLOCATED &&
            sector >= item->start_sector &&
            sector - item->start_sector < item->sector_count) {
            return index;
        }
    }
    return plan->item_count;
}

enum classicsetup_partition_selection_result
classicsetup_show_partition_selection(
    const struct classicsetup_disk_info *disk,
    struct classicsetup_partition_plan *plan,
    enum classicsetup_install_mode install_mode,
    bool scan_failed,
    size_t *selected_item)
{
    size_t selected = 0;

    if (plan->item_count > 0 && *selected_item < plan->item_count) {
        selected = *selected_item;
    }

    for (;;) {
        const struct classicsetup_plan_item *item;
        int key;

        selected = classicsetup_plan_normalize_index(plan, selected);
        draw_partition_screen(
            disk,
            plan,
            install_mode,
            scan_failed,
            selected);
        key = getch();

        if (key == KEY_UP && selected > 0) {
            --selected;
        } else if (key == KEY_DOWN && selected + 1 < plan->item_count) {
            ++selected;
        } else if (plan->item_count > 0 &&
                   (key == '\n' || key == '\r' || key == KEY_ENTER)) {
            bool was_unallocated = plan->items[selected].state ==
                                   CLASSICSETUP_PLAN_UNALLOCATED;
            size_t target_index;

            if (classicsetup_plan_prepare_install_target_for_mode(
                    plan,
                    install_mode,
                    selected,
                    &target_index) == 0) {
                selected = classicsetup_plan_normalize_index(
                    plan,
                    target_index);
                *selected_item = selected;
                return CLASSICSETUP_PARTITION_SELECTION_CONTINUE;
            }
            if (was_unallocated) {
                enum modal_result modal = show_windows_layout_error();

                if (modal == MODAL_QUIT) {
                    return CLASSICSETUP_PARTITION_SELECTION_QUIT;
                }
            } else {
                beep();
            }
        } else if (plan->item_count > 0 && (key == 'c' || key == 'C')) {
            item = &plan->items[selected];
            if (item->state == CLASSICSETUP_PLAN_UNALLOCATED) {
                unsigned long long size_mb;
                enum modal_result modal = prompt_create_size(item, &size_mb);

                if (modal == MODAL_QUIT) {
                    return CLASSICSETUP_PARTITION_SELECTION_QUIT;
                }
                if (modal == MODAL_ACCEPT &&
                    classicsetup_plan_create_partition(
                        plan,
                        selected,
                        size_mb,
                        &selected) != 0) {
                    beep();
                }
                selected = classicsetup_plan_normalize_index(plan, selected);
            }
        } else if (plan->item_count > 0 && (key == 'd' || key == 'D')) {
            item = &plan->items[selected];
            if (item->state == CLASSICSETUP_PLAN_EXISTING ||
                item->state == CLASSICSETUP_PLAN_NEW) {
                unsigned long long start_sector = item->start_sector;
                enum modal_result modal = confirm_delete(item);

                if (modal == MODAL_QUIT) {
                    return CLASSICSETUP_PARTITION_SELECTION_QUIT;
                }
                if (modal == MODAL_ACCEPT) {
                    if (classicsetup_plan_delete_partition(plan, selected) == 0) {
                        selected = find_unallocated_at(plan, start_sector);
                    } else {
                        beep();
                    }
                    selected = classicsetup_plan_normalize_index(plan, selected);
                }
            }
        } else if (key == 'u' || key == 'U') {
            if (classicsetup_plan_has_windows_layout_for_mode(
                    plan,
                    install_mode)) {
                enum modal_result modal = confirm_undo_windows_layout();

                if (modal == MODAL_QUIT) {
                    return CLASSICSETUP_PARTITION_SELECTION_QUIT;
                }
                if (modal == MODAL_ACCEPT) {
                    *selected_item = selected;
                    return CLASSICSETUP_PARTITION_SELECTION_UNDO_WINDOWS_LAYOUT;
                }
            } else {
                beep();
            }
        } else if (key == 'b' || key == 'B') {
            return CLASSICSETUP_PARTITION_SELECTION_BACK;
        } else if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_PARTITION_SELECTION_QUIT;
        }
    }
}
