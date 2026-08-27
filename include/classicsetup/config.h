#ifndef CLASSICSETUP_CONFIG_H
#define CLASSICSETUP_CONFIG_H

#include <stdbool.h>

#include "classicsetup/apply.h"
#include "classicsetup/disk.h"
#include "classicsetup/format.h"
#include "classicsetup/format_apply.h"
#include "classicsetup/install_mode.h"
#include "classicsetup/partition.h"
#include "classicsetup/partition_plan.h"
#include "classicsetup/recommended.h"
#include "classicsetup/setup_mode.h"

enum {
    CLASSICSETUP_CONFIG_MAX_ORIGINAL_PARTITIONS = 64
};

enum classicsetup_keyboard_type {
    CLASSICSETUP_KEYBOARD_KOREAN_103_106,
    CLASSICSETUP_KEYBOARD_PC_AT_101,
    CLASSICSETUP_KEYBOARD_PC_AT_101_COMPATIBLE,
    CLASSICSETUP_KEYBOARD_OTHER,
    CLASSICSETUP_KEYBOARD_TYPE_COUNT
};

struct classicsetup_config {
    enum classicsetup_keyboard_type keyboard_type;
    enum classicsetup_setup_mode setup_mode;
    enum classicsetup_install_mode install_mode;
    struct classicsetup_disk_info selected_disk;
    bool has_selected_disk;
    enum classicsetup_partition_target_type partition_target_type;
    struct classicsetup_partition_info selected_partition;
    struct classicsetup_unallocated_info selected_unallocated;
    struct classicsetup_partition_info
        original_partitions[CLASSICSETUP_CONFIG_MAX_ORIGINAL_PARTITIONS];
    size_t original_partition_count;
    struct classicsetup_partition_plan partition_plan;
    bool has_partition_plan;
    struct classicsetup_plan_item selected_plan_target;
    bool has_selected_plan_target;
    struct classicsetup_format_plan selected_format_plan;
    struct classicsetup_format_plan
        role_format_plans[CLASSICSETUP_PARTITION_ROLE_COUNT];
    struct classicsetup_apply_plan apply_plan;
    bool has_apply_plan;
    struct classicsetup_apply_result apply_result;
    struct classicsetup_format_apply_plan format_apply_plan;
    bool has_format_apply_plan;
    struct classicsetup_format_result format_result;
    struct classicsetup_recommended_plan recommended_plan;
    bool has_recommended_plan;
    enum classicsetup_recommended_result_code recommended_result;
};

void classicsetup_config_set_setup_mode(
    struct classicsetup_config *config,
    enum classicsetup_setup_mode setup_mode);

void classicsetup_config_set_install_mode(
    struct classicsetup_config *config,
    enum classicsetup_install_mode install_mode);

void classicsetup_config_reset_partition_plan(
    struct classicsetup_config *config);

void classicsetup_config_clear_apply_state(
    struct classicsetup_config *config);

void classicsetup_config_clear_format_apply_state(
    struct classicsetup_config *config);

int classicsetup_config_select_plan_item(
    struct classicsetup_config *config,
    size_t item_index);

void classicsetup_config_revalidate_plan_selection(
    struct classicsetup_config *config);

int classicsetup_config_undo_windows_layout(
    struct classicsetup_config *config,
    size_t *restored_unallocated_index);

int classicsetup_config_set_format_plan(
    struct classicsetup_config *config,
    enum classicsetup_format_mode windows_mode);

int classicsetup_config_set_recommended_plan(
    struct classicsetup_config *config,
    const struct classicsetup_recommended_plan *plan);

#endif
