#ifndef CLASSICSETUP_INSTALL_MODE_SELECTION_H
#define CLASSICSETUP_INSTALL_MODE_SELECTION_H

#include "classicsetup/install_mode.h"

enum classicsetup_install_mode_selection_result {
    CLASSICSETUP_INSTALL_MODE_SELECTION_CONTINUE,
    CLASSICSETUP_INSTALL_MODE_SELECTION_BACK,
    CLASSICSETUP_INSTALL_MODE_SELECTION_QUIT
};

enum classicsetup_install_mode_selection_result
classicsetup_show_install_mode_selection(
    enum classicsetup_install_mode *install_mode);

#endif
