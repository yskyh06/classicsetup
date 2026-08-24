#ifndef CLASSICSETUP_FORMAT_H
#define CLASSICSETUP_FORMAT_H

#include <stdbool.h>

#include "classicsetup/partition_plan.h"

enum classicsetup_filesystem_type {
    CLASSICSETUP_FS_NONE,
    CLASSICSETUP_FS_FAT32,
    CLASSICSETUP_FS_NTFS
};

enum classicsetup_format_mode {
    CLASSICSETUP_FORMAT_NONE,
    CLASSICSETUP_FORMAT_QUICK,
    CLASSICSETUP_FORMAT_FULL
};

struct classicsetup_format_plan {
    bool valid;
    enum classicsetup_filesystem_type filesystem;
    enum classicsetup_format_mode mode;
};

enum classicsetup_format_mode classicsetup_default_format_mode(void);

enum classicsetup_format_mode classicsetup_format_move_option(
    enum classicsetup_format_mode current,
    int direction);

int classicsetup_format_policy_for_role(
    enum classicsetup_partition_role role,
    enum classicsetup_format_mode windows_mode,
    struct classicsetup_format_plan *format_plan);

#endif
