#define _POSIX_C_SOURCE 200809L

#include "classicsetup/apply.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

enum {
    APPLY_SCAN_CAPACITY = 64,
    APPLY_VERIFY_ATTEMPTS = 10,
    APPLY_VERIFY_DELAY_NS = 100000000
};

static const char *guid_for_role(enum classicsetup_partition_role role)
{
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_EFI:
        return CLASSICSETUP_GPT_TYPE_EFI;
    case CLASSICSETUP_PARTITION_ROLE_MSR:
        return CLASSICSETUP_GPT_TYPE_MSR;
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        return CLASSICSETUP_GPT_TYPE_BASIC_DATA;
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        return CLASSICSETUP_GPT_TYPE_RECOVERY;
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        return NULL;
    }
    return NULL;
}

static const char *name_for_role(enum classicsetup_partition_role role)
{
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_EFI:
        return "EFI System Partition";
    case CLASSICSETUP_PARTITION_ROLE_MSR:
        return "Microsoft Reserved Partition";
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        return "Windows Partition";
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        return "Windows Recovery Partition";
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
        return "System Reserved Partition";
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        return NULL;
    }
    return NULL;
}

static int mbr_metadata_for_role(
    enum classicsetup_partition_role role,
    unsigned int *type,
    int *bootable)
{
    if (type == NULL || bootable == NULL) {
        return -1;
    }
    *bootable = 0;
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED:
        *type = CLASSICSETUP_MBR_TYPE_NTFS;
        *bootable = 1;
        return 0;
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        *type = CLASSICSETUP_MBR_TYPE_NTFS;
        return 0;
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        *type = CLASSICSETUP_MBR_TYPE_RECOVERY;
        return 0;
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_EFI:
    case CLASSICSETUP_PARTITION_ROLE_MSR:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        return -1;
    }
    return -1;
}

static int safe_device_identity(const struct classicsetup_disk_info *disk)
{
    char expected[CLASSICSETUP_DISK_PATH_SIZE];
    size_t index;
    int written;

    if (disk == NULL || disk->name[0] == '\0') {
        return 0;
    }
    for (index = 0; disk->name[index] != '\0'; ++index) {
        unsigned char character = (unsigned char)disk->name[index];

        if (!(character >= 'a' && character <= 'z') &&
            !(character >= 'A' && character <= 'Z') &&
            !(character >= '0' && character <= '9') &&
            character != '_' && character != '-') {
            return 0;
        }
    }
    written = snprintf(expected, sizeof(expected), "/dev/%s", disk->name);
    return written >= 0 && (size_t)written < sizeof(expected) &&
           strcmp(expected, disk->device_path) == 0;
}

int classicsetup_build_apply_plan_for_mode(
    enum classicsetup_install_mode install_mode,
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_plan *partition_plan,
    size_t original_partition_count,
    struct classicsetup_apply_plan *apply_plan)
{
    struct classicsetup_apply_plan temporary = {0};
    size_t output_index = 0;
    size_t index;

    if (disk == NULL || partition_plan == NULL || apply_plan == NULL ||
        (install_mode != CLASSICSETUP_INSTALL_UEFI_GPT &&
         install_mode != CLASSICSETUP_INSTALL_BIOS_MBR) ||
        original_partition_count != 0 || !safe_device_identity(disk) ||
        !classicsetup_plan_validate(partition_plan) ||
        !classicsetup_plan_has_windows_layout_for_mode(
            partition_plan,
            install_mode) ||
        partition_plan->disk_sector_count !=
            disk->size_bytes / CLASSICSETUP_SECTOR_SIZE_BYTES) {
        return -1;
    }

    temporary.table_type =
        install_mode == CLASSICSETUP_INSTALL_BIOS_MBR
            ? CLASSICSETUP_PARTITION_TABLE_MBR
            : CLASSICSETUP_PARTITION_TABLE_GPT;
    temporary.target_disk = *disk;
    temporary.disk_sector_count = partition_plan->disk_sector_count;

    for (index = 0; index < partition_plan->item_count; ++index) {
        const struct classicsetup_plan_item *source =
            &partition_plan->items[index];
        struct classicsetup_apply_partition *destination;
        const char *guid = NULL;
        const char *name;

        if (source->state == CLASSICSETUP_PLAN_UNALLOCATED) {
            continue;
        }
        if (source->state != CLASSICSETUP_PLAN_NEW ||
            output_index >= CLASSICSETUP_APPLY_MAX_PARTITIONS) {
            return -1;
        }
        name = name_for_role(source->role);
        if (name == NULL) {
            return -1;
        }

        destination = &temporary.partitions[output_index++];
        destination->role = source->role;
        destination->start_sector = source->start_sector;
        destination->sector_count = source->sector_count;
        snprintf(destination->name, sizeof(destination->name), "%s", name);
        if (temporary.table_type == CLASSICSETUP_PARTITION_TABLE_GPT) {
            guid = guid_for_role(source->role);
            if (guid == NULL) {
                return -1;
            }
            snprintf(
                destination->type_guid,
                sizeof(destination->type_guid),
                "%s",
                guid);
        } else if (mbr_metadata_for_role(
                       source->role,
                       &destination->mbr_type,
                       &destination->bootable) != 0) {
            return -1;
        }
    }
    temporary.partition_count = output_index;

    if (!classicsetup_validate_apply_plan(&temporary)) {
        return -1;
    }
    *apply_plan = temporary;
    return 0;
}

int classicsetup_build_apply_plan(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_plan *partition_plan,
    size_t original_partition_count,
    struct classicsetup_apply_plan *apply_plan)
{
    return classicsetup_build_apply_plan_for_mode(
        CLASSICSETUP_INSTALL_UEFI_GPT,
        disk,
        partition_plan,
        original_partition_count,
        apply_plan);
}

static int validate_gpt_apply_plan(
    const struct classicsetup_apply_plan *apply_plan)
{
    const enum classicsetup_partition_role expected_roles[] = {
        CLASSICSETUP_PARTITION_ROLE_EFI,
        CLASSICSETUP_PARTITION_ROLE_MSR,
        CLASSICSETUP_PARTITION_ROLE_WINDOWS,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY
    };
    unsigned long long previous_end = 0;
    size_t index;

    if (apply_plan->partition_count !=
            sizeof(expected_roles) / sizeof(expected_roles[0]) ||
        apply_plan->disk_sector_count <
            2ULL * CLASSICSETUP_SECTORS_PER_MB) {
        return 0;
    }

    for (index = 0; index < apply_plan->partition_count; ++index) {
        const struct classicsetup_apply_partition *partition =
            &apply_plan->partitions[index];
        const char *expected_guid = guid_for_role(expected_roles[index]);
        const char *expected_name = name_for_role(expected_roles[index]);
        unsigned long long end;

        if (partition->role != expected_roles[index] ||
            partition->sector_count == 0 || expected_guid == NULL ||
            expected_name == NULL ||
            strcmp(partition->type_guid, expected_guid) != 0 ||
            strcmp(partition->name, expected_name) != 0 ||
            partition->mbr_type != 0 || partition->bootable ||
            partition->start_sector < CLASSICSETUP_SECTORS_PER_MB ||
            partition->start_sector % CLASSICSETUP_SECTORS_PER_MB != 0 ||
            partition->start_sector < previous_end ||
            partition->start_sector > apply_plan->disk_sector_count -
                                          CLASSICSETUP_SECTORS_PER_MB ||
            partition->sector_count >
                apply_plan->disk_sector_count - partition->start_sector) {
            return 0;
        }
        end = partition->start_sector + partition->sector_count;
        if (end > apply_plan->disk_sector_count -
                      CLASSICSETUP_SECTORS_PER_MB) {
            return 0;
        }
        previous_end = end;
    }
    return 1;
}

static int validate_mbr_apply_plan(
    const struct classicsetup_apply_plan *apply_plan)
{
    const enum classicsetup_partition_role expected_roles[] = {
        CLASSICSETUP_PARTITION_ROLE_SYSTEM_RESERVED,
        CLASSICSETUP_PARTITION_ROLE_WINDOWS,
        CLASSICSETUP_PARTITION_ROLE_RECOVERY
    };
    unsigned long long previous_end = 0;
    size_t index;

    if (apply_plan->disk_sector_count > CLASSICSETUP_MBR_MAX_SECTORS ||
        apply_plan->partition_count !=
            sizeof(expected_roles) / sizeof(expected_roles[0]) ||
        apply_plan->partition_count > 4) {
        return 0;
    }

    for (index = 0; index < apply_plan->partition_count; ++index) {
        const struct classicsetup_apply_partition *partition =
            &apply_plan->partitions[index];
        const char *expected_name = name_for_role(expected_roles[index]);
        unsigned int expected_type;
        int expected_bootable;
        unsigned long long end;

        if (mbr_metadata_for_role(
                expected_roles[index],
                &expected_type,
                &expected_bootable) != 0 ||
            partition->role != expected_roles[index] ||
            partition->sector_count == 0 || expected_name == NULL ||
            strcmp(partition->name, expected_name) != 0 ||
            partition->type_guid[0] != '\0' ||
            partition->mbr_type != expected_type ||
            partition->bootable != expected_bootable ||
            partition->start_sector < CLASSICSETUP_SECTORS_PER_MB ||
            partition->start_sector % CLASSICSETUP_SECTORS_PER_MB != 0 ||
            (index == 0 &&
             partition->start_sector != CLASSICSETUP_SECTORS_PER_MB) ||
            partition->start_sector < previous_end ||
            (index > 0 && partition->start_sector != previous_end) ||
            partition->start_sector > apply_plan->disk_sector_count ||
            partition->sector_count >
                apply_plan->disk_sector_count - partition->start_sector) {
            return 0;
        }
        end = partition->start_sector + partition->sector_count;
        if (end > apply_plan->disk_sector_count) {
            return 0;
        }
        previous_end = end;
    }

    return apply_plan->partitions[0].sector_count ==
               CLASSICSETUP_DEFAULT_SYSTEM_RESERVED_MB *
                   CLASSICSETUP_SECTORS_PER_MB &&
           apply_plan->partitions[1].sector_count >=
               CLASSICSETUP_MIN_WINDOWS_MB *
                   CLASSICSETUP_SECTORS_PER_MB &&
           apply_plan->partitions[2].sector_count ==
               CLASSICSETUP_DEFAULT_RECOVERY_MB *
                   CLASSICSETUP_SECTORS_PER_MB;
}

int classicsetup_validate_apply_plan(
    const struct classicsetup_apply_plan *apply_plan)
{
    if (apply_plan == NULL || !safe_device_identity(&apply_plan->target_disk) ||
        apply_plan->disk_sector_count !=
            apply_plan->target_disk.size_bytes /
                CLASSICSETUP_SECTOR_SIZE_BYTES) {
        return 0;
    }
    if (apply_plan->table_type == CLASSICSETUP_PARTITION_TABLE_GPT) {
        return validate_gpt_apply_plan(apply_plan);
    }
    if (apply_plan->table_type == CLASSICSETUP_PARTITION_TABLE_MBR) {
        return validate_mbr_apply_plan(apply_plan);
    }
    return 0;
}

static int append_script(
    char *script,
    size_t script_size,
    size_t *used,
    const char *format,
    ...)
{
    va_list arguments;
    int written;

    if (*used >= script_size) {
        return -1;
    }
    va_start(arguments, format);
    written = vsnprintf(
        script + *used,
        script_size - *used,
        format,
        arguments);
    va_end(arguments);
    if (written < 0 || (size_t)written >= script_size - *used) {
        return -1;
    }
    *used += (size_t)written;
    return 0;
}

int classicsetup_render_sfdisk_script(
    const struct classicsetup_apply_plan *apply_plan,
    char *script,
    size_t script_size)
{
    size_t used = 0;
    size_t index;

    if (script == NULL || script_size == 0 ||
        !classicsetup_validate_apply_plan(apply_plan)) {
        return -1;
    }
    script[0] = '\0';
    if (append_script(
            script,
            script_size,
            &used,
            apply_plan->table_type == CLASSICSETUP_PARTITION_TABLE_GPT
                ? "label: gpt\nunit: sectors\n\n"
                : "label: dos\nunit: sectors\n\n") != 0) {
        return -1;
    }

    for (index = 0; index < apply_plan->partition_count; ++index) {
        const struct classicsetup_apply_partition *partition =
            &apply_plan->partitions[index];

        if (apply_plan->table_type == CLASSICSETUP_PARTITION_TABLE_GPT) {
            if (append_script(
                    script,
                    script_size,
                    &used,
                    "start=%llu, size=%llu, type=%s, name=\"%s\"\n",
                    partition->start_sector,
                    partition->sector_count,
                    partition->type_guid,
                    partition->name) != 0) {
                return -1;
            }
        } else if (append_script(
                       script,
                       script_size,
                       &used,
                       "start=%llu, size=%llu, type=%02x%s\n",
                       partition->start_sector,
                       partition->sector_count,
                       partition->mbr_type,
                       partition->bootable ? ", bootable" : "") != 0) {
            return -1;
        }
    }
    return 0;
}

int classicsetup_disk_identity_matches(
    const struct classicsetup_disk_info *selected,
    const struct classicsetup_disk_info *current)
{
    if (selected == NULL || current == NULL) {
        return 0;
    }
    if (strcmp(selected->name, current->name) != 0 ||
        strcmp(selected->device_path, current->device_path) != 0 ||
        selected->size_bytes != current->size_bytes ||
        strcmp(selected->model, current->model) != 0) {
        return 0;
    }
    if (selected->has_serial &&
        (!current->has_serial ||
         strcmp(selected->serial, current->serial) != 0)) {
        return 0;
    }
    if (selected->has_wwn &&
        (!current->has_wwn || strcmp(selected->wwn, current->wwn) != 0)) {
        return 0;
    }
    if (selected->has_logical_sector_size &&
        (!current->has_logical_sector_size ||
         selected->logical_sector_size != current->logical_sector_size)) {
        return 0;
    }
    if (selected->has_removable &&
        (!current->has_removable ||
         selected->removable != current->removable)) {
        return 0;
    }
    if (selected->transport[0] != '\0' &&
        strcmp(selected->transport, current->transport) != 0) {
        return 0;
    }
    if (selected->sysfs_path[0] != '\0' &&
        strcmp(selected->sysfs_path, current->sysfs_path) != 0) {
        return 0;
    }
    return 1;
}

int classicsetup_revalidate_target_disk(
    const struct classicsetup_disk_info *selected,
    struct classicsetup_disk_info *current)
{
    struct classicsetup_disk_info disks[APPLY_SCAN_CAPACITY];
    struct stat status;
    size_t disk_count = 0;
    size_t index;

    if (selected == NULL || current == NULL ||
        classicsetup_scan_disks(
            disks,
            APPLY_SCAN_CAPACITY,
            &disk_count) != 0) {
        return -1;
    }
    for (index = 0; index < disk_count; ++index) {
        if (strcmp(disks[index].name, selected->name) == 0) {
            if (!classicsetup_disk_identity_matches(selected, &disks[index]) ||
                stat(selected->device_path, &status) != 0 ||
                !S_ISBLK(status.st_mode)) {
                return -1;
            }
            *current = disks[index];
            return 0;
        }
    }
    return -1;
}

enum classicsetup_apply_safety_code classicsetup_evaluate_apply_safety(
    const struct classicsetup_apply_safety_inputs *inputs)
{
    if (inputs == NULL) {
        return CLASSICSETUP_APPLY_SAFETY_INVALID_PLAN;
    }
    if (inputs->environment == CLASSICSETUP_ENV_WSL) {
        return CLASSICSETUP_APPLY_SAFETY_WSL;
    }
    if (inputs->table_type == CLASSICSETUP_PARTITION_TABLE_MBR) {
        return CLASSICSETUP_APPLY_SAFETY_MBR_NOT_ENABLED;
    }
    if (!classicsetup_environment_allows_apply(inputs->environment)) {
        return CLASSICSETUP_APPLY_SAFETY_NOT_SUPPORTED_VM;
    }
    if (!inputs->destructive_unlocked) {
        return CLASSICSETUP_APPLY_SAFETY_LOCKED;
    }
    if (!inputs->disk_identity_valid) {
        return CLASSICSETUP_APPLY_SAFETY_DISK_IDENTITY;
    }
    if (inputs->system_disk_status ==
        CLASSICSETUP_SYSTEM_DISK_TARGET_IN_USE) {
        return CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK;
    }
    if (inputs->system_disk_status != CLASSICSETUP_SYSTEM_DISK_SAFE) {
        return CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK_UNKNOWN;
    }
    if (inputs->existing_partition_count != 0) {
        return CLASSICSETUP_APPLY_SAFETY_EXISTING_PARTITIONS;
    }
    if (!inputs->logical_sector_size_supported) {
        return CLASSICSETUP_APPLY_SAFETY_UNSUPPORTED_SECTOR_SIZE;
    }
    if (!inputs->apply_plan_valid) {
        return CLASSICSETUP_APPLY_SAFETY_INVALID_PLAN;
    }
    if (!inputs->tool_available) {
        return CLASSICSETUP_APPLY_SAFETY_TOOL_UNAVAILABLE;
    }
    return CLASSICSETUP_APPLY_SAFETY_OK;
}

int classicsetup_verify_partition_ranges(
    const struct classicsetup_apply_plan *apply_plan,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count)
{
    size_t index;

    if (!classicsetup_validate_apply_plan(apply_plan) ||
        partitions == NULL ||
        partition_count != apply_plan->partition_count) {
        return 0;
    }
    for (index = 0; index < partition_count; ++index) {
        if (partitions[index].start_sector !=
                apply_plan->partitions[index].start_sector ||
            partitions[index].sector_count !=
                apply_plan->partitions[index].sector_count) {
            return 0;
        }
    }
    return 1;
}

static const char *find_sfdisk(void)
{
    if (access("/usr/sbin/sfdisk", X_OK) == 0) {
        return "/usr/sbin/sfdisk";
    }
    if (access("/sbin/sfdisk", X_OK) == 0) {
        return "/sbin/sfdisk";
    }
    return NULL;
}

static int target_uses_512_byte_logical_sectors(const char *disk_name)
{
    char path[512];
    char text[64];
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
    if (file == NULL) {
        return 0;
    }
    if (fgets(text, sizeof(text), file) == NULL) {
        fclose(file);
        return 0;
    }
    fclose(file);

    value = strtoul(text, &end, 10);
    while (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r') {
        ++end;
    }
    return *end == '\0' && value == CLASSICSETUP_SECTOR_SIZE_BYTES;
}

static void collect_apply_safety(
    const struct classicsetup_apply_plan *apply_plan,
    const char *sfdisk_path,
    struct classicsetup_apply_safety_inputs *safety)
{
    struct classicsetup_disk_info current_disk;
    struct classicsetup_partition_info partitions[APPLY_SCAN_CAPACITY];
    size_t partition_count = 0;

    memset(safety, 0, sizeof(*safety));
    safety->system_disk_status = CLASSICSETUP_SYSTEM_DISK_UNKNOWN;
    if (apply_plan != NULL) {
        safety->table_type = apply_plan->table_type;
    }
    if (classicsetup_detect_environment(&safety->environment) != 0) {
        safety->environment = CLASSICSETUP_ENV_UNKNOWN;
    }
    safety->destructive_unlocked = classicsetup_destructive_unlock_enabled(
        getenv("CLASSICSETUP_ALLOW_DESTRUCTIVE"));
    safety->apply_plan_valid = classicsetup_validate_apply_plan(apply_plan);
    safety->tool_available = sfdisk_path != NULL;

    if (safety->environment == CLASSICSETUP_ENV_WSL || apply_plan == NULL) {
        return;
    }
    safety->disk_identity_valid = classicsetup_revalidate_target_disk(
        &apply_plan->target_disk,
        &current_disk) == 0;
    safety->system_disk_status = classicsetup_check_system_disk(
        apply_plan->target_disk.name);
    if (safety->disk_identity_valid &&
        classicsetup_scan_partitions(
            &current_disk,
            partitions,
            APPLY_SCAN_CAPACITY,
            &partition_count) != 0) {
        safety->disk_identity_valid = 0;
    }
    safety->existing_partition_count = partition_count;
    safety->logical_sector_size_supported =
        target_uses_512_byte_logical_sectors(apply_plan->target_disk.name);
}

static int verify_applied_layout(
    const struct classicsetup_apply_plan *apply_plan)
{
    struct classicsetup_partition_info partitions[APPLY_SCAN_CAPACITY];
    struct timespec delay = {
        .tv_sec = 0,
        .tv_nsec = APPLY_VERIFY_DELAY_NS
    };
    size_t attempt;

    for (attempt = 0; attempt < APPLY_VERIFY_ATTEMPTS; ++attempt) {
        struct classicsetup_disk_info current_disk;
        size_t partition_count = 0;

        if (classicsetup_revalidate_target_disk(
                &apply_plan->target_disk,
                &current_disk) == 0 &&
            classicsetup_scan_partitions(
                &current_disk,
                partitions,
                APPLY_SCAN_CAPACITY,
                &partition_count) == 0 &&
            classicsetup_verify_partition_ranges(
                apply_plan,
                partitions,
                partition_count)) {
            return 1;
        }
        nanosleep(&delay, NULL);
    }
    return 0;
}

int classicsetup_execute_apply_plan(
    const struct classicsetup_apply_plan *apply_plan,
    struct classicsetup_apply_result *result)
{
    struct classicsetup_apply_safety_inputs safety = {0};
    char script[CLASSICSETUP_SFDISK_SCRIPT_SIZE];
    const char *sfdisk_path;
    char *arguments[7];

    if (result == NULL) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    result->code = CLASSICSETUP_APPLY_RESULT_NOT_RUN;
    result->safety_code = CLASSICSETUP_APPLY_SAFETY_INVALID_PLAN;

    if (apply_plan != NULL &&
        apply_plan->table_type == CLASSICSETUP_PARTITION_TABLE_MBR) {
        result->code = CLASSICSETUP_APPLY_RESULT_BLOCKED;
        result->safety_code = CLASSICSETUP_APPLY_SAFETY_MBR_NOT_ENABLED;
        return 0;
    }

    sfdisk_path = find_sfdisk();
    collect_apply_safety(apply_plan, sfdisk_path, &safety);
    result->environment = safety.environment;
    result->safety_code = classicsetup_evaluate_apply_safety(&safety);
    if (result->safety_code != CLASSICSETUP_APPLY_SAFETY_OK) {
        result->code = CLASSICSETUP_APPLY_RESULT_BLOCKED;
        return 0;
    }

    if (classicsetup_render_sfdisk_script(
            apply_plan,
            script,
            sizeof(script)) != 0) {
        result->code = CLASSICSETUP_APPLY_RESULT_BLOCKED;
        result->safety_code = CLASSICSETUP_APPLY_SAFETY_INVALID_PLAN;
        return 0;
    }

    collect_apply_safety(apply_plan, sfdisk_path, &safety);
    result->environment = safety.environment;
    result->safety_code = classicsetup_evaluate_apply_safety(&safety);
    if (result->safety_code != CLASSICSETUP_APPLY_SAFETY_OK) {
        result->code = CLASSICSETUP_APPLY_RESULT_BLOCKED;
        return 0;
    }

    arguments[0] = (char *)sfdisk_path;
    arguments[1] = "--lock";
    arguments[2] = "--wipe";
    arguments[3] = "never";
    arguments[4] = (char *)apply_plan->target_disk.device_path;
    arguments[5] = NULL;
    arguments[6] = NULL;
    if (classicsetup_run_process_with_input(
            sfdisk_path,
            arguments,
            script,
            &result->process) != 0 ||
        !result->process.exited || result->process.exit_status != 0) {
        result->code = CLASSICSETUP_APPLY_RESULT_PROCESS_FAILED;
        return 0;
    }
    if (!verify_applied_layout(apply_plan)) {
        result->code = CLASSICSETUP_APPLY_RESULT_VERIFY_FAILED;
        return 0;
    }
    result->code = CLASSICSETUP_APPLY_RESULT_SUCCESS;
    return 0;
}

const char *classicsetup_apply_safety_message(
    enum classicsetup_apply_safety_code code)
{
    switch (code) {
    case CLASSICSETUP_APPLY_SAFETY_OK:
        return "All destructive safety checks passed.";
    case CLASSICSETUP_APPLY_SAFETY_WSL:
        return "Destructive disk operations are disabled under WSL.";
    case CLASSICSETUP_APPLY_SAFETY_NOT_SUPPORTED_VM:
        return "Apply is allowed only in a verified VirtualBox or VMware VM.";
    case CLASSICSETUP_APPLY_SAFETY_LOCKED:
        return "Set CLASSICSETUP_ALLOW_DESTRUCTIVE=YES in the test VM.";
    case CLASSICSETUP_APPLY_SAFETY_DISK_IDENTITY:
        return "The target disk identity could not be revalidated.";
    case CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK:
        return "The selected disk contains the running Linux system.";
    case CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK_UNKNOWN:
        return "The running Linux system disk could not be identified safely.";
    case CLASSICSETUP_APPLY_SAFETY_EXISTING_PARTITIONS:
        return "M7 apply requires a test disk with no existing partitions.";
    case CLASSICSETUP_APPLY_SAFETY_UNSUPPORTED_SECTOR_SIZE:
        return "M7 apply supports only 512-byte logical sectors.";
    case CLASSICSETUP_APPLY_SAFETY_MBR_NOT_ENABLED:
        return "Legacy BIOS/MBR apply is not enabled for destructive testing yet.";
    case CLASSICSETUP_APPLY_SAFETY_INVALID_PLAN:
        return "The partition apply plan is incomplete or invalid.";
    case CLASSICSETUP_APPLY_SAFETY_TOOL_UNAVAILABLE:
        return "The sfdisk executable is unavailable.";
    }
    return "Unknown safety failure.";
}
