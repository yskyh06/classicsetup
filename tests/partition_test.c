#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "classicsetup/partition.h"

static struct classicsetup_disk_info make_disk(
    const char *name,
    unsigned long long sector_count)
{
    struct classicsetup_disk_info disk = {0};

    snprintf(disk.name, sizeof(disk.name), "%s", name);
    snprintf(disk.device_path, sizeof(disk.device_path), "/dev/%s", name);
    disk.size_bytes = sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;
    return disk;
}

static void fixture_path(char *path, size_t path_size, const char *directory)
{
    snprintf(
        path,
        path_size,
        "%s/%s",
        CLASSICSETUP_TEST_FIXTURE_DIR,
        directory);
}

static void test_sda_partitions_and_unallocated(void)
{
    struct classicsetup_disk_info disk = make_disk("sda", 125000000ULL);
    struct classicsetup_partition_info partitions[8];
    struct classicsetup_unallocated_info spaces[8];
    char path[512];
    size_t partition_count = 0;
    size_t space_count = 0;

    fixture_path(path, sizeof(path), "sys_block");
    assert(classicsetup_scan_partitions_from(
               path,
               &disk,
               partitions,
               8,
               &partition_count) == 0);
    assert(partition_count == 3);

    assert(strcmp(partitions[0].name, "sda1") == 0);
    assert(strcmp(partitions[0].device_path, "/dev/sda1") == 0);
    assert(partitions[0].number == 1);
    assert(partitions[0].start_sector == 2048ULL);
    assert(partitions[0].sector_count == 1000000ULL);
    assert(partitions[0].size_bytes == 512000000ULL);

    assert(strcmp(partitions[1].name, "sda4") == 0);
    assert(partitions[1].number == 4);
    assert(partitions[1].start_sector == 1500000ULL);
    assert(partitions[1].sector_count == 100000ULL);

    assert(strcmp(partitions[2].name, "sda2") == 0);
    assert(partitions[2].number == 2);
    assert(partitions[2].start_sector == 2000000ULL);
    assert(partitions[2].sector_count == 50000000ULL);

    assert(classicsetup_calculate_unallocated(
               &disk,
               partitions,
               partition_count,
               spaces,
               8,
               &space_count) == 0);
    assert(space_count == 4);
    assert(spaces[0].start_sector == 0ULL);
    assert(spaces[0].sector_count == 2048ULL);
    assert(spaces[1].start_sector == 1002048ULL);
    assert(spaces[1].sector_count == 497952ULL);
    assert(spaces[2].start_sector == 1600000ULL);
    assert(spaces[2].sector_count == 400000ULL);
    assert(spaces[3].start_sector == 52000000ULL);
    assert(spaces[3].sector_count == 73000000ULL);
}

static void test_nvme_partition_name(void)
{
    struct classicsetup_disk_info disk = make_disk("nvme0n1", 2000000ULL);
    struct classicsetup_partition_info partitions[4];
    char path[512];
    size_t count = 0;

    fixture_path(path, sizeof(path), "sys_block");
    assert(classicsetup_scan_partitions_from(
               path,
               &disk,
               partitions,
               4,
               &count) == 0);
    assert(count == 1);
    assert(strcmp(partitions[0].name, "nvme0n1p1") == 0);
    assert(strcmp(partitions[0].device_path, "/dev/nvme0n1p1") == 0);
    assert(partitions[0].number == 1);
}

static void test_no_partitions(void)
{
    struct classicsetup_disk_info disk = make_disk("sda", 10000ULL);
    struct classicsetup_partition_info partitions[2];
    struct classicsetup_unallocated_info spaces[2];
    char path[512];
    size_t partition_count = 99;
    size_t space_count = 99;

    fixture_path(path, sizeof(path), "partition_empty");
    assert(classicsetup_scan_partitions_from(
               path,
               &disk,
               partitions,
               2,
               &partition_count) == 0);
    assert(partition_count == 0);

    assert(classicsetup_calculate_unallocated(
               &disk,
               partitions,
               partition_count,
               spaces,
               2,
               &space_count) == 0);
    assert(space_count == 1);
    assert(spaces[0].start_sector == 0ULL);
    assert(spaces[0].sector_count == 10000ULL);
}

static void test_missing_sysfs(void)
{
    struct classicsetup_disk_info disk = make_disk("sda", 10000ULL);
    struct classicsetup_partition_info partitions[2];
    size_t count = 99;

    assert(classicsetup_scan_partitions_from(
               "/classicsetup/path/that/does/not/exist",
               &disk,
               partitions,
               2,
               &count) == -1);
    assert(count == 0);
}

int main(void)
{
    test_sda_partitions_and_unallocated();
    test_nvme_partition_name();
    test_no_partitions();
    test_missing_sysfs();
    return 0;
}
