#ifndef CLASSICSETUP_FORMAT_APPLY_H
#define CLASSICSETUP_FORMAT_APPLY_H

#include <stddef.h>

#include "classicsetup/apply.h"
#include "classicsetup/environment.h"
#include "classicsetup/format.h"
#include "classicsetup/partition.h"
#include "classicsetup/process.h"
#include "classicsetup/system_disk.h"

enum {
    CLASSICSETUP_FORMAT_APPLY_MAX_PARTITIONS = 4,
    CLASSICSETUP_FORMAT_LABEL_SIZE = 16,
    CLASSICSETUP_FORMAT_TOOL_PATH_SIZE = 128,
    CLASSICSETUP_FORMAT_ARGUMENT_CAPACITY = 8
};

struct classicsetup_format_partition {
    enum classicsetup_partition_role role;
    char device_path[CLASSICSETUP_PARTITION_PATH_SIZE];
    unsigned long long start_sector;
    unsigned long long sector_count;
    enum classicsetup_filesystem_type filesystem;
    enum classicsetup_format_mode mode;
    char label[CLASSICSETUP_FORMAT_LABEL_SIZE];
};

struct classicsetup_format_apply_plan {
    struct classicsetup_apply_plan partition_apply_plan;
    struct classicsetup_format_partition
        partitions[CLASSICSETUP_FORMAT_APPLY_MAX_PARTITIONS];
    size_t partition_count;
    struct classicsetup_format_partition msr_partition;
    int has_msr_partition;
};

struct classicsetup_format_tools {
    char fat[CLASSICSETUP_FORMAT_TOOL_PATH_SIZE];
    char ntfs[CLASSICSETUP_FORMAT_TOOL_PATH_SIZE];
    char blkid[CLASSICSETUP_FORMAT_TOOL_PATH_SIZE];
};

enum classicsetup_format_mount_status {
    CLASSICSETUP_FORMAT_NOT_MOUNTED,
    CLASSICSETUP_FORMAT_MOUNTED,
    CLASSICSETUP_FORMAT_MOUNT_UNKNOWN
};

enum classicsetup_format_safety_code {
    CLASSICSETUP_FORMAT_SAFETY_OK,
    CLASSICSETUP_FORMAT_SAFETY_WSL,
    CLASSICSETUP_FORMAT_SAFETY_NOT_SUPPORTED_VM,
    CLASSICSETUP_FORMAT_SAFETY_LOCKED,
    CLASSICSETUP_FORMAT_SAFETY_MBR_NOT_ENABLED,
    CLASSICSETUP_FORMAT_SAFETY_DISK_IDENTITY,
    CLASSICSETUP_FORMAT_SAFETY_SYSTEM_DISK,
    CLASSICSETUP_FORMAT_SAFETY_SYSTEM_DISK_UNKNOWN,
    CLASSICSETUP_FORMAT_SAFETY_UNSUPPORTED_SECTOR_SIZE,
    CLASSICSETUP_FORMAT_SAFETY_INVALID_PLAN,
    CLASSICSETUP_FORMAT_SAFETY_PARTITION_LAYOUT,
    CLASSICSETUP_FORMAT_SAFETY_PARTITION_DEVICE,
    CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNTED,
    CLASSICSETUP_FORMAT_SAFETY_PARTITION_MOUNT_UNKNOWN,
    CLASSICSETUP_FORMAT_SAFETY_TOOL_UNAVAILABLE
};

struct classicsetup_format_safety_inputs {
    enum classicsetup_environment environment;
    enum classicsetup_partition_table_type table_type;
    int destructive_unlocked;
    int disk_identity_valid;
    enum classicsetup_system_disk_status system_disk_status;
    int logical_sector_size_supported;
    int format_plan_valid;
    int partition_layout_valid;
    int partition_device_valid;
    enum classicsetup_format_mount_status mount_status;
    int formatter_available;
    int verifier_available;
};

enum classicsetup_format_result_code {
    CLASSICSETUP_FORMAT_RESULT_NOT_RUN,
    CLASSICSETUP_FORMAT_RESULT_BLOCKED,
    CLASSICSETUP_FORMAT_RESULT_PROCESS_FAILED,
    CLASSICSETUP_FORMAT_RESULT_VERIFY_FAILED,
    CLASSICSETUP_FORMAT_RESULT_SUCCESS
};

struct classicsetup_format_result {
    enum classicsetup_format_result_code code;
    enum classicsetup_format_safety_code safety_code;
    enum classicsetup_environment environment;
    enum classicsetup_partition_role failed_role;
    size_t completed_count;
    struct classicsetup_process_result process;
    struct classicsetup_process_result verification_process;
};

struct classicsetup_format_executor_ops {
    int (*collect_safety)(
        const struct classicsetup_format_apply_plan *plan,
        const struct classicsetup_format_partition *partition,
        struct classicsetup_format_safety_inputs *inputs,
        void *context);
    int (*run_formatter)(
        const struct classicsetup_format_partition *partition,
        struct classicsetup_process_result *result,
        void *context);
    int (*verify_filesystem)(
        const struct classicsetup_format_partition *partition,
        struct classicsetup_process_result *result,
        void *context);
    void *context;
};

int classicsetup_build_format_apply_plan(
    const struct classicsetup_apply_plan *partition_apply_plan,
    const struct classicsetup_format_plan
        role_format_plans[CLASSICSETUP_PARTITION_ROLE_COUNT],
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_format_apply_plan *format_apply_plan);

int classicsetup_validate_format_apply_plan(
    const struct classicsetup_format_apply_plan *plan);

int classicsetup_match_partition_device(
    const struct classicsetup_apply_partition *expected,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_partition_info *matched);

int classicsetup_resolve_format_tools(
    struct classicsetup_format_tools *tools);

int classicsetup_build_format_arguments(
    const struct classicsetup_format_partition *partition,
    const struct classicsetup_format_tools *tools,
    char *arguments[],
    size_t capacity,
    const char **executable);

int classicsetup_build_blkid_arguments(
    const struct classicsetup_format_partition *partition,
    const struct classicsetup_format_tools *tools,
    char *arguments[],
    size_t capacity,
    const char **executable);

int classicsetup_filesystem_type_matches(
    enum classicsetup_filesystem_type expected,
    const char *detected);

enum classicsetup_format_mount_status
classicsetup_check_device_mounted_from(
    unsigned int device_major,
    unsigned int device_minor,
    const char *mountinfo_path);

enum classicsetup_format_mount_status classicsetup_check_device_mounted(
    const char *device_path);

enum classicsetup_format_safety_code classicsetup_evaluate_format_safety(
    const struct classicsetup_format_safety_inputs *inputs);

int classicsetup_execute_format_apply_plan_with_ops(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_executor_ops *ops,
    struct classicsetup_format_result *result);

int classicsetup_execute_format_apply_plan(
    const struct classicsetup_format_apply_plan *plan,
    struct classicsetup_format_result *result);

const char *classicsetup_format_safety_message(
    enum classicsetup_format_safety_code code);

#endif
