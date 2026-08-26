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

static int parse_mount_details(
    const char *line,
    char *filesystem,
    size_t filesystem_size,
    char *source,
    size_t source_size)
{
    const char *separator = strstr(line, " - ");
    char parsed_filesystem[64];
    char parsed_source[256];

    if (separator == NULL ||
        sscanf(
            separator + 3,
            "%63s %255s",
            parsed_filesystem,
            parsed_source) != 2 ||
        strlen(parsed_filesystem) >= filesystem_size ||
        strlen(parsed_source) >= source_size) {
        return -1;
    }
    strcpy(filesystem, parsed_filesystem);
    strcpy(source, parsed_source);
    return 0;
}

static int filesystem_is_pseudo(const char *filesystem)
{
    static const char *const pseudo_filesystems[] = {
        "autofs",
        "binfmt_misc",
        "bpf",
        "cgroup",
        "cgroup2",
        "configfs",
        "debugfs",
        "devpts",
        "devtmpfs",
        "efivarfs",
        "fusectl",
        "hugetlbfs",
        "mqueue",
        "nsfs",
        "proc",
        "pstore",
        "ramfs",
        "rpc_pipefs",
        "securityfs",
        "sysfs",
        "tmpfs",
        "tracefs"
    };
    size_t index;

    for (index = 0;
         index < sizeof(pseudo_filesystems) / sizeof(pseudo_filesystems[0]);
         ++index) {
        if (strcmp(filesystem, pseudo_filesystems[index]) == 0) {
            return 1;
        }
    }
    return 0;
}

static int source_is_loop_device(const char *source)
{
    return strncmp(source, "/dev/loop", strlen("/dev/loop")) == 0;
}

static int mount_is_standalone_loop_or_snap(
    const char *mount_point,
    const char *filesystem,
    const char *source)
{
    return source_is_loop_device(source) ||
           (strcmp(filesystem, "squashfs") == 0 &&
            (strcmp(mount_point, "/snap") == 0 ||
             strncmp(mount_point, "/snap/", strlen("/snap/")) == 0));
}

static int source_is_target_device(
    const char *source,
    const char *target_disk_name)
{
    const char *name;
    const char *suffix;

    if (strncmp(source, "/dev/", strlen("/dev/")) != 0) {
        return 0;
    }
    name = source + strlen("/dev/");
    if (strncmp(name, target_disk_name, strlen(target_disk_name)) != 0) {
        return 0;
    }
    suffix = name + strlen(target_disk_name);
    if (*suffix == '\0') {
        return 1;
    }
    if (*suffix == 'p') {
        ++suffix;
    }
    if (*suffix == '\0') {
        return 0;
    }
    while (*suffix != '\0') {
        if (*suffix < '0' || *suffix > '9') {
            return 0;
        }
        ++suffix;
    }
    return 1;
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
        char filesystem[64];
        char source[256];
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
        if (parse_mount_details(
                line,
                filesystem,
                sizeof(filesystem),
                source,
                sizeof(source)) != 0) {
            if (protected_mount || strncmp(device_id, "0:", 2) != 0) {
                fclose(mountinfo);
                return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
            }
            continue;
        }
        if (resolve_mount_disk(
                sys_dev_block_path,
                device_id,
                disk_name,
                sizeof(disk_name)) != 0) {
            if (protected_mount) {
                fclose(mountinfo);
                return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
            }
            if (source_is_target_device(source, target_disk_name)) {
                fclose(mountinfo);
                return CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE;
            }
            if (filesystem_is_pseudo(filesystem) ||
                mount_is_standalone_loop_or_snap(
                    mount_point,
                    filesystem,
                    source) ||
                strncmp(device_id, "0:", 2) == 0) {
                continue;
            }
            fclose(mountinfo);
            return CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
        }
        if (strcmp(disk_name, target_disk_name) == 0) {
            fclose(mountinfo);
            return CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE;
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
