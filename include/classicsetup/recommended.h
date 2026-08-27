#ifndef CLASSICSETUP_RECOMMENDED_H
#define CLASSICSETUP_RECOMMENDED_H

#include <stddef.h>

#include "classicsetup/apply.h"
#include "classicsetup/format.h"
#include "classicsetup/format_apply.h"
#include "classicsetup/environment.h"
#include "classicsetup/system_disk.h"

enum classicsetup_firmware_mode {
    CLASSICSETUP_FIRMWARE_UNKNOWN,
    CLASSICSETUP_FIRMWARE_UEFI,
    CLASSICSETUP_FIRMWARE_BIOS
};

enum classicsetup_disk_class {
    CLASSICSETUP_DISK_RAW_EMPTY,
    CLASSICSETUP_DISK_PARTITIONED_EMPTY,
    CLASSICSETUP_DISK_WINDOWS,
    CLASSICSETUP_DISK_WINDOWS_ENCRYPTED_LOCKED,
    CLASSICSETUP_DISK_WINDOWS_ENCRYPTED_UNLOCKED,
    CLASSICSETUP_DISK_WINDOWS_COMPLEX,
    CLASSICSETUP_DISK_DATA_PRESENT,
    CLASSICSETUP_DISK_MULTI_OS,
    CLASSICSETUP_DISK_UNKNOWN_FILESYSTEM,
    CLASSICSETUP_DISK_SYSTEM,
    CLASSICSETUP_DISK_INSTALL_MEDIA,
    CLASSICSETUP_DISK_REMOVABLE,
    CLASSICSETUP_DISK_UNKNOWN
};

enum classicsetup_recommended_disk_action {
    CLASSICSETUP_RECOMMENDED_AUTO_INSTALL_ALLOWED,
    CLASSICSETUP_RECOMMENDED_REINITIALIZE_WITH_WARNING,
    CLASSICSETUP_RECOMMENDED_KEEP_FILES_FUTURE,
    CLASSICSETUP_RECOMMENDED_EXPLICIT_ERASE_ONLY,
    CLASSICSETUP_RECOMMENDED_ADVANCED_ONLY,
    CLASSICSETUP_RECOMMENDED_BLOCK
};

struct classicsetup_disk_facts {
    int partition_scan_succeeded;
    int partition_table_present;
    size_t partition_count;
    int has_usable_unallocated;
    int filesystems_inspected;
    int all_partitions_confirmed_empty;
    int windows_detected;
    int encryption_detected;
    int encryption_unlocked;
    int user_data_detected;
    int multiple_operating_systems;
    int complex_storage;
    int unknown_filesystem;
    enum classicsetup_system_disk_status system_disk_status;
};

struct classicsetup_disk_assessment {
    struct classicsetup_disk_info disk;
    enum classicsetup_disk_class disk_class;
    enum classicsetup_recommended_disk_action action;
    size_t partition_count;
    int selectable;
    char presentation[192];
};

struct classicsetup_recommended_plan {
    enum classicsetup_install_mode install_mode;
    struct classicsetup_partition_plan partition_plan;
    struct classicsetup_plan_item selected_target;
    struct classicsetup_format_plan selected_format_plan;
    struct classicsetup_format_plan
        role_format_plans[CLASSICSETUP_PARTITION_ROLE_COUNT];
    struct classicsetup_apply_plan apply_plan;
};

enum classicsetup_recommended_result_code {
    CLASSICSETUP_RECOMMENDED_NOT_RUN,
    CLASSICSETUP_RECOMMENDED_BLOCKED,
    CLASSICSETUP_RECOMMENDED_PARTITION_FAILED,
    CLASSICSETUP_RECOMMENDED_FORMAT_PLAN_FAILED,
    CLASSICSETUP_RECOMMENDED_FORMAT_FAILED,
    CLASSICSETUP_RECOMMENDED_SUCCESS
};

struct classicsetup_recommended_executor_ops {
    int (*revalidate_empty_disk)(
        const struct classicsetup_disk_info *disk,
        void *context);
    int (*execute_partition)(
        const struct classicsetup_apply_plan *plan,
        struct classicsetup_apply_result *result,
        void *context);
    int (*build_format_plan)(
        const struct classicsetup_recommended_plan *plan,
        struct classicsetup_format_apply_plan *format_plan,
        void *context);
    int (*execute_format)(
        const struct classicsetup_format_apply_plan *plan,
        struct classicsetup_format_result *result,
        void *context);
    void *context;
};

enum classicsetup_firmware_mode classicsetup_detect_firmware(void);

enum classicsetup_firmware_mode classicsetup_detect_firmware_from(
    const char *sys_firmware_path);

enum classicsetup_disk_class classicsetup_classify_disk(
    const struct classicsetup_disk_info *disk,
    int partition_scan_succeeded,
    size_t partition_count,
    int has_usable_unallocated,
    enum classicsetup_system_disk_status system_disk_status);

enum classicsetup_disk_class classicsetup_classify_disk_facts(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_disk_facts *facts,
    enum classicsetup_environment environment);

enum classicsetup_recommended_disk_action
classicsetup_recommended_policy_for_disk(
    enum classicsetup_disk_class disk_class);

const char *classicsetup_disk_class_presentation(
    enum classicsetup_disk_class disk_class);

const char *classicsetup_recommended_policy_reason(
    enum classicsetup_disk_class disk_class,
    enum classicsetup_firmware_mode firmware);

int classicsetup_disk_class_is_recommended_selectable(
    enum classicsetup_disk_class disk_class);

int classicsetup_recommended_assessment_is_selectable(
    const struct classicsetup_disk_assessment *assessment,
    enum classicsetup_firmware_mode firmware);

int classicsetup_recommended_result_can_continue(
    enum classicsetup_recommended_result_code result_code);

int classicsetup_assess_disk(
    const struct classicsetup_disk_info *disk,
    struct classicsetup_disk_assessment *assessment);

int classicsetup_assess_disk_in_environment(
    const struct classicsetup_disk_info *disk,
    enum classicsetup_environment environment,
    struct classicsetup_disk_assessment *assessment);

int classicsetup_build_recommended_plan(
    enum classicsetup_firmware_mode firmware,
    const struct classicsetup_disk_info *disk,
    enum classicsetup_disk_class disk_class,
    struct classicsetup_recommended_plan *plan);

enum classicsetup_recommended_result_code
classicsetup_execute_recommended_plan_with_ops(
    const struct classicsetup_recommended_plan *plan,
    const struct classicsetup_recommended_executor_ops *ops,
    struct classicsetup_apply_result *partition_result,
    struct classicsetup_format_apply_plan *format_plan,
    struct classicsetup_format_result *format_result);

enum classicsetup_recommended_result_code
classicsetup_execute_recommended_plan(
    const struct classicsetup_recommended_plan *plan,
    struct classicsetup_apply_result *partition_result,
    struct classicsetup_format_apply_plan *format_plan,
    struct classicsetup_format_result *format_result);

#endif
