#include "classicsetup/gui.h"

#include <string.h>

void classicsetup_gui_session_reset(
    struct classicsetup_gui_session *session)
{
    classicsetup_gui_session_reset_for_entry(
        session,
        CLASSICSETUP_GUI_ENTRY_RECOMMENDED);
}

void classicsetup_gui_session_reset_for_entry(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_entry_mode entry_mode)
{
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
    session->entry_mode = entry_mode;
    session->page = entry_mode == CLASSICSETUP_GUI_ENTRY_AFTER_ADVANCED
                        ? CLASSICSETUP_GUI_PAGE_NETWORK
                        : CLASSICSETUP_GUI_PAGE_DISK;
    session->windows_version = CLASSICSETUP_GUI_WINDOWS_11;
    classicsetup_network_snapshot_reset(&session->network);
}

enum classicsetup_gui_page classicsetup_gui_page_next(
    enum classicsetup_gui_page page)
{
    if (page < CLASSICSETUP_GUI_PAGE_DISK ||
        page >= CLASSICSETUP_GUI_PAGE_SUMMARY) {
        return CLASSICSETUP_GUI_PAGE_SUMMARY;
    }
    return (enum classicsetup_gui_page)(page + 1);
}

enum classicsetup_gui_page classicsetup_gui_page_back(
    enum classicsetup_gui_page page)
{
    if (page <= CLASSICSETUP_GUI_PAGE_DISK ||
        page >= CLASSICSETUP_GUI_PAGE_COUNT) {
        return CLASSICSETUP_GUI_PAGE_DISK;
    }
    return (enum classicsetup_gui_page)(page - 1);
}

enum classicsetup_gui_back_action classicsetup_gui_page_back_for_entry(
    enum classicsetup_gui_entry_mode entry_mode,
    enum classicsetup_gui_page page,
    enum classicsetup_gui_page *destination)
{
    if (destination == NULL) {
        return CLASSICSETUP_GUI_BACK_TO_TUI;
    }
    if (page == CLASSICSETUP_GUI_PAGE_DISK ||
        (entry_mode == CLASSICSETUP_GUI_ENTRY_AFTER_ADVANCED &&
         page == CLASSICSETUP_GUI_PAGE_NETWORK) ||
        page < CLASSICSETUP_GUI_PAGE_DISK ||
        page >= CLASSICSETUP_GUI_PAGE_COUNT) {
        *destination = page;
        return CLASSICSETUP_GUI_BACK_TO_TUI;
    }
    *destination = classicsetup_gui_page_back(page);
    return CLASSICSETUP_GUI_BACK_TO_PAGE;
}

int classicsetup_gui_select_disk(
    struct classicsetup_gui_session *session,
    size_t index)
{
    if (session == NULL || index >= session->assessment_count ||
        !session->assessments[index].selectable ||
        session->firmware != CLASSICSETUP_FIRMWARE_UEFI) {
        return -1;
    }
    session->selected_disk_index = index;
    session->has_selected_disk = true;
    return 0;
}

int classicsetup_gui_set_windows_version(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_windows_version version)
{
    if (session == NULL ||
        (version != CLASSICSETUP_GUI_WINDOWS_11 &&
         version != CLASSICSETUP_GUI_WINDOWS_10)) {
        return -1;
    }
    session->windows_version = version;
    return 0;
}

int classicsetup_gui_session_init_after_advanced(
    struct classicsetup_gui_session *session,
    const struct classicsetup_disk_info *prepared_disk,
    bool partition_apply_succeeded,
    bool format_apply_verified)
{
    if (session == NULL || prepared_disk == NULL ||
        !partition_apply_succeeded || !format_apply_verified) {
        return -1;
    }
    classicsetup_gui_session_reset_for_entry(
        session,
        CLASSICSETUP_GUI_ENTRY_AFTER_ADVANCED);
    session->advanced_storage_prepared = true;
    session->has_prepared_disk = true;
    session->prepared_disk = *prepared_disk;
    return 0;
}
