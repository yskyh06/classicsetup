#ifndef CLASSICSETUP_QUIT_H
#define CLASSICSETUP_QUIT_H

enum classicsetup_quit_confirmation_result {
    CLASSICSETUP_QUIT_CONTINUE_SETUP,
    CLASSICSETUP_QUIT_CONFIRM
};

enum classicsetup_quit_confirmation_result classicsetup_confirm_quit(void);

#endif
