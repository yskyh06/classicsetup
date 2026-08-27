#ifndef CLASSICSETUP_FORMAT_APPLY_TUI_H
#define CLASSICSETUP_FORMAT_APPLY_TUI_H

#include "classicsetup/format_apply.h"

enum classicsetup_format_apply_preview_result {
    CLASSICSETUP_FORMAT_APPLY_PREVIEW_CONTINUE,
    CLASSICSETUP_FORMAT_APPLY_PREVIEW_BACK,
    CLASSICSETUP_FORMAT_APPLY_PREVIEW_QUIT
};

enum classicsetup_format_apply_confirmation_result {
    CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_APPLY,
    CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_BACK,
    CLASSICSETUP_FORMAT_APPLY_CONFIRMATION_QUIT
};

enum classicsetup_format_apply_result_screen_result {
    CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_CONTINUE,
    CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_BACK,
    CLASSICSETUP_FORMAT_APPLY_RESULT_SCREEN_QUIT
};

enum classicsetup_format_apply_preview_result
classicsetup_show_format_apply_preview(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_result *previous_result,
    int valid);

enum classicsetup_format_apply_confirmation_result
classicsetup_show_format_apply_confirmation(
    const struct classicsetup_format_apply_plan *plan);

enum classicsetup_format_apply_result_screen_result
classicsetup_show_format_apply_result(
    const struct classicsetup_format_apply_plan *plan,
    const struct classicsetup_format_result *result);

#endif
