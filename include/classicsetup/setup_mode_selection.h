#ifndef CLASSICSETUP_SETUP_MODE_SELECTION_H
#define CLASSICSETUP_SETUP_MODE_SELECTION_H

#include "classicsetup/setup_mode.h"

enum classicsetup_setup_mode_selection_result {
    CLASSICSETUP_SETUP_MODE_SELECTION_CONTINUE,
    CLASSICSETUP_SETUP_MODE_SELECTION_QUIT
};

enum classicsetup_setup_mode_selection_result
classicsetup_show_setup_mode_selection(
    enum classicsetup_setup_mode *setup_mode);

#endif
