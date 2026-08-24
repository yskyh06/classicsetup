#ifndef CLASSICSETUP_KEYBOARD_H
#define CLASSICSETUP_KEYBOARD_H

#include "classicsetup/config.h"

enum classicsetup_keyboard_result {
    CLASSICSETUP_KEYBOARD_CONTINUE,
    CLASSICSETUP_KEYBOARD_BACK,
    CLASSICSETUP_KEYBOARD_QUIT
};

enum classicsetup_keyboard_result classicsetup_show_keyboard(
    enum classicsetup_keyboard_type *keyboard_type);

#endif
