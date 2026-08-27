#include "classicsetup/recommended_tui.h"

#include <ncurses.h>
#include <stdio.h>

#include "classicsetup/apply.h"
#include "classicsetup/format_apply.h"
#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

static const char *disk_class_name(enum classicsetup_disk_class disk_class)
{
    switch (disk_class) {
    case CLASSICSETUP_DISK_EMPTY:
        return "Empty disk";
    case CLASSICSETUP_DISK_HAS_UNALLOCATED_SPACE:
        return "Contains partitions and unallocated space";
    case CLASSICSETUP_DISK_HAS_EXISTING_PARTITIONS:
        return "Contains existing partitions";
    case CLASSICSETUP_DISK_SYSTEM:
        return "Running Linux system disk - not available";
    case CLASSICSETUP_DISK_INSTALL_MEDIA:
        return "ClassicSetup installation media - not available";
    case CLASSICSETUP_DISK_REMOVABLE:
        return "Removable disk - Advanced only";
    case CLASSICSETUP_DISK_UNKNOWN:
        return "Identity or safety status unknown";
    }
    return "Unknown";
}

static void format_size(
    unsigned long long bytes,
    char *text,
    size_t text_size)
{
    if (bytes >= 1099511627776ULL) {
        snprintf(text, text_size, "%.1f TiB", (double)bytes / 1099511627776.0);
    } else if (bytes >= 1073741824ULL) {
        snprintf(text, text_size, "%.1f GiB", (double)bytes / 1073741824.0);
    } else {
        snprintf(text, text_size, "%.1f MiB", (double)bytes / 1048576.0);
    }
}

enum classicsetup_recommended_disk_result
classicsetup_show_recommended_disk_selection(
    const struct classicsetup_disk_assessment *assessments,
    size_t assessment_count,
    enum classicsetup_firmware_mode firmware,
    int scan_failed,
    size_t *selected_index)
{
    size_t selected = selected_index != NULL &&
                              *selected_index < assessment_count
                          ? *selected_index
                          : 0;

    for (;;) {
        size_t index;
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Choose a Disk");
        classicsetup_tui_add_centered(
            3,
            firmware == CLASSICSETUP_FIRMWARE_UEFI
                ? "Recommended installation uses UEFI with GPT."
                : firmware == CLASSICSETUP_FIRMWARE_BIOS
                      ? "Legacy BIOS automatic installation is not enabled."
                      : "Firmware mode could not be identified safely.");
        if (scan_failed || assessment_count == 0) {
            classicsetup_tui_add_centered(
                LINES / 2,
                scan_failed ? "Disk information could not be read safely."
                            : "No disks were found.");
        } else {
            for (index = 0;
                 index < assessment_count && 6 + (int)(index * 3) < LINES - 5;
                 ++index) {
                char line[256];
                char size[32];

                format_size(
                    assessments[index].disk.size_bytes,
                    size,
                    sizeof(size));
                if (index == selected) {
                    attron(A_REVERSE | A_BOLD);
                }
                snprintf(
                    line,
                    sizeof(line),
                    "%c %s    %s",
                    index == selected ? '>' : ' ',
                    assessments[index].disk.model,
                    size);
                classicsetup_tui_add_centered(6 + (int)(index * 3), line);
                if (index == selected) {
                    attroff(A_REVERSE | A_BOLD);
                }
                classicsetup_tui_add_centered(
                    7 + (int)(index * 3),
                    disk_class_name(assessments[index].disk_class));
                snprintf(
                    line,
                    sizeof(line),
                    "Device: %s",
                    assessments[index].disk.device_path);
                classicsetup_tui_add_centered(8 + (int)(index * 3), line);
            }
        }
        attron(A_BOLD);
        if (assessment_count > 0 &&
            (!assessments[selected].selectable ||
             firmware != CLASSICSETUP_FIRMWARE_UEFI)) {
            classicsetup_tui_add_centered(
                LINES - 5,
                "Choose another disk, or press B and select Advanced installation.");
        }
        classicsetup_tui_add_centered(
            LINES - 3,
            "UP/DOWN=Select    ENTER=Continue    B=Back    Q=Quit");
        attroff(A_BOLD);
        refresh();

        key = getch();
        if (key == KEY_UP && selected > 0) {
            --selected;
        } else if (key == KEY_DOWN && selected + 1 < assessment_count) {
            ++selected;
        } else if ((key == '\n' || key == '\r' || key == KEY_ENTER) &&
                   assessment_count > 0) {
            if (firmware == CLASSICSETUP_FIRMWARE_UEFI &&
                assessments[selected].selectable) {
                if (selected_index != NULL) {
                    *selected_index = selected;
                }
                return CLASSICSETUP_RECOMMENDED_DISK_CONTINUE;
            }
            beep();
        } else if (key == 'b' || key == 'B') {
            return CLASSICSETUP_RECOMMENDED_DISK_BACK;
        } else if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_RECOMMENDED_DISK_QUIT;
        }
    }
}

enum classicsetup_simple_screen_result
classicsetup_show_windows_source_placeholder(void)
{
    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Windows Source");
        classicsetup_tui_add_centered(
            LINES / 2 - 1,
            "Windows download and image selection will be added next.");
        classicsetup_tui_add_centered(
            LINES / 2 + 1,
            "M9 uses a placeholder source only.");
        classicsetup_tui_add_centered(
            LINES - 3,
            "ENTER=Continue    B=Back    Q=Quit");
        refresh();
        key = getch();
        if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            return CLASSICSETUP_SIMPLE_CONTINUE;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_SIMPLE_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_SIMPLE_QUIT;
        }
    }
}

enum classicsetup_install_summary_result classicsetup_show_install_summary(
    const struct classicsetup_recommended_plan *plan)
{
    for (;;) {
        char line[256];
        char size[32];
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Ready to Install");
        classicsetup_tui_add_centered(3, "Windows source: Placeholder");
        if (plan != NULL) {
            format_size(
                plan->apply_plan.target_disk.size_bytes,
                size,
                sizeof(size));
            snprintf(
                line,
                sizeof(line),
                "Target: %s    %s",
                plan->apply_plan.target_disk.model,
                size);
            classicsetup_tui_add_centered(6, line);
            classicsetup_tui_add_centered(8, "Installation mode: UEFI / GPT");
        }
        classicsetup_tui_add_centered(
            11,
            "ClassicSetup will create the required Windows partitions,");
        classicsetup_tui_add_centered(
            12,
            "format the Windows partition as NTFS Quick, and verify the disk.");
        attron(A_BOLD);
        classicsetup_tui_add_centered(15, "WARNING: All data on this disk will be erased.");
        classicsetup_tui_add_centered(
            LINES - 3,
            "A=Install    B=Back    Q=Quit");
        attroff(A_BOLD);
        refresh();

        key = getch();
        if (classicsetup_key_is_apply(key)) {
            return CLASSICSETUP_INSTALL_SUMMARY_INSTALL;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_INSTALL_SUMMARY_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_INSTALL_SUMMARY_QUIT;
        }
    }
}

enum classicsetup_simple_screen_result classicsetup_show_recommended_result(
    enum classicsetup_recommended_result_code result_code,
    const struct classicsetup_apply_result *partition_result,
    const struct classicsetup_format_result *format_result)
{
    const int success =
        classicsetup_recommended_result_can_continue(result_code);

    for (;;) {
        int key;

        classicsetup_tui_begin_screen(
            success ? "ClassicSetup - Storage Preparation Complete"
                    : "ClassicSetup - Installation Result");
        if (success) {
            classicsetup_tui_add_centered(
                LINES / 2 - 1,
                "The GPT layout and filesystems were applied and verified.");
            classicsetup_tui_add_centered(
                LINES / 2 + 1,
                "Windows image application is not implemented yet.");
        } else if (result_code == CLASSICSETUP_RECOMMENDED_BLOCKED) {
            classicsetup_tui_add_centered(
                LINES / 2,
                "Recommended installation was blocked by a safety check.");
        } else if (result_code ==
                   CLASSICSETUP_RECOMMENDED_PARTITION_FAILED) {
            classicsetup_tui_add_centered(
                LINES / 2,
                partition_result == NULL ||
                        partition_result->code !=
                            CLASSICSETUP_APPLY_RESULT_BLOCKED
                    ? "Partition apply failed."
                    : classicsetup_apply_safety_message(
                          partition_result->safety_code));
        } else if (result_code == CLASSICSETUP_RECOMMENDED_FORMAT_FAILED) {
            classicsetup_tui_add_centered(
                LINES / 2,
                format_result == NULL ||
                        format_result->code !=
                            CLASSICSETUP_FORMAT_RESULT_BLOCKED
                    ? "Filesystem apply failed."
                    : classicsetup_format_safety_message(
                          format_result->safety_code));
        } else {
            classicsetup_tui_add_centered(
                LINES / 2,
                "The filesystem apply plan could not be built safely.");
        }
        classicsetup_tui_add_centered(
            LINES - 3,
            success ? "ENTER=Continue    B=Back    Q=Quit"
                    : "B=Back    Q=Quit");
        refresh();
        key = getch();
        if (success && (key == '\n' || key == '\r' || key == KEY_ENTER)) {
            return CLASSICSETUP_SIMPLE_CONTINUE;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_SIMPLE_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_SIMPLE_QUIT;
        }
    }
}

enum classicsetup_simple_screen_result
classicsetup_show_gui_transition_placeholder(void)
{
    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Next Stage");
        classicsetup_tui_add_centered(
            LINES / 2 - 1,
            "The TUI-to-GUI transition boundary is ready.");
        classicsetup_tui_add_centered(
            LINES / 2 + 1,
            "GTK and Windows image application are not implemented yet.");
        classicsetup_tui_add_centered(
            LINES - 3,
            "ENTER=Finish    B=Back    Q=Quit");
        refresh();
        key = getch();
        if (key == '\n' || key == '\r' || key == KEY_ENTER) {
            return CLASSICSETUP_SIMPLE_CONTINUE;
        }
        if (key == 'b' || key == 'B') {
            return CLASSICSETUP_SIMPLE_BACK;
        }
        if (classicsetup_key_is_quit(key)) {
            return CLASSICSETUP_SIMPLE_QUIT;
        }
    }
}
