#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "classicsetup/format_apply.h"

static struct classicsetup_apply_plan make_partition_apply_plan(
    enum classicsetup_install_mode mode,
    const char *disk_name,
    const char *disk_path)
{
    struct classicsetup_disk_info disk = {0};
    struct classicsetup_partition_plan partition_plan;
    struct classicsetup_apply_plan apply_plan;
    size_t target_index;

    snprintf(disk.name, sizeof(disk.name), "%s", disk_name);
    snprintf(disk.device_path, sizeof(disk.device_path), "%s", disk_path);
    snprintf(disk.model, sizeof(disk.model), "%s", "M8 Test Disk");
    disk.size_bytes = 8192ULL * CLASSICSETUP_SECTORS_PER_MB *
                      CLASSICSETUP_SECTOR_SIZE_BYTES;
    assert(classicsetup_plan_init(&disk, NULL, 0, &partition_plan) == 0);
    assert(classicsetup_plan_prepare_install_target_for_mode(
               &partition_plan,
               mode,
               0,
               &target_index) == 0);
    assert(partition_plan.items[target_index].role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(classicsetup_build_apply_plan_for_mode(
               mode,
               &disk,
               &partition_plan,
               0,
               &apply_plan) == 0);
    return apply_plan;
}

static void make_role_formats(
    const struct classicsetup_apply_plan *apply_plan,
    enum classicsetup_format_mode windows_mode,
    struct classicsetup_format_plan
        formats[CLASSICSETUP_PARTITION_ROLE_COUNT])
{
    size_t index;

    memset(
        formats,
        0,
        sizeof(*formats) * CLASSICSETUP_PARTITION_ROLE_COUNT);
    for (index = 0; index < apply_plan->partition_count; ++index) {
        enum classicsetup_partition_role role =
            apply_plan->partitions[index].role;

        assert(classicsetup_format_policy_for_role(
                   role,
                   windows_mode,
                   &formats[role]) == 0);
    }
}

static void make_scanned_partitions(
    const struct classicsetup_apply_plan *apply_plan,
    const char *const paths[],
    struct classicsetup_partition_info *partitions,
    int reverse)
{
    size_t index;

    for (index = 0; index < apply_plan->partition_count; ++index) {
        size_t destination = reverse
                                 ? apply_plan->partition_count - index - 1
                                 : index;

        snprintf(
            partitions[destination].device_path,
            sizeof(partitions[destination].device_path),
            "%s",
            paths[index]);
        partitions[destination].number = (unsigned int)index + 1U;
        partitions[destination].start_sector =
            apply_plan->partitions[index].start_sector;
        partitions[destination].sector_count =
            apply_plan->partitions[index].sector_count;
        partitions[destination].size_bytes =
            partitions[destination].sector_count *
            CLASSICSETUP_SECTOR_SIZE_BYTES;
    }
}

static struct classicsetup_format_apply_plan make_gpt_format_plan(
    enum classicsetup_format_mode windows_mode)
{
    static const char *const paths[] = {
        "/dev/nvme0n1p1",
        "/dev/nvme0n1p2",
        "/dev/nvme0n1p3",
        "/dev/nvme0n1p4"
    };
    struct classicsetup_apply_plan apply_plan =
        make_partition_apply_plan(
            CLASSICSETUP_INSTALL_UEFI_GPT,
            "nvme0n1",
            "/dev/nvme0n1");
    struct classicsetup_format_plan
        formats[CLASSICSETUP_PARTITION_ROLE_COUNT];
    struct classicsetup_partition_info partitions[4] = {0};
    struct classicsetup_format_apply_plan format_plan;

    make_role_formats(&apply_plan, windows_mode, formats);
    make_scanned_partitions(&apply_plan, paths, partitions, 1);
    assert(classicsetup_build_format_apply_plan(
               &apply_plan,
               formats,
               partitions,
               4,
               &format_plan) == 0);
    return format_plan;
}

static struct classicsetup_format_apply_plan make_mbr_format_plan(void)
{
    static const char *const paths[] = {
        "/dev/sdz1",
        "/dev/sdz2",
        "/dev/sdz3"
    };
    struct classicsetup_apply_plan apply_plan =
        make_partition_apply_plan(
            CLASSICSETUP_INSTALL_BIOS_MBR,
            "sdz",
            "/dev/sdz");
    struct classicsetup_format_plan
        formats[CLASSICSETUP_PARTITION_ROLE_COUNT];
    struct classicsetup_partition_info partitions[3] = {0};
    struct classicsetup_format_apply_plan format_plan;

    make_role_formats(&apply_plan, CLASSICSETUP_FORMAT_QUICK, formats);
    make_scanned_partitions(&apply_plan, paths, partitions, 0);
    assert(classicsetup_build_format_apply_plan(
               &apply_plan,
               formats,
               partitions,
               3,
               &format_plan) == 0);
    return format_plan;
}

static void test_gpt_format_plan_and_range_matching(void)
{
    struct classicsetup_format_apply_plan plan =
        make_gpt_format_plan(CLASSICSETUP_FORMAT_FULL);

    assert(classicsetup_validate_format_apply_plan(&plan));
    assert(plan.partition_apply_plan.table_type ==
           CLASSICSETUP_PARTITION_TABLE_GPT);
    assert(plan.partition_count == 3);
    assert(plan.has_msr_partition);
    assert(plan.msr_partition.role == CLASSICSETUP_PARTITION_ROLE_MSR);
    assert(strcmp(
               plan.msr_partition.device_path,
               "/dev/nvme0n1p2") == 0);
    assert(plan.msr_partition.filesystem == CLASSICSETUP_FS_NONE);
    assert(plan.msr_partition.mode == CLASSICSETUP_FORMAT_NONE);
    assert(plan.partitions[0].role == CLASSICSETUP_PARTITION_ROLE_EFI);
    assert(strcmp(plan.partitions[0].device_path, "/dev/nvme0n1p1") == 0);
    assert(plan.partitions[0].filesystem == CLASSICSETUP_FS_FAT32);
    assert(plan.partitions[0].mode == CLASSICSETUP_FORMAT_QUICK);
    assert(strcmp(plan.partitions[0].label, "SYSTEM") == 0);
    assert(plan.partitions[1].role == CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(strcmp(plan.partitions[1].device_path, "/dev/nvme0n1p3") == 0);
    assert(plan.partitions[1].filesystem == CLASSICSETUP_FS_NTFS);
    assert(plan.partitions[1].mode == CLASSICSETUP_FORMAT_FULL);
    assert(plan.partitions[2].role == CLASSICSETUP_PARTITION_ROLE_RECOVERY);
    assert(strcmp(plan.partitions[2].device_path, "/dev/nvme0n1p4") == 0);
    assert(plan.partitions[2].filesystem == CLASSICSETUP_FS_NTFS);
    assert(plan.partitions[2].mode == CLASSICSETUP_FORMAT_QUICK);
}

static void test_mmc_path_and_mismatch_rejection(void)
{
    static const char *const paths[] = {
        "/dev/mmcblk0p1",
        "/dev/mmcblk0p2",
        "/dev/mmcblk0p3",
        "/dev/mmcblk0p4"
    };
    struct classicsetup_apply_plan apply_plan =
        make_partition_apply_plan(
            CLASSICSETUP_INSTALL_UEFI_GPT,
            "mmcblk0",
            "/dev/mmcblk0");
    struct classicsetup_format_plan
        formats[CLASSICSETUP_PARTITION_ROLE_COUNT];
    struct classicsetup_partition_info partitions[4] = {0};
    struct classicsetup_format_apply_plan output;
    struct classicsetup_format_apply_plan unchanged;

    make_role_formats(&apply_plan, CLASSICSETUP_FORMAT_QUICK, formats);
    make_scanned_partitions(&apply_plan, paths, partitions, 0);
    assert(classicsetup_build_format_apply_plan(
               &apply_plan,
               formats,
               partitions,
               4,
               &output) == 0);
    assert(strcmp(output.partitions[1].device_path, "/dev/mmcblk0p3") == 0);

    memset(&output, 0x5a, sizeof(output));
    unchanged = output;
    ++partitions[2].sector_count;
    assert(classicsetup_build_format_apply_plan(
               &apply_plan,
               formats,
               partitions,
               4,
               &output) == -1);
    assert(memcmp(&output, &unchanged, sizeof(output)) == 0);
}

static void assert_arguments(
    char *arguments[],
    const char *const expected[])
{
    size_t index = 0;

    while (expected[index] != NULL) {
        assert(arguments[index] != NULL);
        assert(strcmp(arguments[index], expected[index]) == 0);
        ++index;
    }
    assert(arguments[index] == NULL);
}

static void test_formatter_and_blkid_arguments(void)
{
    struct classicsetup_format_apply_plan plan =
        make_gpt_format_plan(CLASSICSETUP_FORMAT_QUICK);
    struct classicsetup_format_tools tools = {0};
    char *arguments[CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY];
    const char *executable;
    const char *const fat_expected[] = {
        "/tools/mkfs.fat", "-F", "32", "-n", "SYSTEM",
        "/dev/nvme0n1p1", NULL
    };
    const char *const quick_expected[] = {
        "/tools/mkfs.ntfs", "-f", "-L", "Windows",
        "/dev/nvme0n1p3", NULL
    };
    const char *const full_expected[] = {
        "/tools/mkfs.ntfs", "-L", "Windows",
        "/dev/nvme0n1p3", NULL
    };
    const char *const blkid_expected[] = {
        "/tools/blkid", "-p", "-o", "value", "-s", "TYPE",
        "/dev/nvme0n1p1", NULL
    };

    strcpy(tools.fat, "/tools/mkfs.fat");
    strcpy(tools.ntfs, "/tools/mkfs.ntfs");
    strcpy(tools.blkid, "/tools/blkid");
    assert(classicsetup_build_format_arguments(
               &plan.partitions[0],
               &tools,
               arguments,
               CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY,
               &executable) == 0);
    assert(strcmp(executable, tools.fat) == 0);
    assert_arguments(arguments, fat_expected);

    assert(classicsetup_build_format_arguments(
               &plan.partitions[1],
               &tools,
               arguments,
               CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY,
               &executable) == 0);
    assert_arguments(arguments, quick_expected);
    plan.partitions[1].mode = CLASSICSETUP_FORMAT_FULL;
    assert(classicsetup_build_format_arguments(
               &plan.partitions[1],
               &tools,
               arguments,
               CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY,
               &executable) == 0);
    assert_arguments(arguments, full_expected);
    assert(classicsetup_build_blkid_arguments(
               &plan.partitions[0],
               &tools,
               arguments,
               CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY,
               &executable) == 0);
    assert_arguments(arguments, blkid_expected);

    assert(classicsetup_filesystem_type_matches(
        CLASSICSETUP_FS_FAT32,
        "vfat\n"));
    assert(classicsetup_filesystem_type_matches(
        CLASSICSETUP_FS_FAT32,
        "FAT32"));
    assert(classicsetup_filesystem_type_matches(
        CLASSICSETUP_FS_NTFS,
        "ntfs\n"));
    assert(!classicsetup_filesystem_type_matches(
        CLASSICSETUP_FS_NTFS,
        "vfat\n"));
}

static void test_blkid_type_result_classification(void)
{
    struct classicsetup_process_result result = {
        .exited = 1,
        .exit_status = 2
    };

    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_NO_FILESYSTEM);

    result.exit_status = 0;
    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_NO_FILESYSTEM);

    strcpy(result.output, "ntfs\n");
    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_FILESYSTEM_PRESENT);

    strcpy(result.output, "ntfs extra\n");
    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_FILESYSTEM_VERIFICATION_ERROR);

    result.output[0] = '\0';
    result.exit_status = 4;
    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_FILESYSTEM_VERIFICATION_ERROR);

    result.exit_status = 2;
    strcpy(result.output, "blkid: permission denied\n");
    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_FILESYSTEM_VERIFICATION_ERROR);

    result.output[0] = '\0';
    result.exit_status = 8;
    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_FILESYSTEM_VERIFICATION_ERROR);

    result.exit_status = 0;
    result.exited = 0;
    assert(classicsetup_classify_blkid_type_result(&result) ==
           CLASSICSETUP_FILESYSTEM_VERIFICATION_ERROR);
}

static struct classicsetup_format_safety_inputs safe_inputs(void)
{
    struct classicsetup_format_safety_inputs inputs = {
        .environment = CLASSICSETUP_ENV_VMWARE,
        .table_type = CLASSICSETUP_PARTITION_TABLE_GPT,
        .destructive_unlocked = 1,
        .disk_identity_valid = 1,
        .system_disk_status = CLASSICSETUP_SYSTEM_DISK_SAFE,
        .logical_sector_size_supported = 1,
        .format_plan_valid = 1,
        .partition_layout_valid = 1,
        .partition_device_valid = 1,
        .mount_status = CLASSICSETUP_FORMAT_NOT_MOUNTED,
        .formatter_available = 1,
        .verifier_available = 1
    };

    return inputs;
}

static void test_format_safety_matrix(void)
{
    struct classicsetup_format_safety_inputs inputs = safe_inputs();

    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_OK);
    inputs.environment = CLASSICSETUP_ENV_WSL;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_WSL);
    inputs = safe_inputs();
    inputs.environment = CLASSICSETUP_ENV_UNKNOWN;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_NOT_SUPPORTED_VM);
    inputs = safe_inputs();
    inputs.table_type = CLASSICSETUP_PARTITION_TABLE_MBR;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_MBR_NOT_ENABLED);
    inputs = safe_inputs();
    inputs.destructive_unlocked = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_LOCKED);
    inputs = safe_inputs();
    inputs.disk_identity_valid = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_DISK_IDENTITY);
    inputs = safe_inputs();
    inputs.logical_sector_size_supported = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_UNSUPPORTED_SECTOR_SIZE);
    inputs = safe_inputs();
    inputs.format_plan_valid = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN);
    inputs = safe_inputs();
    inputs.system_disk_status = CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_SYSTEM_DISK);
    inputs = safe_inputs();
    inputs.partition_layout_valid = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_PARTITION_LAYOUT);
    inputs = safe_inputs();
    inputs.partition_device_valid = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_PARTITION_DEVICE);
    inputs = safe_inputs();
    inputs.mount_status = CLASSICSETUP_FORMAT_MOUNTED;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNTED);
    inputs = safe_inputs();
    inputs.mount_status = CLASSICSETUP_FORMAT_MOUNT_UNKNOWN;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNT_UNKNOWN);
    inputs = safe_inputs();
    inputs.formatter_available = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_TOOL_UNAVAILABLE);
    inputs = safe_inputs();
    inputs.verifier_available = 0;
    assert(classicsetup_evaluate_format_safety(&inputs) ==
           CLASSICSETUP_FORMAT_SAFETY_TOOL_UNAVAILABLE);
}

static void test_mountinfo_check(void)
{
    char path[] = "/tmp/classicsetup-format-mount-XXXXXX";
    int descriptor = mkstemp(path);
    FILE *file;

    assert(descriptor >= 0);
    file = fdopen(descriptor, "w");
    assert(file != NULL);
    assert(fputs(
               "24 1 8:2 / / rw - ext4 /dev/sda2 rw\n"
               "25 1 259:3 / /mnt/test rw - ntfs /dev/nvme0n1p3 rw\n",
               file) >= 0);
    assert(fclose(file) == 0);
    assert(classicsetup_check_device_mounted_from(259, 3, path) ==
           CLASSICSETUP_FORMAT_MOUNTED);
    assert(classicsetup_check_device_mounted_from(259, 4, path) ==
           CLASSICSETUP_FORMAT_NOT_MOUNTED);
    assert(classicsetup_check_device_mounted_from(
               259,
               4,
               "/missing/classicsetup-mountinfo") ==
           CLASSICSETUP_FORMAT_MOUNT_UNKNOWN);
    assert(unlink(path) == 0);
}

struct mock_context {
    struct classicsetup_format_safety_inputs safety;
    int safety_calls;
    int formatter_calls;
    int verifier_calls;
    int fail_formatter_at;
    int fail_verifier_at;
    int block_safety_at;
    int msr_probe_exit_status;
    const char *msr_probe_output;
};

static int mock_collect_safety(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_partition *partition,
    struct classicsetup_format_safety_inputs *inputs,
    void *opaque)
{
    struct mock_context *context = opaque;

    assert(plan != NULL);
    assert(partition != NULL);
    *inputs = context->safety;
    ++context->safety_calls;
    if (context->safety_calls == context->block_safety_at) {
        inputs->partition_layout_valid = 0;
    }
    return 0;
}

static int mock_run_formatter(
    const struct classicsetup_format_partition *partition,
    struct classicsetup_process_result *result,
    void *opaque)
{
    struct mock_context *context = opaque;
    int call = context->formatter_calls++;

    assert(partition->role != CLASSICSETUP_PARTITION_ROLE_MSR);
    memset(result, 0, sizeof(*result));
    result->exited = 1;
    result->exit_status = call == context->fail_formatter_at ? 1 : 0;
    return 0;
}

static int mock_verify_filesystem(
    const struct classicsetup_format_partition *partition,
    struct classicsetup_process_result *result,
    void *opaque)
{
    struct mock_context *context = opaque;
    enum classicsetup_filesystem_probe_result probe_result;
    int call = context->verifier_calls++;

    memset(result, 0, sizeof(*result));
    result->exited = 1;
    if (partition->filesystem == CLASSICSETUP_FS_NONE) {
        result->exit_status = context->msr_probe_exit_status;
        if (context->msr_probe_output != NULL) {
            snprintf(
                result->output,
                sizeof(result->output),
                "%s",
                context->msr_probe_output);
        }
    } else {
        result->exit_status = 0;
        snprintf(
            result->output,
            sizeof(result->output),
            "%s\n",
            partition->filesystem == CLASSICSETUP_FS_FAT32
                ? "vfat"
                : "ntfs");
    }
    if (call == context->fail_verifier_at) {
        return 0;
    }
    probe_result = classicsetup_classify_blkid_type_result(result);
    if (partition->filesystem == CLASSICSETUP_FS_NONE) {
        return probe_result == CLASSICSETUP_NO_FILESYSTEM;
    }
    return probe_result == CLASSICSETUP_FILESYSTEM_PRESENT &&
           classicsetup_filesystem_type_matches(
               partition->filesystem,
               result->output);
}

static struct classicsetup_format_executor_ops make_mock_ops(
    struct mock_context *context)
{
    struct classicsetup_format_executor_ops ops = {
        .collect_safety = mock_collect_safety,
        .run_formatter = mock_run_formatter,
        .verify_filesystem = mock_verify_filesystem,
        .context = context
    };

    return ops;
}

static struct mock_context make_mock_context(void)
{
    struct mock_context context = {
        .safety = {
            .environment = CLASSICSETUP_ENV_VMWARE,
            .table_type = CLASSICSETUP_PARTITION_TABLE_GPT,
            .destructive_unlocked = 1,
            .disk_identity_valid = 1,
            .system_disk_status = CLASSICSETUP_SYSTEM_DISK_SAFE,
            .logical_sector_size_supported = 1,
            .format_plan_valid = 1,
            .partition_layout_valid = 1,
            .partition_device_valid = 1,
            .mount_status = CLASSICSETUP_FORMAT_NOT_MOUNTED,
            .formatter_available = 1,
            .verifier_available = 1
        },
        .fail_formatter_at = -1,
        .fail_verifier_at = -1,
        .block_safety_at = -1,
        .msr_probe_exit_status = 2
    };

    return context;
}

static void test_mock_executor_success_and_failures(void)
{
    struct classicsetup_format_apply_plan plan =
        make_gpt_format_plan(CLASSICSETUP_FORMAT_QUICK);
    struct classicsetup_format_result result;
    struct mock_context context = make_mock_context();
    struct classicsetup_format_executor_ops ops = make_mock_ops(&context);

    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_SUCCESS);
    assert(result.completed_count == 3);
    assert(result.failed_role == CLASSICSETUP_PARTITION_ROLE_NONE);
    assert(context.safety_calls == 8);
    assert(context.formatter_calls == 3);
    assert(context.verifier_calls == 4);

    context = make_mock_context();
    context.msr_probe_exit_status = 0;
    ops = make_mock_ops(&context);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_SUCCESS);
    assert(context.formatter_calls == 3);
    assert(context.verifier_calls == 4);

    context = make_mock_context();
    context.msr_probe_exit_status = 0;
    context.msr_probe_output = "ntfs\n";
    ops = make_mock_ops(&context);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_VERIFY_FAILED);
    assert(result.failed_role == CLASSICSETUP_PARTITION_ROLE_MSR);
    assert(result.completed_count == 3);
    assert(context.formatter_calls == 3);
    assert(context.verifier_calls == 4);

    context = make_mock_context();
    context.fail_formatter_at = 1;
    ops = make_mock_ops(&context);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_PROCESS_FAILED);
    assert(result.failed_role == CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(result.completed_count == 1);
    assert(context.formatter_calls == 2);
    assert(context.verifier_calls == 1);

    context = make_mock_context();
    context.fail_verifier_at = 1;
    ops = make_mock_ops(&context);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_VERIFY_FAILED);
    assert(result.failed_role == CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(result.completed_count == 1);
    assert(context.formatter_calls == 2);
    assert(context.verifier_calls == 2);

    context = make_mock_context();
    context.fail_verifier_at = 3;
    ops = make_mock_ops(&context);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_VERIFY_FAILED);
    assert(result.failed_role == CLASSICSETUP_PARTITION_ROLE_MSR);
    assert(result.completed_count == 3);
    assert(context.formatter_calls == 3);
    assert(context.verifier_calls == 4);

    context = make_mock_context();
    context.safety.mount_status = CLASSICSETUP_FORMAT_MOUNTED;
    ops = make_mock_ops(&context);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_BLOCKED);
    assert(result.safety_code ==
           CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNTED);
    assert(context.formatter_calls == 0);

    context = make_mock_context();
    context.block_safety_at = 2;
    ops = make_mock_ops(&context);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_BLOCKED);
    assert(result.safety_code ==
           CLASSICSETUP_FORMAT_SAFETY_PARTITION_LAYOUT);
    assert(context.safety_calls == 2);
    assert(context.formatter_calls == 0);
}

static void test_mbr_executor_is_blocked_before_callbacks(void)
{
    struct classicsetup_format_apply_plan plan = make_mbr_format_plan();
    struct classicsetup_format_result result;
    struct mock_context context = make_mock_context();
    struct classicsetup_format_executor_ops ops = make_mock_ops(&context);

    assert(classicsetup_validate_format_apply_plan(&plan));
    assert(plan.partition_count == 3);
    assert(plan.partitions[0].role ==
           CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED);
    assert(classicsetup_execute_format_apply_plan_with_ops(
               &plan,
               &ops,
               &result) == 0);
    assert(result.code == CLASSICSETUP_FORMAT_RESULT_BLOCKED);
    assert(result.safety_code ==
           CLASSICSETUP_FORMAT_SAFETY_MBR_NOT_ENABLED);
    assert(context.safety_calls == 0);
    assert(context.formatter_calls == 0);
    assert(context.verifier_calls == 0);
}

static void test_current_wsl_execution_is_blocked(void)
{
    enum classicsetup_environment environment;

    if (classicsetup_detect_environment(&environment) == 0 &&
        environment == CLASSICSETUP_ENV_WSL) {
        struct classicsetup_format_apply_plan plan =
            make_gpt_format_plan(CLASSICSETUP_FORMAT_QUICK);
        struct classicsetup_format_result result;

        assert(classicsetup_execute_format_apply_plan(&plan, &result) == 0);
        assert(result.code == CLASSICSETUP_FORMAT_RESULT_BLOCKED);
        assert(result.safety_code == CLASSICSETUP_FORMAT_SAFETY_WSL);
        assert(result.completed_count == 0);
        assert(!result.process.exited);
    }
}

int main(void)
{
    test_gpt_format_plan_and_range_matching();
    test_mmc_path_and_mismatch_rejection();
    test_formatter_and_blkid_arguments();
    test_blkid_type_result_classification();
    test_format_safety_matrix();
    test_mountinfo_check();
    test_mock_executor_success_and_failures();
    test_mbr_executor_is_blocked_before_callbacks();
    test_current_wsl_execution_is_blocked();
    return 0;
}
