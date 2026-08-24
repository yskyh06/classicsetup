#include "classicsetup/partition.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SYSFS_PATH_SIZE = 512,
    SYSFS_VALUE_SIZE = 64
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

static int parse_unsigned_long_long(
    const char *text,
    unsigned long long *value)
{
    char *end;
    unsigned long long parsed;

    errno = 0;
    parsed = strtoull(text, &end, 10);
    if (errno != 0 || end == text) {
        return -1;
    }
    while (isspace((unsigned char)*end)) {
        ++end;
    }
    if (*end != '\0') {
        return -1;
    }

    *value = parsed;
    return 0;
}

static int read_sector_value(
    const char *partition_path,
    const char *file_name,
    unsigned long long *value)
{
    char path[SYSFS_PATH_SIZE];
    char text[SYSFS_VALUE_SIZE];
    int written = snprintf(
        path,
        sizeof(path),
        "%s/%s",
        partition_path,
        file_name);

    if (written < 0 || (size_t)written >= sizeof(path) ||
        read_text_file(path, text, sizeof(text)) != 0) {
        return -1;
    }
    return parse_unsigned_long_long(text, value);
}

static int is_partition_entry(
    const char *disk_path,
    const char *disk_name,
    const char *entry_name,
    unsigned int *number)
{
    char partition_path[SYSFS_PATH_SIZE];
    unsigned long long parsed_number;
    size_t disk_name_length = strlen(disk_name);
    int written;

    if (strncmp(entry_name, disk_name, disk_name_length) != 0 ||
        entry_name[disk_name_length] == '\0') {
        return 0;
    }

    written = snprintf(
        partition_path,
        sizeof(partition_path),
        "%s/%s",
        disk_path,
        entry_name);
    if (written < 0 || (size_t)written >= sizeof(partition_path) ||
        read_sector_value(partition_path, "partition", &parsed_number) != 0 ||
        parsed_number > UINT_MAX) {
        return 0;
    }

    *number = (unsigned int)parsed_number;
    return 1;
}

static int read_partition(
    const char *disk_path,
    const char *name,
    unsigned int number,
    struct classicsetup_partition_info *partition)
{
    char partition_path[SYSFS_PATH_SIZE];
    int written = snprintf(
        partition_path,
        sizeof(partition_path),
        "%s/%s",
        disk_path,
        name);

    if (written < 0 || (size_t)written >= sizeof(partition_path) ||
        read_sector_value(
            partition_path,
            "start",
            &partition->start_sector) != 0 ||
        read_sector_value(
            partition_path,
            "size",
            &partition->sector_count) != 0 ||
        partition->sector_count >
            ULLONG_MAX / CLASSICSETUP_SECTOR_SIZE_BYTES) {
        return -1;
    }

    snprintf(partition->name, sizeof(partition->name), "%s", name);
    snprintf(
        partition->device_path,
        sizeof(partition->device_path),
        "/dev/%s",
        name);
    partition->number = number;
    partition->size_bytes =
        partition->sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;
    return 0;
}

static int compare_partitions(const void *left, const void *right)
{
    const struct classicsetup_partition_info *left_partition = left;
    const struct classicsetup_partition_info *right_partition = right;

    if (left_partition->start_sector < right_partition->start_sector) {
        return -1;
    }
    if (left_partition->start_sector > right_partition->start_sector) {
        return 1;
    }
    if (left_partition->number < right_partition->number) {
        return -1;
    }
    if (left_partition->number > right_partition->number) {
        return 1;
    }
    return strcmp(left_partition->name, right_partition->name);
}

int classicsetup_scan_partitions_from(
    const char *sys_block_path,
    const struct classicsetup_disk_info *disk,
    struct classicsetup_partition_info *partitions,
    size_t capacity,
    size_t *partition_count)
{
    char disk_path[SYSFS_PATH_SIZE];
    struct dirent *entry = NULL;
    DIR *directory;
    int written;

    if (sys_block_path == NULL || disk == NULL || partition_count == NULL ||
        (capacity > 0 && partitions == NULL)) {
        return -1;
    }

    *partition_count = 0;
    written = snprintf(
        disk_path,
        sizeof(disk_path),
        "%s/%s",
        sys_block_path,
        disk->name);
    if (written < 0 || (size_t)written >= sizeof(disk_path)) {
        return -1;
    }

    directory = opendir(disk_path);
    if (directory == NULL) {
        return -1;
    }

    errno = 0;
    while (*partition_count < capacity) {
        struct classicsetup_partition_info partition = {0};
        unsigned int number;

        errno = 0;
        entry = readdir(directory);
        if (entry == NULL) {
            break;
        }
        if (entry->d_name[0] == '.' ||
            strlen(entry->d_name) >= CLASSICSETUP_PARTITION_NAME_SIZE ||
            !is_partition_entry(
                disk_path,
                disk->name,
                entry->d_name,
                &number) ||
            read_partition(
                disk_path,
                entry->d_name,
                number,
                &partition) != 0) {
            continue;
        }

        partitions[*partition_count] = partition;
        ++*partition_count;
    }

    if (entry == NULL && errno != 0) {
        closedir(directory);
        return -1;
    }

    closedir(directory);
    if (*partition_count > 1) {
        qsort(
            partitions,
            *partition_count,
            sizeof(*partitions),
            compare_partitions);
    }
    return 0;
}

int classicsetup_scan_partitions(
    const struct classicsetup_disk_info *disk,
    struct classicsetup_partition_info *partitions,
    size_t capacity,
    size_t *partition_count)
{
    return classicsetup_scan_partitions_from(
        "/sys/block",
        disk,
        partitions,
        capacity,
        partition_count);
}

static void add_unallocated_space(
    unsigned long long start_sector,
    unsigned long long sector_count,
    struct classicsetup_unallocated_info *spaces,
    size_t capacity,
    size_t *space_count)
{
    if (sector_count == 0 || *space_count >= capacity) {
        return;
    }

    spaces[*space_count].start_sector = start_sector;
    spaces[*space_count].sector_count = sector_count;
    spaces[*space_count].size_bytes =
        sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;
    ++*space_count;
}

int classicsetup_calculate_unallocated(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_unallocated_info *spaces,
    size_t capacity,
    size_t *space_count)
{
    unsigned long long total_sectors;
    unsigned long long cursor = 0;
    size_t index;

    if (disk == NULL || space_count == NULL ||
        (partition_count > 0 && partitions == NULL) ||
        (capacity > 0 && spaces == NULL)) {
        return -1;
    }

    *space_count = 0;
    total_sectors = disk->size_bytes / CLASSICSETUP_SECTOR_SIZE_BYTES;

    for (index = 0; index < partition_count; ++index) {
        unsigned long long partition_start = partitions[index].start_sector;
        unsigned long long partition_end;

        if (partition_start > total_sectors) {
            partition_start = total_sectors;
        }
        if (partition_start > cursor) {
            add_unallocated_space(
                cursor,
                partition_start - cursor,
                spaces,
                capacity,
                space_count);
        }

        if (partitions[index].sector_count >
            ULLONG_MAX - partitions[index].start_sector) {
            partition_end = total_sectors;
        } else {
            partition_end = partitions[index].start_sector +
                            partitions[index].sector_count;
            if (partition_end > total_sectors) {
                partition_end = total_sectors;
            }
        }
        if (partition_end > cursor) {
            cursor = partition_end;
        }
    }

    if (cursor < total_sectors) {
        add_unallocated_space(
            cursor,
            total_sectors - cursor,
            spaces,
            capacity,
            space_count);
    }
    return 0;
}
