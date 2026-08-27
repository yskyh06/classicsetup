#include <assert.h>

#include "classicsetup/gui.h"

static void test_page_navigation(void)
{
    assert(classicsetup_gui_page_next(CLASSICSETUP_GUI_PAGE_DISK) ==
           CLASSICSETUP_GUI_PAGE_NETWORK);
    assert(classicsetup_gui_page_next(CLASSICSETUP_GUI_PAGE_OPTIONS) ==
           CLASSICSETUP_GUI_PAGE_SUMMARY);
    assert(classicsetup_gui_page_next(CLASSICSETUP_GUI_PAGE_SUMMARY) ==
           CLASSICSETUP_GUI_PAGE_SUMMARY);
    assert(classicsetup_gui_page_back(CLASSICSETUP_GUI_PAGE_NETWORK) ==
           CLASSICSETUP_GUI_PAGE_DISK);
    assert(classicsetup_gui_page_back(CLASSICSETUP_GUI_PAGE_DISK) ==
           CLASSICSETUP_GUI_PAGE_DISK);
}

static void test_session_and_disk_selection(void)
{
    struct classicsetup_gui_session session;

    classicsetup_gui_session_reset(&session);
    assert(session.page == CLASSICSETUP_GUI_PAGE_DISK);
    assert(session.windows_version == CLASSICSETUP_GUI_WINDOWS_11);
    session.firmware = CLASSICSETUP_FIRMWARE_UEFI;
    session.assessment_count = 2;
    session.assessments[0].selectable = 0;
    session.assessments[1].selectable = 1;
    assert(classicsetup_gui_select_disk(&session, 0) != 0);
    assert(!session.has_selected_disk);
    assert(classicsetup_gui_select_disk(&session, 1) == 0);
    assert(session.has_selected_disk);
    assert(session.selected_disk_index == 1);
    assert(classicsetup_gui_set_windows_version(
               &session,
               CLASSICSETUP_GUI_WINDOWS_10) == 0);
    assert(session.windows_version == CLASSICSETUP_GUI_WINDOWS_10);
    assert(classicsetup_gui_set_windows_version(
               &session,
               (enum classicsetup_gui_windows_version)99) != 0);
}

static void test_selection_is_fail_closed_for_bios(void)
{
    struct classicsetup_gui_session session;

    classicsetup_gui_session_reset(&session);
    session.firmware = CLASSICSETUP_FIRMWARE_BIOS;
    session.assessment_count = 1;
    session.assessments[0].selectable = 1;
    assert(classicsetup_gui_select_disk(&session, 0) != 0);
    assert(!session.has_selected_disk);
}

static void test_gtk_disabled_result(void)
{
    struct classicsetup_gui_session session;

    classicsetup_gui_session_reset(&session);
    assert(classicsetup_gui_run(&session) == CLASSICSETUP_GUI_UNAVAILABLE);
}

int main(void)
{
    test_page_navigation();
    test_session_and_disk_selection();
    test_selection_is_fail_closed_for_bios();
    test_gtk_disabled_result();
    return 0;
}
