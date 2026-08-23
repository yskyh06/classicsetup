#ifndef CLASSICSETUP_WELCOME_H
#define CLASSICSETUP_WELCOME_H

enum classicsetup_welcome_result {
    CLASSICSETUP_WELCOME_CONTINUE,
    CLASSICSETUP_WELCOME_QUIT
};

enum classicsetup_welcome_result classicsetup_show_welcome(void);

#endif
