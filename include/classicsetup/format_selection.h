#ifndef CLASSICSETUP_FORMAT_SELECTION_H
#define CLASSICSETUP_FORMAT_SELECTION_H

#include "classicsetup/format.h"

enum classicsetup_format_selection_result {
    CLASSICSETUP_FORMAT_SELECTION_CONTINUE,
    CLASSICSETUP_FORMAT_SELECTION_BACK,
    CLASSICSETUP_FORMAT_SELECTION_QUIT
};

enum classicsetup_format_selection_result classicsetup_show_format_selection(
    enum classicsetup_format_mode *format_mode);

#endif
