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
        attron(A_BOLD);
        classicsetup_tui_add_centered(
            3,
            "WARNING: The planned GPT layout will erase partition information");
        classicsetup_tui_add_centered(4, "on the selected test disk.");
        attroff(A_BOLD);

        if (!valid || apply_plan == NULL) {
            classicsetup_tui_add_centered(
                LINES / 2,
                "This layout is not eligible for the restricted M7 apply path.");
            classicsetup_tui_add_centered(
                LINES - 3,
                "B=Back    Q=Quit");
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
            classicsetup_tui_add_centered(6, line);
            classicsetup_tui_add_centered(8, "Planned GPT layout:");
            for (index = 0;
                 index < apply_plan->partition_count &&
                 10 + (int)index < LINES - 5;
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
                classicsetup_tui_add_centered(10 + (int)index, line);
            }
            classicsetup_tui_add_centered(
                LINES - 5,
                "No changes have been written yet.");
            attron(A_BOLD);
            classicsetup_tui_add_centered(
                LINES - 3,
                "ENTER=Continue    B=Back    Q=Quit");
            attroff(A_BOLD);
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
        attron(A_BOLD);
        classicsetup_tui_add_centered(LINES / 2 - 5, "WARNING");
        classicsetup_tui_add_centered(
            LINES / 2 - 3,
            "ALL EXISTING PARTITION DATA ON THIS TEST DISK WILL BE LOST:");
        attroff(A_BOLD);
        snprintf(
            line,
            sizeof(line),
            "%s  %s",
            apply_plan->target_disk.device_path,
            apply_plan->target_disk.model);
        classicsetup_tui_add_centered(LINES / 2 - 1, line);
        classicsetup_tui_add_centered(LINES / 2, disk_size);
        classicsetup_tui_add_centered(
            LINES / 2 + 2,
            "Press A to apply the GPT partition changes.");
        attron(A_BOLD);
        classicsetup_tui_add_centered(
            LINES - 3,
            "A=Apply    B=Back    Q=Quit");
        attroff(A_BOLD);
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
        message = "The GPT partition layout was applied and verified.";
    } else if (result->code == CLASSICSETUP_APPLY_RESULT_BLOCKED) {
        message = classicsetup_apply_safety_message(result->safety_code);
    } else if (result->code == CLASSICSETUP_APPLY_RESULT_PROCESS_FAILED) {
        message = "sfdisk failed. No automatic rollback was attempted.";
    } else {
        message = "The resulting partition layout could not be verified.";
    }

    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Apply Result");
        classicsetup_tui_add_centered(LINES / 2 - 2, message);
        if (!success) {
            classicsetup_tui_add_centered(
                LINES / 2,
                "Review the safety checks before trying again.");
        } else {
            classicsetup_tui_add_centered(
                LINES / 2,
                "No filesystem has been created in this milestone.");
        }
        attron(A_BOLD);
        classicsetup_tui_add_centered(
            LINES - 3,
            success ? "ENTER=Continue    B=Back    Q=Quit"
                    : "B=Back    Q=Quit");
        attroff(A_BOLD);
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
