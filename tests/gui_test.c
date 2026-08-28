#include <assert.h>
#include <string.h>

#include "classicsetup/gui.h"

static void test_page_navigation(void)
{
    enum classicsetup_gui_page destination;

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
    assert(classicsetup_gui_page_back_for_entry(
               CLASSICSETUP_GUI_ENTRY_RECOMMENDED,
               CLASSICSETUP_GUI_PAGE_NETWORK,
               &destination) == CLASSICSETUP_GUI_BACK_TO_PAGE);
    assert(destination == CLASSICSETUP_GUI_PAGE_DISK);
    assert(classicsetup_gui_page_back_for_entry(
               CLASSICSETUP_GUI_ENTRY_AFTER_ADVANCED,
               CLASSICSETUP_GUI_PAGE_NETWORK,
               &destination) == CLASSICSETUP_GUI_BACK_TO_TUI);
    assert(classicsetup_gui_page_back_for_entry(
               CLASSICSETUP_GUI_ENTRY_AFTER_ADVANCED,
               CLASSICSETUP_GUI_PAGE_WINDOWS_VERSION,
               &destination) == CLASSICSETUP_GUI_BACK_TO_PAGE);
    assert(destination == CLASSICSETUP_GUI_PAGE_NETWORK);
}

static void test_session_and_disk_selection(void)
{
    struct classicsetup_gui_session session;

    classicsetup_gui_session_reset(&session);
    assert(session.entry_mode == CLASSICSETUP_GUI_ENTRY_RECOMMENDED);
    assert(session.page == CLASSICSETUP_GUI_PAGE_DISK);
    assert(session.windows_version == CLASSICSETUP_GUI_WINDOWS_11);
    assert(session.network.state == CLASSICSETUP_NETWORK_UNAVAILABLE);
    assert(!classicsetup_network_can_continue(&session.network));
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
    session.network.state = CLASSICSETUP_NETWORK_CONNECTED;
    session.network.connectivity =
        CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET;
    session.network.wifi_count = 1;
    classicsetup_gui_session_reset(&session);
    assert(session.network.state == CLASSICSETUP_NETWORK_UNAVAILABLE);
    assert(session.network.wifi_count == 0);
}

static void test_advanced_entry_requires_verified_storage(void)
{
    struct classicsetup_gui_session session;
    struct classicsetup_disk_info disk = {0};

    (void)strcpy(disk.name, "sdb");
    (void)strcpy(disk.device_path, "/dev/sdb");
    (void)strcpy(disk.model, "Prepared test disk");
    disk.size_bytes = 64ULL * 1024ULL * 1024ULL * 1024ULL;
    assert(classicsetup_gui_session_init_after_advanced(
               &session, &disk, false, true) != 0);
    assert(classicsetup_gui_session_init_after_advanced(
               &session, &disk, true, false) != 0);
    assert(classicsetup_gui_session_init_after_advanced(
               &session, &disk, true, true) == 0);
    assert(session.entry_mode == CLASSICSETUP_GUI_ENTRY_AFTER_ADVANCED);
    assert(session.page == CLASSICSETUP_GUI_PAGE_NETWORK);
    assert(session.advanced_storage_prepared);
    assert(session.has_prepared_disk);
    assert(strcmp(session.prepared_disk.device_path, "/dev/sdb") == 0);
    assert(session.assessment_count == 0);
    assert(!session.has_selected_disk);
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
    test_advanced_entry_requires_verified_storage();
    test_gtk_disabled_result();
    return 0;
}
