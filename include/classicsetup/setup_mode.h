#ifndef CLASSICSETUP_SETUP_MODE_H
#define CLASSICSETUP_SETUP_MODE_H

enum classicsetup_setup_mode {
    CLASSICSETUP_SETUP_RECOMMENDED,
    CLASSICSETUP_SETUP_ADVANCED,
    CLASSICSETUP_SETUP_MODE_COUNT
};

enum classicsetup_setup_mode classicsetup_default_setup_mode(void);

#endif
