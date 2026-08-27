#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "classicsetup/config.h"
#include "classicsetup/partition_plan.h"

static struct classicsetup_disk_info make_disk(void)
{
    struct classicsetup_disk_info disk = {0};

    strcpy(disk.name, "sda");
    strcpy(disk.device_path, "/dev/sda");
    disk.size_bytes = 20000ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    return disk;
}

static void make_partitions(struct classicsetup_partition_info *partitions)
{
    memset(partitions, 0, 2 * sizeof(partitions[0]));

    strcpy(partitions[0].name, "sda1");
    strcpy(partitions[0].device_path, "/dev/sda1");
    partitions[0].number = 1;
    partitions[0].start_sector = 2048ULL;
    partitions[0].sector_count = 2048ULL;
    partitions[0].size_bytes =
        partitions[0].sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;

    strcpy(partitions[1].name, "sda2");
    strcpy(partitions[1].device_path, "/dev/sda2");
    partitions[1].number = 2;
    partitions[1].start_sector = 8192ULL;
    partitions[1].sector_count = 2048ULL;
    partitions[1].size_bytes =
        partitions[1].sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;
}

static size_t find_item(
    const struct classicsetup_partition_plan *plan,
    enum classicsetup_plan_item_state state,
    unsigned long long start_sector)
{
    size_t index;

    for (index = 0; index < plan->item_count; ++index) {
        if (plan->items[index].state == state &&
            plan->items[index].start_sector == start_sector) {
            return index;
        }
    }
    return plan->item_count;
}

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

static void assert_sorted(const struct classicsetup_partition_plan *plan)
{
    size_t index;

    for (index = 1; index < plan->item_count; ++index) {
        assert(plan->items[index - 1].start_sector <=
               plan->items[index].start_sector);
    }
}

static void assert_unallocated_does_not_overlap_occupied(
    const struct classicsetup_partition_plan *plan)
{
    size_t free_index;

    for (free_index = 0; free_index < plan->item_count; ++free_index) {
        const struct classicsetup_plan_item *space =
            &plan->items[free_index];
        size_t occupied_index;

        if (space->state != CLASSICSETUP_PLAN_UNALLOCATED) {
            continue;
        }
        for (occupied_index = 0;
             occupied_index < plan->item_count;
             ++occupied_index) {
            const struct classicsetup_plan_item *occupied =
                &plan->items[occupied_index];

            if (occupied->state != CLASSICSETUP_PLAN_EXISTING &&
                occupied->state != CLASSICSETUP_PLAN_NEW) {
                continue;
            }
            assert(space->start_sector + space->sector_count <=
                       occupied->start_sector ||
                   occupied->start_sector + occupied->sector_count <=
                       space->start_sector);
        }
    }
}

static void test_create_delete_and_merge(void)
{
    struct classicsetup_disk_info disk = make_disk();
    struct classicsetup_partition_info partitions[2];
    struct classicsetup_partition_plan plan;
    size_t index;
    size_t created_index;

    make_partitions(partitions);
    assert(classicsetup_plan_init(&disk, partitions, 2, &plan) == 0);
    assert(plan.item_count == 5);
    assert(find_item(&plan, CLASSICSETUP_PLAN_EXISTING, 2048ULL) <
           plan.item_count);
    assert(find_item(&plan, CLASSICSETUP_PLAN_EXISTING, 8192ULL) <
           plan.item_count);
    assert(plan.items[find_item(
               &plan,
               CLASSICSETUP_PLAN_EXISTING,
               2048ULL)].role == CLASSICSETUP_PARTITION_ROLE_GENERIC);
    assert(find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 4096ULL) <
           plan.item_count);
    assert_sorted(&plan);
    assert(classicsetup_plan_has_no_overlap(&plan));
    assert(classicsetup_plan_validate(&plan));

    index = find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 4096ULL);
    assert(classicsetup_plan_create_partition(
               &plan,
               index,
               1,
               &created_index) == 0);
    assert(plan.items[created_index].state == CLASSICSETUP_PLAN_NEW);
    assert(plan.items[created_index].role == CLASSICSETUP_PARTITION_ROLE_GENERIC);
    assert(plan.items[created_index].start_sector == 4096ULL);
    assert(plan.items[created_index].sector_count == 2048ULL);
    assert(find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 6144ULL) <
           plan.item_count);
    assert_sorted(&plan);
    assert(classicsetup_plan_has_no_overlap(&plan));
    assert(classicsetup_plan_validate(&plan));

    index = find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 6144ULL);
    assert(classicsetup_plan_create_partition(
               &plan,
               index,
               0,
               &created_index) == -1);
    assert(classicsetup_plan_create_partition(
               &plan,
               index,
               2,
               &created_index) == -1);

    index = find_item(&plan, CLASSICSETUP_PLAN_EXISTING, 8192ULL);
    assert(classicsetup_plan_delete_partition(&plan, index) == 0);
    assert(find_item(&plan, CLASSICSETUP_PLAN_DELETED, 8192ULL) <
           plan.item_count);
    index = find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 6144ULL);
    assert(index < plan.item_count);
    assert(plan.items[index].sector_count == 13856ULL);
    assert_sorted(&plan);
    assert(classicsetup_plan_has_no_overlap(&plan));
    assert(classicsetup_plan_validate(&plan));

    index = find_item(&plan, CLASSICSETUP_PLAN_NEW, 4096ULL);
    assert(classicsetup_plan_delete_partition(&plan, index) == 0);
    assert(find_item(&plan, CLASSICSETUP_PLAN_NEW, 4096ULL) ==
           plan.item_count);
    index = find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 4096ULL);
    assert(index < plan.item_count);
    assert(plan.items[index].sector_count == 15904ULL);
    assert_sorted(&plan);
    assert(classicsetup_plan_has_no_overlap(&plan));
    assert(classicsetup_plan_validate(&plan));

    assert(classicsetup_plan_create_partition(
               &plan,
               index,
               1,
               &created_index) == 0);
    assert(created_index < plan.item_count);
    assert(plan.items[created_index].state == CLASSICSETUP_PLAN_NEW);
    assert(classicsetup_plan_validate(&plan));
}

static void test_windows_layout_and_roles(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_info partition = {0};
    struct classicsetup_partition_plan plan;
    struct classicsetup_config config = {0};
    unsigned long long total_sectors =
        4096ULL * CLASSICSETUP_SECTORS_PER_MB + 123ULL;
    unsigned long long fixed_sectors =
        (CLASSICSETUP_DEFAULT_EFI_MB + CLASSICSETUP_DEFAULT_MSR_MB +
         CLASSICSETUP_DEFAULT_RECOVERY_MB) *
        CLASSICSETUP_SECTORS_PER_MB;
    unsigned long long expected_windows =
        ((total_sectors - 2ULL * CLASSICSETUP_SECTORS_PER_MB - fixed_sectors) /
         CLASSICSETUP_SECTORS_PER_MB) *
        CLASSICSETUP_SECTORS_PER_MB;
    enum classicsetup_partition_role expected_roles[] = {
        CLASSICSETUP_PARTITION_ROLE_EFI,
        CLASSICSETUP_PARTITION_ROLE_MSR,
        CLASSICSETUP_PARTITION_ROLE_WINDOWS,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY
    };
    size_t free_index;
    size_t windows_index;
    size_t new_count = 0;
    size_t index;

    strcpy(disk.name, "sda");
    strcpy(disk.device_path, "/dev/sda");
    disk.size_bytes = total_sectors * CLASSICSETUP_SECTOR_SIZE_BYTES;
    strcpy(partition.name, "sda1");
    strcpy(partition.device_path, "/dev/sda1");
    partition.number = 1;
    partition.start_sector = 0;
    partition.sector_count = 1;
    partition.size_bytes = CLASSICSETUP_SECTOR_SIZE_BYTES;

    assert(classicsetup_plan_init(&disk, &partition, 1, &plan) == 0);
    free_index = find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 1ULL);
    assert(free_index < plan.item_count);
    assert(classicsetup_plan_create_windows_layout(
               &plan,
               free_index,
               &windows_index) == 0);
    assert(plan.items[windows_index].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(plan.items[windows_index].sector_count == expected_windows);
    assert(classicsetup_plan_has_no_overlap(&plan));
    assert(classicsetup_plan_validate(&plan));
    assert_unallocated_does_not_overlap_occupied(&plan);
    assert_sorted(&plan);

    for (index = 0; index < plan.item_count; ++index) {
        const struct classicsetup_plan_item *item = &plan.items[index];

        if (item->state != CLASSICSETUP_PLAN_NEW) {
            continue;
        }
        assert(new_count < sizeof(expected_roles) / sizeof(expected_roles[0]));
        assert(item->role == expected_roles[new_count]);
        assert(item->start_sector % CLASSICSETUP_SECTORS_PER_MB == 0);
        ++new_count;
    }
    assert(new_count == sizeof(expected_roles) / sizeof(expected_roles[0]));

    index = find_role(&plan, CLASSICSETUP_PARTITION_ROLE_EFI);
    assert(index < plan.item_count);
    assert(plan.items[index].sector_count ==
           CLASSICSETUP_DEFAULT_EFI_MB * CLASSICSETUP_SECTORS_PER_MB);
    index = find_role(&plan, CLASSICSETUP_PARTITION_ROLE_MSR);
    assert(index < plan.item_count);
    assert(plan.items[index].sector_count ==
           CLASSICSETUP_DEFAULT_MSR_MB * CLASSICSETUP_SECTORS_PER_MB);
    index = find_role(&plan, CLASSICSETUP_PARTITION_ROLE_RECOVERY);
    assert(index < plan.item_count);
    assert(plan.items[index].sector_count ==
           CLASSICSETUP_DEFAULT_RECOVERY_MB * CLASSICSETUP_SECTORS_PER_MB);

    index = find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 1ULL);
    assert(index < plan.item_count);
    assert(plan.items[index].sector_count ==
           CLASSICSETUP_SECTORS_PER_MB - 1ULL);
    index = find_role(&plan, CLASSICSETUP_PARTITION_ROLE_RECOVERY);
    assert(index < plan.item_count);
    index = find_item(
        &plan,
        CLASSICSETUP_PLAN_UNALLOCATED,
        plan.items[index].start_sector + plan.items[index].sector_count);
    assert(index < plan.item_count);
    assert(plan.items[index].sector_count ==
           CLASSICSETUP_SECTORS_PER_MB + 123ULL);

    config.partition_plan = plan;
    config.has_partition_plan = true;
    windows_index = find_role(
        &config.partition_plan,
        CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(classicsetup_config_select_plan_item(&config, windows_index) == 0);
    assert(config.has_selected_plan_target);
    assert(config.selected_plan_target.role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    classicsetup_config_revalidate_plan_selection(&config);
    assert(config.selected_plan_target.role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);

    index = find_role(&config.partition_plan, CLASSICSETUP_PARTITION_ROLE_EFI);
    assert(classicsetup_plan_delete_partition(
               &config.partition_plan,
               index) == 0);
    assert(find_role(
               &config.partition_plan,
               CLASSICSETUP_PARTITION_ROLE_EFI) ==
           config.partition_plan.item_count);
    assert(classicsetup_plan_has_no_overlap(&config.partition_plan));
    assert(classicsetup_plan_validate(&config.partition_plan));
}

static void test_unallocated_continue_creates_windows_target(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan plan;
    struct classicsetup_config config = {0};
    struct classicsetup_plan_item snapshot;
    size_t restored_index;
    size_t target_index;

    disk.size_bytes = 4096ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &plan) == 0);
    assert(plan.item_count == 1);
    assert(plan.items[0].state == CLASSICSETUP_PLAN_UNALLOCATED);

    assert(classicsetup_plan_prepare_install_target(
               &plan,
               0,
               &target_index) == 0);
    assert(target_index < plan.item_count);
    assert(plan.items[target_index].state == CLASSICSETUP_PLAN_NEW);
    assert(plan.items[target_index].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(find_role(&plan, CLASSICSETUP_PARTITION_ROLE_EFI) < plan.item_count);
    assert(find_role(&plan, CLASSICSETUP_PARTITION_ROLE_MSR) < plan.item_count);
    assert(find_role(&plan, CLASSICSETUP_PARTITION_ROLE_RECOVERY) <
           plan.item_count);
    assert(classicsetup_plan_validate(&plan));
    assert_unallocated_does_not_overlap_occupied(&plan);

    snapshot = plan.items[target_index];
    assert(classicsetup_plan_find_matching_item(
               &plan,
               &snapshot,
               &restored_index) == 0);
    assert(restored_index == target_index);

    config.partition_plan = plan;
    config.has_partition_plan = true;
    assert(classicsetup_config_select_plan_item(
               &config,
               target_index) == 0);
    assert(config.selected_plan_target.role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
}

static void test_prepare_failure_is_transactional(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan plan;
    struct classicsetup_partition_plan before;
    unsigned long long sectors =
        (CLASSICSETUP_DEFAULT_EFI_MB + CLASSICSETUP_DEFAULT_MSR_MB +
         CLASSICSETUP_DEFAULT_RECOVERY_MB + CLASSICSETUP_MIN_WINDOWS_MB) *
        CLASSICSETUP_SECTORS_PER_MB - 1ULL;
    size_t target_index = 99;

    disk.size_bytes = sectors * CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &plan) == 0);
    before = plan;
    assert(classicsetup_plan_prepare_install_target(
               &plan,
               0,
               &target_index) == -1);
    assert(memcmp(&plan, &before, sizeof(plan)) == 0);
    assert(target_index == 99);
}

static void test_windows_layout_delete_and_recreate(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan plan;
    enum classicsetup_partition_role roles[] = {
        CLASSICSETUP_PARTITION_ROLE_EFI,
        CLASSICSETUP_PARTITION_ROLE_MSR,
        CLASSICSETUP_PARTITION_ROLE_WINDOWS,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY
    };
    size_t target_index;
    size_t index;

    disk.size_bytes = 4096ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &plan) == 0);
    assert(classicsetup_plan_prepare_install_target(
               &plan,
               0,
               &target_index) == 0);

    for (index = 0; index < sizeof(roles) / sizeof(roles[0]); ++index) {
        size_t delete_index = find_role(&plan, roles[index]);

        assert(delete_index < plan.item_count);
        assert(classicsetup_plan_delete_partition(
                   &plan,
                   delete_index) == 0);
        assert(classicsetup_plan_validate(&plan));
    }
    assert(plan.item_count == 1);
    assert(plan.items[0].state == CLASSICSETUP_PLAN_UNALLOCATED);
    assert(plan.items[0].start_sector == 0);
    assert(plan.items[0].sector_count == plan.disk_sector_count);

    assert(classicsetup_plan_prepare_install_target(
               &plan,
               0,
               &target_index) == 0);
    assert(plan.items[target_index].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(classicsetup_plan_validate(&plan));
    assert_unallocated_does_not_overlap_occupied(&plan);
}

static void test_windows_layout_undo(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_info existing = {0};
    struct classicsetup_config config = {0};
    struct classicsetup_partition_plan before_failed_undo;
    size_t manual_free_index;
    size_t manual_index;
    size_t layout_free_index;
    size_t windows_index;
    size_t restored_index = 99;
    size_t index;

    disk.size_bytes = 8192ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    strcpy(existing.name, "sda1");
    strcpy(existing.device_path, "/dev/sda1");
    existing.number = 1;
    existing.start_sector = CLASSICSETUP_SECTORS_PER_MB;
    existing.sector_count = CLASSICSETUP_SECTORS_PER_MB;
    existing.size_bytes = existing.sector_count *
                          CLASSICSETUP_SECTOR_SIZE_BYTES;

    assert(classicsetup_plan_init(
               &disk,
               &existing,
               1,
               &config.partition_plan) == 0);
    config.has_partition_plan = true;

    manual_free_index = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_UNALLOCATED,
        0);
    assert(manual_free_index < config.partition_plan.item_count);
    assert(classicsetup_plan_create_partition(
               &config.partition_plan,
               manual_free_index,
               1,
               &manual_index) == 0);
    assert(config.partition_plan.items[manual_index].role ==
           CLASSICSETUP_PARTITION_ROLE_GENERIC);

    layout_free_index = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_UNALLOCATED,
        2ULL * CLASSICSETUP_SECTORS_PER_MB);
    assert(layout_free_index < config.partition_plan.item_count);
    assert(classicsetup_plan_create_windows_layout(
               &config.partition_plan,
               layout_free_index,
               &windows_index) == 0);
    assert(classicsetup_plan_has_windows_layout(&config.partition_plan));
    assert(classicsetup_config_select_plan_item(
               &config,
               windows_index) == 0);
    assert(classicsetup_config_set_format_plan(
               &config,
               CLASSICSETUP_FORMAT_QUICK) == 0);
    assert(config.selected_format_plan.valid);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_EFI].valid);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_WINDOWS].valid);

    assert(classicsetup_config_undo_windows_layout(
               &config,
               &restored_index) == 0);
    assert(!classicsetup_plan_has_windows_layout(&config.partition_plan));
    assert(find_role(
               &config.partition_plan,
               CLASSICSETUP_PARTITION_ROLE_EFI) ==
           config.partition_plan.item_count);
    assert(find_role(
               &config.partition_plan,
               CLASSICSETUP_PARTITION_ROLE_MSR) ==
           config.partition_plan.item_count);
    assert(find_role(
               &config.partition_plan,
               CLASSICSETUP_PARTITION_ROLE_WINDOWS) ==
           config.partition_plan.item_count);
    assert(find_role(
               &config.partition_plan,
               CLASSICSETUP_PARTITION_ROLE_RECOVERY) ==
           config.partition_plan.item_count);

    manual_index = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_NEW,
        0);
    assert(manual_index < config.partition_plan.item_count);
    assert(config.partition_plan.items[manual_index].role ==
           CLASSICSETUP_PARTITION_ROLE_GENERIC);
    index = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_EXISTING,
        CLASSICSETUP_SECTORS_PER_MB);
    assert(index < config.partition_plan.item_count);
    assert(restored_index < config.partition_plan.item_count);
    assert(config.partition_plan.items[restored_index].state ==
           CLASSICSETUP_PLAN_UNALLOCATED);
    assert(config.partition_plan.items[restored_index].start_sector ==
           2ULL * CLASSICSETUP_SECTORS_PER_MB);
    assert(classicsetup_plan_normalize_index(
               &config.partition_plan,
               restored_index) == restored_index);
    assert(classicsetup_plan_validate(&config.partition_plan));
    assert(classicsetup_plan_has_no_overlap(&config.partition_plan));
    assert_unallocated_does_not_overlap_occupied(&config.partition_plan);

    assert(!config.has_selected_plan_target);
    assert(config.selected_plan_target.state == 0);
    assert(config.selected_plan_target.start_sector == 0);
    assert(config.selected_plan_target.sector_count == 0);
    assert(config.partition_target_type ==
           CLASSICSETUP_PARTITION_TARGET_NONE);
    assert(!config.selected_format_plan.valid);
    for (index = 0;
         index < CLASSICSETUP_PARTITION_ROLE_COUNT;
         ++index) {
        assert(!config.role_format_plans[index].valid);
    }

    before_failed_undo = config.partition_plan;
    restored_index = 99;
    assert(classicsetup_plan_undo_windows_layout(
               &config.partition_plan,
               &restored_index) == -1);
    assert(memcmp(
               &config.partition_plan,
               &before_failed_undo,
               sizeof(config.partition_plan)) == 0);
    assert(restored_index == 99);
}

static void test_incomplete_windows_layout_is_not_undoable(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan plan;
    struct classicsetup_partition_plan before;
    size_t windows_index;
    size_t efi_index;
    size_t restored_index = 77;

    disk.size_bytes = 4096ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &plan) == 0);
    assert(classicsetup_plan_create_windows_layout(
               &plan,
               0,
               &windows_index) == 0);
    efi_index = find_role(&plan, CLASSICSETUP_PARTITION_ROLE_EFI);
    assert(efi_index < plan.item_count);
    assert(classicsetup_plan_delete_partition(&plan, efi_index) == 0);
    assert(!classicsetup_plan_has_windows_layout(&plan));

    before = plan;
    assert(classicsetup_plan_undo_windows_layout(
               &plan,
               &restored_index) == -1);
    assert(memcmp(&plan, &before, sizeof(plan)) == 0);
    assert(restored_index == 77);
}

static void test_plan_validator_rejects_inconsistent_ranges(void)
{
    struct classicsetup_partition_plan valid = {0};
    struct classicsetup_partition_plan invalid;

    valid.disk_sector_count = 1000;
    valid.item_count = 1;
    valid.items[0].state = CLASSICSETUP_PLAN_UNALLOCATED;
    valid.items[0].role = CLASSICSETUP_PARTITION_ROLE_NONE;
    valid.items[0].sector_count = 1000;
    valid.items[0].size_bytes =
        1000ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_validate(&valid));

    invalid = valid;
    invalid.item_count = 2;
    invalid.items[0].state = CLASSICSETUP_PLAN_NEW;
    invalid.items[0].role = CLASSICSETUP_PARTITION_ROLE_WINDOWS;
    invalid.items[0].sector_count = 800;
    invalid.items[0].size_bytes =
        800ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    invalid.items[1] = valid.items[0];
    assert(!classicsetup_plan_validate(&invalid));

    invalid = valid;
    invalid.items[0].start_sector = 1;
    invalid.items[0].sector_count = 999;
    invalid.items[0].size_bytes =
        999ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(!classicsetup_plan_validate(&invalid));

    invalid = valid;
    invalid.items[0].sector_count = 1001;
    invalid.items[0].size_bytes =
        1001ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(!classicsetup_plan_validate(&invalid));

    invalid = valid;
    invalid.items[0].size_bytes = 1;
    assert(!classicsetup_plan_validate(&invalid));
}

static void test_selection_index_helpers(void)
{
    struct classicsetup_partition_plan plan = {0};
    struct classicsetup_plan_item target;
    size_t index = 99;

    assert(classicsetup_plan_normalize_index(&plan, 100) == 0);

    plan.disk_sector_count = 1000;
    plan.item_count = 1;
    plan.items[0].state = CLASSICSETUP_PLAN_UNALLOCATED;
    plan.items[0].role = CLASSICSETUP_PARTITION_ROLE_NONE;
    plan.items[0].sector_count = 1000;
    plan.items[0].size_bytes =
        1000ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_normalize_index(&plan, 5) == 0);

    plan.item_count = 3;
    assert(classicsetup_plan_normalize_index(&plan, 3) == 2);
    plan.item_count = 2;
    assert(classicsetup_plan_normalize_index(&plan, 2) == 1);
    plan.item_count = 1;

    target = plan.items[0];
    assert(classicsetup_plan_find_matching_item(
               &plan,
               &target,
               &index) == 0);
    assert(index == 0);
    target.start_sector = 1;
    assert(classicsetup_plan_find_matching_item(
               &plan,
               &target,
               &index) == -1);
}

static void test_windows_layout_rejects_without_mutation(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan plan;
    struct classicsetup_partition_plan before;
    unsigned long long insufficient_sectors =
        (CLASSICSETUP_DEFAULT_EFI_MB + CLASSICSETUP_DEFAULT_MSR_MB +
         CLASSICSETUP_DEFAULT_RECOVERY_MB + CLASSICSETUP_MIN_WINDOWS_MB) *
        CLASSICSETUP_SECTORS_PER_MB - 1ULL;
    unsigned long long aligned;
    size_t windows_index = 0;

    disk.size_bytes = insufficient_sectors * CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &plan) == 0);
    before = plan;
    assert(classicsetup_plan_create_windows_layout(
               &plan,
               0,
               &windows_index) == -1);
    assert(memcmp(&plan, &before, sizeof(plan)) == 0);

    memset(&plan, 0, sizeof(plan));
    plan.disk_sector_count = ULLONG_MAX;
    plan.item_count = 1;
    plan.items[0].state = CLASSICSETUP_PLAN_UNALLOCATED;
    plan.items[0].role = CLASSICSETUP_PARTITION_ROLE_NONE;
    plan.items[0].start_sector = ULLONG_MAX - 100ULL;
    plan.items[0].sector_count = 200ULL;
    before = plan;
    assert(classicsetup_plan_create_windows_layout(
               &plan,
               0,
               &windows_index) == -1);
    assert(memcmp(&plan, &before, sizeof(plan)) == 0);

    assert(classicsetup_plan_align_sector(
               ULLONG_MAX,
               CLASSICSETUP_SECTORS_PER_MB,
               &aligned) == -1);
}

static void test_manual_maximum_mb_floor(void)
{
    unsigned long long available =
        123ULL * CLASSICSETUP_SECTORS_PER_MB +
        CLASSICSETUP_SECTORS_PER_MB - 1ULL;
    unsigned long long maximum = classicsetup_plan_max_size_mb(available);
    unsigned long long sectors;

    assert(maximum == 123ULL);
    assert(classicsetup_plan_size_mb_to_sectors(
               maximum,
               available,
               &sectors) == 0);
    assert(sectors <= available);
    assert(classicsetup_plan_size_mb_to_sectors(
               maximum + 1ULL,
               available,
               &sectors) == -1);
}

static void test_explicit_adjacent_merge(void)
{
    struct classicsetup_partition_plan plan = {0};
    size_t index;

    plan.disk_sector_count = 4000ULL;
    plan.item_count = 2;
    plan.items[0].state = CLASSICSETUP_PLAN_UNALLOCATED;
    plan.items[0].start_sector = 1000ULL;
    plan.items[0].sector_count = 1000ULL;
    plan.items[0].size_bytes = 512000ULL;
    plan.items[1].state = CLASSICSETUP_PLAN_UNALLOCATED;
    plan.items[1].start_sector = 2000ULL;
    plan.items[1].sector_count = 1000ULL;
    plan.items[1].size_bytes = 512000ULL;

    assert(classicsetup_plan_merge_unallocated(&plan) == 0);
    assert(plan.item_count == 1);
    index = find_item(&plan, CLASSICSETUP_PLAN_UNALLOCATED, 1000ULL);
    assert(index == 0);
    assert(plan.items[index].sector_count == 2000ULL);
}

static void test_config_selection_persists(void)
{
    struct classicsetup_disk_info disk = make_disk();
    struct classicsetup_partition_info partitions[2];
    struct classicsetup_config config = {0};
    size_t selected;
    size_t delete_index;
    size_t free_index;
    size_t created_index;

    make_partitions(partitions);
    assert(classicsetup_plan_init(
               &disk,
               partitions,
               2,
               &config.partition_plan) == 0);
    config.has_partition_plan = true;

    selected = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_EXISTING,
        2048ULL);
    assert(classicsetup_config_select_plan_item(&config, selected) == 0);
    assert(config.has_selected_plan_target);
    assert(strcmp(config.selected_plan_target.device_path, "/dev/sda1") == 0);

    delete_index = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_EXISTING,
        8192ULL);
    assert(classicsetup_plan_delete_partition(
               &config.partition_plan,
               delete_index) == 0);
    classicsetup_config_revalidate_plan_selection(&config);
    assert(config.has_selected_plan_target);
    assert(strcmp(config.selected_plan_target.device_path, "/dev/sda1") == 0);

    free_index = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_UNALLOCATED,
        4096ULL);
    assert(classicsetup_plan_create_partition(
               &config.partition_plan,
               free_index,
               1,
               &created_index) == 0);
    assert(classicsetup_config_select_plan_item(&config, created_index) == 0);
    assert(config.partition_target_type == CLASSICSETUP_PARTITION_TARGET_NEW);
    assert(config.selected_plan_target.state == CLASSICSETUP_PLAN_NEW);
    assert(config.selected_plan_target.start_sector == 4096ULL);
    classicsetup_config_revalidate_plan_selection(&config);
    assert(config.has_selected_plan_target);

    created_index = find_item(
        &config.partition_plan,
        CLASSICSETUP_PLAN_NEW,
        4096ULL);
    assert(classicsetup_plan_delete_partition(
               &config.partition_plan,
               created_index) == 0);
    classicsetup_config_revalidate_plan_selection(&config);
    assert(!config.has_selected_plan_target);
    assert(config.partition_target_type == CLASSICSETUP_PARTITION_TARGET_NONE);

    assert(classicsetup_plan_size_mb_to_sectors(
               ULLONG_MAX,
               ULLONG_MAX,
               &config.partition_plan.disk_sector_count) == -1);
}

static void test_bios_windows_layout_and_undo(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_config config = {0};
    enum classicsetup_partition_role expected_roles[] = {
        CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED,
        CLASSICSETUP_PARTITION_ROLE_WINDOWS,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY
    };
    size_t windows_index;
    size_t restored_index;
    size_t role_index = 0;
    size_t index;

    disk.size_bytes = 4096ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    config.install_mode = CLASSICSETUP_INSTALL_BIOS_MBR;
    assert(classicsetup_plan_init(
               &disk,
               NULL,
               0,
               &config.partition_plan) == 0);
    config.has_partition_plan = true;
    assert(classicsetup_plan_prepare_install_target_for_mode(
               &config.partition_plan,
               config.install_mode,
               0,
               &windows_index) == 0);
    assert(config.partition_plan.items[windows_index].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(classicsetup_plan_has_windows_layout_for_mode(
        &config.partition_plan,
        CLASSICSETUP_INSTALL_BIOS_MBR));
    assert(!classicsetup_plan_has_windows_layout(&config.partition_plan));
    assert(find_role(
               &config.partition_plan,
               CLASSICSETUP_PARTITION_ROLE_EFI) ==
           config.partition_plan.item_count);
    assert(find_role(
               &config.partition_plan,
               CLASSICSETUP_PARTITION_ROLE_MSR) ==
           config.partition_plan.item_count);

    for (index = 0; index < config.partition_plan.item_count; ++index) {
        const struct classicsetup_plan_item *item =
            &config.partition_plan.items[index];

        if (item->state != CLASSICSETUP_PLAN_NEW) {
            continue;
        }
        assert(role_index <
               sizeof(expected_roles) / sizeof(expected_roles[0]));
        assert(item->role == expected_roles[role_index++]);
        assert(item->start_sector % CLASSICSETUP_SECTORS_PER_MB == 0);
    }
    assert(role_index == sizeof(expected_roles) / sizeof(expected_roles[0]));
    index = find_role(
        &config.partition_plan,
        CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED);
    assert(index < config.partition_plan.item_count);
    assert(config.partition_plan.items[index].sector_count ==
           CLASSICSETUP_DEFAULT_SYSTEM_RESERVED_MB *
               CLASSICSETUP_SECTORS_PER_MB);
    index = find_role(
        &config.partition_plan,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY);
    assert(index < config.partition_plan.item_count);
    assert(config.partition_plan.items[index].sector_count ==
           CLASSICSETUP_DEFAULT_RECOVERY_MB *
               CLASSICSETUP_SECTORS_PER_MB);
    assert(classicsetup_plan_validate(&config.partition_plan));
    assert(classicsetup_plan_has_no_overlap(&config.partition_plan));

    assert(classicsetup_config_select_plan_item(
               &config,
               windows_index) == 0);
    assert(classicsetup_config_set_format_plan(
               &config,
               CLASSICSETUP_FORMAT_FULL) == 0);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED].valid);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED].filesystem ==
           CLASSICSETUP_FS_NTFS);
    assert(config.role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED].mode ==
           CLASSICSETUP_FORMAT_QUICK);

    assert(classicsetup_config_undo_windows_layout(
               &config,
               &restored_index) == 0);
    assert(restored_index < config.partition_plan.item_count);
    assert(config.partition_plan.items[restored_index].state ==
           CLASSICSETUP_PLAN_UNALLOCATED);
    assert(config.partition_plan.item_count == 1);
    assert(!config.has_selected_plan_target);
    assert(!config.selected_format_plan.valid);
    assert(classicsetup_plan_validate(&config.partition_plan));
}

static void test_bios_mbr_size_limit(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_info existing[2] = {0};
    struct classicsetup_partition_plan plan;
    struct classicsetup_partition_plan before;
    size_t free_index;
    size_t windows_index = 99;

    disk.size_bytes = CLASSICSETUP_MBR_MAX_SECTORS *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &plan) == 0);
    assert(classicsetup_plan_create_bios_windows_layout(
               &plan,
               0,
               &windows_index) == 0);
    assert(classicsetup_plan_has_windows_layout_for_mode(
        &plan,
        CLASSICSETUP_INSTALL_BIOS_MBR));

    disk.size_bytes = (CLASSICSETUP_MBR_MAX_SECTORS + 1ULL) *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &plan) == 0);
    before = plan;
    windows_index = 99;
    assert(classicsetup_plan_create_bios_windows_layout(
               &plan,
               0,
               &windows_index) == -1);
    assert(memcmp(&plan, &before, sizeof(plan)) == 0);
    assert(windows_index == 99);

    disk.size_bytes = 4096ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    existing[0].start_sector = CLASSICSETUP_SECTORS_PER_MB;
    existing[0].sector_count = CLASSICSETUP_SECTORS_PER_MB;
    existing[0].size_bytes = existing[0].sector_count *
                             CLASSICSETUP_SECTOR_SIZE_BYTES;
    existing[1].start_sector = 2ULL * CLASSICSETUP_SECTORS_PER_MB;
    existing[1].sector_count = CLASSICSETUP_SECTORS_PER_MB;
    existing[1].size_bytes = existing[1].sector_count *
                             CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, existing, 2, &plan) == 0);
    free_index = find_item(
        &plan,
        CLASSICSETUP_PLAN_UNALLOCATED,
        3ULL * CLASSICSETUP_SECTORS_PER_MB);
    assert(free_index < plan.item_count);
    before = plan;
    assert(classicsetup_plan_create_bios_windows_layout(
               &plan,
               free_index,
               &windows_index) == -1);
    assert(memcmp(&plan, &before, sizeof(plan)) == 0);
}

static void populate_stale_config(struct classicsetup_config *config)
{
    config->original_partition_count = 1;
    config->has_partition_plan = true;
    config->partition_plan.disk_sector_count = 1000;
    config->partition_plan.item_count = 1;
    config->partition_plan.items[0].state = CLASSICSETUP_PLAN_NEW;
    config->partition_plan.items[0].role =
        CLASSICSETUP_PARTITION_ROLE_WINDOWS;
    config->partition_plan.items[0].sector_count = 1000;
    config->partition_plan.items[0].size_bytes =
        1000ULL * CLASSICSETUP_SECTOR_SIZE_BYTES;
    config->selected_plan_target = config->partition_plan.items[0];
    config->has_selected_plan_target = true;
    config->selected_partition.sector_count = 1000;
    config->selected_unallocated.sector_count = 1000;
    config->selected_format_plan.valid = true;
    config->role_format_plans[
        CLASSICSETUP_PARTITION_ROLE_WINDOWS].valid = true;
    config->has_apply_plan = true;
    config->apply_result.code = CLASSICSETUP_APPLY_RESULT_SUCCESS;
}

static void assert_mode_change_cleared(
    const struct classicsetup_config *config)
{
    assert(!config->has_partition_plan);
    assert(config->original_partition_count == 0);
    assert(config->partition_plan.item_count == 0);
    assert(!config->has_selected_plan_target);
    assert(config->selected_partition.sector_count == 0);
    assert(config->selected_unallocated.sector_count == 0);
    assert(!config->selected_format_plan.valid);
    assert(!config->role_format_plans[
               CLASSICSETUP_PARTITION_ROLE_WINDOWS].valid);
    assert(!config->has_apply_plan);
    assert(config->apply_plan.partition_count == 0);
    assert(config->apply_result.code == CLASSICSETUP_APPLY_RESULT_NOT_RUN);
}

static void test_install_mode_default_and_reset(void)
{
    struct classicsetup_config config = {0};

    assert(classicsetup_default_install_mode() ==
           CLASSICSETUP_INSTALL_UEFI_GPT);
    assert(config.install_mode == CLASSICSETUP_INSTALL_UEFI_GPT);

    populate_stale_config(&config);
    classicsetup_config_set_install_mode(
        &config,
        CLASSICSETUP_INSTALL_BIOS_MBR);
    assert(config.install_mode == CLASSICSETUP_INSTALL_BIOS_MBR);
    assert_mode_change_cleared(&config);

    populate_stale_config(&config);
    classicsetup_config_set_install_mode(
        &config,
        CLASSICSETUP_INSTALL_UEFI_GPT);
    assert(config.install_mode == CLASSICSETUP_INSTALL_UEFI_GPT);
    assert_mode_change_cleared(&config);
}

int main(void)
{
    test_create_delete_and_merge();
    test_explicit_adjacent_merge();
    test_config_selection_persists();
    test_windows_layout_and_roles();
    test_windows_layout_rejects_without_mutation();
    test_manual_maximum_mb_floor();
    test_unallocated_continue_creates_windows_target();
    test_prepare_failure_is_transactional();
    test_windows_layout_delete_and_recreate();
    test_windows_layout_undo();
    test_incomplete_windows_layout_is_not_undoable();
    test_plan_validator_rejects_inconsistent_ranges();
    test_selection_index_helpers();
    test_bios_windows_layout_and_undo();
    test_bios_mbr_size_limit();
    test_install_mode_default_and_reset();
    return 0;
}
