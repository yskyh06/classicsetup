#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "classicsetup/apply.h"

#ifndef CLASSICSETUP_TEST_FIXTURE_DIR
#define CLASSICSETUP_TEST_FIXTURE_DIR "tests/fixtures"
#endif

static void make_fixture_path(
    char *path,
    size_t path_size,
    const char *environment,
    const char *file)
{
    int written = snprintf(
        path,
        path_size,
        "%s/environment/%s/%s",
        CLASSICSETUP_TEST_FIXTURE_DIR,
        environment,
        file);

    assert(written >= 0 && (size_t)written < path_size);
}

static enum classicsetup_environment detect_fixture(const char *name)
{
    char version[512];
    char osrelease[512];
    char dmi[512];
    enum classicsetup_environment environment;

    make_fixture_path(version, sizeof(version), name, "version");
    make_fixture_path(osrelease, sizeof(osrelease), name, "osrelease");
    make_fixture_path(dmi, sizeof(dmi), name, "dmi");
    assert(classicsetup_detect_environment_from(
               version,
               osrelease,
               dmi,
               &environment) == 0);
    return environment;
}

static struct classicsetup_apply_plan make_apply_plan(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan partition_plan;
    struct classicsetup_apply_plan apply_plan;
    size_t target_index;

    strcpy(disk.name, "sdz");
    strcpy(disk.device_path, "/dev/sdz");
    strcpy(disk.model, "ClassicSetup Test Disk");
    disk.size_bytes = 8192ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;

    assert(classicsetup_plan_init(&disk, NULL, 0, &partition_plan) == 0);
    assert(classicsetup_plan_prepare_install_target(
               &partition_plan,
               0,
               &target_index) == 0);
    assert(partition_plan.items[target_index].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(classicsetup_build_apply_plan(
               &disk,
               &partition_plan,
               0,
               &apply_plan) == 0);
    return apply_plan;
}

static struct classicsetup_apply_plan make_mbr_apply_plan(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan partition_plan;
    struct classicsetup_apply_plan apply_plan;
    size_t target_index;

    strcpy(disk.name, "sdz");
    strcpy(disk.device_path, "/dev/sdz");
    strcpy(disk.model, "ClassicSetup Test Disk");
    disk.size_bytes = 8192ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;

    assert(classicsetup_plan_init(&disk, NULL, 0, &partition_plan) == 0);
    assert(classicsetup_plan_prepare_install_target_for_mode(
               &partition_plan,
               CLASSICSETUP_INSTALL_BIOS_MBR,
               0,
               &target_index) == 0);
    assert(partition_plan.items[target_index].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(classicsetup_build_apply_plan_for_mode(
               CLASSICSETUP_INSTALL_BIOS_MBR,
               &disk,
               &partition_plan,
               0,
               &apply_plan) == 0);
    return apply_plan;
}

static void test_environment_detection(void)
{
    char virtualbox_dmi[512];
    enum classicsetup_environment environment;

    assert(detect_fixture("wsl") == CLASSICSETUP_ENV_WSL);
    assert(detect_fixture("virtualbox") == CLASSICSETUP_ENV_VIRTUALBOX);
    assert(detect_fixture("vmware") == CLASSICSETUP_ENV_VMWARE);
    assert(detect_fixture("unknown") == CLASSICSETUP_ENV_UNKNOWN);
    assert(!classicsetup_environment_allows_apply(CLASSICSETUP_ENV_WSL));
    assert(!classicsetup_environment_allows_apply(CLASSICSETUP_ENV_UNKNOWN));
    assert(classicsetup_environment_allows_apply(
        CLASSICSETUP_ENV_VIRTUALBOX));
    assert(classicsetup_environment_allows_apply(CLASSICSETUP_ENV_VMWARE));
    assert(classicsetup_destructive_unlock_enabled("YES"));
    assert(!classicsetup_destructive_unlock_enabled(NULL));
    assert(!classicsetup_destructive_unlock_enabled("yes"));
    assert(!classicsetup_destructive_unlock_enabled("YES "));

    make_fixture_path(
        virtualbox_dmi,
        sizeof(virtualbox_dmi),
        "virtualbox",
        "dmi");
    assert(classicsetup_detect_environment_from(
               "/missing/classicsetup-version",
               "/missing/classicsetup-osrelease",
               virtualbox_dmi,
               &environment) == 0);
    assert(environment == CLASSICSETUP_ENV_UNKNOWN);
}

static void test_apply_plan_and_golden_script(void)
{
    struct classicsetup_apply_plan apply_plan = make_apply_plan();
    char script[CLASSICSETUP_SFDISK_SCRIPT_SIZE];
    const char *expected =
        "label: gpt\n"
        "unit: sectors\n"
        "\n"
        "start=2048, size=532480, "
        "type=c12a7328-f81f-11d2-ba4b-00a0c93ec93b, "
        "name=\"EFI System Partition\"\n"
        "start=534528, size=32768, "
        "type=e3c9e316-0b5c-4db8-817d-f92df00215ae, "
        "name=\"Microsoft Reserved Partition\"\n"
        "start=567296, size=14110720, "
        "type=ebd0a0a2-b9e5-4433-87c0-68b6b72699c7, "
        "name=\"Windows Partition\"\n"
        "start=14678016, size=2097152, "
        "type=de94bba4-06d1-4d40-a16a-bfd50179d6ac, "
        "name=\"Windows Recovery Partition\"\n";
    enum classicsetup_partition_role roles[] = {
        CLASSICSETUP_PARTITION_ROLE_EFI,
        CLASSICSETUP_PARTITION_ROLE_MSR,
        CLASSICSETUP_PARTITION_ROLE_WINDOWS,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY
    };
    const char *guids[] = {
        CLASSICSETUP_GPT_TYPE_EFI,
        CLASSICSETUP_GPT_TYPE_MSR,
        CLASSICSETUP_GPT_TYPE_BASIC_DATA,
        CLASSICSETUP_GPT_TYPE_RECOVERY
    };
    size_t index;

    assert(classicsetup_validate_apply_plan(&apply_plan));
    assert(apply_plan.table_type == CLASSICSETUP_PARTITION_TABLE_GPT);
    assert(apply_plan.partition_count == 4);
    for (index = 0; index < apply_plan.partition_count; ++index) {
        assert(apply_plan.partitions[index].role == roles[index]);
        assert(strcmp(apply_plan.partitions[index].type_guid, guids[index]) == 0);
    }
    assert(classicsetup_render_sfdisk_script(
               &apply_plan,
               script,
               sizeof(script)) == 0);
    assert(strcmp(script, expected) == 0);
    assert(classicsetup_render_sfdisk_script(
               &apply_plan,
               script,
               16) == -1);
}

static void test_mbr_apply_plan_render_and_gate(void)
{
    struct classicsetup_apply_plan apply_plan = make_mbr_apply_plan();
    struct classicsetup_apply_plan invalid;
    struct classicsetup_apply_result result;
    char script[CLASSICSETUP_SFDISK_SCRIPT_SIZE];
    const char *expected =
        "label: dos\n"
        "unit: sectors\n"
        "\n"
        "start=2048, size=1126400, type=07, bootable\n"
        "start=1128448, size=13551616, type=07\n"
        "start=14680064, size=2097152, type=27\n";

    assert(apply_plan.table_type == CLASSICSETUP_PARTITION_TABLE_MBR);
    assert(apply_plan.partition_count == 3);
    assert(apply_plan.partitions[0].role ==
           CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED);
    assert(apply_plan.partitions[0].mbr_type == CLASSICSETUP_MBR_TYPE_NTFS);
    assert(apply_plan.partitions[0].bootable);
    assert(apply_plan.partitions[1].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(apply_plan.partitions[1].mbr_type == CLASSICSETUP_MBR_TYPE_NTFS);
    assert(!apply_plan.partitions[1].bootable);
    assert(apply_plan.partitions[2].role ==
           CLASSICSETUP_PARTITION_ROLE_RECOVERY);
    assert(apply_plan.partitions[2].mbr_type ==
           CLASSICSETUP_MBR_TYPE_RECOVERY);
    assert(!apply_plan.partitions[2].bootable);
    assert(classicsetup_validate_apply_plan(&apply_plan));
    assert(classicsetup_render_sfdisk_script(
               &apply_plan,
               script,
               sizeof(script)) == 0);
    assert(strcmp(script, expected) == 0);

    invalid = apply_plan;
    invalid.partitions[0].bootable = 0;
    assert(!classicsetup_validate_apply_plan(&invalid));
    invalid = apply_plan;
    invalid.partitions[0].role = CLASSICSETUP_PARTITION_ROLE_EFI;
    assert(!classicsetup_validate_apply_plan(&invalid));
    invalid = apply_plan;
    invalid.disk_sector_count = CLASSICSETUP_MBR_MAX_SECTORS + 1ULL;
    invalid.target_disk.size_bytes = invalid.disk_sector_count *
                                     CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(!classicsetup_validate_apply_plan(&invalid));

    memset(&result, 0x5a, sizeof(result));
    assert(classicsetup_execute_apply_plan(&apply_plan, &result) == 0);
    assert(result.code == CLASSICSETUP_APPLY_RESULT_BLOCKED);
    assert(result.safety_code ==
           CLASSICSETUP_APPLY_SAFETY_MBR_NOT_ENABLED);
    assert(!result.process.exited);
}

static void test_apply_plan_rejections(void)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan partition_plan;
    struct classicsetup_partition_plan before;
    struct classicsetup_apply_plan apply_plan;
    struct classicsetup_apply_plan unchanged;
    size_t target_index;
    size_t efi_index;
    size_t generic_index;

    strcpy(disk.name, "sdz");
    strcpy(disk.device_path, "/dev/sdz");
    strcpy(disk.model, "ClassicSetup Test Disk");
    disk.size_bytes = 8192ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &partition_plan) == 0);
    memset(&apply_plan, 0x5a, sizeof(apply_plan));
    unchanged = apply_plan;
    before = partition_plan;
    assert(classicsetup_build_apply_plan(
               &disk,
               &partition_plan,
               0,
               &apply_plan) == -1);
    assert(memcmp(&partition_plan, &before, sizeof(partition_plan)) == 0);
    assert(memcmp(&apply_plan, &unchanged, sizeof(apply_plan)) == 0);

    assert(classicsetup_plan_prepare_install_target(
               &partition_plan,
               0,
               &target_index) == 0);
    assert(classicsetup_build_apply_plan(
               &disk,
               &partition_plan,
               1,
               &apply_plan) == -1);

    for (efi_index = 0;
         efi_index < partition_plan.item_count;
         ++efi_index) {
        if (partition_plan.items[efi_index].role ==
            CLASSICSETUP_PARTITION_ROLE_EFI) {
            break;
        }
    }
    assert(efi_index < partition_plan.item_count);
    assert(classicsetup_plan_delete_partition(
               &partition_plan,
               efi_index) == 0);
    assert(classicsetup_build_apply_plan(
               &disk,
               &partition_plan,
               0,
               &apply_plan) == -1);

    apply_plan = make_apply_plan();
    apply_plan.partitions[0].start_sector = 0;
    assert(!classicsetup_validate_apply_plan(&apply_plan));
    apply_plan = make_apply_plan();
    strcpy(apply_plan.partitions[0].name, "Injected\" name");
    assert(!classicsetup_validate_apply_plan(&apply_plan));

    assert(classicsetup_plan_init(&disk, NULL, 0, &partition_plan) == 0);
    assert(classicsetup_plan_create_partition(
               &partition_plan,
               0,
               1,
               &generic_index) == 0);
    assert(partition_plan.items[generic_index].role ==
           CLASSICSETUP_PARTITION_ROLE_GENERIC);
    for (target_index = 0;
         target_index < partition_plan.item_count;
         ++target_index) {
        if (partition_plan.items[target_index].state ==
                CLASSICSETUP_PLAN_UNALLOCATED &&
            partition_plan.items[target_index].sector_count >
                2048ULL * CLASSICSETUP_SECTORS_PER_MB) {
            break;
        }
    }
    assert(target_index < partition_plan.item_count);
    assert(classicsetup_plan_create_windows_layout(
               &partition_plan,
               target_index,
               &efi_index) == 0);
    assert(classicsetup_build_apply_plan(
               &disk,
               &partition_plan,
               0,
               &apply_plan) == -1);
}

static struct classicsetup_apply_safety_inputs safe_inputs(void)
{
    struct classicsetup_apply_safety_inputs inputs = {
        .environment = CLASSICSETUP_ENV_VMWARE,
        .destructive_unlocked = 1,
        .disk_identity_valid = 1,
        .system_disk_status = CLASSICSETUP_SYSTEM_DISK_SAFE,
        .existing_partition_count = 0,
        .logical_sector_size_supported = 1,
        .apply_plan_valid = 1,
        .tool_available = 1
    };

    return inputs;
}

static void test_safety_matrix(void)
{
    struct classicsetup_apply_safety_inputs inputs = safe_inputs();

    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_OK);
    inputs.table_type = CLASSICSETUP_PARTITION_TABLE_MBR;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_MBR_NOT_ENABLED);
    inputs.table_type = CLASSICSETUP_PARTITION_TABLE_GPT;
    inputs.environment = CLASSICSETUP_ENV_VIRTUALBOX;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_OK);
    inputs.environment = CLASSICSETUP_ENV_WSL;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_WSL);
    inputs.environment = CLASSICSETUP_ENV_UNKNOWN;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_NOT_SUPPORTED_VM);

    inputs = safe_inputs();
    inputs.destructive_unlocked = 0;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_LOCKED);
    inputs = safe_inputs();
    inputs.disk_identity_valid = 0;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_DISK_IDENTITY);
    inputs = safe_inputs();
    inputs.system_disk_status = CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK);
    inputs.system_disk_status = CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK_UNKNOWN);
    inputs = safe_inputs();
    inputs.existing_partition_count = 1;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_EXISTING_PARTITIONS);
    inputs = safe_inputs();
    inputs.logical_sector_size_supported = 0;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_UNSUPPORTED_SECTOR_SIZE);
    inputs = safe_inputs();
    inputs.apply_plan_valid = 0;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_INVALID_PLAN);
    inputs = safe_inputs();
    inputs.tool_available = 0;
    assert(classicsetup_evaluate_apply_safety(&inputs) ==
           CLASSICSETUP_APPLY_SAFETY_TOOL_UNAVAILABLE);
}

static void write_mountinfo_entry(
    FILE *file,
    unsigned int mount_id,
    const char *device_id,
    const char *mount_point,
    const char *filesystem,
    const char *source)
{
    assert(file != NULL);
    assert(fprintf(
               file,
               "%u 1 %s / %s rw - %s %s rw\n",
               mount_id,
               device_id,
               mount_point,
               filesystem,
               source) > 0);
}

static void write_root_mount(
    const char *path,
    const char *device_id,
    const char *source)
{
    FILE *file = fopen(path, "w");

    write_mountinfo_entry(file, 24, device_id, "/", "ext4", source);
    assert(fclose(file) == 0);
}

static void test_system_disk_protection(void)
{
    char directory[] = "/tmp/classicsetup-system-XXXXXX";
    char sys_path[512];
    char mountinfo_path[512];
    char sda_link[512];
    char sdb_partition_link[512];
    char sdb_root_link[512];
    char sdc_link[512];
    char loop_link[512];
    char dm_link[512];

    assert(mkdtemp(directory) != NULL);
    assert(snprintf(
               sys_path,
               sizeof(sys_path),
               "%s/sys-dev-block",
               directory) > 0);
    assert(mkdir(sys_path, 0700) == 0);
    assert(snprintf(
               mountinfo_path,
               sizeof(mountinfo_path),
               "%s/mountinfo",
               directory) > 0);
    assert(snprintf(
               sda_link,
               sizeof(sda_link),
               "%s/8:2",
               sys_path) > 0);
    assert(snprintf(
               sdb_partition_link,
               sizeof(sdb_partition_link),
               "%s/8:17",
               sys_path) > 0);
    assert(snprintf(
               sdb_root_link,
               sizeof(sdb_root_link),
               "%s/8:18",
               sys_path) > 0);
    assert(snprintf(
               sdc_link,
               sizeof(sdc_link),
               "%s/8:33",
               sys_path) > 0);
    assert(snprintf(
               loop_link,
               sizeof(loop_link),
               "%s/7:0",
               sys_path) > 0);
    assert(snprintf(
               dm_link,
               sizeof(dm_link),
               "%s/253:0",
               sys_path) > 0);
    assert(symlink("../../devices/pci/block/sda/sda2", sda_link) == 0);
    assert(symlink(
               "../../devices/pci/block/sdb/sdb1",
               sdb_partition_link) == 0);
    assert(symlink(
               "../../devices/pci/block/sdb/sdb2",
               sdb_root_link) == 0);
    assert(symlink("../../devices/pci/block/sdc/sdc1", sdc_link) == 0);
    assert(symlink("../../devices/virtual/block/loop0", loop_link) == 0);
    assert(symlink("../../devices/virtual/block/dm-0", dm_link) == 0);

    write_root_mount(mountinfo_path, "8:2", "/dev/sda2");
    {
        FILE *file = fopen(mountinfo_path, "a");
        unsigned int index;

        assert(file != NULL);
        for (index = 0; index <= 10; ++index) {
            char device_id[32];
            char mount_point[64];
            char source[32];

            assert(snprintf(
                       device_id,
                       sizeof(device_id),
                       "7:%u",
                       index) > 0);
            assert(snprintf(
                       mount_point,
                       sizeof(mount_point),
                       "/snap/test/%u",
                       index) > 0);
            assert(snprintf(
                       source,
                       sizeof(source),
                       "/dev/loop%u",
                       index) > 0);
            write_mountinfo_entry(
                file,
                30 + index,
                device_id,
                mount_point,
                "squashfs",
                source);
        }
        write_mountinfo_entry(
            file,
            50,
            "0:55",
            "/run/user/1000",
            "tmpfs",
            "tmpfs");
        assert(fclose(file) == 0);
    }

    assert(classicsetup_check_system_disk_from(
               "sdb",
               mountinfo_path,
               sys_path) == CLASSICSETUP_SYSTEM_DISK_SAFE);

    write_root_mount(mountinfo_path, "8:18", "/dev/sdb2");
    assert(classicsetup_check_system_disk_from(
               "sdb",
               mountinfo_path,
               sys_path) == CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE);

    write_root_mount(mountinfo_path, "8:2", "/dev/sda2");
    {
        FILE *file = fopen(mountinfo_path, "a");

        write_mountinfo_entry(
            file,
            25,
            "8:17",
            "/mnt/test",
            "ext4",
            "/dev/sdb1");
        assert(fclose(file) == 0);
    }
    assert(classicsetup_check_system_disk_from(
               "sdb",
               mountinfo_path,
               sys_path) == CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE);

    write_root_mount(mountinfo_path, "8:2", "/dev/sda2");
    {
        FILE *file = fopen(mountinfo_path, "a");

        write_mountinfo_entry(
            file,
            26,
            "8:33",
            "/srv/data",
            "ext4",
            "/dev/sdc1");
        assert(fclose(file) == 0);
    }
    assert(classicsetup_check_system_disk_from(
               "sdb",
               mountinfo_path,
               sys_path) == CLASSICSETUP_SYSTEM_DISK_SAFE);

    write_root_mount(mountinfo_path, "8:99", "/dev/sda2");
    assert(classicsetup_check_system_disk_from(
               "sdb",
               mountinfo_path,
               sys_path) == CLASSICSETUP_SYSTEM_DISK_UNKNOWN);

    write_root_mount(mountinfo_path, "253:0", "/dev/mapper/ubuntu-root");
    assert(classicsetup_check_system_disk_from(
               "sdb",
               mountinfo_path,
               sys_path) == CLASSICSETUP_SYSTEM_DISK_UNKNOWN);

    write_root_mount(mountinfo_path, "8:2", "/dev/sda2");
    {
        FILE *file = fopen(mountinfo_path, "a");

        write_mountinfo_entry(
            file,
            27,
            "253:0",
            "/mnt/target-stack",
            "ext4",
            "/dev/mapper/sdb-data");
        assert(fclose(file) == 0);
    }
    assert(classicsetup_check_system_disk_from(
               "sdb",
               mountinfo_path,
               sys_path) == CLASSICSETUP_SYSTEM_DISK_UNKNOWN);

    assert(unlink(dm_link) == 0);
    assert(unlink(loop_link) == 0);
    assert(unlink(sdc_link) == 0);
    assert(unlink(sdb_root_link) == 0);
    assert(unlink(sdb_partition_link) == 0);
    assert(unlink(sda_link) == 0);
    assert(unlink(mountinfo_path) == 0);
    assert(rmdir(sys_path) == 0);
    assert(rmdir(directory) == 0);
}

static void test_disk_identity_and_post_apply_verification(void)
{
    struct classicsetup_apply_plan apply_plan = make_apply_plan();
    struct classicsetup_disk_info current = apply_plan.target_disk;
    struct classicsetup_partition_info partitions[4] = {0};
    size_t index;

    assert(classicsetup_disk_identity_matches(
        &apply_plan.target_disk,
        &current));
    ++current.size_bytes;
    assert(!classicsetup_disk_identity_matches(
        &apply_plan.target_disk,
        &current));

    for (index = 0; index < 4; ++index) {
        partitions[index].start_sector =
            apply_plan.partitions[index].start_sector;
        partitions[index].sector_count =
            apply_plan.partitions[index].sector_count;
    }
    assert(classicsetup_verify_partition_ranges(
        &apply_plan,
        partitions,
        4));
    ++partitions[2].sector_count;
    assert(!classicsetup_verify_partition_ranges(
        &apply_plan,
        partitions,
        4));
    assert(!classicsetup_verify_partition_ranges(
        &apply_plan,
        partitions,
        3));
}

static void test_process_exit_status(void)
{
    struct classicsetup_process_result result;
    char *false_arguments[] = {"/bin/false", NULL};
    char *true_arguments[] = {"/bin/true", NULL};

    assert(classicsetup_run_process_with_input(
               "/bin/false",
               false_arguments,
               "",
               &result) == 0);
    assert(result.exited);
    assert(result.exit_status != 0);
    assert(classicsetup_run_process_with_input(
               "/bin/true",
               true_arguments,
               "test input",
               &result) == 0);
    assert(result.exited);
    assert(result.exit_status == 0);
}

static void test_current_wsl_execution_is_blocked(void)
{
    enum classicsetup_environment environment;

    if (classicsetup_detect_environment(&environment) == 0 &&
        environment == CLASSICSETUP_ENV_WSL) {
        struct classicsetup_apply_plan apply_plan = make_apply_plan();
        struct classicsetup_apply_result result;

        assert(classicsetup_execute_apply_plan(&apply_plan, &result) == 0);
        assert(result.code == CLASSICSETUP_APPLY_RESULT_BLOCKED);
        assert(result.safety_code == CLASSICSETUP_APPLY_SAFETY_WSL);
        assert(!result.process.exited);
    }
}

int main(void)
{
    test_environment_detection();
    test_apply_plan_and_golden_script();
    test_mbr_apply_plan_render_and_gate();
    test_apply_plan_rejections();
    test_safety_matrix();
    test_system_disk_protection();
    test_disk_identity_and_post_apply_verification();
    test_process_exit_status();
    test_current_wsl_execution_is_blocked();
    return 0;
}
