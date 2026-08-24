#include "classicsetup/partition_plan.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int state_order(enum classicsetup_plan_item_state state)
{
    switch (state) {
    case CLASSICSETUP_PLAN_DELETED:
        return 0;
    case CLASSICSETUP_PLAN_EXISTING:
        return 1;
    case CLASSICSETUP_PLAN_NEW:
        return 2;
    case CLASSICSETUP_PLAN_UNALLOCATED:
        return 3;
    }

    return 4;
}

static int compare_plan_items(const void *left, const void *right)
{
    const struct classicsetup_plan_item *left_item = left;
    const struct classicsetup_plan_item *right_item = right;
    int left_order;
    int right_order;

    if (left_item->start_sector < right_item->start_sector) {
        return -1;
    }
    if (left_item->start_sector > right_item->start_sector) {
        return 1;
    }

    left_order = state_order(left_item->state);
    right_order = state_order(right_item->state);
    if (left_order < right_order) {
        return -1;
    }
    if (left_order > right_order) {
        return 1;
    }
    return 0;
}

static int compare_spaces(const void *left, const void *right)
{
    const struct classicsetup_unallocated_info *left_space = left;
    const struct classicsetup_unallocated_info *right_space = right;

    if (left_space->start_sector < right_space->start_sector) {
        return -1;
    }
    if (left_space->start_sector > right_space->start_sector) {
        return 1;
    }
    return 0;
}

static int append_unallocated(
    struct classicsetup_partition_plan *plan,
    unsigned long long start_sector,
    unsigned long long sector_count)
{
    struct classicsetup_plan_item *item;

    if (sector_count == 0) {
        return 0;
    }
    if (plan->item_count >= CLASSICSETUP_PLAN_MAX_ITEMS ||
        sector_count > ULLONG_MAX / CLASSICSETUP_SECTOR_SIZE_BYTES) {
        return -1;
    }

    item = &plan->items[plan->item_count++];
    memset(item, 0, sizeof(*item));
    item->state = CLASSICSETUP_PLAN_UNALLOCATED;
    item->role = CLASSICSETUP_PARTITION_ROLE_NONE;
    item->start_sector = start_sector;
    item->sector_count = sector_count;
    item->size_bytes = sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;
    return 0;
}

static void remove_item(
    struct classicsetup_partition_plan *plan,
    size_t item_index)
{
    if (item_index + 1 < plan->item_count) {
        memmove(
            &plan->items[item_index],
            &plan->items[item_index + 1],
            (plan->item_count - item_index - 1) * sizeof(plan->items[0]));
    }
    --plan->item_count;
}

int classicsetup_plan_merge_unallocated(
    struct classicsetup_partition_plan *plan)
{
    struct classicsetup_unallocated_info spaces[CLASSICSETUP_PLAN_MAX_ITEMS];
    size_t space_count = 0;
    size_t write_index = 0;
    size_t index;

    if (plan == NULL) {
        return -1;
    }

    for (index = 0; index < plan->item_count; ++index) {
        if (plan->items[index].state == CLASSICSETUP_PLAN_UNALLOCATED) {
            spaces[space_count].start_sector = plan->items[index].start_sector;
            spaces[space_count].sector_count = plan->items[index].sector_count;
            spaces[space_count].size_bytes = plan->items[index].size_bytes;
            ++space_count;
        } else {
            plan->items[write_index++] = plan->items[index];
        }
    }
    plan->item_count = write_index;

    if (space_count > 1) {
        qsort(spaces, space_count, sizeof(spaces[0]), compare_spaces);
    }

    for (index = 0; index < space_count; ++index) {
        unsigned long long start = spaces[index].start_sector;
        unsigned long long count = spaces[index].sector_count;

        if (index + 1 < space_count && count <= ULLONG_MAX - start) {
            unsigned long long end = start + count;

            while (index + 1 < space_count &&
                   spaces[index + 1].start_sector <= end) {
                unsigned long long next_end;

                if (spaces[index + 1].sector_count >
                    ULLONG_MAX - spaces[index + 1].start_sector) {
                    next_end = ULLONG_MAX;
                } else {
                    next_end = spaces[index + 1].start_sector +
                               spaces[index + 1].sector_count;
                }
                if (next_end > end) {
                    end = next_end;
                }
                ++index;
            }
            count = end - start;
        }

        if (append_unallocated(plan, start, count) != 0) {
            return -1;
        }
    }

    if (plan->item_count > 1) {
        qsort(
            plan->items,
            plan->item_count,
            sizeof(plan->items[0]),
            compare_plan_items);
    }
    return 0;
}

int classicsetup_plan_rebuild_unallocated(
    struct classicsetup_partition_plan *plan)
{
    unsigned long long cursor = 0;
    size_t write_index = 0;
    size_t occupied_count;
    size_t index;

    if (plan == NULL) {
        return -1;
    }

    for (index = 0; index < plan->item_count; ++index) {
        if (plan->items[index].state != CLASSICSETUP_PLAN_UNALLOCATED) {
            plan->items[write_index++] = plan->items[index];
        }
    }
    plan->item_count = write_index;

    if (plan->item_count > 1) {
        qsort(
            plan->items,
            plan->item_count,
            sizeof(plan->items[0]),
            compare_plan_items);
    }
    occupied_count = plan->item_count;

    for (index = 0; index < occupied_count; ++index) {
        const struct classicsetup_plan_item *item = &plan->items[index];
        unsigned long long start;
        unsigned long long end;

        if (item->state == CLASSICSETUP_PLAN_DELETED) {
            continue;
        }

        start = item->start_sector;
        if (start > plan->disk_sector_count) {
            start = plan->disk_sector_count;
        }
        if (start > cursor && append_unallocated(plan, cursor, start - cursor) != 0) {
            return -1;
        }

        if (item->sector_count > ULLONG_MAX - item->start_sector) {
            end = plan->disk_sector_count;
        } else {
            end = item->start_sector + item->sector_count;
            if (end > plan->disk_sector_count) {
                end = plan->disk_sector_count;
            }
        }
        if (end > cursor) {
            cursor = end;
        }
    }

    if (cursor < plan->disk_sector_count &&
        append_unallocated(
            plan,
            cursor,
            plan->disk_sector_count - cursor) != 0) {
        return -1;
    }
    return classicsetup_plan_merge_unallocated(plan);
}

int classicsetup_plan_init(
    const struct classicsetup_disk_info *disk,
    const struct classicsetup_partition_info *partitions,
    size_t partition_count,
    struct classicsetup_partition_plan *plan)
{
    size_t index;

    if (disk == NULL || plan == NULL ||
        (partition_count > 0 && partitions == NULL) ||
        partition_count > CLASSICSETUP_PLAN_MAX_ITEMS) {
        return -1;
    }

    memset(plan, 0, sizeof(*plan));
    plan->disk_sector_count =
        disk->size_bytes / CLASSICSETUP_SECTOR_SIZE_BYTES;

    for (index = 0; index < partition_count; ++index) {
        struct classicsetup_plan_item *item = &plan->items[plan->item_count++];

        item->state = CLASSICSETUP_PLAN_EXISTING;
        item->role = CLASSICSETUP_PARTITION_ROLE_GENERIC;
        snprintf(item->name, sizeof(item->name), "%s", partitions[index].name);
        snprintf(
            item->device_path,
            sizeof(item->device_path),
            "%s",
            partitions[index].device_path);
        item->number = partitions[index].number;
        item->start_sector = partitions[index].start_sector;
        item->sector_count = partitions[index].sector_count;
        item->size_bytes = partitions[index].size_bytes;
    }

    if (classicsetup_plan_rebuild_unallocated(plan) != 0 ||
        !classicsetup_plan_validate(plan)) {
        return -1;
    }
    return 0;
}

int classicsetup_plan_size_mb_to_sectors(
    unsigned long long size_mb,
    unsigned long long available_sectors,
    unsigned long long *sector_count)
{
    if (sector_count == NULL || size_mb == 0 ||
        size_mb > ULLONG_MAX / CLASSICSETUP_SECTORS_PER_MB) {
        return -1;
    }

    *sector_count = size_mb * CLASSICSETUP_SECTORS_PER_MB;
    if (*sector_count > available_sectors) {
        return -1;
    }
    return 0;
}

unsigned long long classicsetup_plan_max_size_mb(
    unsigned long long available_sectors)
{
    return available_sectors / CLASSICSETUP_SECTORS_PER_MB;
}

int classicsetup_plan_align_sector(
    unsigned long long sector,
    unsigned long long alignment,
    unsigned long long *aligned_sector)
{
    unsigned long long remainder;
    unsigned long long adjustment;

    if (alignment == 0 || aligned_sector == NULL) {
        return -1;
    }

    remainder = sector % alignment;
    if (remainder == 0) {
        *aligned_sector = sector;
        return 0;
    }

    adjustment = alignment - remainder;
    if (sector > ULLONG_MAX - adjustment) {
        return -1;
    }
    *aligned_sector = sector + adjustment;
    return 0;
}

int classicsetup_plan_create_partition(
    struct classicsetup_partition_plan *plan,
    size_t unallocated_index,
    unsigned long long size_mb,
    size_t *created_index)
{
    struct classicsetup_partition_plan temporary;
    struct classicsetup_plan_item new_item = {0};
    unsigned long long sector_count;
    unsigned long long start_sector;
    size_t index;

    if (plan == NULL || created_index == NULL ||
        !classicsetup_plan_validate(plan) ||
        unallocated_index >= plan->item_count ||
        plan->items[unallocated_index].state !=
            CLASSICSETUP_PLAN_UNALLOCATED ||
        classicsetup_plan_size_mb_to_sectors(
            size_mb,
            plan->items[unallocated_index].sector_count,
            &sector_count) != 0 ||
        plan->item_count >= CLASSICSETUP_PLAN_MAX_ITEMS) {
        return -1;
    }

    temporary = *plan;
    start_sector = temporary.items[unallocated_index].start_sector;
    new_item.state = CLASSICSETUP_PLAN_NEW;
    new_item.role = CLASSICSETUP_PARTITION_ROLE_GENERIC;
    new_item.start_sector = start_sector;
    new_item.sector_count = sector_count;
    new_item.size_bytes = sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;
    temporary.items[temporary.item_count++] = new_item;

    if (classicsetup_plan_rebuild_unallocated(&temporary) != 0 ||
        !classicsetup_plan_validate(&temporary)) {
        return -1;
    }

    for (index = 0; index < temporary.item_count; ++index) {
        if (temporary.items[index].state == CLASSICSETUP_PLAN_NEW &&
            temporary.items[index].start_sector == start_sector &&
            temporary.items[index].sector_count == sector_count) {
            *plan = temporary;
            *created_index = index;
            return 0;
        }
    }
    return -1;
}

static int append_new_partition(
    struct classicsetup_partition_plan *plan,
    enum classicsetup_partition_role role,
    const char *name,
    unsigned long long start_sector,
    unsigned long long sector_count)
{
    struct classicsetup_plan_item *item;

    if (plan->item_count >= CLASSICSETUP_PLAN_MAX_ITEMS ||
        sector_count == 0 ||
        sector_count > ULLONG_MAX / CLASSICSETUP_SECTOR_SIZE_BYTES) {
        return -1;
    }

    item = &plan->items[plan->item_count++];
    memset(item, 0, sizeof(*item));
    item->state = CLASSICSETUP_PLAN_NEW;
    item->role = role;
    snprintf(item->name, sizeof(item->name), "%s", name);
    item->start_sector = start_sector;
    item->sector_count = sector_count;
    item->size_bytes = sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES;
    return 0;
}

static int windows_layout_sizes(
    unsigned long long available_sectors,
    unsigned long long *efi_sectors,
    unsigned long long *msr_sectors,
    unsigned long long *windows_sectors,
    unsigned long long *recovery_sectors)
{
    unsigned long long fixed_sectors;
    unsigned long long minimum_windows_sectors;
    unsigned long long remaining;

    *efi_sectors =
        CLASSICSETUP_DEFAULT_EFI_MB * CLASSICSETUP_SECTORS_PER_MB;
    *msr_sectors =
        CLASSICSETUP_DEFAULT_MSR_MB * CLASSICSETUP_SECTORS_PER_MB;
    *recovery_sectors =
        CLASSICSETUP_DEFAULT_RECOVERY_MB * CLASSICSETUP_SECTORS_PER_MB;
    minimum_windows_sectors =
        CLASSICSETUP_MIN_WINDOWS_MB * CLASSICSETUP_SECTORS_PER_MB;

    if (*efi_sectors > ULLONG_MAX - *msr_sectors ||
        *efi_sectors + *msr_sectors > ULLONG_MAX - *recovery_sectors) {
        return -1;
    }
    fixed_sectors = *efi_sectors + *msr_sectors + *recovery_sectors;
    if (fixed_sectors > available_sectors ||
        minimum_windows_sectors > available_sectors - fixed_sectors) {
        return -1;
    }

    remaining = available_sectors - fixed_sectors;
    *windows_sectors =
        (remaining / CLASSICSETUP_SECTORS_PER_MB) *
        CLASSICSETUP_SECTORS_PER_MB;
    if (*windows_sectors < minimum_windows_sectors) {
        return -1;
    }
    return 0;
}

int classicsetup_plan_create_windows_layout(
    struct classicsetup_partition_plan *plan,
    size_t unallocated_index,
    size_t *windows_index)
{
    struct classicsetup_partition_plan temporary;
    unsigned long long aligned_start;
    unsigned long long available_end;
    unsigned long long available_sectors;
    unsigned long long efi_sectors;
    unsigned long long msr_sectors;
    unsigned long long windows_sectors;
    unsigned long long recovery_sectors;
    unsigned long long start;
    size_t index;

    if (plan == NULL || windows_index == NULL ||
        !classicsetup_plan_validate(plan) ||
        unallocated_index >= plan->item_count ||
        plan->items[unallocated_index].state !=
            CLASSICSETUP_PLAN_UNALLOCATED ||
        plan->item_count > CLASSICSETUP_PLAN_MAX_ITEMS - 4 ||
        plan->items[unallocated_index].sector_count >
            ULLONG_MAX - plan->items[unallocated_index].start_sector) {
        return -1;
    }

    available_end = plan->items[unallocated_index].start_sector +
                    plan->items[unallocated_index].sector_count;
    if (available_end == plan->disk_sector_count) {
        if (available_end < CLASSICSETUP_SECTORS_PER_MB) {
            return -1;
        }
        available_end -= CLASSICSETUP_SECTORS_PER_MB;
    }
    aligned_start = plan->items[unallocated_index].start_sector;
    if (aligned_start < CLASSICSETUP_SECTORS_PER_MB) {
        aligned_start = CLASSICSETUP_SECTORS_PER_MB;
    }
    if (classicsetup_plan_align_sector(
            aligned_start,
            CLASSICSETUP_SECTORS_PER_MB,
            &aligned_start) != 0 ||
        aligned_start > available_end) {
        return -1;
    }
    available_sectors = available_end - aligned_start;
    if (windows_layout_sizes(
            available_sectors,
            &efi_sectors,
            &msr_sectors,
            &windows_sectors,
            &recovery_sectors) != 0) {
        return -1;
    }

    temporary = *plan;
    start = aligned_start;
    if (append_new_partition(
            &temporary,
            CLASSICSETUP_PARTITION_ROLE_EFI,
            "EFI System Partition",
            start,
            efi_sectors) != 0) {
        return -1;
    }
    start += efi_sectors;
    if (append_new_partition(
            &temporary,
            CLASSICSETUP_PARTITION_ROLE_MSR,
            "Microsoft Reserved Partition",
            start,
            msr_sectors) != 0) {
        return -1;
    }
    start += msr_sectors;
    if (append_new_partition(
            &temporary,
            CLASSICSETUP_PARTITION_ROLE_WINDOWS,
            "Windows Partition",
            start,
            windows_sectors) != 0) {
        return -1;
    }
    start += windows_sectors;
    if (append_new_partition(
            &temporary,
            CLASSICSETUP_PARTITION_ROLE_RECOVERY,
            "Recovery Partition",
            start,
            recovery_sectors) != 0 ||
        classicsetup_plan_rebuild_unallocated(&temporary) != 0 ||
        !classicsetup_plan_validate(&temporary)) {
        return -1;
    }

    for (index = 0; index < temporary.item_count; ++index) {
        if (temporary.items[index].state == CLASSICSETUP_PLAN_NEW &&
            temporary.items[index].role ==
                CLASSICSETUP_PARTITION_ROLE_WINDOWS &&
            temporary.items[index].start_sector == aligned_start +
                efi_sectors + msr_sectors) {
            *plan = temporary;
            *windows_index = index;
            return 0;
        }
    }
    return -1;
}

static int is_windows_layout_role(enum classicsetup_partition_role role)
{
    return role == CLASSICSETUP_PARTITION_ROLE_EFI ||
           role == CLASSICSETUP_PARTITION_ROLE_MSR ||
           role == CLASSICSETUP_PARTITION_ROLE_WINDOWS ||
           role == CLASSICSETUP_PARTITION_ROLE_RECOVERY;
}

int classicsetup_plan_has_windows_layout(
    const struct classicsetup_partition_plan *plan)
{
    size_t role_counts[CLASSICSETUP_PARTITION_ROLE_COUNT] = {0};
    const struct classicsetup_plan_item
        *role_items[CLASSICSETUP_PARTITION_ROLE_COUNT] = {0};
    const struct classicsetup_plan_item *efi;
    const struct classicsetup_plan_item *msr;
    const struct classicsetup_plan_item *windows;
    const struct classicsetup_plan_item *recovery;
    size_t index;

    if (!classicsetup_plan_validate(plan)) {
        return 0;
    }

    for (index = 0; index < plan->item_count; ++index) {
        const struct classicsetup_plan_item *item = &plan->items[index];

        if (item->state == CLASSICSETUP_PLAN_NEW &&
            is_windows_layout_role(item->role)) {
            ++role_counts[item->role];
            role_items[item->role] = item;
        }
    }

    if (role_counts[CLASSICSETUP_PARTITION_ROLE_EFI] != 1 ||
        role_counts[CLASSICSETUP_PARTITION_ROLE_MSR] != 1 ||
        role_counts[CLASSICSETUP_PARTITION_ROLE_WINDOWS] != 1 ||
        role_counts[CLASSICSETUP_PARTITION_ROLE_RECOVERY] != 1) {
        return 0;
    }

    efi = role_items[CLASSICSETUP_PARTITION_ROLE_EFI];
    msr = role_items[CLASSICSETUP_PARTITION_ROLE_MSR];
    windows = role_items[CLASSICSETUP_PARTITION_ROLE_WINDOWS];
    recovery = role_items[CLASSICSETUP_PARTITION_ROLE_RECOVERY];
    return efi->start_sector + efi->sector_count == msr->start_sector &&
           msr->start_sector + msr->sector_count == windows->start_sector &&
           windows->start_sector + windows->sector_count ==
               recovery->start_sector;
}

int classicsetup_plan_undo_windows_layout(
    struct classicsetup_partition_plan *plan,
    size_t *restored_unallocated_index)
{
    struct classicsetup_partition_plan temporary;
    unsigned long long layout_start = ULLONG_MAX;
    size_t write_index = 0;
    size_t index;

    if (plan == NULL || restored_unallocated_index == NULL ||
        !classicsetup_plan_has_windows_layout(plan)) {
        return -1;
    }

    temporary = *plan;
    for (index = 0; index < temporary.item_count; ++index) {
        const struct classicsetup_plan_item *item = &temporary.items[index];

        if (item->state == CLASSICSETUP_PLAN_NEW &&
            is_windows_layout_role(item->role)) {
            if (item->start_sector < layout_start) {
                layout_start = item->start_sector;
            }
            continue;
        }
        temporary.items[write_index++] = *item;
    }
    temporary.item_count = write_index;

    if (layout_start == ULLONG_MAX ||
        classicsetup_plan_rebuild_unallocated(&temporary) != 0 ||
        !classicsetup_plan_validate(&temporary)) {
        return -1;
    }

    for (index = 0; index < temporary.item_count; ++index) {
        const struct classicsetup_plan_item *item = &temporary.items[index];

        if (item->state == CLASSICSETUP_PLAN_UNALLOCATED &&
            layout_start >= item->start_sector &&
            layout_start - item->start_sector < item->sector_count) {
            *plan = temporary;
            *restored_unallocated_index = index;
            return 0;
        }
    }
    return -1;
}

int classicsetup_plan_delete_partition(
    struct classicsetup_partition_plan *plan,
    size_t item_index)
{
    struct classicsetup_partition_plan temporary;

    if (plan == NULL || !classicsetup_plan_validate(plan) ||
        item_index >= plan->item_count) {
        return -1;
    }

    temporary = *plan;
    if (temporary.items[item_index].state == CLASSICSETUP_PLAN_EXISTING) {
        temporary.items[item_index].state = CLASSICSETUP_PLAN_DELETED;
    } else if (temporary.items[item_index].state == CLASSICSETUP_PLAN_NEW) {
        remove_item(&temporary, item_index);
    } else {
        return -1;
    }

    if (classicsetup_plan_rebuild_unallocated(&temporary) != 0 ||
        !classicsetup_plan_validate(&temporary)) {
        return -1;
    }
    *plan = temporary;
    return 0;
}

int classicsetup_plan_has_no_overlap(
    const struct classicsetup_partition_plan *plan)
{
    unsigned long long previous_end = 0;
    size_t index;

    if (plan == NULL) {
        return 0;
    }

    for (index = 0; index < plan->item_count; ++index) {
        const struct classicsetup_plan_item *item = &plan->items[index];
        unsigned long long end;

        if (item->state == CLASSICSETUP_PLAN_DELETED) {
            continue;
        }
        if (item->start_sector < previous_end ||
            item->sector_count > ULLONG_MAX - item->start_sector) {
            return 0;
        }

        end = item->start_sector + item->sector_count;
        if (end > plan->disk_sector_count) {
            return 0;
        }
        previous_end = end;
    }
    return 1;
}

int classicsetup_plan_item_is_install_target(
    const struct classicsetup_plan_item *item)
{
    if (item == NULL ||
        (item->state != CLASSICSETUP_PLAN_EXISTING &&
         item->state != CLASSICSETUP_PLAN_NEW)) {
        return 0;
    }

    return item->role == CLASSICSETUP_PARTITION_ROLE_GENERIC ||
           item->role == CLASSICSETUP_PARTITION_ROLE_WINDOWS;
}

int classicsetup_plan_validate(
    const struct classicsetup_partition_plan *plan)
{
    unsigned long long cursor = 0;
    size_t index;

    if (plan == NULL || plan->item_count > CLASSICSETUP_PLAN_MAX_ITEMS) {
        return 0;
    }

    for (index = 0; index < plan->item_count; ++index) {
        const struct classicsetup_plan_item *item = &plan->items[index];

        if (index > 0 && compare_plan_items(
                &plan->items[index - 1],
                item) > 0) {
            return 0;
        }
        if (item->sector_count == 0 ||
            item->sector_count > ULLONG_MAX / CLASSICSETUP_SECTOR_SIZE_BYTES ||
            item->size_bytes !=
                item->sector_count * CLASSICSETUP_SECTOR_SIZE_BYTES ||
            item->start_sector > plan->disk_sector_count ||
            item->sector_count >
                plan->disk_sector_count - item->start_sector) {
            return 0;
        }
        switch (item->state) {
        case CLASSICSETUP_PLAN_EXISTING:
        case CLASSICSETUP_PLAN_NEW:
        case CLASSICSETUP_PLAN_DELETED:
            if (item->role <= CLASSICSETUP_PARTITION_ROLE_NONE ||
                item->role >= CLASSICSETUP_PARTITION_ROLE_COUNT) {
                return 0;
            }
            break;
        case CLASSICSETUP_PLAN_UNALLOCATED:
            if (item->role != CLASSICSETUP_PARTITION_ROLE_NONE) {
                return 0;
            }
            break;
        default:
            return 0;
        }
    }

    for (index = 0; index < plan->item_count; ++index) {
        const struct classicsetup_plan_item *item = &plan->items[index];

        if (item->state == CLASSICSETUP_PLAN_DELETED) {
            continue;
        }
        if (item->start_sector != cursor) {
            return 0;
        }
        cursor = item->start_sector + item->sector_count;
    }
    return cursor == plan->disk_sector_count;
}

size_t classicsetup_plan_normalize_index(
    const struct classicsetup_partition_plan *plan,
    size_t selected_index)
{
    if (plan == NULL || plan->item_count == 0) {
        return 0;
    }
    if (selected_index >= plan->item_count) {
        return plan->item_count - 1;
    }
    return selected_index;
}

int classicsetup_plan_find_matching_item(
    const struct classicsetup_partition_plan *plan,
    const struct classicsetup_plan_item *target,
    size_t *item_index)
{
    size_t index;

    if (plan == NULL || target == NULL || item_index == NULL ||
        !classicsetup_plan_validate(plan)) {
        return -1;
    }

    for (index = 0; index < plan->item_count; ++index) {
        const struct classicsetup_plan_item *item = &plan->items[index];

        if (item->state == target->state && item->role == target->role &&
            item->start_sector == target->start_sector &&
            item->sector_count == target->sector_count) {
            *item_index = index;
            return 0;
        }
    }
    return -1;
}

int classicsetup_plan_prepare_install_target(
    struct classicsetup_partition_plan *plan,
    size_t selected_index,
    size_t *target_index)
{
    if (plan == NULL || target_index == NULL ||
        !classicsetup_plan_validate(plan) ||
        selected_index >= plan->item_count) {
        return -1;
    }

    if (classicsetup_plan_item_is_install_target(
            &plan->items[selected_index])) {
        *target_index = selected_index;
        return 0;
    }
    if (plan->items[selected_index].state ==
        CLASSICSETUP_PLAN_UNALLOCATED) {
        return classicsetup_plan_create_windows_layout(
            plan,
            selected_index,
            target_index);
    }
    return -1;
}
