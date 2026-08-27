#include "classicsetup/apply_tui.h"

#include <ncurses.h>
#include <stdio.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

static void format_binary_size(
    unsigned long long bytes,
    char *text,
    size_t text_size)
{
    const double value = (double)bytes;

    if (bytes >= 1024ULL * 1024ULL * 1024ULL) {
        snprintf(
            text,
            text_size,
            "%.1f GiB",
            value / (1024.0 * 1024.0 * 1024.0));
    } else {
        snprintf(text, text_size, "%.1f MiB", value / (1024.0 * 1024.0));
    }
}

static const char *preview_role_name(enum classicsetup_partition_role role)
{
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_EFI:
        return "EFI System";
    case CLASSICSETUP_PARTITION_ROLE_MSR:
        return "Microsoft Reserved";
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        return "Windows";
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        return "Recovery";
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
        return "System Reserved";
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        return "Unsupported";
    }
    return "Unknown";
}

enum classicsetup_apply_preview_result classicsetup_show_apply_preview(
    const struct classicsetup_apply_plan *apply_plan,
    int valid)
{
    for (;;) {
        size_t index;

        classicsetup_tui_begin_screen("ClassicSetup - Disk Changes");
        classicsetup_tui_draw_warning(
            3,
            "WARNING: The planned partition layout will erase partition information");
        classicsetup_tui_draw_wrapped_text(
            4,
            4,
            classicsetup_tui_canvas_width() - 8,
            "on the selected test disk.");

        if (!valid || apply_plan == NULL) {
            classicsetup_tui_draw_wrapped_text(
                7,
                4,
                classicsetup_tui_canvas_width() - 8,
                "This layout is not eligible for the restricted M7 apply path.");
            classicsetup_tui_draw_footer("B=Back    Q=Quit");
        } else {
            char line[384];
            char disk_size[32];

            format_binary_size(
                apply_plan->target_disk.size_bytes,
                disk_size,
                sizeof(disk_size));
            snprintf(
                line,
                sizeof(line),
                "Disk: %s  %s  %s",
                apply_plan->target_disk.device_path,
                disk_size,
                apply_plan->target_disk.model);
            classicsetup_tui_add_text(6, 4, line);
            snprintf(
                line,
                sizeof(line),
                "Partition table: %s    Firmware mode: %s",
                apply_plan->table_type == CLASSICSETUP_PARTITION_TABLE_MBR
                    ? "MBR"
                    : "GPT",
                apply_plan->table_type == CLASSICSETUP_PARTITION_TABLE_MBR
                    ? "Legacy BIOS"
                    : "UEFI");
            classicsetup_tui_add_text(7, 4, line);
            classicsetup_tui_add_text(9, 4, "Planned partition layout:");
            for (index = 0;
                 index < apply_plan->partition_count &&
                 10 + (int)index < classicsetup_tui_canvas_height() - 3;
                 ++index) {
                char size[32];

                format_binary_size(
                    apply_plan->partitions[index].sector_count *
                        CLASSICSETUP_SECTOR_SIZE_BYTES,
                    size,
                    sizeof(size));
                snprintf(
                    line,
                    sizeof(line),
                    "%zu  %-20s %12s",
                    index + 1,
                    preview_role_name(apply_plan->partitions[index].role),
                    size);
                classicsetup_tui_add_text(10 + (int)index, 6, line);
            }
            classicsetup_tui_add_text(16, 4, "No changes have been written yet.");
            classicsetup_tui_draw_footer(
                "ENTER=Continue    B=Back    Q=Quit");
        }
        refresh();

        {
            int key = getch();

            if (valid &&
                (key == '\n' || key == '\r' || key == KEY_ENTER)) {
                return CLASSICSETUP_APPLY_PREVIEW_CONTINUE;
            }
            if (key == 'b' || key == 'B') {
                return CLASSICSETUP_APPLY_PREVIEW_BACK;
            }
            if (classicsetup_key_is_quit(key)) {
                return CLASSICSETUP_APPLY_PREVIEW_QUIT;
            }
        }
    }
}

enum classicsetup_apply_confirmation_result
classicsetup_show_apply_confirmation(
    const struct classicsetup_apply_plan *apply_plan)
{
    char disk_size[32];
    char line[384];

    format_binary_size(
        apply_plan->target_disk.size_bytes,
        disk_size,
        sizeof(disk_size));

    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Confirm Disk Changes");
        classicsetup_tui_draw_warning(3, "WARNING");
        classicsetup_tui_draw_wrapped_text(
            5,
            4,
            classicsetup_tui_canvas_width() - 8,
            "ALL EXISTING PARTITION DATA ON THIS TEST DISK WILL BE LOST:");
        snprintf(
            line,
            sizeof(line),
            "%s  %s",
            apply_plan->target_disk.device_path,
            apply_plan->target_disk.model);
        classicsetup_tui_add_text(7, 4, line);
        classicsetup_tui_add_text(8, 4, disk_size);
        classicsetup_tui_draw_wrapped_text(
            10,
            4,
            classicsetup_tui_canvas_width() - 8,
            "Press A to apply the partition table changes.");
        classicsetup_tui_draw_footer("A=Apply    B=Back    Q=Quit");
        refresh();

        key = getch();
        if (classicsetup_key_is_apply(key)) {
            return CLASSICSETUP_APPLY_CONFIRMATION_APPLY;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_APPLY_CONFIRMATION_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_APPLY_CONFIRMATION_QUIT;
        }
    }
}

enum classicsetup_apply_result_screen_result classicsetup_show_apply_result(
    const struct classicsetup_apply_result *result)
{
    int success = result->code == CLASSICSETUP_APPLY_RESULT_SUCCESS;
    const char *message;

    if (success) {
        message = "The partition layout was applied and verified.";
    } else if (result->code == CLASSICSETUP_APPLY_RESULT_BLOCKED) {
        message = classicsetup_apply_safety_message(result->safety_code);
    } else if (result->code == CLASSICSETUP_APPLY_RESULT_PROCESS_FAILED) {
        message = "sfdisk failed. No automatic rollback was attempted.";
    } else {
        message = "The resulting partition layout could not be verified.";
    }

    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Partition Apply Result");
        classicsetup_tui_add_text(4, 4, message);
        if (!success) {
            classicsetup_tui_draw_wrapped_text(
                6,
                4,
                classicsetup_tui_canvas_width() - 8,
                "Review the safety checks before trying again.");
        } else {
            classicsetup_tui_draw_wrapped_text(
                6,
                4,
                classicsetup_tui_canvas_width() - 8,
                "Filesystem creation is the next confirmed step.");
        }
        classicsetup_tui_draw_footer(
            success ? "ENTER=Continue    B=Back    Q=Quit"
                    : "B=Back    Q=Quit");
        refresh();

        key = getch();
        if (success &&
            (key == '\n' || key == '\r' || key == KEY_ENTER)) {
            return CLASSICSETUP_APPLY_RESULT_SCREEN_CONTINUE;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_APPLY_RESULT_SCREEN_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_APPLY_RESULT_SCREEN_QUIT;
        }
    }
}
