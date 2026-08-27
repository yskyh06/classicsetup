#ifndef CLASSICSETUP_INSTALL_MODE_H
#define CLASSICSETUP_INSTALL_MODE_H

enum classicsetup_install_mode {
    CLASSICSETUP_INSTALL_UEFI_GPT,
    CLASSICSETUP_INSTALL_BIOS_MBR,
    CLASSICSETUP_INSTALL_MODE_COUNT
};

enum classicsetup_install_mode classicsetup_default_install_mode(void);

#endif
