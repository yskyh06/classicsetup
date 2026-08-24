#ifndef CLASSICSETUP_DISK_H
#define CLASSICSETUP_DISK_H

#include <stddef.h>

enum {
    CLASSICSETUP_DISK_NAME_SIZE = 64,
    CLASSICSETUP_DISK_PATH_SIZE = 128,
    CLASSICSETUP_DISK_MODEL_SIZE = 128,
    CLASSICSETUP_SECTOR_SIZE_BYTES = 512
};

struct classicsetup_disk_info {
    char name[CLASSICSETUP_DISK_NAME_SIZE];
    char device_path[CLASSICSETUP_DISK_PATH_SIZE];
    char model[CLASSICSETUP_DISK_MODEL_SIZE];
    unsigned long long size_bytes;
};

int classicsetup_scan_disks(
    struct classicsetup_disk_info *disks,
    size_t capacity,
    size_t *disk_count);

int classicsetup_scan_disks_from(
    const char *sys_block_path,
    struct classicsetup_disk_info *disks,
    size_t capacity,
    size_t *disk_count);

#endif
