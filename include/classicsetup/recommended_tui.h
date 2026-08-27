#ifndef CLASSICSETUP_RECOMMENDED_TUI_H
#define CLASSICSETUP_RECOMMENDED_TUI_H

#include <stddef.h>

#include "classicsetup/recommended.h"

enum classicsetup_recommended_disk_result {
    CLASSICSETUP_RECOMMENDED_DISK_CONTINUE,
    CLASSICSETUP_RECOMMENDED_DISK_BACK,
    CLASSICSETUP_RECOMMENDED_DISK_QUIT
};

enum classicsetup_simple_screen_result {
    CLASSICSETUP_SIMPLE_CONTINUE,
    CLASSICSETUP_SIMPLE_BACK,
    CLASSICSETUP_SIMPLE_QUIT
};

enum classicsetup_install_summary_result {
    CLASSICSETUP_INSTALL_SUMMARY_INSTALL,
    CLASSICSETUP_INSTALL_SUMMARY_BACK,
    CLASSICSETUP_INSTALL_SUMMARY_QUIT
};

enum classicsetup_recommended_disk_result
classicsetup_show_recommended_disk_selection(
    const struct classicsetup_disk_assessment *assessments,
    size_t assessment_count,
    enum classicsetup_firmware_mode firmware,
    int scan_failed,
    size_t *selected_index);

enum classicsetup_simple_screen_result
classicsetup_show_windows_source_placeholder(void);

enum classicsetup_simple_screen_result
classicsetup_show_recommended_gui_transition(void);

enum classicsetup_simple_screen_result
classicsetup_show_recommended_gui_unavailable(void);

enum classicsetup_simple_screen_result
classicsetup_show_network_placeholder(void);

enum classicsetup_simple_screen_result
classicsetup_show_windows_version_placeholder(void);

enum classicsetup_simple_screen_result
classicsetup_show_windows_download_placeholder(void);

enum classicsetup_simple_screen_result
classicsetup_show_install_options_placeholder(void);

enum classicsetup_install_summary_result classicsetup_show_install_summary(
    const struct classicsetup_recommended_plan *plan);

enum classicsetup_simple_screen_result classicsetup_show_recommended_result(
    enum classicsetup_recommended_result_code result_code,
    const struct classicsetup_apply_result *partition_result,
    const struct classicsetup_format_result *format_result);

enum classicsetup_simple_screen_result
classicsetup_show_next_stage_placeholder(void);

#endif
