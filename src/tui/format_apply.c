#include "classicsetup/format_apply_tui.h"

#include <ncurses.h>
#include <stdio.h>

#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

static const char *role_name(enum classicsetup_partition_role role)
{
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_EFI:
        return "EFI System Partition";
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        return "Windows Partition";
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        return "Recovery Partition";
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
        return "System Reserved Partition";
    case CLASSICSETUP_PARTITION_ROLE_MSR:
        return "Microsoft Reserved Partition";
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        return "Unsupported Partition";
    }
    return "Unknown Partition";
}

static const char *filesystem_name(enum classicsetup_filesystem_type filesystem)
{
    if (filesystem == CLASSICSETUP_FS_FAT32) {
        return "FAT32";
    }
    if (filesystem == CLASSICSETUP_FS_NTFS) {
        return "NTFS";
    }
    return "None";
}

static const char *mode_name(enum classicsetup_format_mode mode)
{
    return mode == CLASSICSETUP_FORMAT_FULL ? "Full" : "Quick";
}

enum classicsetup_format_apply_preview_result
classicsetup_show_format_apply_preview(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_result *previous_result,
    int valid)
{
    for (;;) {
        size_t index;

        classicsetup_tui_begin_screen("ClassicSetup - Format Partitions");
        classicsetup_tui_add_centered(
            3,
            "The following filesystems will be created:");
        if (!valid || plan == NULL) {
            classicsetup_tui_add_centered(
                LINES / 2,
                "The filesystem apply plan could not be built safely.");
            classicsetup_tui_draw_footer("B=Back    Q=Quit");
        } else {
            char line[384];

            snprintf(
                line,
                sizeof(line),
                "Disk: %s    Partition table: %s",
                plan->partition_apply_plan.target_disk.device_path,
                plan->partition_apply_plan.table_type ==
                        CLASSICSETUP_PARTITION_TABLE_MBR
                    ? "MBR"
                    : "GPT");
            classicsetup_tui_add_centered(5, line);
            for (index = 0;
                 index < plan->partition_count &&
                 7 + (int)(index * 2) < LINES - 7;
                 ++index) {
                const struct classicsetup_format_partition *partition =
                    &plan->partitions[index];

                snprintf(
                    line,
                    sizeof(line),
                    "%s  %s",
                    role_name(partition->role),
                    partition->device_path);
                classicsetup_tui_add_centered(
                    7 + (int)(index * 2),
                    line);
                snprintf(
                    line,
                    sizeof(line),
                    "%s %s",
                    filesystem_name(partition->filesystem),
                    mode_name(partition->mode));
                classicsetup_tui_add_centered(
                    8 + (int)(index * 2),
                    line);
            }
            if (plan->partition_apply_plan.table_type ==
                CLASSICSETUP_PARTITION_TABLE_GPT) {
                classicsetup_tui_add_centered(
                    LINES - 6,
                    "Microsoft Reserved will not be formatted.");
            }
            classicsetup_tui_add_centered(
                LINES - 5,
                previous_result != NULL &&
                        previous_result->code !=
                            CLASSICSETUP_FORMAT_RESULT_NOT_RUN &&
                        previous_result->code !=
                            CLASSICSETUP_FORMAT_RESULT_BLOCKED
                    ? "WARNING: Some partitions may already be formatted."
                    : "No filesystem changes have been written yet.");
            classicsetup_tui_draw_footer(
                "ENTER=Continue    B=Back    Q=Quit");
        }
        refresh();

        {
            int key = getch();

            if (valid &&
                (key == '\n' || key == '\r' || key == KEY_ENTER)) {
                return CLASSICSETUP_FORMAT_APPLY_PREVIEW_CONTINUE;
            }
            if (key == 'b' || key == 'B') {
                return CLASSICSETUP_FORMAT_APPLY_PREVIEW_BACK;
            }
            if (classicsetup_key_is_quit(key)) {
                return CLASSICSETUP_FORMAT_APPLY_PREVIEW_QUIT;
            }
        }
    }
}

enum classicsetup_format_apply_confirmation_result
classicsetup_show_format_apply_confirmation(
    const struct classicsetup_format_apply_plan *plan)
{
    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Confirm Format");
        attron(A_BOLD);
        classicsetup_tui_add_centered(LINES / 2 - 5, "WARNING");
        classicsetup_tui_add_centered(
            LINES / 2 - 3,
            "Existing data on the listed partitions will be destroyed.");
        attroff(A_BOLD);
        if (plan != NULL) {
            classicsetup_tui_add_centered(
                LINES / 2 - 1,
                plan->partition_apply_plan.target_disk.device_path);
        }
        classicsetup_tui_add_centered(
            LINES / 2 + 2,
            "Press A to create the planned filesystems.");
        classicsetup_tui_draw_footer("A=Apply    B=Back    Q=Quit");
        refresh();

        key = getch();
        if (classicsetup_key_is_apply(key)) {
            return CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_APPLY;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_QUIT;
        }
    }
}

enum classicsetup_format_apply_result_screen_result
classicsetup_show_format_apply_result(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_result *result)
{
    int success = result != NULL &&
                  result->code == CLASSICSETUP_FORMAT_RESULT_SUCCESS;

    for (;;) {
        int key;

        classicsetup_tui_begin_screen(
            success ? "ClassicSetup - Format Complete"
                    : "ClassicSetup - Format Result");
        if (success && plan != NULL) {
            size_t index;

            for (index = 0;
                 index < plan->partition_count &&
                 LINES / 2 - 4 + (int)index < LINES - 5;
                 ++index) {
                char line[256];

                snprintf(
                    line,
                    sizeof(line),
                    "%s %s: verified",
                    role_name(plan->partitions[index].role),
                    filesystem_name(plan->partitions[index].filesystem));
                classicsetup_tui_add_centered(
                    LINES / 2 - 4 + (int)index,
                    line);
            }
            classicsetup_tui_add_centered(
                LINES / 2 + 1,
                "The filesystems were created and verified successfully.");
        } else if (result != NULL) {
            char line[256];

            if (result->code == CLASSICSETUP_FORMAT_RESULT_BLOCKED) {
                classicsetup_tui_add_centered(
                    LINES / 2 - 2,
                    classicsetup_format_safety_message(result->safety_code));
            } else if (result->code ==
                       CLASSICSETUP_FORMAT_RESULT_PROCESS_FAILED) {
                snprintf(
                    line,
                    sizeof(line),
                    "The formatter failed for %s.",
                    role_name(result->failed_role));
                classicsetup_tui_add_centered(LINES / 2 - 2, line);
            } else {
                snprintf(
                    line,
                    sizeof(line),
                    "Filesystem verification failed for %s.",
                    role_name(result->failed_role));
                classicsetup_tui_add_centered(LINES / 2 - 2, line);
            }
            snprintf(
                line,
                sizeof(line),
                "%zu partition(s) were completed before the failure.",
                result->completed_count);
            classicsetup_tui_add_centered(LINES / 2, line);
            classicsetup_tui_add_centered(
                LINES / 2 + 1,
                "The disk may now be partially formatted; no rollback was attempted.");
        }
        classicsetup_tui_draw_footer(
            success ? "ENTER=Continue    B=Back    Q=Quit"
                    : "B=Back    Q=Quit");
        refresh();

        key = getch();
        if (success &&
            (key == '\n' || key == '\r' || key == KEY_ENTER)) {
            return CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_CONTINUE;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_QUIT;
        }
    }
}
