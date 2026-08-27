#define _POSIX_C_SOURCE 200809L

#include "classicsetup/format_apply.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>

enum {
    FORMAT_SCAN_CAPACITY = 64,
    FORMAT_PATH_SIZE = 512,
    FORMAT_VALUE_SIZE = 64
};

static const char *label_for_role(enum classicsetup_partition_role role)
{
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_EFI:
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
        return "SYSTEM";
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        return "Windows";
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        return "Recovery";
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_MSR:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        return NULL;
    }
    return NULL;
}

static int safe_device_path(const char *path)
{
    size_t index;

    if (path == NULL || strncmp(path, "/dev/", 5) != 0 || path[5] == '\0') {
        return 0;
    }
    for (index = 5; path[index] != '\0'; ++index) {
        unsigned char character = (unsigned char)path[index];

        if (!isalnum(character) && character != '_' && character != '-' &&
            character != '.' && character != '/') {
            return 0;
        }
    }
    return index < CLASSICSETUP_PARTITION_PATH_SIZE;
}

static int format_policy_matches(
    enum classicsetup_partition_role role,
    const struct classicsetup_format_plan *format)
{
    if (format == NULL || !format->valid) {
        return 0;
    }
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_EFI:
        return format->filesystem == CLASSICSETUP_FS_FAT32 &&
               format->mode == CLASSICSETUP_FORMAT_QUICK;
    case CLASSICSETUP_PARTITION_ROLE_MSR:
        return format->filesystem == CLASSICSETUP_FS_NONE &&
               format->mode == CLASSICSETUP_FORMAT_NONE;
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        return format->filesystem == CLASSICSETUP_FS_NTFS &&
               (format->mode == CLASSICSETUP_FORMAT_QUICK ||
                format->mode == CLASSICSETUP_FORMAT_FULL);
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
        return format->filesystem == CLASSICSETUP_FS_NTFS &&
               format->mode == CLASSICSETUP_FORMAT_QUICK;
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        return 0;
    }
    return 0;
}

int classicsetup_match_partition_device(
    const struct classicsetup_apply_partition *expected,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_partition_info *matched)
{
    size_t match_index = 0;
    size_t match_count = 0;
    size_t index;

    if (expected == NULL || matched == NULL ||
        (partition_count > 0 && partitions == NULL)) {
        return -1;
    }
    for (index = 0; index < partition_count; ++index) {
        if (partitions[index].start_sector == expected->start_sector &&
            partitions[index].sector_count == expected->sector_count) {
            match_index = index;
            ++match_count;
        }
    }
    if (match_count != 1 ||
        !safe_device_path(partitions[match_index].device_path)) {
        return -1;
    }
    *matched = partitions[match_index];
    return 0;
}

int classicsetup_build_format_apply_plan(
    const struct classicsetup_apply_plan *partition_apply_plan,
    const struct classicsetup_format_plan
        role_format_plans[CLASSICSETUP_PARTITION_ROLE_COUNT],
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_format_apply_plan *format_apply_plan)
{
    struct classicsetup_format_apply_plan temporary = {0};
    size_t output_index = 0;
    size_t index;

    if (partition_apply_plan == NULL || role_format_plans == NULL ||
        partitions == NULL || format_apply_plan == NULL ||
        !classicsetup_validate_apply_plan(partition_apply_plan) ||
        partition_count != partition_apply_plan->partition_count) {
        return -1;
    }
    temporary.partition_apply_plan = *partition_apply_plan;
    for (index = 0; index < partition_apply_plan->partition_count; ++index) {
        const struct classicsetup_apply_partition *expected =
            &partition_apply_plan->partitions[index];
        const struct classicsetup_format_plan *format;
        struct classicsetup_partition_info matched;
        struct classicsetup_format_partition *target;
        const char *label;

        if (expected->role <= CLASSICSETUP_PARTITION_ROLE_NONE ||
            expected->role >= CLASSICSETUP_PARTITION_ROLE_COUNT) {
            return -1;
        }
        format = &role_format_plans[expected->role];
        if (!format_policy_matches(expected->role, format) ||
            classicsetup_match_partition_device(
                expected,
                partitions,
                partition_count,
                &matched) != 0) {
            return -1;
        }
        if (expected->role == CLASSICSETUP_PARTITION_ROLE_MSR) {
            temporary.msr_partition.role = expected->role;
            snprintf(
                temporary.msr_partition.device_path,
                sizeof(temporary.msr_partition.device_path),
                "%s",
                matched.device_path);
            temporary.msr_partition.start_sector = expected->start_sector;
            temporary.msr_partition.sector_count = expected->sector_count;
            temporary.msr_partition.filesystem = CLASSICSETUP_FS_NONE;
            temporary.msr_partition.mode = CLASSICSETUP_FORMAT_NONE;
            temporary.has_msr_partition = 1;
            continue;
        }
        if (output_index >= CLASSICSETUP_FORMAT_APPLY_MAX_PARTITIONS) {
            return -1;
        }
        label = label_for_role(expected->role);
        if (label == NULL) {
            return -1;
        }
        target = &temporary.partitions[output_index++];
        target->role = expected->role;
        snprintf(
            target->device_path,
            sizeof(target->device_path),
            "%s",
            matched.device_path);
        target->start_sector = expected->start_sector;
        target->sector_count = expected->sector_count;
        target->filesystem = format->filesystem;
        target->mode = format->mode;
        snprintf(target->label, sizeof(target->label), "%s", label);
    }
    temporary.partition_count = output_index;
    if (!classicsetup_validate_format_apply_plan(&temporary)) {
        return -1;
    }
    *format_apply_plan = temporary;
    return 0;
}

int classicsetup_validate_format_apply_plan(
    const struct classicsetup_format_apply_plan *plan)
{
    size_t target_index = 0;
    size_t index;

    if (plan == NULL ||
        !classicsetup_validate_apply_plan(&plan->partition_apply_plan) ||
        plan->partition_count != 3) {
        return 0;
    }
    for (index = 0;
         index < plan->partition_apply_plan.partition_count;
         ++index) {
        const struct classicsetup_apply_partition *expected =
            &plan->partition_apply_plan.partitions[index];
        const struct classicsetup_format_partition *target;
        const char *expected_label;

        if (expected->role == CLASSICSETUP_PARTITION_ROLE_MSR) {
            continue;
        }
        if (target_index >= plan->partition_count) {
            return 0;
        }
        target = &plan->partitions[target_index++];
        expected_label = label_for_role(expected->role);
        size_t previous_index;

        if (target->role != expected->role || expected_label == NULL ||
            target->start_sector != expected->start_sector ||
            target->sector_count != expected->sector_count ||
            !safe_device_path(target->device_path) ||
            strcmp(target->label, expected_label) != 0 ||
            !format_policy_matches(
                target->role,
                &(struct classicsetup_format_plan){
                    .valid = true,
                    .filesystem = target->filesystem,
                    .mode = target->mode
                })) {
            return 0;
        }
        for (previous_index = 0;
             previous_index + 1 < target_index;
             ++previous_index) {
            if (strcmp(
                    target->device_path,
                    plan->partitions[previous_index].device_path) == 0) {
                return 0;
            }
        }
    }
    if (target_index != plan->partition_count) {
        return 0;
    }
    if (plan->partition_apply_plan.table_type ==
        CLASSICSETUP_PARTITION_TABLE_GPT) {
        const struct classicsetup_apply_partition *expected_msr =
            &plan->partition_apply_plan.partitions[1];
        size_t formatted_index;

        if (!plan->has_msr_partition ||
            plan->msr_partition.role !=
                CLASSICSETUP_PARTITION_ROLE_MSR ||
            plan->msr_partition.filesystem != CLASSICSETUP_FS_NONE ||
            plan->msr_partition.mode != CLASSICSETUP_FORMAT_NONE ||
            !safe_device_path(plan->msr_partition.device_path) ||
            plan->msr_partition.start_sector !=
                expected_msr->start_sector ||
            plan->msr_partition.sector_count !=
                expected_msr->sector_count) {
            return 0;
        }
        for (formatted_index = 0;
             formatted_index < plan->partition_count;
             ++formatted_index) {
            if (strcmp(
                    plan->msr_partition.device_path,
                    plan->partitions[formatted_index].device_path) == 0) {
                return 0;
            }
        }
        return 1;
    }
    return !plan->has_msr_partition;
}

static const char *find_executable(const char *const paths[])
{
    size_t index;

    for (index = 0; paths[index] != NULL; ++index) {
        if (access(paths[index], X_OK) == 0) {
            return paths[index];
        }
    }
    return NULL;
}

static void store_tool_path(char *destination, size_t size, const char *path)
{
    if (path != NULL) {
        snprintf(destination, size, "%s", path);
    }
}

int classicsetup_resolve_format_tools(struct classicsetup_format_tools *tools)
{
    static const char *const fat_paths[] = {
        "/usr/sbin/mkfs.fat",
        "/usr/sbin/mkfs.vfat",
        "/sbin/mkfs.fat",
        "/sbin/mkfs.vfat",
        NULL
    };
    static const char *const ntfs_paths[] = {
        "/usr/sbin/mkfs.ntfs",
        "/sbin/mkfs.ntfs",
        NULL
    };
    static const char *const blkid_paths[] = {
        "/usr/sbin/blkid",
        "/sbin/blkid",
        "/usr/bin/blkid",
        "/bin/blkid",
        NULL
    };

    if (tools == NULL) {
        return -1;
    }
    memset(tools, 0, sizeof(*tools));
    store_tool_path(
        tools->fat,
        sizeof(tools->fat),
        find_executable(fat_paths));
    store_tool_path(
        tools->ntfs,
        sizeof(tools->ntfs),
        find_executable(ntfs_paths));
    store_tool_path(
        tools->blkid,
        sizeof(tools->blkid),
        find_executable(blkid_paths));
    return tools->fat[0] != '\0' && tools->ntfs[0] != '\0' &&
                   tools->blkid[0] != '\0'
               ? 0
               : -1;
}

int classicsetup_build_format_arguments(
    const struct classicsetup_format_partition *partition,
    const struct classicsetup_format_tools *tools,
    char *arguments[],
    size_t capacity,
    const char **executable)
{
    size_t used = 0;

    if (partition == NULL || tools == NULL || arguments == NULL ||
        executable == NULL || capacity < CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY ||
        !safe_device_path(partition->device_path)) {
        return -1;
    }
    if (partition->filesystem == CLASSICSETUP_FS_FAT32 &&
        partition->mode == CLASSICSETUP_FORMAT_QUICK &&
        tools->fat[0] != '\0') {
        *executable = tools->fat;
        arguments[used++] = (char *)tools->fat;
        arguments[used++] = "-F";
        arguments[used++] = "32";
        arguments[used++] = "-n";
        arguments[used++] = (char *)partition->label;
        arguments[used++] = (char *)partition->device_path;
    } else if (partition->filesystem == CLASSICSETUP_FS_NTFS &&
               (partition->mode == CLASSICSETUP_FORMAT_QUICK ||
                partition->mode == CLASSICSETUP_FORMAT_FULL) &&
               tools->ntfs[0] != '\0') {
        *executable = tools->ntfs;
        arguments[used++] = (char *)tools->ntfs;
        if (partition->mode == CLASSICSETUP_FORMAT_QUICK) {
            arguments[used++] = "-f";
        }
        arguments[used++] = "-L";
        arguments[used++] = (char *)partition->label;
        arguments[used++] = (char *)partition->device_path;
    } else {
        return -1;
    }
    arguments[used] = NULL;
    return 0;
}

int classicsetup_build_blkid_arguments(
    const struct classicsetup_format_partition *partition,
    const struct classicsetup_format_tools *tools,
    char *arguments[],
    size_t capacity,
    const char **executable)
{
    if (partition == NULL || tools == NULL || arguments == NULL ||
        executable == NULL ||
        capacity < CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY ||
        tools->blkid[0] == '\0' || !safe_device_path(partition->device_path)) {
        return -1;
    }
    *executable = tools->blkid;
    arguments[0] = (char *)tools->blkid;
    arguments[1] = "-p";
    arguments[2] = "-o";
    arguments[3] = "value";
    arguments[4] = "-s";
    arguments[5] = "TYPE";
    arguments[6] = (char *)partition->device_path;
    arguments[7] = NULL;
    return 0;
}

int classicsetup_filesystem_type_matches(
    enum classicsetup_filesystem_type expected,
    const char *detected)
{
    char value[32];
    size_t length;

    if (detected == NULL || strlen(detected) >= sizeof(value)) {
        return 0;
    }
    snprintf(value, sizeof(value), "%s", detected);
    length = strlen(value);
    while (length > 0 && isspace((unsigned char)value[length - 1])) {
        value[--length] = '\0';
    }
    if (expected == CLASSICSETUP_FS_FAT32) {
        return strcasecmp(value, "vfat") == 0 ||
               strcasecmp(value, "fat") == 0 ||
               strcasecmp(value, "fat32") == 0;
    }
    if (expected == CLASSICSETUP_FS_NTFS) {
        return strcasecmp(value, "ntfs") == 0;
    }
    return 0;
}

enum classicsetup_format_mount_status
classicsetup_check_device_mounted_from(
    unsigned int device_major,
    unsigned int device_minor,
    const char *mountinfo_path)
{
    char line[4096];
    FILE *mountinfo;

    if (mountinfo_path == NULL) {
        return CLASSICSETUP_FORMAT_MOUNT_UNKNOWN;
    }
    mountinfo = fopen(mountinfo_path, "r");
    if (mountinfo == NULL) {
        return CLASSICSETUP_FORMAT_MOUNT_UNKNOWN;
    }
    while (fgets(line, sizeof(line), mountinfo) != NULL) {
        unsigned int current_major;
        unsigned int current_minor;

        if (sscanf(
                line,
                "%*u %*u %u:%u",
                &current_major,
                &current_minor) != 2) {
            fclose(mountinfo);
            return CLASSICSETUP_FORMAT_MOUNT_UNKNOWN;
        }
        if (current_major == device_major && current_minor == device_minor) {
            fclose(mountinfo);
            return CLASSICSETUP_FORMAT_MOUNTED;
        }
    }
    if (ferror(mountinfo)) {
        fclose(mountinfo);
        return CLASSICSETUP_FORMAT_MOUNT_UNKNOWN;
    }
    fclose(mountinfo);
    return CLASSICSETUP_FORMAT_NOT_MOUNTED;
}

enum classicsetup_format_mount_status classicsetup_check_device_mounted(
    const char *device_path)
{
    struct stat status;

    if (device_path == NULL || stat(device_path, &status) != 0 ||
        !S_ISBLK(status.st_mode)) {
        return CLASSICSETUP_FORMAT_MOUNT_UNKNOWN;
    }
    return classicsetup_check_device_mounted_from(
        major(status.st_rdev),
        minor(status.st_rdev),
        "/proc/self/mountinfo");
}

enum classicsetup_format_safety_code classicsetup_evaluate_format_safety(
    const struct classicsetup_format_safety_inputs *inputs)
{
    if (inputs == NULL) {
        return CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN;
    }
    if (inputs->environment == CLASSICSETUP_ENV_WSL) {
        return CLASSICSETUP_FORMAT_SAFETY_WSL;
    }
    if (inputs->table_type == CLASSICSETUP_PARTITION_TABLE_MBR) {
        return CLASSICSETUP_FORMAT_SAFETY_MBR_NOT_ENABLED;
    }
    if (!classicsetup_environment_allows_apply(inputs->environment)) {
        return CLASSICSETUP_FORMAT_SAFETY_NOT_SUPPORTED_VM;
    }
    if (!inputs->destructive_unlocked) {
        return CLASSICSETUP_FORMAT_SAFETY_LOCKED;
    }
    if (!inputs->disk_identity_valid) {
        return CLASSICSETUP_FORMAT_SAFETY_DISK_IDENTITY;
    }
    if (inputs->system_disk_status ==
        CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE) {
        return CLASSICSETUP_FORMAT_SAFETY_SYSTEM_DISK;
    }
    if (inputs->system_disk_status != CLASSICSETUP_SYSTEM_DISK_SAFE) {
        return CLASSICSETUP_FORMAT_SAFETY_SYSTEM_DISK_UNKNOWN;
    }
    if (!inputs->logical_sector_size_supported) {
        return CLASSICSETUP_FORMAT_SAFETY_UNSUPPORTED_SECTOR_SIZE;
    }
    if (!inputs->format_plan_valid) {
        return CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN;
    }
    if (!inputs->partition_layout_valid) {
        return CLASSICSETUP_FORMAT_SAFETY_PARTITION_LAYOUT;
    }
    if (!inputs->partition_device_valid) {
        return CLASSICSETUP_FORMAT_SAFETY_PARTITION_DEVICE;
    }
    if (inputs->mount_status == CLASSICSETUP_FORMAT_MOUNTED) {
        return CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNTED;
    }
    if (inputs->mount_status != CLASSICSETUP_FORMAT_NOT_MOUNTED) {
        return CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNT_UNKNOWN;
    }
    if (!inputs->formatter_available || !inputs->verifier_available) {
        return CLASSICSETUP_FORMAT_SAFETY_TOOL_UNAVAILABLE;
    }
    return CLASSICSETUP_FORMAT_SAFETY_OK;
}

static int target_uses_512_byte_logical_sectors(const char *disk_name)
{
    char path[FORMAT_PATH_SIZE];
    char text[FORMAT_VALUE_SIZE];
    char *end;
    unsigned long value;
    FILE *file;
    int written = snprintf(
        path,
        sizeof(path),
        "/sys/block/%s/queue/logical_block_size",
        disk_name);

    if (written < 0 || (size_t)written >= sizeof(path)) {
        return 0;
    }
    file = fopen(path, "r");
    if (file == NULL || fgets(text, sizeof(text), file) == NULL) {
        if (file != NULL) {
            fclose(file);
        }
        return 0;
    }
    fclose(file);
    errno = 0;
    value = strtoul(text, &end, 10);
    while (isspace((unsigned char)*end)) {
        ++end;
    }
    return errno == 0 && *end == '\0' &&
           value == CLASSICSETUP_SECTOR_SIZE_BYTES;
}

static int collect_real_safety(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_partition *partition,
    struct classicsetup_format_safety_inputs *inputs,
    void *context)
{
    struct classicsetup_disk_info current_disk;
    struct classicsetup_partition_info partitions[FORMAT_SCAN_CAPACITY];
    struct classicsetup_partition_info matched;
    struct classicsetup_format_tools tools;
    struct stat status;
    size_t partition_count = 0;

    (void)context;
    if (inputs == NULL) {
        return -1;
    }
    memset(inputs, 0, sizeof(*inputs));
    inputs->system_disk_status = CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
    inputs->mount_status = CLASSICSETUP_FORMAT_MOUNT_UNKNOWN;
    if (plan == NULL || partition == NULL) {
        return 0;
    }
    inputs->table_type = plan->partition_apply_plan.table_type;
    if (classicsetup_detect_environment(&inputs->environment) != 0) {
        inputs->environment = CLASSICSETUP_ENV_UNKNOWN;
    }
    inputs->destructive_unlocked = classicsetup_destructive_unlock_enabled(
        getenv("CLASSICSETUP_ALLOW_DESTRUCTIVE"));
    inputs->format_plan_valid =
        classicsetup_validate_format_apply_plan(plan);
    classicsetup_resolve_format_tools(&tools);
    if (partition->filesystem == CLASSICSETUP_FS_NONE) {
        inputs->formatter_available = 1;
    } else {
        inputs->formatter_available =
            partition->filesystem == CLASSICSETUP_FS_FAT32
                ? tools.fat[0] != '\0'
                : tools.ntfs[0] != '\0';
    }
    inputs->verifier_available = tools.blkid[0] != '\0';
    if (inputs->environment == CLASSICSETUP_ENV_WSL ||
        plan->partition_apply_plan.table_type ==
            CLASSICSETUP_PARTITION_TABLE_MBR) {
        return 0;
    }
    inputs->disk_identity_valid = classicsetup_revalidate_target_disk(
        &plan->partition_apply_plan.target_disk,
        &current_disk) == 0;
    inputs->system_disk_status = classicsetup_check_system_disk(
        plan->partition_apply_plan.target_disk.name);
    inputs->logical_sector_size_supported =
        target_uses_512_byte_logical_sectors(
            plan->partition_apply_plan.target_disk.name);
    if (!inputs->disk_identity_valid ||
        classicsetup_scan_partitions(
            &current_disk,
            partitions,
            FORMAT_SCAN_CAPACITY,
            &partition_count) != 0) {
        return 0;
    }
    inputs->partition_layout_valid = classicsetup_verify_partition_ranges(
        &plan->partition_apply_plan,
        partitions,
        partition_count);
    inputs->partition_device_valid =
        classicsetup_match_partition_device(
            &(struct classicsetup_apply_partition){
                .role = partition->role,
                .start_sector = partition->start_sector,
                .sector_count = partition->sector_count
            },
            partitions,
            partition_count,
            &matched) == 0 &&
        strcmp(matched.device_path, partition->device_path) == 0 &&
        stat(partition->device_path, &status) == 0 &&
        S_ISBLK(status.st_mode);
    if (inputs->partition_device_valid) {
        inputs->mount_status = classicsetup_check_device_mounted(
            partition->device_path);
    }
    return 0;
}

static int run_real_formatter(
    const struct classicsetup_format_partition *partition,
    struct classicsetup_process_result *result,
    void *context)
{
    struct classicsetup_format_tools tools;
    char *arguments[CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY];
    const char *executable;

    (void)context;
    classicsetup_resolve_format_tools(&tools);
    if (classicsetup_build_format_arguments(
            partition,
            &tools,
            arguments,
            CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY,
            &executable) != 0) {
        return -1;
    }
    return classicsetup_run_process(executable, arguments, result);
}

static int verify_real_filesystem(
    const struct classicsetup_format_partition *partition,
    struct classicsetup_process_result *result,
    void *context)
{
    struct classicsetup_format_tools tools;
    char *arguments[CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY];
    const char *executable;

    (void)context;
    classicsetup_resolve_format_tools(&tools);
    if (classicsetup_build_blkid_arguments(
            partition,
            &tools,
            arguments,
            CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY,
            &executable) != 0 ||
        classicsetup_run_process(executable, arguments, result) != 0 ||
        !result->exited) {
        return -1;
    }
    if (partition->filesystem == CLASSICSETUP_FS_NONE) {
        return result->exit_status == 2 && result->output[0] == '\0';
    }
    if (result->exit_status != 0) {
        return -1;
    }
    return classicsetup_filesystem_type_matches(
        partition->filesystem,
        result->output);
}

int classicsetup_execute_format_apply_plan_with_ops(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_executor_ops *ops,
    struct classicsetup_format_result *result)
{
    size_t index;

    if (result == NULL || ops == NULL || ops->collect_safety == NULL ||
        ops->run_formatter == NULL || ops->verify_filesystem == NULL) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->code = CLASSICSETUP_FORMAT_RESULT_NOT_RUN;
    result->safety_code = CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN;
    result->failed_role = CLASSICSETUP_PARTITION_ROLE_NONE;
    if (plan != NULL &&
        plan->partition_apply_plan.table_type ==
            CLASSICSETUP_PARTITION_TABLE_MBR) {
        result->code = CLASSICSETUP_FORMAT_RESULT_BLOCKED;
        result->safety_code =
            CLASSICSETUP_FORMAT_SAFETY_MBR_NOT_ENABLED;
        return 0;
    }
    if (!classicsetup_validate_format_apply_plan(plan)) {
        result->code = CLASSICSETUP_FORMAT_RESULT_BLOCKED;
        return 0;
    }

    for (index = 0; index < plan->partition_count; ++index) {
        const struct classicsetup_format_partition *partition =
            &plan->partitions[index];
        struct classicsetup_format_safety_inputs safety;
        enum classicsetup_format_safety_code safety_code;
        int verification;
        int check;

        result->failed_role = partition->role;
        memset(&result->process, 0, sizeof(result->process));
        memset(
            &result->verification_process,
            0,
            sizeof(result->verification_process));
        for (check = 0; check < 2; ++check) {
            if (ops->collect_safety(
                    plan,
                    partition,
                    &safety,
                    ops->context) != 0) {
                result->code = CLASSICSETUP_FORMAT_RESULT_BLOCKED;
                result->safety_code =
                    CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN;
                return 0;
            }
            result->environment = safety.environment;
            safety_code = classicsetup_evaluate_format_safety(&safety);
            if (safety_code != CLASSICSETUP_FORMAT_SAFETY_OK) {
                result->code = CLASSICSETUP_FORMAT_RESULT_BLOCKED;
                result->safety_code = safety_code;
                return 0;
            }
        }
        result->safety_code = CLASSICSETUP_FORMAT_SAFETY_OK;
        if (ops->run_formatter(
                partition,
                &result->process,
                ops->context) != 0 ||
            !result->process.exited || result->process.exit_status != 0) {
            result->code = CLASSICSETUP_FORMAT_RESULT_PROCESS_FAILED;
            return 0;
        }
        verification = ops->verify_filesystem(
            partition,
            &result->verification_process,
            ops->context);
        if (verification != 1) {
            result->code = CLASSICSETUP_FORMAT_RESULT_VERIFY_FAILED;
            return 0;
        }
        ++result->completed_count;
    }
    if (plan->has_msr_partition) {
        struct classicsetup_format_safety_inputs safety;
        enum classicsetup_format_safety_code safety_code;
        int check;
        int verification;

        result->failed_role = CLASSICSETUP_PARTITION_ROLE_MSR;
        memset(&result->process, 0, sizeof(result->process));
        memset(
            &result->verification_process,
            0,
            sizeof(result->verification_process));
        for (check = 0; check < 2; ++check) {
            if (ops->collect_safety(
                    plan,
                    &plan->msr_partition,
                    &safety,
                    ops->context) != 0) {
                result->code = CLASSICSETUP_FORMAT_RESULT_BLOCKED;
                result->safety_code =
                    CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN;
                return 0;
            }
            result->environment = safety.environment;
            safety_code = classicsetup_evaluate_format_safety(&safety);
            if (safety_code != CLASSICSETUP_FORMAT_SAFETY_OK) {
                result->code = CLASSICSETUP_FORMAT_RESULT_BLOCKED;
                result->safety_code = safety_code;
                return 0;
            }
        }
        verification = ops->verify_filesystem(
            &plan->msr_partition,
            &result->verification_process,
            ops->context);
        if (verification != 1) {
            result->code = CLASSICSETUP_FORMAT_RESULT_VERIFY_FAILED;
            return 0;
        }
    }
    result->failed_role = CLASSICSETUP_PARTITION_ROLE_NONE;
    result->code = CLASSICSETUP_FORMAT_RESULT_SUCCESS;
    return 0;
}

int classicsetup_execute_format_apply_plan(
    const struct classicsetup_format_apply_plan *plan,
    struct classicsetup_format_result *result)
{
    const struct classicsetup_format_executor_ops ops = {
        .collect_safety = collect_real_safety,
        .run_formatter = run_real_formatter,
        .verify_filesystem = verify_real_filesystem,
        .context = NULL
    };

    return classicsetup_execute_format_apply_plan_with_ops(
        plan,
        &ops,
        result);
}

const char *classicsetup_format_safety_message(
    enum classicsetup_format_safety_code code)
{
    switch (code) {
    case CLASSICSETUP_FORMAT_SAFETY_OK:
        return "All filesystem safety checks passed.";
    case CLASSICSETUP_FORMAT_SAFETY_WSL:
        return "Filesystem formatting is disabled under WSL.";
    case CLASSICSETUP_FORMAT_SAFETY_NOT_SUPPORTED_VM:
        return "Formatting is allowed only in a verified VirtualBox or VMware VM.";
    case CLASSICSETUP_FORMAT_SAFETY_LOCKED:
        return "Set CLASSICSETUP_ALLOW_DESTRUCTIVE=YES in the test VM.";
    case CLASSICSETUP_FORMAT_SAFETY_MBR_NOT_ENABLED:
        return "Legacy BIOS/MBR formatting is not enabled for destructive testing yet.";
    case CLASSICSETUP_FORMAT_SAFETY_DISK_IDENTITY:
        return "The format target disk identity could not be revalidated.";
    case CLASSICSETUP_FORMAT_SAFETY_SYSTEM_DISK:
        return "The format target belongs to the running Linux system disk.";
    case CLASSICSETUP_FORMAT_SAFETY_SYSTEM_DISK_UNKNOWN:
        return "The running Linux system disk could not be identified safely.";
    case CLASSICSETUP_FORMAT_SAFETY_UNSUPPORTED_SECTOR_SIZE:
        return "Formatting supports only 512-byte logical sectors.";
    case CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN:
        return "The immutable filesystem apply plan is invalid.";
    case CLASSICSETUP_FORMAT_SAFETY_PARTITION_LAYOUT:
        return "The current partition ranges no longer match the apply plan.";
    case CLASSICSETUP_FORMAT_SAFETY_PARTITION_DEVICE:
        return "A format target is not the expected block partition.";
    case CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNTED:
        return "A format target partition is mounted.";
    case CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNT_UNKNOWN:
        return "The target partition mount state could not be verified safely.";
    case CLASSICSETUP_FORMAT_SAFETY_TOOL_UNAVAILABLE:
        return "A required filesystem formatter or blkid is unavailable.";
    }
    return "Unknown filesystem safety failure.";
}
