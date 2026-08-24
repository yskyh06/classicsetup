#ifndef CLASSICSETUP_SYSTEM_DISK_H
#define CLASSICSETUP_SYSTEM_DISK_H

enum classicsetup_system_disk_status {
    CLASSICSETUP_SYSTEM_DISK_SAFE,
    CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE,
    CLASSICSETUP_SYSTEM_DISK_UNKNOWN
};

enum classicsetup_system_disk_status classicsetup_check_system_disk(
    const char *target_disk_name);

enum classicsetup_system_disk_status classicsetup_check_system_disk_from(
    const char *target_disk_name,
    const char *mountinfo_path,
    const char *sys_dev_block_path);

#endif
