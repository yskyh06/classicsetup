#include "classicsetup/app.h"

#include <stdlib.h>
#include <string.h>

#include "classicsetup/after_format.h"
#include "classicsetup/apply.h"
#include "classicsetup/apply_tui.h"
#include "classicsetup/config.h"
#include "classicsetup/disk.h"
#include "classicsetup/disk_selection.h"
#include "classicsetup/format_selection.h"
#include "classicsetup/install_mode_selection.h"
#include "classicsetup/keyboard.h"
#include "classicsetup/partition.h"
#include "classicsetup/partition_selection.h"
#include "classicsetup/quit.h"
#include "classicsetup/state.h"
#include "classicsetup/tui.h"
#include "classicsetup/welcome.h"

static enum classicsetup_event show_welcome(void)
{
    enum classicsetup_welcome_result result = classicsetup_show_welcome();

    if (result == CLASSICSETUP_WELCOME_CONTINUE) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }

    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_keyboard(struct classicsetup_config *config)
{
    enum classicsetup_keyboard_result result =
        classicsetup_show_keyboard(&config->keyboard_type);

    switch (result) {
    case CLASSICSETUP_KEYBOARD_CONTINUE:
        return CLASSICSETUP_EVENT_CONTINUE;
    case CLASSICSETUP_KEYBOARD_BACK:
        return CLASSICSETUP_EVENT_BACK;
    case CLASSICSETUP_KEYBOARD_QUIT:
        return CLASSICSETUP_EVENT_QUIT_REQUEST;
    }

    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_install_mode(
    struct classicsetup_config *config)
{
    enum classicsetup_install_mode selected = config->install_mode;
    enum classicsetup_install_mode_selection_result result =
        classicsetup_show_install_mode_selection(&selected);

    switch (result) {
    case CLASSICSETUP_INSTALL_MODE_SELECTION_CONTINUE:
        classicsetup_config_set_install_mode(config, selected);
        return CLASSICSETUP_EVENT_CONTINUE;
    case CLASSICSETUP_INSTALL_MODE_SELECTION_BACK:
        return CLASSICSETUP_EVENT_BACK;
    case CLASSICSETUP_INSTALL_MODE_SELECTION_QUIT:
        return CLASSICSETUP_EVENT_QUIT_REQUEST;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_disk(struct classicsetup_config *config)
{
    enum { MAX_DISKS = 32 };
    struct classicsetup_disk_info disks[MAX_DISKS];
    enum classicsetup_disk_selection_result result;
    size_t disk_count = 0;
    size_t selected_index = 0;
    size_t index;
    bool scan_failed;

    scan_failed = classicsetup_scan_disks(disks, MAX_DISKS, &disk_count) != 0;

    if (config->has_selected_disk) {
        for (index = 0; index < disk_count; ++index) {
            if (strcmp(
                    disks[index].device_path,
                    config->selected_disk.device_path) == 0) {
                selected_index = index;
                break;
            }
        }
    }

    result = classicsetup_show_disk_selection(
        disks,
        disk_count,
        scan_failed,
        &selected_index);

    switch (result) {
    case CLASSICSETUP_DISK_SELECTION_CONTINUE:
        if (!config->has_selected_disk ||
            strcmp(
                config->selected_disk.device_path,
                disks[selected_index].device_path) != 0) {
            classicsetup_config_reset_partition_plan(config);
        }
        config->selected_disk = disks[selected_index];
        config->has_selected_disk = true;
        return CLASSICSETUP_EVENT_CONTINUE;
    case CLASSICSETUP_DISK_SELECTION_BACK:
        return CLASSICSETUP_EVENT_BACK;
    case CLASSICSETUP_DISK_SELECTION_QUIT:
        return CLASSICSETUP_EVENT_QUIT_REQUEST;
    }

    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_partition(struct classicsetup_config *config)
{
    enum classicsetup_partition_selection_result result;
    size_t selected_item = 0;
    size_t index;
    bool scan_failed = false;

    classicsetup_config_clear_apply_state(config);

    if (config->has_partition_plan &&
        !classicsetup_plan_validate(&config->partition_plan)) {
        classicsetup_config_reset_partition_plan(config);
        scan_failed = true;
    }

    if (!config->has_partition_plan && !scan_failed) {
        scan_failed = classicsetup_scan_partitions(
                          &config->selected_disk,
                          config->original_partitions,
                          CLASSICSETUP_CONFIG_MAX_ORIGINAL_PARTITIONS,
                          &config->original_partition_count) != 0;

        if (!scan_failed &&
            classicsetup_plan_init(
                &config->selected_disk,
                config->original_partitions,
                config->original_partition_count,
                &config->partition_plan) == 0) {
            config->has_partition_plan = true;
        } else if (!scan_failed) {
            scan_failed = true;
        }
    }

    if (config->has_selected_plan_target &&
        classicsetup_plan_find_matching_item(
            &config->partition_plan,
            &config->selected_plan_target,
            &index) == 0) {
        selected_item = index;
    }
    selected_item = classicsetup_plan_normalize_index(
        &config->partition_plan,
        selected_item);

    for (;;) {
        result = classicsetup_show_partition_selection(
            &config->selected_disk,
            &config->partition_plan,
            config->install_mode,
            scan_failed,
            &selected_item);
        if (result !=
            CLASSICSETUP_PARTITION_SELECTION_UNDO_WINDOWS_LAYOUT) {
            break;
        }
        if (classicsetup_config_undo_windows_layout(
                config,
                &selected_item) != 0) {
            selected_item = classicsetup_plan_normalize_index(
                &config->partition_plan,
                selected_item);
        }
    }

    if (config->has_partition_plan &&
        !classicsetup_plan_validate(&config->partition_plan)) {
        classicsetup_config_reset_partition_plan(config);
        return CLASSICSETUP_EVENT_CANCEL;
    }
    classicsetup_config_revalidate_plan_selection(config);

    switch (result) {
    case CLASSICSETUP_PARTITION_SELECTION_CONTINUE:
        if (classicsetup_config_select_plan_item(
                config,
                selected_item) != 0) {
            return CLASSICSETUP_EVENT_CANCEL;
        }
        return CLASSICSETUP_EVENT_CONTINUE;
    case CLASSICSETUP_PARTITION_SELECTION_BACK:
        return CLASSICSETUP_EVENT_BACK;
    case CLASSICSETUP_PARTITION_SELECTION_UNDO_WINDOWS_LAYOUT:
        return CLASSICSETUP_EVENT_CANCEL;
    case CLASSICSETUP_PARTITION_SELECTION_QUIT:
        return CLASSICSETUP_EVENT_QUIT_REQUEST;
    }

    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_format(struct classicsetup_config *config)
{
    enum classicsetup_format_mode mode = classicsetup_default_format_mode();
    enum classicsetup_format_selection_result result;

    if (config->selected_format_plan.valid &&
        config->selected_format_plan.mode == CLASSICSETUP_FORMAT_FULL) {
        mode = CLASSICSETUP_FORMAT_FULL;
    }

    result = classicsetup_show_format_selection(&mode);
    switch (result) {
    case CLASSICSETUP_FORMAT_SELECTION_CONTINUE:
        if (classicsetup_config_set_format_plan(config, mode) != 0) {
            return CLASSICSETUP_EVENT_CANCEL;
        }
        return CLASSICSETUP_EVENT_CONTINUE;
    case CLASSICSETUP_FORMAT_SELECTION_CANCEL:
        return CLASSICSETUP_EVENT_CANCEL;
    case CLASSICSETUP_FORMAT_SELECTION_QUIT:
        return CLASSICSETUP_EVENT_QUIT_REQUEST;
    }

    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_apply_preview(
    struct classicsetup_config *config)
{
    enum classicsetup_apply_preview_result result;

    config->has_apply_plan = classicsetup_build_apply_plan_for_mode(
        config->install_mode,
        &config->selected_disk,
        &config->partition_plan,
        config->original_partition_count,
        &config->apply_plan) == 0;
    result = classicsetup_show_apply_preview(
        &config->apply_plan,
        config->has_apply_plan);

    if (result == CLASSICSETUP_APPLY_PREVIEW_CONTINUE) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_APPLY_PREVIEW_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_apply_confirmation(
    struct classicsetup_config *config)
{
    enum classicsetup_apply_confirmation_result result;

    if (!config->has_apply_plan ||
        !classicsetup_validate_apply_plan(&config->apply_plan)) {
        return CLASSICSETUP_EVENT_BACK;
    }
    result = classicsetup_show_apply_confirmation(&config->apply_plan);
    if (result == CLASSICSETUP_APPLY_CONFIRMATION_APPLY) {
        if (classicsetup_execute_apply_plan(
                &config->apply_plan,
                &config->apply_result) != 0) {
            memset(&config->apply_result, 0, sizeof(config->apply_result));
            config->apply_result.code =
                CLASSICSETUP_APPLY_RESULT_PROCESS_FAILED;
        }
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_APPLY_CONFIRMATION_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_apply_result(
    const struct classicsetup_config *config)
{
    enum classicsetup_apply_result_screen_result result =
        classicsetup_show_apply_result(&config->apply_result);

    if (result == CLASSICSETUP_APPLY_RESULT_SCREEN_CONTINUE) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_APPLY_RESULT_SCREEN_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_after_format(
    const struct classicsetup_config *config)
{
    enum classicsetup_after_format_result result =
        classicsetup_show_after_format(
            config->apply_result.code == CLASSICSETUP_APPLY_RESULT_SUCCESS);

    if (result == CLASSICSETUP_AFTER_FORMAT_FINISH) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_AFTER_FORMAT_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

int classicsetup_run(void)
{
    struct classicsetup_config config = {
        .keyboard_type = CLASSICSETUP_KEYBOARD_KOREAN_103_106,
        .install_mode = CLASSICSETUP_INSTALL_UEFI_GPT,
        .has_selected_disk = false,
        .partition_target_type = CLASSICSETUP_PARTITION_TARGET_NONE
    };
    enum classicsetup_state state = CLASSICSETUP_STATE_WELCOME;

    if (!classicsetup_tui_init()) {
        return EXIT_FAILURE;
    }

    while (state != CLASSICSETUP_STATE_EXIT) {
        enum classicsetup_event event = CLASSICSETUP_EVENT_QUIT_REQUEST;

        if (state == CLASSICSETUP_STATE_WELCOME) {
            event = show_welcome();
        } else if (state == CLASSICSETUP_STATE_KEYBOARD) {
            event = show_keyboard(&config);
        } else if (state == CLASSICSETUP_STATE_INSTALL_MODE) {
            event = show_install_mode(&config);
        } else if (state == CLASSICSETUP_STATE_DISK) {
            event = show_disk(&config);
        } else if (state == CLASSICSETUP_STATE_PARTITION) {
            event = show_partition(&config);
        } else if (state == CLASSICSETUP_STATE_FORMAT) {
            event = show_format(&config);
        } else if (state == CLASSICSETUP_STATE_APPLY_PREVIEW) {
            event = show_apply_preview(&config);
        } else if (state == CLASSICSETUP_STATE_APPLY_CONFIRMATION) {
            event = show_apply_confirmation(&config);
        } else if (state == CLASSICSETUP_STATE_APPLY_RESULT) {
            event = show_apply_result(&config);
        } else if (state == CLASSICSETUP_STATE_AFTER_FORMAT) {
            event = show_after_format(&config);
        }

        if (event == CLASSICSETUP_EVENT_QUIT_REQUEST) {
            bool confirmed = classicsetup_confirm_quit() ==
                             CLASSICSETUP_QUIT_CONFIRM;

            state = classicsetup_resolve_quit_request(state, confirmed);
            continue;
        }
        state = classicsetup_next_state(state, event);
    }

    classicsetup_tui_shutdown();
    return EXIT_SUCCESS;
}
