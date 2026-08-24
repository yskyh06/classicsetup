#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "classicsetup/config.h"
#include "classicsetup/format.h"
#include "classicsetup/partition_plan.h"

static size_t find_role(
    const struct classicsetup_partition_plan *plan,
    enum classicsetup_partition_role role)
{
    size_t index;

    for (index = 0; index < plan->item_count; ++index) {
        if (plan->items[index].role == role) {
            return index;
        }
    }
    return plan->item_count;
}

static void assert_policy(
    enum classicsetup_partition_role role,
    enum classicsetup_format_mode selected_mode,
    enum classicsetup_filesystem_type expected_filesystem,
    enum classicsetup_format_mode expected_mode)
{
    struct classicsetup_format_plan plan = {0};

    assert(classicsetup_format_policy_for_role(
               role,
               selected_mode,
               &plan) == 0);
    assert(plan.valid);
    assert(plan.filesystem == expected_filesystem);
    assert(plan.mode == expected_mode);
}

static void test_default_and_option_movement(void)
{
    assert(classicsetup_default_format_mode() == CLASSICSETUP_FORMAT_QUICK);
    assert(classicsetup_format_move_option(
               CLASSICSETUP_FORMAT_QUICK,
               1) == CLASSICSETUP_FORMAT_FULL);
    assert(classicsetup_format_move_option(
               CLASSICSETUP_FORMAT_FULL,
               -1) == CLASSICSETUP_FORMAT_QUICK);
    assert(classicsetup_format_move_option(
               CLASSICSETUP_FORMAT_FULL,
               0) == CLASSICSETUP_FORMAT_FULL);
}

static void test_role_policies(void)
{
    struct classicsetup_format_plan invalid = {0};

    assert_policy(
        CLASSICSETUP_PARTITION_ROLE_EFI,
        CLASSICSETUP_FORMAT_FULL,
        CLASSICSETUP_FS_FAT32,
        CLASSICSETUP_FORMAT_QUICK);
    assert_policy(
        CLASSICSETUP_PARTITION_ROLE_MSR,
        CLASSICSETUP_FORMAT_FULL,
        CLASSICSETUP_FS_NONE,
        CLASSICSETUP_FORMAT_NONE);
    assert_policy(
        CLASSICSETUP_PARTITION_ROLE_WINDOWS,
        CLASSICSETUP_FORMAT_FULL,
        CLASSICSETUP_FS_NTFS,
        CLASSICSETUP_FORMAT_FULL);
    assert_policy(
        CLASSICSETUP_PARTITION_ROLE_RECOVERY,
        CLASSICSETUP_FORMAT_FULL,
        CLASSICSETUP_FS_NTFS,
        CLASSICSETUP_FORMAT_QUICK);
    assert_policy(
        CLASSICSETUP_PARTITION_ROLE_GENERIC,
        CLASSICSETUP_FORMAT_QUICK,
        CLASSICSETUP_FS_NTFS,
        CLASSICSETUP_FORMAT_QUICK);

    assert(classicsetup_format_policy_for_role(
               CLASSICSETUP_PARTITION_ROLE_NONE,
               CLASSICSETUP_FORMAT_QUICK,
               &invalid) == -1);
    assert(!invalid.valid);
    assert(classicsetup_format_policy_for_role(
               CLASSICSETUP_PARTITION_ROLE_WINDOWS,
               CLASSICSETUP_FORMAT_NONE,
               &invalid) == -1);
}

static void test_install_target_policy(void)
{
    struct classicsetup_plan_item item = {0};

    item.state = CLASSICSETUP_PLAN_EXISTING;
    item.role = CLASSICSETUP_PARTITION_ROLE_GENERIC;
    assert(classicsetup_plan_item_is_install_target(&item));
    item.state = CLASSICSETUP_PLAN_NEW;
    assert(classicsetup_plan_item_is_install_target(&item));
    item.role = CLASSICSETUP_PARTITION_ROLE_WINDOWS;
    assert(classicsetup_plan_item_is_install_target(&item));

    item.role = CLASSICSETUP_PARTITION_ROLE_EFI;
    assert(!classicsetup_plan_item_is_install_target(&item));
    item.role = CLASSICSETUP_PARTITION_ROLE_MSR;
    assert(!classicsetup_plan_item_is_install_target(&item));
    item.role = CLASSICSETUP_PARTITION_ROLE_RECOVERY;
    assert(!classicsetup_plan_item_is_install_target(&item));
    item.state = CLASSICSETUP_PLAN_DELETED;
    item.role = CLASSICSETUP_PARTITION_ROLE_GENERIC;
    assert(!classicsetup_plan_item_is_install_target(&item));
    item.state = CLASSICSETUP_PLAN_UNALLOCATED;
    item.role = CLASSICSETUP_PARTITION_ROLE_NONE;
    assert(!classicsetup_plan_item_is_install_target(&item));
    assert(!classicsetup_plan_item_is_install_target(NULL));
}

static void test_config_stores_automatic_role_plans(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_config config = {0};
    size_t free_index;
    size_t windows_index;

    disk.size_bytes = 4096ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(
               &disk,
               NULL,
               0,
               &config.partition_plan) == 0);
    config.has_partition_plan = true;
    free_index = find_role(
        &config.partition_plan,
        CLASSICSETUP_PARTITION_ROLE_NONE);
    assert(free_index < config.partition_plan.item_count);
    assert(classicsetup_plan_create_windows_layout(
               &config.partition_plan,
               free_index,
               &windows_index) == 0);
    assert(classicsetup_config_select_plan_item(
               &config,
               windows_index) == 0);
    assert(config.selected_plan_target.role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    config.has_apply_plan = true;
    config.apply_result.code = CLASSICSETUP_APPLY_RESULT_SUCCESS;
    assert(classicsetup_config_set_format_plan(
               &config,
               CLASSICSETUP_FORMAT_FULL) == 0);

    assert(!config.has_apply_plan);
    assert(config.apply_result.code == CLASSICSETUP_APPLY_RESULT_NOT_RUN);

    assert(config.selected_format_plan.valid);
    assert(config.selected_format_plan.filesystem == CLASSICSETUP_FS_NTFS);
    assert(config.selected_format_plan.mode == CLASSICSETUP_FORMAT_FULL);
    assert(config.role_format_plans[CLASSICSETUP_PARTITION_ROLE_EFI].filesystem ==
           CLASSICSETUP_FS_FAT32);
    assert(config.role_format_plans[CLASSICSETUP_PARTITION_ROLE_EFI].mode ==
           CLASSICSETUP_FORMAT_QUICK);
    assert(config.role_format_plans[CLASSICSETUP_PARTITION_ROLE_MSR].valid);
    assert(config.role_format_plans[CLASSICSETUP_PARTITION_ROLE_MSR].filesystem ==
           CLASSICSETUP_FS_NONE);
    assert(config.role_format_plans[CLASSICSETUP_PARTITION_ROLE_MSR].mode ==
           CLASSICSETUP_FORMAT_NONE);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_WINDOWS].mode ==
           CLASSICSETUP_FORMAT_FULL);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_RECOVERY].filesystem ==
           CLASSICSETUP_FS_NTFS);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_RECOVERY].mode ==
           CLASSICSETUP_FORMAT_QUICK);
}

static void test_config_rejects_non_install_targets(void)
{
    enum classicsetup_partition_role blocked_roles[] = {
        CLASSICSETUP_PARTITION_ROLE_EFI,
        CLASSICSETUP_PARTITION_ROLE_MSR,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY,
        CLASSICSETUP_PARTITION_ROLE_NONE
    };
    struct classicsetup_config config = {0};
    size_t index;

    config.has_partition_plan = true;
    config.partition_plan.item_count = 1;
    config.partition_plan.items[0].state = CLASSICSETUP_PLAN_NEW;

    for (index = 0;
         index < sizeof(blocked_roles) / sizeof(blocked_roles[0]);
         ++index) {
        config.partition_plan.items[0].role = blocked_roles[index];
        if (blocked_roles[index] == CLASSICSETUP_PARTITION_ROLE_NONE) {
            config.partition_plan.items[0].state =
                CLASSICSETUP_PLAN_UNALLOCATED;
        }
        assert(classicsetup_config_select_plan_item(&config, 0) == -1);
        config.partition_plan.items[0].state = CLASSICSETUP_PLAN_NEW;
    }

    config.partition_plan.items[0].state = CLASSICSETUP_PLAN_DELETED;
    config.partition_plan.items[0].role = CLASSICSETUP_PARTITION_ROLE_GENERIC;
    assert(classicsetup_config_select_plan_item(&config, 0) == -1);
}

static void test_disk_change_reset_clears_stale_plan(void)
{
    struct classicsetup_config config = {0};

    config.has_partition_plan = true;
    config.partition_plan.disk_sector_count = 1000;
    config.partition_plan.item_count = 1;
    config.partition_plan.items[0].state = CLASSICSETUP_PLAN_NEW;
    config.partition_plan.items[0].role =
        CLASSICSETUP_PARTITION_ROLE_WINDOWS;
    config.partition_plan.items[0].sector_count = 1000;
    config.partition_plan.items[0].size_bytes =
        1000ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    config.selected_plan_target = config.partition_plan.items[0];
    config.has_selected_plan_target = true;
    config.selected_format_plan.valid = true;
    config.role_format_plans[
        CLASSICSETUP_PARTITION_ROLE_WINDOWS].valid = true;
    config.has_apply_plan = true;
    config.apply_result.code = CLASSICSETUP_APPLY_RESULT_SUCCESS;

    classicsetup_config_reset_partition_plan(&config);
    assert(!config.has_partition_plan);
    assert(config.partition_plan.item_count == 0);
    assert(!config.has_selected_plan_target);
    assert(!config.selected_format_plan.valid);
    assert(!config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_WINDOWS].valid);
    assert(!config.has_apply_plan);
    assert(config.apply_result.code == CLASSICSETUP_APPLY_RESULT_NOT_RUN);
}

int main(void)
{
    test_default_and_option_movement();
    test_role_policies();
    test_install_target_policy();
    test_config_stores_automatic_role_plans();
    test_config_rejects_non_install_targets();
    test_disk_change_reset_clears_stale_plan();
    return 0;
}
