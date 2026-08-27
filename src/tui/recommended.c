#include "classicsetup/recommended_tui.h"

#include <ncurses.h>
#include <stdio.h>

#include "classicsetup/apply.h"
#include "classicsetup/format_apply.h"
#include "classicsetup/keymap.h"
#include "classicsetup/tui.h"

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
        size_t first = 0;
        const int visible_items = classicsetup_tui_compact_list_height(
            (int)assessment_count,
            4);
        const int list_top = 6;
        const int frame_bottom = list_top + visible_items * 3;
        int key;

        if (selected >= (size_t)visible_items) {
            first = selected - (size_t)visible_items + 1;
        }

        classicsetup_tui_begin_screen("ClassicSetup - Choose a Disk");
        classicsetup_tui_add_text(
            3,
            3,
            "The following list shows the disks available for Windows installation.");
        classicsetup_tui_add_text(
            4,
            3,
            "Use the UP and DOWN ARROW keys to select a disk.");
        if (scan_failed || assessment_count == 0) {
            classicsetup_tui_add_text(
                8,
                4,
                scan_failed ? "Disk information could not be read safely."
                            : "No disks were found.");
        } else {
            int frame_right = classicsetup_tui_canvas_width() - 3;

            if (frame_bottom > list_top && frame_right > 20) {
                classicsetup_tui_draw_frame(
                    list_top - 1,
                    2,
                    frame_bottom,
                    frame_right);
            }
            for (index = first;
                 index < assessment_count &&
                 index < first + (size_t)visible_items;
                 ++index) {
                char line[256];
                char size[32];

                format_size(
                    assessments[index].disk.size_bytes,
                    size,
                    sizeof(size));
                snprintf(
                    line,
                    sizeof(line),
                    "%s    %s",
                    assessments[index].disk.model,
                    size);
                classicsetup_tui_draw_list_row(
                    list_top + (int)((index - first) * 3),
                    4,
                    classicsetup_tui_canvas_width() - 8,
                    line,
                    index == selected);
                classicsetup_tui_add_text(
                    list_top + 1 + (int)((index - first) * 3),
                    6,
                    assessments[index].presentation);
                snprintf(
                    line,
                    sizeof(line),
                    "Device: %s",
                    assessments[index].disk.device_path);
                classicsetup_tui_add_text(
                    list_top + 2 + (int)((index - first) * 3),
                    6,
                    line);
            }
        }
        if (assessment_count > 0 &&
            (!assessments[selected].selectable ||
             firmware != CLASSICSETUP_FIRMWARE_UEFI)) {
            classicsetup_tui_draw_warning(
                frame_bottom + 1 < classicsetup_tui_canvas_height() - 1
                    ? frame_bottom + 1
                    : classicsetup_tui_canvas_height() - 2,
                classicsetup_recommended_policy_reason(
                    assessments[selected].disk_class,
                    firmware));
        }
        classicsetup_tui_draw_footer(
            "UP/DOWN=Select    ENTER=Continue    B=Back    Q=Quit");
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

static enum classicsetup_simple_screen_result show_placeholder(
    const char *title,
    const char *line_one,
    const char *line_two)
{
    for (;;) {
        int key;

        classicsetup_tui_begin_screen(title);
        classicsetup_tui_add_text(3, 3, line_one);
        classicsetup_tui_add_text(5, 3, line_two);
        classicsetup_tui_draw_bullet(8, "Press ENTER to continue.");
        classicsetup_tui_draw_bullet(10, "Press B to return to the previous step.");
        classicsetup_tui_draw_footer("ENTER=Continue    B=Back    Q=Quit");
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

enum classicsetup_simple_screen_result
classicsetup_show_windows_source_placeholder(void)
{
    return classicsetup_show_windows_download_placeholder();
}

enum classicsetup_simple_screen_result
classicsetup_show_recommended_gui_transition(void)
{
    return show_placeholder(
        "ClassicSetup - Recommended Installation",
        "Recommended installation is separated from the Advanced TUI.",
        "A GTK frontend will take ownership at this boundary in a future milestone.");
}

enum classicsetup_simple_screen_result
classicsetup_show_network_placeholder(void)
{
    return show_placeholder(
        "ClassicSetup - Network",
        "Network selection will be provided by the Recommended GUI.",
        "No network configuration is changed in this milestone.");
}

enum classicsetup_simple_screen_result
classicsetup_show_windows_version_placeholder(void)
{
    return show_placeholder(
        "ClassicSetup - Windows Version",
        "Windows 10 and Windows 11 selection will be added later.",
        "No source has been selected or downloaded.");
}

enum classicsetup_simple_screen_result
classicsetup_show_windows_download_placeholder(void)
{
    return show_placeholder(
        "ClassicSetup - Windows Download",
        "Windows source discovery and download will be added later.",
        "No ISO, WIM, or ESD operation is performed.");
}

enum classicsetup_simple_screen_result
classicsetup_show_install_options_placeholder(void)
{
    return show_placeholder(
        "ClassicSetup - Installation Options",
        "Recommended unattended installation options will be added later.",
        "The current storage plan continues to use safe automatic defaults.");
}

enum classicsetup_install_summary_result classicsetup_show_install_summary(
    const struct classicsetup_recommended_plan *plan)
{
    for (;;) {
        char line[256];
        char size[32];
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Ready to Install");
        classicsetup_tui_add_text(4, 4, "Windows source: Not selected (placeholder)");
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
            classicsetup_tui_add_text(7, 6, line);
            classicsetup_tui_add_text(9, 6, "Installation mode: UEFI / GPT");
        }
        classicsetup_tui_add_text(
            11,
            6,
            "ClassicSetup will create the required Windows partitions,");
        classicsetup_tui_add_text(
            12,
            6,
            "format the Windows partition as NTFS Quick, and verify the disk.");
        classicsetup_tui_draw_warning(15, "WARNING: All data on this disk will be erased.");
        classicsetup_tui_draw_footer("A=Install    B=Back    Q=Quit");
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
            classicsetup_tui_add_text(
                5,
                4,
                "The GPT layout and filesystems were applied and verified.");
            classicsetup_tui_add_text(
                7,
                4,
                "Windows image application is not implemented yet.");
        } else if (result_code == CLASSICSETUP_RECOMMENDED_BLOCKED) {
            classicsetup_tui_add_text(
                6,
                4,
                "Recommended installation was blocked by a safety check.");
        } else if (result_code ==
                   CLASSICSETUP_RECOMMENDED_PARTITION_FAILED) {
            classicsetup_tui_add_text(
                6,
                4,
                partition_result == NULL ||
                        partition_result->code !=
                            CLASSICSETUP_APPLY_RESULT_BLOCKED
                    ? "Partition apply failed."
                    : classicsetup_apply_safety_message(
                          partition_result->safety_code));
        } else if (result_code == CLASSICSETUP_RECOMMENDED_FORMAT_FAILED) {
            classicsetup_tui_add_text(
                6,
                4,
                format_result == NULL ||
                        format_result->code !=
                            CLASSICSETUP_FORMAT_RESULT_BLOCKED
                    ? "Filesystem apply failed."
                    : classicsetup_format_safety_message(
                          format_result->safety_code));
        } else {
            classicsetup_tui_add_text(
                6,
                4,
                "The filesystem apply plan could not be built safely.");
        }
        classicsetup_tui_draw_footer(
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
classicsetup_show_next_stage_placeholder(void)
{
    for (;;) {
        int key;

        classicsetup_tui_begin_screen("ClassicSetup - Next Stage");
        classicsetup_tui_add_text(
            4,
            4,
            "The TUI-to-GUI transition boundary is ready.");
        classicsetup_tui_add_text(
            6,
            4,
            "GTK and Windows image application are not implemented yet.");
        classicsetup_tui_draw_footer("ENTER=Finish    B=Back    Q=Quit");
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
