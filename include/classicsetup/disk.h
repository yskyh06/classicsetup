#ifndef CLASSICSETUP_DISK_H
#define CLASSICSETUP_DISK_H

#include <stddef.h>
#include <stdbool.h>

enum {
    CLASSICSETUP_DISK_NAME_SIZE = 64,
    CLASSICSETUP_DISK_PATH_SIZE = 128,
    CLASSICSETUP_DISK_MODEL_SIZE = 128,
    CLASSICSETUP_DISK_ID_SIZE = 128,
    CLASSICSETUP_DISK_TRANSPORT_SIZE = 32,
    CLASSICSETUP_DISK_SYSFS_PATH_SIZE = 512,
    CLASSICSETUP_SECTOR_SIZE_BYTES = 512
};

struct classicsetup_disk_info {
    char name[CLASSICSETUP_DISK_NAME_SIZE];
    char device_path[CLASSICSETUP_DISK_PATH_SIZE];
    char model[CLASSICSETUP_DISK_MODEL_SIZE];
    char serial[CLASSICSETUP_DISK_ID_SIZE];
    char wwn[CLASSICSETUP_DISK_ID_SIZE];
    char transport[CLASSICSETUP_DISK_TRANSPORT_SIZE];
    char sysfs_path[CLASSICSETUP_DISK_SYSFS_PATH_SIZE];
    unsigned long long size_bytes;
    unsigned int logical_sector_size;
    bool has_serial;
    bool has_wwn;
    bool has_logical_sector_size;
    bool removable;
    bool has_removable;
};

int classicsetup_disk_has_recommended_identity(
    const struct classicsetup_disk_info *disk);

int classicsetup_disk_has_vm_test_identity(
    const struct classicsetup_disk_info *disk);

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
