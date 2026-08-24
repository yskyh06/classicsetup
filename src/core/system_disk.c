#define _POSIX_C_SOURCE 200809L

#include "classicsetup/system_disk.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

enum {
    MOUNTINFO_LINE_SIZE = 4096,
    DEVICE_ID_SIZE = 64,
    MOUNT_POINT_SIZE = 256,
    SYSFS_PATH_SIZE = 512,
    LINK_TARGET_SIZE = 1024,
    DISK_NAME_SIZE = 64
};

static int is_protected_mount(const char *mount_point)
{
    return strcmp(mount_point, "/") == 0 ||
           strcmp(mount_point, "/boot") == 0 ||
           strcmp(mount_point, "/boot/efi") == 0;
}

static int protected_filesystem_is_simple(
    const char *mount_point,
    const char *line)
{
    const char *separator = strstr(line, " - ");
    char filesystem[64];

    if (separator == NULL ||
        sscanf(separator + 3, "%63s", filesystem) != 1) {
        return 0;
    }
    if (strcmp(mount_point, "/boot/efi") == 0) {
        return strcmp(filesystem, "vfat") == 0 ||
               strcmp(filesystem, "fat") == 0 ||
               strcmp(filesystem, "fat32") == 0;
    }
    return strcmp(filesystem, "ext4") == 0 ||
           strcmp(filesystem, "xfs") == 0;
}

static int disk_name_from_link_target(
    const char *link_target,
    char *disk_name,
    size_t disk_name_size)
{
    const char *block = strstr(link_target, "/block/");
    const char *end;
    size_t length;

    if (block == NULL) {
        return -1;
    }
    block += strlen("/block/");
    end = strchr(block, '/');
    length = end == NULL ? strlen(block) : (size_t)(end - block);
    if (length == 0 || length >= disk_name_size) {
        return -1;
    }
    memcpy(disk_name, block, length);
    disk_name[length] = '\0';
    return 0;
}

static int resolve_mount_disk(
    const char *sys_dev_block_path,
    const char *device_id,
    char *disk_name,
    size_t disk_name_size)
{
    char path[SYSFS_PATH_SIZE];
    char link_target[LINK_TARGET_SIZE];
    ssize_t length;
    int written = snprintf(
        path,
        sizeof(path),
        "%s/%s",
        sys_dev_block_path,
        device_id);

    if (written < 0 || (size_t)written >= sizeof(path)) {
        return -1;
    }
    length = readlink(path, link_target, sizeof(link_target) - 1);
    if (length < 0 || (size_t)length >= sizeof(link_target)) {
        return -1;
    }
    link_target[length] = '\0';
    if (strstr(link_target, "/virtual/") != NULL ||
        disk_name_from_link_target(
            link_target,
            disk_name,
            disk_name_size) != 0 ||
        strncmp(disk_name, "dm-", 3) == 0 ||
        strncmp(disk_name, "md", 2) == 0) {
        return -1;
    }
    return 0;
}

enum classicsetup_system_disk_status classicsetup_check_system_disk_from(
    const char *target_disk_name,
    const char *mountinfo_path,
    const char *sys_dev_block_path)
{
    char line[MOUNTINFO_LINE_SIZE];
    FILE *mountinfo;
    int found_root = 0;

    if (target_disk_name == NULL || target_disk_name[0] == '\0' ||
        mountinfo_path == NULL || sys_dev_block_path == NULL) {
        return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
    }

    mountinfo = fopen(mountinfo_path, "r");
    if (mountinfo == NULL) {
        return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
    }

    while (fgets(line, sizeof(line), mountinfo) != NULL) {
        char device_id[DEVICE_ID_SIZE];
        char mount_point[MOUNT_POINT_SIZE];
        char disk_name[DISK_NAME_SIZE];
        int protected_mount;

        if (sscanf(
                line,
                "%*s %*s %63s %*s %255s",
                device_id,
                mount_point) != 2) {
            continue;
        }
        protected_mount = is_protected_mount(mount_point);
        if (strcmp(mount_point, "/") == 0) {
            found_root = 1;
        }
        if (resolve_mount_disk(
                sys_dev_block_path,
                device_id,
                disk_name,
                sizeof(disk_name)) != 0) {
            if (protected_mount || strncmp(device_id, "0:", 2) != 0) {
                fclose(mountinfo);
                return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
            }
            continue;
        }
        if (strcmp(disk_name, target_disk_name) == 0) {
            fclose(mountinfo);
            return CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE;
        }
        if (protected_mount &&
            !protected_filesystem_is_simple(mount_point, line)) {
            fclose(mountinfo);
            return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
        }
    }

    if (ferror(mountinfo)) {
        fclose(mountinfo);
        return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
    }
    fclose(mountinfo);
    return found_root ? CLASSICSETUP_SYSTEM_DISK_SAFE
                      : CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
}

enum classicsetup_system_disk_status classicsetup_check_system_disk(
    const char *target_disk_name)
{
    return classicsetup_check_system_disk_from(
        target_disk_name,
        "/proc/self/mountinfo",
        "/sys/dev/block");
}
