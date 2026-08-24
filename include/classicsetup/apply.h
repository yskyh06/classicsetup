#ifndef CLASSICSETUP_APPLY_H
#define CLASSICSETUP_APPLY_H

#include <stddef.h>

#include "classicsetup/disk.h"
#include "classicsetup/environment.h"
#include "classicsetup/partition.h"
#include "classicsetup/partition_plan.h"
#include "classicsetup/process.h"
#include "classicsetup/system_disk.h"

enum {
    CLASSICSETUP_APPLY_MAX_PARTITIONS = 8,
    CLASSICSETUP_GPT_GUID_SIZE = 37,
    CLASSICSETUP_APPLY_NAME_SIZE = 64,
    CLASSICSETUP_SFDISK_SCRIPT_SIZE = 4096
};

#define CLASSICSETUP_GPT_TYPE_EFI \
    "c12a7328-f81f-11d2-ba4b-00a0c93ec93b"
#define CLASSICSETUP_GPT_TYPE_MSR \
    "e3c9e316-0b5c-4db8-817d-f92df00215ae"
#define CLASSICSETUP_GPT_TYPE_BASIC_DATA \
    "ebd0a0a2-b9e5-4433-87c0-68b6b72699c7"
#define CLASSICSETUP_GPT_TYPE_RECOVERY \
    "de94bba4-06d1-4d40-a16a-bfd50179d6ac"

struct classicsetup_apply_partition {
    enum classicsetup_partition_role role;
    unsigned long long start_sector;
    unsigned long long sector_count;
    char type_guid[CLASSICSETUP_GPT_GUID_SIZE];
    char name[CLASSICSETUP_APPLY_NAME_SIZE];
};

struct classicsetup_apply_plan {
    struct classicsetup_disk_info target_disk;
    struct classicsetup_apply_partition
        partitions[CLASSICSETUP_APPLY_MAX_PARTITIONS];
    size_t partition_count;
    unsigned long long disk_sector_count;
};

enum classicsetup_apply_safety_code {
    CLASSICSETUP_APPLY_SAFETY_OK,
    CLASSICSETUP_APPLY_SAFETY_WSL,
    CLASSICSETUP_APPLY_SAFETY_NOT_SUPPORTED_VM,
    CLASSICSETUP_APPLY_SAFETY_LOCKED,
    CLASSICSETUP_APPLY_SAFETY_DISK_IDENTITY,
    CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK,
    CLASSICSETUP_APPLY_SAFETY_SYSTEM_DISK_UNKNOWN,
    CLASSICSETUP_APPLY_SAFETY_EXISTING_PARTITIONS,
    CLASSICSETUP_APPLY_SAFETY_UNSUPPORTED_SECTOR_SIZE,
    CLASSICSETUP_APPLY_SAFETY_INVALID_PLAN,
    CLASSICSETUP_APPLY_SAFETY_TOOL_UNAVAILABLE
};

struct classicsetup_apply_safety_inputs {
    enum classicsetup_environment environment;
    int destructive_unlocked;
    int disk_identity_valid;
    enum classicsetup_system_disk_status system_disk_status;
    size_t existing_partition_count;
    int logical_sector_size_supported;
    int apply_plan_valid;
    int tool_available;
};

enum classicsetup_apply_result_code {
    CLASSICSETUP_APPLY_RESULT_NOT_RUN,
    CLASSICSETUP_APPLY_RESULT_BLOCKED,
    CLASSICSETUP_APPLY_RESULT_PROCESS_FAILED,
    CLASSICSETUP_APPLY_RESULT_VERIFY_FAILED,
    CLASSICSETUP_APPLY_RESULT_SUCCESS
};

struct classicsetup_apply_result {
    enum classicsetup_apply_result_code code;
    enum classicsetup_apply_safety_code safety_code;
    enum classicsetup_environment environment;
    struct classicsetup_process_result process;
};

int classicsetup_build_apply_plan(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_plan *partition_plan,
    size_t original_partition_count,
    struct classicsetup_apply_plan *apply_plan);

int classicsetup_validate_apply_plan(
    const struct classicsetup_apply_plan *apply_plan);

int classicsetup_render_sfdisk_script(
    const struct classicsetup_apply_plan *apply_plan,
    char *script,
    size_t script_size);

int classicsetup_disk_identity_matches(
    const struct classicsetup_disk_info *selected,
    const struct classicsetup_disk_info *current);

int classicsetup_revalidate_target_disk(
    const struct classicsetup_disk_info *selected,
    struct classicsetup_disk_info *current);

enum classicsetup_apply_safety_code classicsetup_evaluate_apply_safety(
    const struct classicsetup_apply_safety_inputs *inputs);

int classicsetup_verify_partition_ranges(
    const struct classicsetup_apply_plan *apply_plan,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count);

int classicsetup_execute_apply_plan(
    const struct classicsetup_apply_plan *apply_plan,
    struct classicsetup_apply_result *result);

const char *classicsetup_apply_safety_message(
    enum classicsetup_apply_safety_code code);

#endif
