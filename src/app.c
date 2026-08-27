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
#include "classicsetup/format_apply.h"
#include "classicsetup/format_apply_tui.h"
#include "classicsetup/install_mode_selection.h"
#include "classicsetup/keyboard.h"
#include "classicsetup/partition.h"
#include "classicsetup/partition_selection.h"
#include "classicsetup/quit.h"
#include "classicsetup/recommended.h"
#include "classicsetup/recommended_tui.h"
#include "classicsetup/setup_mode.h"
#include "classicsetup/setup_mode_selection.h"
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

static enum classicsetup_event show_setup_mode(
    struct classicsetup_config *config)
{
    enum classicsetup_setup_mode selected = config->setup_mode;

    if (classicsetup_show_setup_mode_selection(&selected) ==
        CLASSICSETUP_SETUP_MODE_SELECTION_CONTINUE) {
        classicsetup_config_set_setup_mode(config, selected);
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_recommended_disk(
    struct classicsetup_config *config)
{
    enum { MAX_DISKS = 32 };
    struct classicsetup_disk_info disks[MAX_DISKS];
    struct classicsetup_disk_assessment assessments[MAX_DISKS];
    struct classicsetup_recommended_plan plan;
    enum classicsetup_recommended_disk_result result;
    enum classicsetup_firmware_mode firmware =
        classicsetup_detect_firmware();
    size_t disk_count = 0;
    size_t selected = 0;
    size_t index;
    int scan_failed = classicsetup_scan_disks(
                          disks,
                          MAX_DISKS,
                          &disk_count) != 0;

    memset(assessments, 0, sizeof(assessments));
    for (index = 0; index < disk_count; ++index) {
        if (classicsetup_assess_disk(&disks[index], &assessments[index]) != 0) {
            assessments[index].disk = disks[index];
            assessments[index].disk_class = CLASSICSETUP_DISK_UNKNOWN;
        }
        if (config->has_selected_disk &&
            strcmp(
                config->selected_disk.device_path,
                disks[index].device_path) == 0) {
            selected = index;
        }
    }
    result = classicsetup_show_recommended_disk_selection(
        assessments,
        disk_count,
        firmware,
        scan_failed,
        &selected);
    if (result == CLASSICSETUP_RECOMMENDED_DISK_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    if (result == CLASSICSETUP_RECOMMENDED_DISK_QUIT) {
        return CLASSICSETUP_EVENT_QUIT_REQUEST;
    }
    if (selected >= disk_count ||
        classicsetup_build_recommended_plan(
            firmware,
            &assessments[selected].disk,
            assessments[selected].disk_class,
            &plan) != 0 ||
        classicsetup_config_set_recommended_plan(config, &plan) != 0) {
        return CLASSICSETUP_EVENT_CANCEL;
    }
    return CLASSICSETUP_EVENT_CONTINUE;
}

static enum classicsetup_event simple_result_to_event(
    enum classicsetup_simple_screen_result result)
{
    if (result == CLASSICSETUP_SIMPLE_CONTINUE) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_SIMPLE_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_recommended_gui_transition(void)
{
    return simple_result_to_event(
        classicsetup_show_recommended_gui_transition());
}

static enum classicsetup_event show_network(void)
{
    return simple_result_to_event(classicsetup_show_network_placeholder());
}

static enum classicsetup_event show_windows_version(void)
{
    return simple_result_to_event(
        classicsetup_show_windows_version_placeholder());
}

static enum classicsetup_event show_windows_download(void)
{
    return simple_result_to_event(
        classicsetup_show_windows_download_placeholder());
}

static enum classicsetup_event show_install_options(void)
{
    return simple_result_to_event(
        classicsetup_show_install_options_placeholder());
}

static enum classicsetup_event show_install_summary(
    struct classicsetup_config *config)
{
    enum classicsetup_install_summary_result result;

    if (!config->has_recommended_plan) {
        return CLASSICSETUP_EVENT_BACK;
    }
    result = classicsetup_show_install_summary(&config->recommended_plan);
    if (result == CLASSICSETUP_INSTALL_SUMMARY_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    if (result == CLASSICSETUP_INSTALL_SUMMARY_QUIT) {
        return CLASSICSETUP_EVENT_QUIT_REQUEST;
    }
    config->recommended_result = classicsetup_execute_recommended_plan(
        &config->recommended_plan,
        &config->apply_result,
        &config->format_apply_plan,
        &config->format_result);
    config->has_format_apply_plan =
        classicsetup_validate_format_apply_plan(
            &config->format_apply_plan);
    return CLASSICSETUP_EVENT_CONTINUE;
}

static enum classicsetup_event show_recommended_result(
    const struct classicsetup_config *config)
{
    enum classicsetup_simple_screen_result result =
        classicsetup_show_recommended_result(
            config->recommended_result,
            &config->apply_result,
            &config->format_result);

    if (result == CLASSICSETUP_SIMPLE_CONTINUE) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_SIMPLE_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_next_stage(void)
{
    return simple_result_to_event(classicsetup_show_next_stage_placeholder());
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
        classicsetup_config_clear_format_apply_state(config);
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

static enum classicsetup_event show_format_apply_preview(
    struct classicsetup_config *config)
{
    struct classicsetup_partition_info partitions[
        CLASSICSETUP_CONFIG_MAX_ORIGINAL_PARTITIONS];
    enum classicsetup_format_apply_preview_result result;
    size_t partition_count = 0;

    memset(&config->format_apply_plan, 0, sizeof(config->format_apply_plan));
    config->has_format_apply_plan = false;
    if (config->apply_result.code == CLASSICSETUP_APPLY_RESULT_SUCCESS &&
        classicsetup_scan_partitions(
            &config->selected_disk,
            partitions,
            CLASSICSETUP_CONFIG_MAX_ORIGINAL_PARTITIONS,
            &partition_count) == 0) {
        config->has_format_apply_plan =
            classicsetup_build_format_apply_plan(
                &config->apply_plan,
                config->role_format_plans,
                partitions,
                partition_count,
                &config->format_apply_plan) == 0;
    }
    result = classicsetup_show_format_apply_preview(
        &config->format_apply_plan,
        &config->format_result,
        config->has_format_apply_plan);
    if (result == CLASSICSETUP_FORMAT_APPLY_PREVIEW_CONTINUE) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_FORMAT_APPLY_PREVIEW_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_format_apply_confirmation(
    struct classicsetup_config *config)
{
    enum classicsetup_format_apply_confirmation_result result;

    if (!config->has_format_apply_plan ||
        !classicsetup_validate_format_apply_plan(
            &config->format_apply_plan)) {
        return CLASSICSETUP_EVENT_BACK;
    }
    result = classicsetup_show_format_apply_confirmation(
        &config->format_apply_plan);
    if (result == CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_APPLY) {
        if (classicsetup_execute_format_apply_plan(
                &config->format_apply_plan,
                &config->format_result) != 0) {
            memset(&config->format_result, 0, sizeof(config->format_result));
            config->format_result.code =
                CLASSICSETUP_FORMAT_RESULT_PROCESS_FAILED;
        }
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_BACK) {
        return CLASSICSETUP_EVENT_BACK;
    }
    return CLASSICSETUP_EVENT_QUIT_REQUEST;
}

static enum classicsetup_event show_format_apply_result(
    const struct classicsetup_config *config)
{
    enum classicsetup_format_apply_result_screen_result result =
        classicsetup_show_format_apply_result(
            &config->format_apply_plan,
            &config->format_result);

    if (result == CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_CONTINUE) {
        return CLASSICSETUP_EVENT_CONTINUE;
    }
    if (result == CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_BACK) {
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
            config->format_result.code ==
                CLASSICSETUP_FORMAT_RESULT_SUCCESS);

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
        .setup_mode = classicsetup_default_setup_mode(),
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
        } else if (state == CLASSICSETUP_STATE_SETUP_MODE) {
            event = show_setup_mode(&config);
        } else if (state == CLASSICSETUP_STATE_RECOMMENDED_GUI_TRANSITION) {
            event = show_recommended_gui_transition();
        } else if (state == CLASSICSETUP_STATE_KEYBOARD) {
            event = show_keyboard(&config);
        } else if (state == CLASSICSETUP_STATE_RECOMMENDED_DISK) {
            event = show_recommended_disk(&config);
        } else if (state == CLASSICSETUP_STATE_NETWORK) {
            event = show_network();
        } else if (state == CLASSICSETUP_STATE_WINDOWS_VERSION) {
            event = show_windows_version();
        } else if (state == CLASSICSETUP_STATE_WINDOWS_DOWNLOAD) {
            event = show_windows_download();
        } else if (state == CLASSICSETUP_STATE_INSTALL_OPTIONS) {
            event = show_install_options();
        } else if (state == CLASSICSETUP_STATE_INSTALL_SUMMARY) {
            event = show_install_summary(&config);
        } else if (state == CLASSICSETUP_STATE_RECOMMENDED_RESULT) {
            event = show_recommended_result(&config);
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
        } else if (state == CLASSICSETUP_STATE_FORMAT_APPLY_PREVIEW) {
            event = show_format_apply_preview(&config);
        } else if (state == CLASSICSETUP_STATE_FORMAT_APPLY_CONFIRMATION) {
            event = show_format_apply_confirmation(&config);
        } else if (state == CLASSICSETUP_STATE_FORMAT_APPLY_RESULT) {
            event = show_format_apply_result(&config);
        } else if (state == CLASSICSETUP_STATE_AFTER_FORMAT) {
            event = show_after_format(&config);
        } else if (state == CLASSICSETUP_STATE_NEXT_STAGE) {
            event = show_next_stage();
        }

        if (event == CLASSICSETUP_EVENT_QUIT_REQUEST) {
            bool confirmed = classicsetup_confirm_quit() ==
                             CLASSICSETUP_QUIT_CONFIRM;

            state = classicsetup_resolve_quit_request(state, confirmed);
            continue;
        }
        state = classicsetup_next_state_for_setup_mode(
            state,
            event,
            config.setup_mode);
    }

    classicsetup_tui_shutdown();
    return EXIT_SUCCESS;
}
