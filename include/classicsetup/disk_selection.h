#ifndef CLASSICSETUP_DISK_SELECTION_H
#define CLASSICSETUP_DISK_SELECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "classicsetup/disk.h"

enum classicsetup_disk_selection_result {
    CLASSICSETUP_DISK_SELECTION_CONTINUE,
    CLASSICSETUP_DISK_SELECTION_BACK,
    CLASSICSETUP_DISK_SELECTION_QUIT
};

enum classicsetup_disk_selection_result classicsetup_show_disk_selection(
    const struct classicsetup_disk_info *disks,
    size_t disk_count,
    bool scan_failed,
    size_t *selected_index);

#endif
