#ifndef CLASSICSETUP_GUI_H
#define CLASSICSETUP_GUI_H

#include <stdbool.h>
#include <stddef.h>

#include "classicsetup/recommended.h"
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
    enum classicsetup_gui_page page;
    struct classicsetup_disk_assessment
        assessments[CLASSICSETUP_GUI_MAX_DISKS];
    size_t assessment_count;
    size_t selected_disk_index;
    enum classicsetup_firmware_mode firmware;
    int scan_failed;
    bool has_selected_disk;
    enum classicsetup_gui_windows_version windows_version;
    bool options_placeholder;
    struct classicsetup_network_snapshot network;
};

void classicsetup_gui_session_reset(
    struct classicsetup_gui_session *session);

enum classicsetup_gui_page classicsetup_gui_page_next(
    enum classicsetup_gui_page page);

enum classicsetup_gui_page classicsetup_gui_page_back(
    enum classicsetup_gui_page page);

int classicsetup_gui_select_disk(
    struct classicsetup_gui_session *session,
    size_t index);

int classicsetup_gui_set_windows_version(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_windows_version version);

int classicsetup_gui_session_init(
    struct classicsetup_gui_session *session);

int classicsetup_gui_run(
    struct classicsetup_gui_session *session);

#endif
