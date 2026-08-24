#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "classicsetup/disk.h"

static void test_mock_disks(void)
{
    struct classicsetup_disk_info disks[8];
    char path[512];
    size_t count = 0;

    snprintf(
        path,
        sizeof(path),
        "%s/sys_block",
        CLASSICSETUP_TEST_FIXTURE_DIR);

    assert(classicsetup_scan_disks_from(path, disks, 8, &count) == 0);
    assert(count == 2);

    assert(strcmp(disks[0].name, "nvme0n1") == 0);
    assert(strcmp(disks[0].device_path, "/dev/nvme0n1") == 0);
    assert(strcmp(disks[0].model, "Mock NVMe Disk") == 0);
    assert(disks[0].size_bytes == 1024000000ULL);

    assert(strcmp(disks[1].name, "sda") == 0);
    assert(strcmp(disks[1].device_path, "/dev/sda") == 0);
    assert(strcmp(disks[1].model, "Mock Virtual Disk") == 0);
    assert(disks[1].size_bytes == 64000000000ULL);
}

static void test_no_disks(void)
{
    struct classicsetup_disk_info disks[2];
    char path[512];
    size_t count = 99;

    snprintf(
        path,
        sizeof(path),
        "%s/empty_sys_block",
        CLASSICSETUP_TEST_FIXTURE_DIR);

    assert(classicsetup_scan_disks_from(path, disks, 2, &count) == 0);
    assert(count == 0);
}

static void test_missing_sysfs(void)
{
    struct classicsetup_disk_info disks[2];
    size_t count = 99;

    assert(classicsetup_scan_disks_from(
               "/classicsetup/path/that/does/not/exist",
               disks,
               2,
               &count) == -1);
    assert(count == 0);
}

int main(void)
{
    test_mock_disks();
    test_no_disks();
    test_missing_sysfs();
    return 0;
}
