#define _POSIX_C_SOURCE 200809L

#include "classicsetup/recommended.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "classicsetup/disk.h"
#include "classicsetup/partition.h"

enum {
    RECOMMENDED_PARTITION_CAPACITY = 64
};

enum classicsetup_firmware_mode classicsetup_detect_firmware_from(
    const char *sys_firmware_path)
{
    char efi_path[512];
    struct stat status;
    int written;

    if (sys_firmware_path == NULL ||
        stat(sys_firmware_path, &status) != 0 ||
        !S_ISDIR(status.st_mode)) {
        return CLASSICSETUP_FIRMWARE_UNKNOWN;
    }
    written = snprintf(
        efi_path,
        sizeof(efi_path),
        "%s/efi",
        sys_firmware_path);
    if (written < 0 || (size_t)written >= sizeof(efi_path)) {
        return CLASSICSETUP_FIRMWARE_UNKNOWN;
    }
    if (stat(efi_path, &status) == 0) {
        return S_ISDIR(status.st_mode) ? CLASSICSETUP_FIRMWARE_UEFI
                                      : CLASSICSETUP_FIRMWARE_UNKNOWN;
    }
    return errno == ENOENT ? CLASSICSETUP_FIRMWARE_BIOS
                           : CLASSICSETUP_FIRMWARE_UNKNOWN;
}

enum classicsetup_firmware_mode classicsetup_detect_firmware(void)
{
    return classicsetup_detect_firmware_from("/sys/firmware");
}

enum classicsetup_disk_class classicsetup_classify_disk(
    const struct classicsetup_disk_info *disk,
    int partition_scan_succeeded,
    size_t partition_count,
    int has_usable_unallocated,
    enum classicsetup_system_disk_status system_disk_status)
{
    if (disk == NULL) {
        return CLASSICSETUP_DISK_UNKNOWN;
    }
    if (system_disk_status == CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE) {
        return disk->has_removable && disk->removable
                   ? CLASSICSETUP_DISK_INSTALL_MEDIA
                   : CLASSICSETUP_DISK_SYSTEM;
    }
    if (system_disk_status != CLASSICSETUP_SYSTEM_DISK_SAFE ||
        !classicsetup_disk_has_recommended_identity(disk) ||
        !partition_scan_succeeded) {
        return CLASSICSETUP_DISK_UNKNOWN;
    }
    if (disk->removable) {
        return CLASSICSETUP_DISK_REMOVABLE;
    }
    if (partition_count == 0) {
        return CLASSICSETUP_DISK_EMPTY;
    }
    return has_usable_unallocated
               ? CLASSICSETUP_DISK_HAS_UNALLOCATED_SPACE
               : CLASSICSETUP_DISK_HAS_EXISTING_PARTITIONS;
}

int classicsetup_disk_class_is_recommended_selectable(
    enum classicsetup_disk_class disk_class)
{
    return disk_class == CLASSICSETUP_DISK_EMPTY;
}

int classicsetup_recommended_result_can_continue(
    enum classicsetup_recommended_result_code result_code)
{
    return result_code == CLASSICSETUP_RECOMMENDED_SUCCESS;
}

static int has_usable_unallocated_space(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count)
{
    struct classicsetup_partition_plan plan;
    const unsigned long long required =
        (CLASSICSETUP_DEFAULT_EFI_MB + CLASSICSETUP_DEFAULT_MSR_MB +
         CLASSICSETUP_DEFAULT_RECOVERY_MB + CLASSICSETUP_MIN_WINDOWS_MB) *
        CLASSICSETUP_SECTORS_PER_MB;
    size_t index;

    if (classicsetup_plan_init(
            disk,
            partitions,
            partition_count,
            &plan) != 0) {
        return 0;
    }
    for (index = 0; index < plan.item_count; ++index) {
        if (plan.items[index].state == CLASSICSETUP_PLAN_UNALLOCATED &&
            plan.items[index].sector_count >= required) {
            return 1;
        }
    }
    return 0;
}

int classicsetup_assess_disk(
    const struct classicsetup_disk_info *disk,
    struct classicsetup_disk_assessment *assessment)
{
    struct classicsetup_partition_info
        partitions[RECOMMENDED_PARTITION_CAPACITY];
    enum classicsetup_system_disk_status system_status;
    size_t partition_count = 0;
    int scan_succeeded;
    int has_unallocated = 0;

    if (disk == NULL || assessment == NULL) {
        return -1;
    }
    memset(assessment, 0, sizeof(*assessment));
    assessment->disk = *disk;
    system_status = classicsetup_check_system_disk(disk->name);
    scan_succeeded = classicsetup_scan_partitions(
                         disk,
                         partitions,
                         RECOMMENDED_PARTITION_CAPACITY,
                         &partition_count) == 0;
    if (scan_succeeded && partition_count > 0) {
        has_unallocated = has_usable_unallocated_space(
            disk,
            partitions,
            partition_count);
    }
    assessment->partition_count = partition_count;
    assessment->disk_class = classicsetup_classify_disk(
        disk,
        scan_succeeded,
        partition_count,
        has_unallocated,
        system_status);
    assessment->selectable =
        classicsetup_disk_class_is_recommended_selectable(
            assessment->disk_class);
    return 0;
}

static int populate_format_policy(
    struct classicsetup_recommended_plan *plan)
{
    size_t index;

    for (index = 0; index < plan->partition_plan.item_count; ++index) {
        const struct classicsetup_plan_item *item =
            &plan->partition_plan.items[index];

        if (item->state != CLASSICSETUP_PLAN_NEW ||
            item->role <= CLASSICSETUP_PARTITION_ROLE_NONE ||
            item->role >= CLASSICSETUP_PARTITION_ROLE_COUNT) {
            continue;
        }
        if (classicsetup_format_policy_for_role(
                item->role,
                CLASSICSETUP_FORMAT_QUICK,
                &plan->role_format_plans[item->role]) != 0) {
            return -1;
        }
    }
    plan->selected_format_plan =
        plan->role_format_plans[CLASSICSETUP_PARTITION_ROLE_WINDOWS];
    return plan->selected_format_plan.valid ? 0 : -1;
}

int classicsetup_build_recommended_plan(
    enum classicsetup_firmware_mode firmware,
    const struct classicsetup_disk_info *disk,
    enum classicsetup_disk_class disk_class,
    struct classicsetup_recommended_plan *plan)
{
    struct classicsetup_recommended_plan temporary = {0};
    size_t target_index;

    if (disk == NULL || plan == NULL ||
        firmware != CLASSICSETUP_FIRMWARE_UEFI ||
        disk_class != CLASSICSETUP_DISK_EMPTY ||
        !classicsetup_disk_has_recommended_identity(disk) ||
        classicsetup_plan_init(disk, NULL, 0, &temporary.partition_plan) != 0 ||
        temporary.partition_plan.item_count == 0 ||
        classicsetup_plan_prepare_install_target_for_mode(
            &temporary.partition_plan,
            CLASSICSETUP_INSTALL_UEFI_GPT,
            0,
            &target_index) != 0 ||
        target_index >= temporary.partition_plan.item_count) {
        return -1;
    }
    temporary.install_mode = CLASSICSETUP_INSTALL_UEFI_GPT;
    temporary.selected_target = temporary.partition_plan.items[target_index];
    if (temporary.selected_target.role !=
            CLASSICSETUP_PARTITION_ROLE_WINDOWS ||
        populate_format_policy(&temporary) != 0 ||
        classicsetup_build_apply_plan_for_mode(
            temporary.install_mode,
            disk,
            &temporary.partition_plan,
            0,
            &temporary.apply_plan) != 0) {
        return -1;
    }
    *plan = temporary;
    return 0;
}

static int actual_revalidate_empty_disk(
    const struct classicsetup_disk_info *disk,
    void *context)
{
    struct classicsetup_disk_info current;
    struct classicsetup_disk_assessment assessment;

    (void)context;
    return classicsetup_revalidate_target_disk(disk, &current) == 0 &&
           classicsetup_assess_disk(&current, &assessment) == 0 &&
           assessment.disk_class == CLASSICSETUP_DISK_EMPTY;
}

static int actual_execute_partition(
    const struct classicsetup_apply_plan *plan,
    struct classicsetup_apply_result *result,
    void *context)
{
    (void)context;
    return classicsetup_execute_apply_plan(plan, result);
}

static int actual_build_format_plan(
    const struct classicsetup_recommended_plan *plan,
    struct classicsetup_format_apply_plan *format_plan,
    void *context)
{
    struct classicsetup_partition_info
        partitions[RECOMMENDED_PARTITION_CAPACITY];
    size_t partition_count = 0;

    (void)context;
    if (classicsetup_scan_partitions(
            &plan->apply_plan.target_disk,
            partitions,
            RECOMMENDED_PARTITION_CAPACITY,
            &partition_count) != 0) {
        return -1;
    }
    return classicsetup_build_format_apply_plan(
        &plan->apply_plan,
        plan->role_format_plans,
        partitions,
        partition_count,
        format_plan);
}

static int actual_execute_format(
    const struct classicsetup_format_apply_plan *plan,
    struct classicsetup_format_result *result,
    void *context)
{
    (void)context;
    return classicsetup_execute_format_apply_plan(plan, result);
}

enum classicsetup_recommended_result_code
classicsetup_execute_recommended_plan_with_ops(
    const struct classicsetup_recommended_plan *plan,
    const struct classicsetup_recommended_executor_ops *ops,
    struct classicsetup_apply_result *partition_result,
    struct classicsetup_format_apply_plan *format_plan,
    struct classicsetup_format_result *format_result)
{
    if (plan == NULL || ops == NULL || partition_result == NULL ||
        format_plan == NULL || format_result == NULL ||
        ops->revalidate_empty_disk == NULL ||
        ops->execute_partition == NULL || ops->build_format_plan == NULL ||
        ops->execute_format == NULL ||
        plan->install_mode != CLASSICSETUP_INSTALL_UEFI_GPT ||
        !classicsetup_validate_apply_plan(&plan->apply_plan)) {
        return CLASSICSETUP_RECOMMENDED_BLOCKED;
    }
    memset(partition_result, 0, sizeof(*partition_result));
    memset(format_plan, 0, sizeof(*format_plan));
    memset(format_result, 0, sizeof(*format_result));
    if (ops->revalidate_empty_disk(
            &plan->apply_plan.target_disk,
            ops->context) != 1) {
        return CLASSICSETUP_RECOMMENDED_BLOCKED;
    }
    if (ops->execute_partition(
            &plan->apply_plan,
            partition_result,
            ops->context) != 0 ||
        partition_result->code != CLASSICSETUP_APPLY_RESULT_SUCCESS) {
        return CLASSICSETUP_RECOMMENDED_PARTITION_FAILED;
    }
    if (ops->build_format_plan(plan, format_plan, ops->context) != 0 ||
        !classicsetup_validate_format_apply_plan(format_plan)) {
        return CLASSICSETUP_RECOMMENDED_FORMAT_PLAN_FAILED;
    }
    if (ops->execute_format(
            format_plan,
            format_result,
            ops->context) != 0 ||
        format_result->code != CLASSICSETUP_FORMAT_RESULT_SUCCESS) {
        return CLASSICSETUP_RECOMMENDED_FORMAT_FAILED;
    }
    return CLASSICSETUP_RECOMMENDED_SUCCESS;
}

enum classicsetup_recommended_result_code
classicsetup_execute_recommended_plan(
    const struct classicsetup_recommended_plan *plan,
    struct classicsetup_apply_result *partition_result,
    struct classicsetup_format_apply_plan *format_plan,
    struct classicsetup_format_result *format_result)
{
    const struct classicsetup_recommended_executor_ops ops = {
        .revalidate_empty_disk = actual_revalidate_empty_disk,
        .execute_partition = actual_execute_partition,
        .build_format_plan = actual_build_format_plan,
        .execute_format = actual_execute_format,
        .context = NULL
    };

    return classicsetup_execute_recommended_plan_with_ops(
        plan,
        &ops,
        partition_result,
        format_plan,
        format_result);
}
