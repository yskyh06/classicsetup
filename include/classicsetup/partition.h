#ifndef CLASSICSETUP_PARTITION_H
#define CLASSICSETUP_PARTITION_H

#include <stddef.h>

#include "classicsetup/disk.h"

enum {
    CLASSICSETUP_PARTITION_NAME_SIZE = 64,
    CLASSICSETUP_PARTITION_PATH_SIZE = 128
};

struct classicsetup_partition_info {
    char name[CLASSICSETUP_PARTITION_NAME_SIZE];
    char device_path[CLASSICSETUP_PARTITION_PATH_SIZE];
    unsigned int number;
    unsigned long long start_sector;
    unsigned long long sector_count;
    unsigned long long size_bytes;
};

struct classicsetup_unallocated_info {
    unsigned long long start_sector;
    unsigned long long sector_count;
    unsigned long long size_bytes;
};

enum classicsetup_partition_target_type {
    CLASSICSETUP_PARTITION_TARGET_NONE,
    CLASSICSETUP_PARTITION_TARGET_EXISTING,
    CLASSICSETUP_PARTITION_TARGET_NEW,
    CLASSICSETUP_PARTITION_TARGET_UNALLOCATED
};

int classicsetup_scan_partitions(
    const struct classicsetup_disk_info *disk,
    struct classicsetup_partition_info *partitions,
    size_t capacity,
    size_t *partition_count);

int classicsetup_scan_partitions_from(
    const char *sys_block_path,
    const struct classicsetup_disk_info *disk,
    struct classicsetup_partition_info *partitions,
    size_t capacity,
    size_t *partition_count);

int classicsetup_calculate_unallocated(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_unallocated_info *spaces,
    size_t capacity,
    size_t *space_count);

#endif
