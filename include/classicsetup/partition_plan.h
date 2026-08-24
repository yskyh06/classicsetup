#ifndef CLASSICSETUP_PARTITION_PLAN_H
#define CLASSICSETUP_PARTITION_PLAN_H

#include <stddef.h>

#include "classicsetup/disk.h"
#include "classicsetup/partition.h"

enum {
    CLASSICSETUP_PLAN_MAX_ITEMS = 256,
    CLASSICSETUP_SECTORS_PER_MB = 2048,
    CLASSICSETUP_DEFAULT_EFI_MB = 260,
    CLASSICSETUP_DEFAULT_MSR_MB = 16,
    CLASSICSETUP_DEFAULT_RECOVERY_MB = 1024,
    CLASSICSETUP_MIN_WINDOWS_MB = 64
};

enum classicsetup_plan_item_state {
    CLASSICSETUP_PLAN_EXISTING,
    CLASSICSETUP_PLAN_NEW,
    CLASSICSETUP_PLAN_DELETED,
    CLASSICSETUP_PLAN_UNALLOCATED
};

enum classicsetup_partition_role {
    CLASSICSETUP_PARTITION_ROLE_NONE,
    CLASSICSETUP_PARTITION_ROLE_GENERIC,
    CLASSICSETUP_PARTITION_ROLE_EFI,
    CLASSICSETUP_PARTITION_ROLE_MSR,
    CLASSICSETUP_PARTITION_ROLE_WINDOWS,
    CLASSICSETUP_PARTITION_ROLE_RECOVERY,
    CLASSICSETUP_PARTITION_ROLE_COUNT
};

struct classicsetup_plan_item {
    enum classicsetup_plan_item_state state;
    enum classicsetup_partition_role role;
    char name[CLASSICSETUP_PARTITION_NAME_SIZE];
    char device_path[CLASSICSETUP_PARTITION_PATH_SIZE];
    unsigned int number;
    unsigned long long start_sector;
    unsigned long long sector_count;
    unsigned long long size_bytes;
};

struct classicsetup_partition_plan {
    struct classicsetup_plan_item items[CLASSICSETUP_PLAN_MAX_ITEMS];
    size_t item_count;
    unsigned long long disk_sector_count;
};

int classicsetup_plan_init(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_partition_plan *plan);

int classicsetup_plan_size_mb_to_sectors(
    unsigned long long size_mb,
    unsigned long long available_sectors,
    unsigned long long *sector_count);

unsigned long long classicsetup_plan_max_size_mb(
    unsigned long long available_sectors);

int classicsetup_plan_align_sector(
    unsigned long long sector,
    unsigned long long alignment,
    unsigned long long *aligned_sector);

int classicsetup_plan_create_partition(
    struct classicsetup_partition_plan *plan,
    size_t unallocated_index,
    unsigned long long size_mb,
    size_t *created_index);

int classicsetup_plan_create_windows_layout(
    struct classicsetup_partition_plan *plan,
    size_t unallocated_index,
    size_t *windows_index);

int classicsetup_plan_has_windows_layout(
    const struct classicsetup_partition_plan *plan);

int classicsetup_plan_undo_windows_layout(
    struct classicsetup_partition_plan *plan,
    size_t *restored_unallocated_index);

int classicsetup_plan_delete_partition(
    struct classicsetup_partition_plan *plan,
    size_t item_index);

int classicsetup_plan_rebuild_unallocated(
    struct classicsetup_partition_plan *plan);

int classicsetup_plan_merge_unallocated(
    struct classicsetup_partition_plan *plan);

int classicsetup_plan_has_no_overlap(
    const struct classicsetup_partition_plan *plan);

int classicsetup_plan_validate(
    const struct classicsetup_partition_plan *plan);

int classicsetup_plan_item_is_install_target(
    const struct classicsetup_plan_item *item);

size_t classicsetup_plan_normalize_index(
    const struct classicsetup_partition_plan *plan,
    size_t selected_index);

int classicsetup_plan_find_matching_item(
    const struct classicsetup_partition_plan *plan,
    const struct classicsetup_plan_item *target,
    size_t *item_index);

int classicsetup_plan_prepare_install_target(
    struct classicsetup_partition_plan *plan,
    size_t selected_index,
    size_t *target_index);

#endif
