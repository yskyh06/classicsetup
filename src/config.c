#include "classicsetup/config.h"

#include <stdio.h>
#include <string.h>

static void clear_format_plans(struct classicsetup_config *config)
{
    memset(&config->selected_format_plan, 0, sizeof(config->selected_format_plan));
    memset(config->role_format_plans, 0, sizeof(config->role_format_plans));
}

void classicsetup_config_clear_apply_state(struct classicsetup_config *config)
{
    memset(&config->apply_plan, 0, sizeof(config->apply_plan));
    config->has_apply_plan = false;
    memset(&config->apply_result, 0, sizeof(config->apply_result));
}

void classicsetup_config_reset_partition_plan(
    struct classicsetup_config *config)
{
    config->original_partition_count = 0;
    memset(&config->partition_plan, 0, sizeof(config->partition_plan));
    config->has_partition_plan = false;
    config->partition_target_type = CLASSICSETUP_PARTITION_TARGET_NONE;
    memset(&config->selected_plan_target, 0, sizeof(config->selected_plan_target));
    config->has_selected_plan_target = false;
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
        classicsetup_plan_undo_windows_layout(
            &config->partition_plan,
            restored_unallocated_index) != 0) {
        return -1;
    }

    classicsetup_config_revalidate_plan_selection(config);
    clear_format_plans(config);
    classicsetup_config_clear_apply_state(config);
    return 0;
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
    return 0;
}
