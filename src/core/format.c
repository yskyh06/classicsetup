#include "classicsetup/format.h"

enum classicsetup_format_mode classicsetup_default_format_mode(void)
{
    return CLASSICSETUP_FORMAT_QUICK;
}

enum classicsetup_format_mode classicsetup_format_move_option(
    enum classicsetup_format_mode current,
    int direction)
{
    if (direction < 0) {
        return CLASSICSETUP_FORMAT_QUICK;
    }
    if (direction > 0) {
        return CLASSICSETUP_FORMAT_FULL;
    }
    if (current == CLASSICSETUP_FORMAT_FULL) {
        return CLASSICSETUP_FORMAT_FULL;
    }
    return CLASSICSETUP_FORMAT_QUICK;
}

int classicsetup_format_policy_for_role(
    enum classicsetup_partition_role role,
    enum classicsetup_format_mode windows_mode,
    struct classicsetup_format_plan *format_plan)
{
    if (format_plan == NULL) {
        return -1;
    }

    format_plan->valid = false;
    format_plan->filesystem = CLASSICSETUP_FS_NONE;
    format_plan->mode = CLASSICSETUP_FORMAT_NONE;
    if (windows_mode != CLASSICSETUP_FORMAT_QUICK &&
        windows_mode != CLASSICSETUP_FORMAT_FULL) {
        return -1;
    }

    format_plan->valid = true;
    switch (role) {
    case CLASSICSETUP_PARTITION_ROLE_GENERIC:
    case CLASSICSETUP_PARTITION_ROLE_WINDOWS:
        format_plan->filesystem = CLASSICSETUP_FS_NTFS;
        format_plan->mode = windows_mode;
        return 0;
    case CLASSICSETUP_PARTITION_ROLE_EFI:
        format_plan->filesystem = CLASSICSETUP_FS_FAT32;
        format_plan->mode = CLASSICSETUP_FORMAT_QUICK;
        return 0;
    case CLASSICSETUP_PARTITION_ROLE_MSR:
        format_plan->filesystem = CLASSICSETUP_FS_NONE;
        format_plan->mode = CLASSICSETUP_FORMAT_NONE;
        return 0;
    case CLASSICSETUP_PARTITION_ROLE_RECOVERY:
        format_plan->filesystem = CLASSICSETUP_FS_NTFS;
        format_plan->mode = CLASSICSETUP_FORMAT_QUICK;
        return 0;
    case CLASSICSETUP_PARTITION_ROLE_NONE:
    case CLASSICSETUP_PARTITION_ROLE_COUNT:
        break;
    }

    format_plan->valid = false;
    format_plan->filesystem = CLASSICSETUP_FS_NONE;
    format_plan->mode = CLASSICSETUP_FORMAT_NONE;
    return -1;
}
