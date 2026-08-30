#ifndef CLASSICSETUP_GUI_H
#define CLASSICSETUP_GUI_H

#include <stdbool.h>
#include <stddef.h>

#include "classicsetup/config.h"
#include "classicsetup/download.h"
#include "classicsetup/network.h"
#include "classicsetup/retail.h"
#include "classicsetup/retail_browser.h"
#include "classicsetup/uup.h"

enum {
    CLASSICSETUP_GUI_MAX_DISKS = 32,
    CLASSICSETUP_GUI_MAX_ETHERNET = 8,
    CLASSICSETUP_GUI_CONNECTION_NAME_SIZE = 96
};

struct classicsetup_gui_ethernet_entry {
    char display_name[CLASSICSETUP_GUI_CONNECTION_NAME_SIZE];
    bool connected;
};

struct classicsetup_gui_network_presentation {
    struct classicsetup_gui_ethernet_entry
        ethernet[CLASSICSETUP_GUI_MAX_ETHERNET];
    size_t ethernet_count;
};

enum classicsetup_gui_page {
    CLASSICSETUP_GUI_PAGE_DISK,
    CLASSICSETUP_GUI_PAGE_NETWORK,
    CLASSICSETUP_GUI_PAGE_WINDOWS_VERSION,
    CLASSICSETUP_GUI_PAGE_DOWNLOAD,
    CLASSICSETUP_GUI_PAGE_OPTIONS,
    CLASSICSETUP_GUI_PAGE_SUMMARY,
    CLASSICSETUP_GUI_PAGE_COUNT
};

enum classicsetup_gui_entry_mode {
    CLASSICSETUP_GUI_ENTRY_RECOMMENDED,
    CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN
};

enum classicsetup_gui_back_action {
    CLASSICSETUP_GUI_BACK_TO_PAGE,
    CLASSICSETUP_GUI_BACK_TO_TUI
};

enum classicsetup_gui_windows_version {
    CLASSICSETUP_GUI_WINDOWS_11,
    CLASSICSETUP_GUI_WINDOWS_10
};

enum classicsetup_gui_run_result {
    CLASSICSETUP_GUI_FINISHED,
    CLASSICSETUP_GUI_BACK,
    CLASSICSETUP_GUI_QUIT,
    CLASSICSETUP_GUI_UNAVAILABLE,
    CLASSICSETUP_GUI_ERROR
};

enum classicsetup_gui_source_change_requirement {
    CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED,
    CLASSICSETUP_GUI_SOURCE_CHANGE_CANCEL_DOWNLOAD,
    CLASSICSETUP_GUI_SOURCE_CHANGE_DISCARD_VERIFIED
};

struct classicsetup_gui_session {
    enum classicsetup_gui_entry_mode entry_mode;
    enum classicsetup_gui_page page;
    struct classicsetup_disk_assessment
        assessments[CLASSICSETUP_GUI_MAX_DISKS];
    size_t assessment_count;
    size_t selected_disk_index;
    enum classicsetup_firmware_mode firmware;
    int scan_failed;
    bool has_selected_disk;
    enum classicsetup_gui_windows_version windows_version;
    enum classicsetup_source_backend source_backend;
    struct classicsetup_source_catalog source_catalog;
    struct classicsetup_source_resolve_diagnostics source_diagnostics;
    char selected_release_name[CLASSICSETUP_SOURCE_NAME_SIZE];
    bool has_selected_release_name;
    enum classicsetup_windows_language selected_language;
    bool has_selected_language;
    enum classicsetup_windows_architecture selected_architecture;
    bool has_selected_architecture;
    size_t selected_release_index;
    bool has_selected_release;
    struct classicsetup_download_status download;
    struct classicsetup_uup_status uup_status;
    struct classicsetup_retail_status retail_status;
    struct classicsetup_retail_browser_status retail_browser_status;
    struct classicsetup_verified_windows_source verified_source;
    struct classicsetup_workspace workspace;
    bool options_placeholder;
    struct classicsetup_network_snapshot network;
    bool advanced_plan_prepared;
    bool has_prepared_disk;
    struct classicsetup_disk_info prepared_disk;
    enum classicsetup_install_mode prepared_install_mode;
    struct classicsetup_partition_plan prepared_partition_plan;
    struct classicsetup_format_plan prepared_format_plan;
    struct classicsetup_format_plan
        prepared_role_format_plans[CLASSICSETUP_PARTITION_ROLE_COUNT];
    bool has_prepared_apply_plan;
    struct classicsetup_apply_plan prepared_apply_plan;
};

void classicsetup_gui_session_reset(
    struct classicsetup_gui_session *session);

void classicsetup_gui_session_reset_for_entry(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_entry_mode entry_mode);

enum classicsetup_gui_page classicsetup_gui_page_next(
    enum classicsetup_gui_page page);

enum classicsetup_gui_page classicsetup_gui_page_back(
    enum classicsetup_gui_page page);

enum classicsetup_gui_back_action classicsetup_gui_page_back_for_entry(
    enum classicsetup_gui_entry_mode entry_mode,
    enum classicsetup_gui_page page,
    enum classicsetup_gui_page *destination);

int classicsetup_gui_select_disk(
    struct classicsetup_gui_session *session,
    size_t index);

int classicsetup_gui_set_windows_version(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_windows_version version);

int classicsetup_gui_set_source_backend(
    struct classicsetup_gui_session *session,
    enum classicsetup_source_backend backend);

int classicsetup_gui_select_release(
    struct classicsetup_gui_session *session,
    size_t index);

int classicsetup_gui_select_release_name(
    struct classicsetup_gui_session *session,
    const char *release_name);

int classicsetup_gui_select_language(
    struct classicsetup_gui_session *session,
    enum classicsetup_windows_language language);

int classicsetup_gui_select_architecture(
    struct classicsetup_gui_session *session,
    enum classicsetup_windows_architecture architecture);

bool classicsetup_gui_source_selection_is_valid(
    const struct classicsetup_gui_session *session);

enum classicsetup_gui_source_change_requirement
classicsetup_gui_source_change_requirement(
    const struct classicsetup_gui_session *session);

void classicsetup_gui_discard_downloaded_source(
    struct classicsetup_gui_session *session);

bool classicsetup_gui_summary_is_ready(
    const struct classicsetup_gui_session *session);

void classicsetup_gui_network_presentation_reset(
    struct classicsetup_gui_network_presentation *presentation);

int classicsetup_gui_network_presentation_add_ethernet(
    struct classicsetup_gui_network_presentation *presentation,
    const char *display_name,
    bool connected);

void classicsetup_gui_network_presentation_from_snapshot(
    struct classicsetup_gui_network_presentation *presentation,
    const struct classicsetup_network_snapshot *snapshot);

int classicsetup_gui_session_init(
    struct classicsetup_gui_session *session);

int classicsetup_gui_session_init_advanced_plan(
    struct classicsetup_gui_session *session,
    const struct classicsetup_config *config);

int classicsetup_gui_run(
    struct classicsetup_gui_session *session);

#endif
