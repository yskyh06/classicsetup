#ifndef CLASSICSETUP_APPLY_TUI_H
#define CLASSICSETUP_APPLY_TUI_H

#include "classicsetup/apply.h"

enum classicsetup_apply_preview_result {
    CLASSICSETUP_APPLY_PREVIEW_CONTINUE,
    CLASSICSETUP_APPLY_PREVIEW_BACK,
    CLASSICSETUP_APPLY_PREVIEW_QUIT
};

enum classicsetup_apply_confirmation_result {
    CLASSICSETUP_APPLY_CONFIRMATION_APPLY,
    CLASSICSETUP_APPLY_CONFIRMATION_BACK,
    CLASSICSETUP_APPLY_CONFIRMATION_QUIT
};

enum classicsetup_apply_result_screen_result {
    CLASSICSETUP_APPLY_RESULT_SCREEN_CONTINUE,
    CLASSICSETUP_APPLY_RESULT_SCREEN_BACK,
    CLASSICSETUP_APPLY_RESULT_SCREEN_QUIT
};

enum classicsetup_apply_preview_result classicsetup_show_apply_preview(
    const struct classicsetup_apply_plan *apply_plan,
    int valid);

enum classicsetup_apply_confirmation_result
classicsetup_show_apply_confirmation(
    const struct classicsetup_apply_plan *apply_plan);

enum classicsetup_apply_result_screen_result classicsetup_show_apply_result(
    const struct classicsetup_apply_result *result);

#endif
