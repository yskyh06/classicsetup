#include "classicsetup/gui.h"

#include <string.h>

void classicsetup_gui_session_reset(
    struct classicsetup_gui_session *session)
{
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
    session->page = CLASSICSETUP_GUI_PAGE_DISK;
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
