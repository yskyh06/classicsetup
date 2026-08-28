#ifndef CLASSICSETUP_GUI_H
#define CLASSICSETUP_GUI_H

#include <stdbool.h>
#include <stddef.h>

#include "classicsetup/config.h"
#include "classicsetup/download.h"
#include "classicsetup/network.h"

enum {
    CLASSICSETUP_GUI_MAX_DISKS = 32
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
    struct classicsetup_source_catalog source_catalog;
    size_t selected_release_index;
    bool has_selected_release;
    struct classicsetup_download_status download;
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

int classicsetup_gui_select_release(
    struct classicsetup_gui_session *session,
    size_t index);

bool classicsetup_gui_summary_is_ready(
    const struct classicsetup_gui_session *session);

int classicsetup_gui_session_init(
    struct classicsetup_gui_session *session);

int classicsetup_gui_session_init_advanced_plan(
    struct classicsetup_gui_session *session,
    const struct classicsetup_config *config);

int classicsetup_gui_run(
    struct classicsetup_gui_session *session);

#endif
