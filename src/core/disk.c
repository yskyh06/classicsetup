#include "classicsetup/disk.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SYSFS_PATH_SIZE = 512
};

static int read_text_file(const char *path, char *value, size_t value_size)
{
    FILE *file;
    size_t length;

    if (value_size == 0) {
        return -1;
    }

    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }

    if (fgets(value, (int)value_size, file) == NULL) {
        fclose(file);
        return -1;
    }

    fclose(file);
    length = strlen(value);
    while (length > 0 && isspace((unsigned char)value[length - 1])) {
        value[--length] = '\0';
    }

    return 0;
}

static int is_ignored_device(const char *name)
{
    return strncmp(name, "loop", 4) == 0 ||
           strncmp(name, "ram", 3) == 0 ||
           strncmp(name, "fd", 2) == 0;
}

static int parse_size_bytes(const char *text, unsigned long long *size_bytes)
{
    char *end;
    unsigned long long sectors;

    errno = 0;
    sectors = strtoull(text, &end, 10);
    if (errno != 0 || end == text) {
        return -1;
    }

    while (isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0' || sectors > ULLONG_MAX / CLASSICSETUP_SECTOR_SIZE_BYTES) {
        return -1;
    }

    *size_bytes = sectors * CLASSICSETUP_SECTOR_SIZE_BYTES;
    return 0;
}

static int parse_unsigned(const char *text, unsigned int *value)
{
    char *end;
    unsigned long parsed;

    errno = 0;
    parsed = strtoul(text, &end, 10);
    while (isspace((unsigned char)*end)) {
        ++end;
    }
    if (errno != 0 || end == text || *end != '\0' || parsed > UINT_MAX) {
        return -1;
    }
    *value = (unsigned int)parsed;
    return 0;
}

static int read_disk_value(
    const char *sys_block_path,
    const char *name,
    const char *relative_path,
    char *value,
    size_t value_size)
{
    char path[SYSFS_PATH_SIZE];
    int written = snprintf(
        path,
        sizeof(path),
        "%s/%s/%s",
        sys_block_path,
        name,
        relative_path);

    return written < 0 || (size_t)written >= sizeof(path)
               ? -1
               : read_text_file(path, value, value_size);
}

static int read_disk(
    const char *sys_block_path,
    const char *name,
    struct classicsetup_disk_info *disk)
{
    char path[SYSFS_PATH_SIZE];
    char size_text[64];
    char value[CLASSICSETUP_DISK_ID_SIZE];
    unsigned int parsed;
    int written;

    written = snprintf(path, sizeof(path), "%s/%s/size", sys_block_path, name);
    if (written < 0 || (size_t)written >= sizeof(path) ||
        read_text_file(path, size_text, sizeof(size_text)) != 0 ||
        parse_size_bytes(size_text, &disk->size_bytes) != 0) {
        return -1;
    }

    snprintf(disk->name, sizeof(disk->name), "%s", name);
    snprintf(disk->device_path, sizeof(disk->device_path), "/dev/%s", name);

    written = snprintf(
        path,
        sizeof(path),
        "%s/%s/device/model",
        sys_block_path,
        name);
    if (written < 0 || (size_t)written >= sizeof(path) ||
        read_text_file(path, disk->model, sizeof(disk->model)) != 0 ||
        disk->model[0] == '\0') {
        snprintf(disk->model, sizeof(disk->model), "Unknown model");
    }

    if (read_disk_value(
            sys_block_path,
            name,
            "device/serial",
            disk->serial,
            sizeof(disk->serial)) == 0 &&
        disk->serial[0] != '\0') {
        disk->has_serial = true;
    }
    if (read_disk_value(
            sys_block_path,
            name,
            "device/wwid",
            disk->wwn,
            sizeof(disk->wwn)) != 0) {
        read_disk_value(
            sys_block_path,
            name,
            "wwid",
            disk->wwn,
            sizeof(disk->wwn));
    }
    disk->has_wwn = disk->wwn[0] != '\0';
    if (read_disk_value(
            sys_block_path,
            name,
            "queue/logical_block_size",
            value,
            sizeof(value)) == 0 &&
        parse_unsigned(value, &parsed) == 0 && parsed > 0) {
        disk->logical_sector_size = parsed;
        disk->has_logical_sector_size = true;
    }
    if (read_disk_value(
            sys_block_path,
            name,
            "removable",
            value,
            sizeof(value)) == 0 &&
        parse_unsigned(value, &parsed) == 0 && parsed <= 1U) {
        disk->removable = parsed == 1U;
        disk->has_removable = true;
    }
    read_disk_value(
        sys_block_path,
        name,
        "device/transport",
        disk->transport,
        sizeof(disk->transport));

    return 0;
}

int classicsetup_disk_has_recommended_identity(
    const struct classicsetup_disk_info *disk)
{
    return disk != NULL && disk->name[0] != '\0' &&
           disk->device_path[0] != '\0' && disk->size_bytes > 0 &&
           disk->model[0] != '\0' &&
           strcmp(disk->model, "Unknown model") != 0 &&
           (disk->has_serial || disk->has_wwn) &&
           disk->has_logical_sector_size &&
           disk->logical_sector_size == CLASSICSETUP_SECTOR_SIZE_BYTES &&
           disk->has_removable;
}

static int compare_disks(const void *left, const void *right)
{
    const struct classicsetup_disk_info *left_disk = left;
    const struct classicsetup_disk_info *right_disk = right;

    return strcmp(left_disk->name, right_disk->name);
}

int classicsetup_scan_disks_from(
    const char *sys_block_path,
    struct classicsetup_disk_info *disks,
    size_t capacity,
    size_t *disk_count)
{
    struct dirent *entry;
    DIR *directory;

    if (sys_block_path == NULL || disk_count == NULL ||
        (capacity > 0 && disks == NULL)) {
        return -1;
    }

    *disk_count = 0;
    directory = opendir(sys_block_path);
    if (directory == NULL) {
        return -1;
    }

    errno = 0;
    entry = NULL;
    while (*disk_count < capacity) {
        struct classicsetup_disk_info disk = {0};

        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) {
            break;
        }
        if (entry->d_name[0] == '.' || is_ignored_device(entry->d_name) ||
            strlen(entry->d_name) >= CLASSICSETUP_DISK_NAME_SIZE) {
            continue;
        }
        if (read_disk(sys_block_path, entry->d_name, &disk) != 0) {
            continue;
        }

        disks[*disk_count] = disk;
        ++*disk_count;
    }

    if (entry == NULL && errno != 0) {
        closedir(directory);
        return -1;
    }

    closedir(directory);
    if (*disk_count > 1) {
        qsort(disks, *disk_count, sizeof(*disks), compare_disks);
    }
    return 0;
}

int classicsetup_scan_disks(
    struct classicsetup_disk_info *disks,
    size_t capacity,
    size_t *disk_count)
{
    return classicsetup_scan_disks_from(
        "/sys/block",
        disks,
        capacity,
        disk_count);
}
