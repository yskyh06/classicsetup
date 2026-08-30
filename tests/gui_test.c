#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

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
               CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN,
               CLASSICSETUP_GUI_PAGE_NETWORK,
               &destination) == CLASSICSETUP_GUI_BACK_TO_TUI);
    assert(classicsetup_gui_page_back_for_entry(
               CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN,
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
    assert(session.source_backend == CLASSICSETUP_SOURCE_MICROSOFT_RETAIL);
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
#if CLASSICSETUP_ENABLE_UUP
    assert(classicsetup_gui_set_source_backend(
               &session,
               CLASSICSETUP_SOURCE_MICROSOFT_UUP) == 0);
    assert(session.source_backend == CLASSICSETUP_SOURCE_MICROSOFT_UUP);
#else
    assert(classicsetup_gui_set_source_backend(
               &session,
               CLASSICSETUP_SOURCE_MICROSOFT_UUP) != 0);
#endif
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

static void test_advanced_entry_preserves_planning_snapshot(void)
{
    struct classicsetup_gui_session session;
    struct classicsetup_config config = {0};

    config.setup_mode = CLASSICSETUP_SETUP_ADVANCED;
    config.install_mode = CLASSICSETUP_INSTALL_UEFI_GPT;
    config.has_selected_disk = true;
    config.has_partition_plan = true;
    config.has_selected_plan_target = true;
    config.selected_format_plan.valid = true;
    config.selected_format_plan.filesystem = CLASSICSETUP_FS_NTFS;
    config.selected_format_plan.mode = CLASSICSETUP_FORMAT_QUICK;
    config.advanced_storage_plan_ready = true;
    config.has_apply_plan = true;
    (void)strcpy(config.selected_disk.name, "sdb");
    (void)strcpy(config.selected_disk.device_path, "/dev/sdb");
    (void)strcpy(config.selected_disk.model, "Planned test disk");
    config.selected_disk.size_bytes =
        64ULL * 1024ULL * 1024ULL * 1024ULL;
    config.partition_plan.disk_sector_count = 42;
    config.apply_plan.partition_count = 4;

    config.advanced_storage_plan_ready = false;
    assert(classicsetup_gui_session_init_advanced_plan(
               &session, &config) != 0);
    config.advanced_storage_plan_ready = true;
    assert(classicsetup_gui_session_init_advanced_plan(
               &session, &config) == 0);
    assert(session.entry_mode == CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN);
    assert(session.page == CLASSICSETUP_GUI_PAGE_NETWORK);
    assert(session.advanced_plan_prepared);
    assert(session.has_prepared_disk);
    assert(strcmp(session.prepared_disk.device_path, "/dev/sdb") == 0);
    assert(session.prepared_install_mode == CLASSICSETUP_INSTALL_UEFI_GPT);
    assert(session.prepared_partition_plan.disk_sector_count == 42);
    assert(session.prepared_format_plan.mode == CLASSICSETUP_FORMAT_QUICK);
    assert(session.has_prepared_apply_plan);
    assert(session.prepared_apply_plan.partition_count == 4);
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

static void test_source_selection_and_summary_gate(void)
{
    struct classicsetup_gui_session session;

    classicsetup_gui_session_reset(&session);
    session.network.state = CLASSICSETUP_NETWORK_CONNECTED;
    session.network.connectivity =
        CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET;
    session.options_placeholder = true;
    session.source_catalog.state = CLASSICSETUP_SOURCE_READY;
    session.source_catalog.release_count = 1;
    session.source_catalog.releases[0].family = CLASSICSETUP_WINDOWS_11;
    assert(classicsetup_gui_select_release(&session, 0) == 0);
    assert(session.has_selected_release);
    assert(!classicsetup_gui_summary_is_ready(&session));
    session.workspace.valid = true;
    session.workspace.verified_iso = true;
    session.download.state = CLASSICSETUP_DOWNLOAD_COMPLETE;
    session.download.error = CLASSICSETUP_DOWNLOAD_ERROR_NONE;
    session.verified_source.backend =
        CLASSICSETUP_SOURCE_MICROSOFT_RETAIL;
    session.verified_source.kind = CLASSICSETUP_VERIFIED_SOURCE_ISO;
    session.verified_source.family = CLASSICSETUP_WINDOWS_11;
    session.verified_source.architecture = CLASSICSETUP_ARCH_X64;
    session.verified_source.language =
        CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN;
    (void)strcpy(session.verified_source.build, "26100.1");
    (void)strcpy(session.verified_source.edition, "Professional");
    session.verified_source.verified = true;
    assert(classicsetup_gui_summary_is_ready(&session));
    assert(classicsetup_gui_set_windows_version(
               &session, CLASSICSETUP_GUI_WINDOWS_10) != 0);
    classicsetup_gui_session_reset(&session);
    assert(!session.has_selected_release);
    assert(session.source_catalog.state == CLASSICSETUP_SOURCE_IDLE);
    assert(session.download.state == CLASSICSETUP_DOWNLOAD_NOT_STARTED);
    assert(!session.workspace.valid);
}

static void test_gtk_disabled_result(void)
{
    struct classicsetup_gui_session session;

    classicsetup_gui_session_reset(&session);
    assert(classicsetup_gui_run(&session) == CLASSICSETUP_GUI_UNAVAILABLE);
}

static void test_network_presentation_supports_multiple_ethernet(void)
{
    struct classicsetup_gui_network_presentation presentation;
    struct classicsetup_network_snapshot snapshot;

    classicsetup_gui_network_presentation_reset(&presentation);
    assert(classicsetup_gui_network_presentation_add_ethernet(
               &presentation, "Ethernet 1", true) == 0);
    assert(classicsetup_gui_network_presentation_add_ethernet(
               &presentation, "USB Ethernet", false) == 0);
    assert(presentation.ethernet_count == 2);
    assert(presentation.ethernet[0].connected);
    assert(!presentation.ethernet[1].connected);

    classicsetup_network_snapshot_reset(&snapshot);
    snapshot.ethernet_available = true;
    snapshot.ethernet_connected = true;
    classicsetup_gui_network_presentation_from_snapshot(
        &presentation, &snapshot);
    assert(presentation.ethernet_count == 1);
    assert(strcmp(
               presentation.ethernet[0].display_name,
               "Local Area Connection") == 0);
    assert(presentation.ethernet[0].connected);
}

static void test_cascading_source_selection_and_lifecycle(void)
{
    struct classicsetup_gui_session session;
    char verified_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    FILE *file;

    classicsetup_gui_session_reset(&session);
    session.source_catalog.state = CLASSICSETUP_SOURCE_READY;
    session.source_catalog.release_count = 3;
    session.source_catalog.releases[0].family = CLASSICSETUP_WINDOWS_11;
    (void)strcpy(
        session.source_catalog.releases[0].release_name,
        "Windows 11 25H2");
    (void)strcpy(
        session.source_catalog.releases[0].language_name, "Korean");
    session.source_catalog.releases[0].language =
        CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN;
    session.source_catalog.releases[0].architecture = CLASSICSETUP_ARCH_X64;
    session.source_catalog.releases[1] = session.source_catalog.releases[0];
    (void)strcpy(
        session.source_catalog.releases[1].language_name, "English");
    session.source_catalog.releases[1].language =
        CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH;
    session.source_catalog.releases[2] = session.source_catalog.releases[1];
    (void)strcpy(
        session.source_catalog.releases[2].release_name,
        "Windows 11 24H2");

    assert(classicsetup_gui_select_release_name(
               &session, "Windows 11 25H2") == 0);
    assert(session.has_selected_release_name);
    assert(!session.has_selected_language);
    assert(classicsetup_gui_select_language(
               &session,
               CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN) == 0);
    assert(session.has_selected_language);
    assert(!session.has_selected_architecture);
    assert(classicsetup_gui_select_architecture(
               &session, CLASSICSETUP_ARCH_ARM64) != 0);
    assert(!classicsetup_gui_source_selection_is_valid(&session));
    assert(classicsetup_gui_select_architecture(
               &session, CLASSICSETUP_ARCH_X64) == 0);
    assert(classicsetup_gui_source_selection_is_valid(&session));
    session.page = CLASSICSETUP_GUI_PAGE_DOWNLOAD;
    session.page = classicsetup_gui_page_back(session.page);
    assert(session.page == CLASSICSETUP_GUI_PAGE_WINDOWS_VERSION);
    assert(classicsetup_gui_source_selection_is_valid(&session));
    assert(classicsetup_gui_source_change_requirement(&session) ==
           CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED);
    assert(classicsetup_gui_select_language(
               &session,
               CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH) == 0);
    assert(!session.has_selected_architecture);
    assert(classicsetup_gui_select_architecture(
               &session, CLASSICSETUP_ARCH_X64) == 0);
    assert(classicsetup_gui_select_release_name(
               &session, "Windows 11 24H2") == 0);
    assert(!session.has_selected_language);
    assert(!session.has_selected_architecture);
    assert(!session.has_selected_release);

    session.download.state = CLASSICSETUP_DOWNLOAD_FAILED;
    assert(classicsetup_gui_source_change_requirement(&session) ==
           CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED);
    session.download.state = CLASSICSETUP_DOWNLOAD_DOWNLOADING;
    assert(classicsetup_gui_source_change_requirement(&session) ==
           CLASSICSETUP_GUI_SOURCE_CHANGE_CANCEL_DOWNLOAD);
    session.download.state = CLASSICSETUP_DOWNLOAD_NOT_STARTED;
    assert(classicsetup_workspace_create(&session.workspace) == 0);
    (void)snprintf(verified_path, sizeof(verified_path), "%s",
                   session.workspace.iso_final_path);
    file = fopen(session.workspace.iso_final_path, "wb");
    assert(file != NULL);
    assert(fclose(file) == 0);
    session.workspace.verified_iso = true;
    session.download.state = CLASSICSETUP_DOWNLOAD_COMPLETE;
    assert(classicsetup_gui_source_change_requirement(&session) ==
           CLASSICSETUP_GUI_SOURCE_CHANGE_DISCARD_VERIFIED);
    classicsetup_gui_discard_downloaded_source(&session);
    assert(!session.workspace.valid);
    assert(access(verified_path, F_OK) != 0);
    assert(session.download.state == CLASSICSETUP_DOWNLOAD_NOT_STARTED);
}

static void test_retail_browser_policy_and_state(void)
{
    struct classicsetup_retail_browser_status status;
    struct classicsetup_windows_release release = {0};
    const char *signed_uri =
        "https://software.download.prss.microsoft.com/dbazure/"
        "Win11_Korean_x64.iso?token=not-logged";

    classicsetup_retail_browser_status_reset(&status);
    assert(status.stage == CLASSICSETUP_RETAIL_BROWSER_IDLE);
    assert(classicsetup_retail_browser_transition(
               &status,
               CLASSICSETUP_RETAIL_BROWSER_PREPARING_MICROSOFT_PAGE) == 0);
    assert(classicsetup_retail_browser_transition(
               &status,
               CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT) == 0);
    assert(!classicsetup_retail_browser_should_show_webview(&status));
    assert(classicsetup_retail_browser_transition(
               &status,
               CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_USER_DOWNLOAD_CLICK) ==
           0);
    assert(classicsetup_retail_browser_transition(
               &status, CLASSICSETUP_RETAIL_BROWSER_COMPLETE) != 0);

    assert(classicsetup_retail_browser_navigation_is_allowed(
        classicsetup_retail_browser_page_uri(
            CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN)));
    assert(strstr(classicsetup_retail_browser_page_uri(
                      CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH),
                  "/en-us/") != NULL);
    assert(classicsetup_retail_browser_should_show_webview(&status));
    assert(!classicsetup_retail_browser_navigation_is_allowed(
        "https://example.com/windows11"));
    assert(!classicsetup_retail_browser_navigation_is_allowed(
        "https://support.microsoft.com/windows11"));
    assert(classicsetup_retail_browser_delivery_uri_is_allowed(signed_uri));
    assert(!classicsetup_retail_browser_delivery_uri_is_allowed(
        "http://software.download.prss.microsoft.com/windows.iso"));
    assert(!classicsetup_retail_browser_delivery_uri_is_allowed(
        "https://software.download.prss.microsoft.com.evil/windows.iso"));
    assert(!classicsetup_retail_browser_delivery_uri_is_allowed(
        "https://software.download.prss.microsoft.com/windows.exe"));
    assert(classicsetup_retail_browser_capture_download(
               signed_uri, &release, &status) == 0);
    assert(status.stage == CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING);
    assert(status.cancel_webkit_download);
    assert(strcmp(status.delivery_host,
                  "software.download.prss.microsoft.com") == 0);
    assert(release.resolved);
    assert(strcmp(release.download_uri, signed_uri) == 0);
    assert(strstr(status.delivery_host, "token") == NULL);
    classicsetup_retail_browser_clear_uri(&release);
    assert(!release.resolved);
    assert(release.download_uri[0] == '\0');

    assert(classicsetup_retail_browser_transition(
               &status, CLASSICSETUP_RETAIL_BROWSER_VERIFYING_ISO) == 0);
    assert(classicsetup_retail_browser_transition(
               &status, CLASSICSETUP_RETAIL_BROWSER_INSPECTING_IMAGE) == 0);
    assert(classicsetup_retail_browser_transition(
               &status, CLASSICSETUP_RETAIL_BROWSER_COMPLETE) == 0);

    classicsetup_retail_browser_status_reset(&status);
    assert(classicsetup_retail_browser_transition(
               &status,
               CLASSICSETUP_RETAIL_BROWSER_PREPARING_MICROSOFT_PAGE) == 0);
    classicsetup_retail_browser_fallback_to_full_page(&status);
    assert(status.full_page_fallback);
    assert(status.stage ==
           CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT);
    assert(classicsetup_retail_browser_capture_download(
               signed_uri, &release, &status) == 0);
    assert(status.cancel_webkit_download);
    classicsetup_retail_browser_clear_uri(&release);
}

static void test_retail_source_fallback_is_single_shot(void)
{
    struct classicsetup_gui_session session;

    classicsetup_gui_session_reset(&session);
    assert(session.retail_source_mode ==
           CLASSICSETUP_GUI_RETAIL_AUTOMATIC);
    assert(classicsetup_gui_retail_try_fido_once(&session));
    assert(!classicsetup_gui_retail_try_fido_once(&session));
    assert(classicsetup_gui_retail_start_webview_once(&session));
    assert(!classicsetup_gui_retail_start_webview_once(&session));

    classicsetup_gui_session_reset(&session);
    assert(classicsetup_gui_set_retail_source_mode(
               &session,
               CLASSICSETUP_GUI_RETAIL_MICROSOFT_PAGE) == 0);
    assert(!classicsetup_gui_retail_try_fido_once(&session));
    assert(classicsetup_gui_retail_start_webview_once(&session));
}

int main(void)
{
    test_page_navigation();
    test_session_and_disk_selection();
    test_selection_is_fail_closed_for_bios();
    test_advanced_entry_preserves_planning_snapshot();
    test_source_selection_and_summary_gate();
    test_network_presentation_supports_multiple_ethernet();
    test_cascading_source_selection_and_lifecycle();
    test_retail_browser_policy_and_state();
    test_retail_source_fallback_is_single_shot();
    test_gtk_disabled_result();
    return 0;
}
