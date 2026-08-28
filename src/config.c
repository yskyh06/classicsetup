#include "classicsetup/config.h"

#include <stdio.h>
#include <string.h>

static void clear_format_plans(struct classicsetup_config *config)
{
    memset(&config->selected_format_plan, 0, sizeof(config->selected_format_plan));
    memset(config->role_format_plans, 0, sizeof(config->role_format_plans));
    config->advanced_storage_plan_ready = false;
}

void classicsetup_config_set_setup_mode(
    struct classicsetup_config *config,
    enum classicsetup_setup_mode setup_mode)
{
    if (config == NULL || setup_mode < CLASSICSETUP_SETUP_RECOMMENDED ||
        setup_mode >= CLASSICSETUP_SETUP_MODE_COUNT ||
        config->setup_mode == setup_mode) {
        return;
    }
    config->setup_mode = setup_mode;
    config->install_mode = CLASSICSETUP_INSTALL_UEFI_GPT;
    memset(&config->selected_disk, 0, sizeof(config->selected_disk));
    config->has_selected_disk = false;
    classicsetup_config_reset_partition_plan(config);
}

void classicsetup_config_set_install_mode(
    struct classicsetup_config *config,
    enum classicsetup_install_mode install_mode)
{
    if (config == NULL || install_mode < CLASSICSETUP_INSTALL_UEFI_GPT ||
        install_mode >= CLASSICSETUP_INSTALL_MODE_COUNT ||
        config->install_mode == install_mode) {
        return;
    }
    config->install_mode = install_mode;
    classicsetup_config_reset_partition_plan(config);
}

void classicsetup_config_clear_apply_state(struct classicsetup_config *config)
{
    memset(&config->apply_plan, 0, sizeof(config->apply_plan));
    config->has_apply_plan = false;
    memset(&config->apply_result, 0, sizeof(config->apply_result));
    config->recommended_result = CLASSICSETUP_RECOMMENDED_NOT_RUN;
    classicsetup_config_clear_format_apply_state(config);
}

void classicsetup_config_clear_format_apply_state(
    struct classicsetup_config *config)
{
    memset(&config->format_apply_plan, 0, sizeof(config->format_apply_plan));
    config->has_format_apply_plan = false;
    memset(&config->format_result, 0, sizeof(config->format_result));
}

void classicsetup_config_reset_partition_plan(
    struct classicsetup_config *config)
{
    config->original_partition_count = 0;
    memset(&config->partition_plan, 0, sizeof(config->partition_plan));
    config->has_partition_plan = false;
    config->partition_target_type = CLASSICSETUP_PARTITION_TARGET_NONE;
    memset(&config->selected_partition, 0, sizeof(config->selected_partition));
    memset(&config->selected_unallocated, 0, sizeof(config->selected_unallocated));
    memset(&config->selected_plan_target, 0, sizeof(config->selected_plan_target));
    config->has_selected_plan_target = false;
    memset(&config->recommended_plan, 0, sizeof(config->recommended_plan));
    config->has_recommended_plan = false;
    clear_format_plans(config);
    classicsetup_config_clear_apply_state(config);
}

int classicsetup_config_select_plan_item(
    struct classicsetup_config *config,
    size_t item_index)
{
    const struct classicsetup_plan_item *item;

    if (config == NULL || !config->has_partition_plan ||
        !classicsetup_plan_validate(&config->partition_plan) ||
        item_index >= config->partition_plan.item_count) {
        return -1;
    }

    item = &config->partition_plan.items[item_index];
    if (!classicsetup_plan_item_is_install_target(item)) {
        return -1;
    }

    config->selected_plan_target = *item;
    config->has_selected_plan_target = true;
    clear_format_plans(config);
    classicsetup_config_clear_apply_state(config);

    if (item->state == CLASSICSETUP_PLAN_EXISTING) {
        memset(&config->selected_partition, 0, sizeof(config->selected_partition));
        snprintf(
            config->selected_partition.name,
            sizeof(config->selected_partition.name),
            "%s",
            item->name);
        snprintf(
            config->selected_partition.device_path,
            sizeof(config->selected_partition.device_path),
            "%s",
            item->device_path);
        config->selected_partition.number = item->number;
        config->selected_partition.start_sector = item->start_sector;
        config->selected_partition.sector_count = item->sector_count;
        config->selected_partition.size_bytes = item->size_bytes;
        config->partition_target_type =
            CLASSICSETUP_PARTITION_TARGET_EXISTING;
    } else if (item->state == CLASSICSETUP_PLAN_NEW) {
        config->partition_target_type = CLASSICSETUP_PARTITION_TARGET_NEW;
    } else {
        return -1;
    }

    return 0;
}

void classicsetup_config_revalidate_plan_selection(
    struct classicsetup_config *config)
{
    size_t index;

    if (config == NULL || !config->has_selected_plan_target) {
        return;
    }

    if (classicsetup_plan_find_matching_item(
            &config->partition_plan,
            &config->selected_plan_target,
            &index) == 0 &&
        classicsetup_plan_item_is_install_target(
            &config->partition_plan.items[index])) {
        config->selected_plan_target = config->partition_plan.items[index];
        return;
    }

    memset(&config->selected_plan_target, 0, sizeof(config->selected_plan_target));
    config->has_selected_plan_target = false;
    config->partition_target_type = CLASSICSETUP_PARTITION_TARGET_NONE;
    clear_format_plans(config);
}

int classicsetup_config_undo_windows_layout(
    struct classicsetup_config *config,
    size_t *restored_unallocated_index)
{
    if (config == NULL || restored_unallocated_index == NULL ||
        !config->has_partition_plan ||
        classicsetup_plan_undo_windows_layout_for_mode(
            &config->partition_plan,
            config->install_mode,
            restored_unallocated_index) != 0) {
        return -1;
    }

    classicsetup_config_revalidate_plan_selection(config);
    clear_format_plans(config);
    classicsetup_config_clear_apply_state(config);
    return 0;
}

bool classicsetup_config_advanced_plan_is_ready(
    const struct classicsetup_config *config)
{
    size_t selected_index;

    if (config == NULL ||
        config->setup_mode != CLASSICSETUP_SETUP_ADVANCED ||
        !config->advanced_storage_plan_ready ||
        !config->has_selected_disk || !config->has_partition_plan ||
        !classicsetup_plan_validate(&config->partition_plan) ||
        !config->has_selected_plan_target ||
        !classicsetup_plan_item_is_install_target(
            &config->selected_plan_target) ||
        classicsetup_plan_find_matching_item(
            &config->partition_plan,
            &config->selected_plan_target,
            &selected_index) != 0 ||
        !config->selected_format_plan.valid ||
        config->selected_format_plan.filesystem != CLASSICSETUP_FS_NTFS ||
        (config->selected_format_plan.mode != CLASSICSETUP_FORMAT_QUICK &&
         config->selected_format_plan.mode != CLASSICSETUP_FORMAT_FULL)) {
        return false;
    }
    return true;
}

int classicsetup_config_set_format_plan(
    struct classicsetup_config *config,
    enum classicsetup_format_mode windows_mode)
{
    struct classicsetup_format_plan selected;
    struct classicsetup_format_plan
        role_plans[CLASSICSETUP_PARTITION_ROLE_COUNT] = {0};
    size_t index;

    if (config == NULL || !config->has_partition_plan ||
        !classicsetup_plan_validate(&config->partition_plan) ||
        !config->has_selected_plan_target ||
        !classicsetup_plan_item_is_install_target(
            &config->selected_plan_target) ||
        classicsetup_format_policy_for_role(
            config->selected_plan_target.role,
            windows_mode,
            &selected) != 0) {
        return -1;
    }

    for (index = 0; index < config->partition_plan.item_count; ++index) {
        const struct classicsetup_plan_item *item =
            &config->partition_plan.items[index];
        enum classicsetup_partition_role role = item->role;

        if (item->state == CLASSICSETUP_PLAN_DELETED ||
            item->state == CLASSICSETUP_PLAN_UNALLOCATED ||
            role <= CLASSICSETUP_PARTITION_ROLE_NONE ||
            role == CLASSICSETUP_PARTITION_ROLE_GENERIC ||
            role >= CLASSICSETUP_PARTITION_ROLE_COUNT) {
            continue;
        }
        if (classicsetup_format_policy_for_role(
                role,
                windows_mode,
                &role_plans[role]) != 0) {
            return -1;
        }
    }

    config->selected_format_plan = selected;
    memcpy(
        config->role_format_plans,
        role_plans,
        sizeof(config->role_format_plans));
    classicsetup_config_clear_apply_state(config);
    config->advanced_storage_plan_ready = true;
    return 0;
}

int classicsetup_config_set_recommended_plan(
    struct classicsetup_config *config,
    const struct classicsetup_recommended_plan *plan)
{
    if (config == NULL || plan == NULL ||
        config->setup_mode != CLASSICSETUP_SETUP_RECOMMENDED ||
        plan->install_mode != CLASSICSETUP_INSTALL_UEFI_GPT ||
        !classicsetup_plan_validate(&plan->partition_plan) ||
        !classicsetup_plan_item_is_install_target(&plan->selected_target) ||
        !plan->selected_format_plan.valid ||
        plan->apply_plan.partition_count == 0) {
        return -1;
    }
    classicsetup_config_reset_partition_plan(config);
    config->install_mode = plan->install_mode;
    config->selected_disk = plan->apply_plan.target_disk;
    config->has_selected_disk = true;
    config->partition_plan = plan->partition_plan;
    config->has_partition_plan = true;
    config->selected_plan_target = plan->selected_target;
    config->has_selected_plan_target = true;
    config->partition_target_type = CLASSICSETUP_PARTITION_TARGET_NEW;
    config->selected_format_plan = plan->selected_format_plan;
    memcpy(
        config->role_format_plans,
        plan->role_format_plans,
        sizeof(config->role_format_plans));
    config->apply_plan = plan->apply_plan;
    config->has_apply_plan = true;
    config->recommended_plan = *plan;
    config->has_recommended_plan = true;
    return 0;
}
