#ifndef CLASSICSETUP_PARTITION_SELECTION_H
#define CLASSICSETUP_PARTITION_SELECTION_H

#include <stdbool.h>
#include <stddef.h>

#include "classicsetup/disk.h"
#include "classicsetup/install_mode.h"
#include "classicsetup/partition_plan.h"

enum classicsetup_partition_selection_result {
    CLASSICSETUP_PARTITION_SELECTION_CONTINUE,
    CLASSICSETUP_PARTITION_SELECTION_BACK,
    CLASSICSETUP_PARTITION_SELECTION_UNDO_WINDOWS_LAYOUT,
    CLASSICSETUP_PARTITION_SELECTION_QUIT
};

enum classicsetup_partition_selection_result
classicsetup_show_partition_selection(
    const struct classicsetup_disk_info *disk,
    struct classicsetup_partition_plan *plan,
    enum classicsetup_install_mode install_mode,
    bool scan_failed,
    size_t *selected_item);

#endif
