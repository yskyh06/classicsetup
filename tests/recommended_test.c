#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "classicsetup/config.h"
#include "classicsetup/recommended.h"
#include "classicsetup/setup_mode.h"

static struct classicsetup_disk_info make_disk(void)
{
    struct classicsetup_disk_info disk = {
        .size_bytes = 64ULL * 1024ULL * 1024ULL * 1024ULL,
        .logical_sector_size = CLASSICSETUP_SECTOR_SIZE_BYTES,
        .has_serial = true,
        .has_logical_sector_size = true,
        .has_removable = true,
        .removable = false
    };

    strcpy(disk.name, "sdz");
    strcpy(disk.device_path, "/dev/sdz");
    strcpy(disk.model, "Mock VMware Empty Disk");
    strcpy(disk.serial, "M9-TEST-DISK-001");
    strcpy(disk.transport, "scsi");
    strcpy(disk.sysfs_path, "/sys/devices/mock/block/sdz");
    return disk;
}

static void test_default_mode_and_firmware_detection(void)
{
    char root[] = "/tmp/classicsetup-firmware-XXXXXX";
    char efi[512];

    assert(classicsetup_default_setup_mode() ==
           CLASSICSETUP_SETUP_RECOMMENDED);
    assert(mkdtemp(root) != NULL);
    assert(classicsetup_detect_firmware_from(root) ==
           CLASSICSETUP_FIRMWARE_BIOS);
    snprintf(efi, sizeof(efi), "%s/efi", root);
    assert(mkdir(efi, 0700) == 0);
    assert(classicsetup_detect_firmware_from(root) ==
           CLASSICSETUP_FIRMWARE_UEFI);
    assert(classicsetup_detect_firmware_from(
               "/missing/classicsetup/firmware") ==
           CLASSICSETUP_FIRMWARE_UNKNOWN);
    assert(rmdir(efi) == 0);
    assert(rmdir(root) == 0);
}

static void test_disk_classification_policy(void)
{
    struct classicsetup_disk_info disk = make_disk();
    struct classicsetup_disk_facts facts = {
        .partition_scan_succeeded = 1,
        .system_disk_status = CLASSICSETUP_SYSTEM_DISK_SAFE
    };

    assert(classicsetup_disk_has_recommended_identity(&disk));
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_RAW_EMPTY);
    assert(classicsetup_disk_class_is_recommended_selectable(
        CLASSICSETUP_DISK_RAW_EMPTY));

    facts.partition_table_present = 1;
    facts.partition_count = 2;
    facts.filesystems_inspected = 1;
    facts.all_partitions_confirmed_empty = 1;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_PARTITIONED_EMPTY);
    assert(classicsetup_recommended_policy_for_disk(
               CLASSICSETUP_DISK_PARTITIONED_EMPTY) ==
           CLASSICSETUP_RECOMMENDED_REINITIALIZE_WITH_WARNING);

    facts.all_partitions_confirmed_empty = 0;
    facts.windows_detected = 1;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_WINDOWS);
    facts.windows_detected = 0;
    facts.user_data_detected = 1;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_DATA_PRESENT);
    facts.user_data_detected = 0;
    facts.encryption_detected = 1;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_WINDOWS_ENCRYPTED_LOCKED);
    assert(!classicsetup_disk_class_is_recommended_selectable(
        CLASSICSETUP_DISK_WINDOWS_ENCRYPTED_LOCKED));

    facts.encryption_detected = 0;
    facts.complex_storage = 1;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_WINDOWS_COMPLEX);
    facts.complex_storage = 0;
    facts.unknown_filesystem = 1;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_UNKNOWN_FILESYSTEM);

    facts.system_disk_status = CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_SYSTEM);

    disk.removable = true;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_INSTALL_MEDIA);
    facts.system_disk_status = CLASSICSETUP_SYSTEM_DISK_SAFE;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_REMOVABLE);

    disk = make_disk();
    disk.has_serial = false;
    disk.serial[0] = '\0';
    memset(&facts, 0, sizeof(facts));
    facts.partition_scan_succeeded = 1;
    facts.system_disk_status = CLASSICSETUP_SYSTEM_DISK_SAFE;
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_UNKNOWN) ==
           CLASSICSETUP_DISK_UNKNOWN);
    assert(classicsetup_disk_has_vm_test_identity(&disk));
    assert(classicsetup_classify_disk_facts(
               &disk, &facts, CLASSICSETUP_ENV_VMWARE) ==
           CLASSICSETUP_DISK_RAW_EMPTY);
    assert(classicsetup_recommended_policy_for_disk(
               CLASSICSETUP_DISK_RAW_EMPTY) ==
           CLASSICSETUP_RECOMMENDED_AUTO_INSTALL_ALLOWED);
    assert(strstr(
               classicsetup_recommended_policy_reason(
                   CLASSICSETUP_DISK_RAW_EMPTY,
                   CLASSICSETUP_FIRMWARE_BIOS),
               "Restart in UEFI") != NULL);
    assert(strcmp(
               classicsetup_disk_class_presentation(
                   CLASSICSETUP_DISK_WINDOWS),
               "Existing Windows installation") == 0);
    assert(!classicsetup_disk_class_is_recommended_selectable(
        CLASSICSETUP_DISK_UNKNOWN));
}

static struct classicsetup_recommended_plan make_plan(void)
{
    struct classicsetup_disk_info disk = make_disk();
    struct classicsetup_recommended_plan plan;

    assert(classicsetup_build_recommended_plan(
               CLASSICSETUP_FIRMWARE_UEFI,
               &disk,
               CLASSICSETUP_DISK_RAW_EMPTY,
               &plan) == 0);
    return plan;
}

static void test_recommended_auto_plan(void)
{
    struct classicsetup_disk_info disk = make_disk();
    struct classicsetup_recommended_plan plan = make_plan();

    assert(plan.install_mode == CLASSICSETUP_INSTALL_UEFI_GPT);
    assert(classicsetup_plan_has_windows_layout_for_mode(
        &plan.partition_plan,
        CLASSICSETUP_INSTALL_UEFI_GPT));
    assert(plan.selected_target.role ==
           CLASSICSETUP_PARTITION_ROLE_WINDOWS);
    assert(plan.selected_format_plan.filesystem == CLASSICSETUP_FS_NTFS);
    assert(plan.selected_format_plan.mode == CLASSICSETUP_FORMAT_QUICK);
    assert(plan.apply_plan.table_type == CLASSICSETUP_PARTITION_TABLE_GPT);
    assert(plan.apply_plan.partition_count == 4);
    assert(classicsetup_build_recommended_plan(
               CLASSICSETUP_FIRMWARE_BIOS,
               &disk,
               CLASSICSETUP_DISK_RAW_EMPTY,
               &plan) == -1);
    assert(classicsetup_build_recommended_plan(
               CLASSICSETUP_FIRMWARE_UNKNOWN,
               &disk,
               CLASSICSETUP_DISK_RAW_EMPTY,
               &plan) == -1);
    assert(classicsetup_build_recommended_plan(
               CLASSICSETUP_FIRMWARE_UEFI,
               &disk,
               CLASSICSETUP_DISK_DATA_PRESENT,
               &plan) == -1);
}

struct mock_context {
    int revalidate_calls;
    int partition_calls;
    int format_plan_calls;
    int format_calls;
    int sequence;
    int partition_order;
    int format_order;
    int empty;
    enum classicsetup_apply_result_code partition_code;
    int format_plan_success;
    enum classicsetup_format_result_code format_code;
};

static int mock_revalidate(
    const struct classicsetup_disk_info *disk,
    void *opaque)
{
    struct mock_context *context = opaque;

    assert(disk != NULL);
    ++context->revalidate_calls;
    return context->empty;
}

static int mock_partition(
    const struct classicsetup_apply_plan *plan,
    struct classicsetup_apply_result *result,
    void *opaque)
{
    struct mock_context *context = opaque;

    assert(classicsetup_validate_apply_plan(plan));
    ++context->partition_calls;
    context->partition_order = ++context->sequence;
    memset(result, 0, sizeof(*result));
    result->code = context->partition_code;
    return 0;
}

static int mock_build_format(
    const struct classicsetup_recommended_plan *plan,
    struct classicsetup_format_apply_plan *format_plan,
    void *opaque)
{
    static const char *const paths[] = {
        "/dev/sdz1", "/dev/sdz2", "/dev/sdz3", "/dev/sdz4"
    };
    struct classicsetup_partition_info partitions[4] = {0};
    struct mock_context *context = opaque;
    size_t index;

    ++context->format_plan_calls;
    if (!context->format_plan_success) {
        return -1;
    }
    for (index = 0; index < 4; ++index) {
        snprintf(
            partitions[index].device_path,
            sizeof(partitions[index].device_path),
            "%s",
            paths[index]);
        partitions[index].start_sector =
            plan->apply_plan.partitions[index].start_sector;
        partitions[index].sector_count =
            plan->apply_plan.partitions[index].sector_count;
    }
    return classicsetup_build_format_apply_plan(
        &plan->apply_plan,
        plan->role_format_plans,
        partitions,
        4,
        format_plan);
}

static int mock_format(
    const struct classicsetup_format_apply_plan *plan,
    struct classicsetup_format_result *result,
    void *opaque)
{
    struct mock_context *context = opaque;

    assert(classicsetup_validate_format_apply_plan(plan));
    ++context->format_calls;
    context->format_order = ++context->sequence;
    memset(result, 0, sizeof(*result));
    result->code = context->format_code;
    return 0;
}

static struct classicsetup_recommended_executor_ops make_ops(
    struct mock_context *context)
{
    struct classicsetup_recommended_executor_ops ops = {
        .revalidate_empty_disk = mock_revalidate,
        .execute_partition = mock_partition,
        .build_format_plan = mock_build_format,
        .execute_format = mock_format,
        .context = context
    };

    return ops;
}

static struct mock_context success_context(void)
{
    struct mock_context context = {
        .empty = 1,
        .partition_code = CLASSICSETUP_APPLY_RESULT_SUCCESS,
        .format_plan_success = 1,
        .format_code = CLASSICSETUP_FORMAT_RESULT_SUCCESS
    };

    return context;
}

static void test_recommended_orchestration(void)
{
    struct classicsetup_recommended_plan plan = make_plan();
    struct classicsetup_apply_result partition_result;
    struct classicsetup_format_apply_plan format_plan;
    struct classicsetup_format_result format_result;
    struct mock_context context = success_context();
    struct classicsetup_recommended_executor_ops ops = make_ops(&context);

    assert(context.partition_calls == 0);
    assert(context.format_calls == 0);
    assert(classicsetup_execute_recommended_plan_with_ops(
               &plan,
               &ops,
               &partition_result,
               &format_plan,
               &format_result) == CLASSICSETUP_RECOMMENDED_SUCCESS);
    assert(context.revalidate_calls == 1);
    assert(context.partition_calls == 1);
    assert(context.format_plan_calls == 1);
    assert(context.format_calls == 1);
    assert(context.partition_order < context.format_order);

    context = success_context();
    context.empty = 0;
    ops = make_ops(&context);
    assert(classicsetup_execute_recommended_plan_with_ops(
               &plan,
               &ops,
               &partition_result,
               &format_plan,
               &format_result) == CLASSICSETUP_RECOMMENDED_BLOCKED);
    assert(context.partition_calls == 0);
    assert(context.format_calls == 0);

    context = success_context();
    context.partition_code = CLASSICSETUP_APPLY_RESULT_BLOCKED;
    ops = make_ops(&context);
    assert(classicsetup_execute_recommended_plan_with_ops(
               &plan,
               &ops,
               &partition_result,
               &format_plan,
               &format_result) ==
           CLASSICSETUP_RECOMMENDED_PARTITION_FAILED);
    assert(context.partition_calls == 1);
    assert(context.format_plan_calls == 0);
    assert(context.format_calls == 0);

    context = success_context();
    context.format_code = CLASSICSETUP_FORMAT_RESULT_VERIFY_FAILED;
    ops = make_ops(&context);
    assert(classicsetup_execute_recommended_plan_with_ops(
               &plan,
               &ops,
               &partition_result,
               &format_plan,
               &format_result) == CLASSICSETUP_RECOMMENDED_FORMAT_FAILED);
    assert(context.format_calls == 1);
    assert(!classicsetup_recommended_result_can_continue(
        CLASSICSETUP_RECOMMENDED_FORMAT_FAILED));
    assert(classicsetup_recommended_result_can_continue(
        CLASSICSETUP_RECOMMENDED_SUCCESS));
}

static void test_setup_mode_change_clears_stale_plan(void)
{
    struct classicsetup_recommended_plan plan = make_plan();
    struct classicsetup_config config = {
        .setup_mode = CLASSICSETUP_SETUP_RECOMMENDED,
        .install_mode = CLASSICSETUP_INSTALL_UEFI_GPT
    };

    assert(classicsetup_config_set_recommended_plan(&config, &plan) == 0);
    assert(config.has_recommended_plan);
    assert(config.has_partition_plan);
    assert(config.has_apply_plan);
    classicsetup_config_set_setup_mode(
        &config,
        CLASSICSETUP_SETUP_ADVANCED);
    assert(config.setup_mode == CLASSICSETUP_SETUP_ADVANCED);
    assert(!config.has_recommended_plan);
    assert(!config.has_selected_disk);
    assert(!config.has_partition_plan);
    assert(!config.has_apply_plan);

    config.has_partition_plan = true;
    config.has_apply_plan = true;
    config.has_recommended_plan = true;
    classicsetup_config_set_setup_mode(
        &config,
        CLASSICSETUP_SETUP_RECOMMENDED);
    assert(config.setup_mode == CLASSICSETUP_SETUP_RECOMMENDED);
    assert(!config.has_recommended_plan);
    assert(!config.has_partition_plan);
    assert(!config.has_apply_plan);
}

int main(void)
{
    test_default_mode_and_firmware_detection();
    test_disk_classification_policy();
    test_recommended_auto_plan();
    test_recommended_orchestration();
    test_setup_mode_change_clears_stale_plan();
    return 0;
}
