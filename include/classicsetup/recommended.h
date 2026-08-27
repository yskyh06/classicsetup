#ifndef CLASSICSETUP_RECOMMENDED_H
#define CLASSICSETUP_RECOMMENDED_H

#include <stddef.h>

#include "classicsetup/apply.h"
#include "classicsetup/format.h"
#include "classicsetup/format_apply.h"
#include "classicsetup/system_disk.h"

enum classicsetup_firmware_mode {
    CLASSICSETUP_FIRMWARE_UNKNOWN,
    CLASSICSETUP_FIRMWARE_UEFI,
    CLASSICSETUP_FIRMWARE_BIOS
};

enum classicsetup_disk_class {
    CLASSICSETUP_DISK_EMPTY,
    CLASSICSETUP_DISK_HAS_UNALLOCATED_SPACE,
    CLASSICSETUP_DISK_HAS_EXISTING_PARTITIONS,
    CLASSICSETUP_DISK_SYSTEM,
    CLASSICSETUP_DISK_INSTALL_MEDIA,
    CLASSICSETUP_DISK_REMOVABLE,
    CLASSICSETUP_DISK_UNKNOWN
};

struct classicsetup_disk_assessment {
    struct classicsetup_disk_info disk;
    enum classicsetup_disk_class disk_class;
    size_t partition_count;
    int selectable;
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

int classicsetup_disk_class_is_recommended_selectable(
    enum classicsetup_disk_class disk_class);

int classicsetup_recommended_result_can_continue(
    enum classicsetup_recommended_result_code result_code);

int classicsetup_assess_disk(
    const struct classicsetup_disk_info *disk,
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
