#ifndef CLASSICSETUP_LICENSE_AGREEMENT_H
#define CLASSICSETUP_LICENSE_AGREEMENT_H

#define CLASSICSETUP_RISK_AGREEMENT_VERSION "2026-08-27.1"

enum classicsetup_license_agreement_result {
    CLASSICSETUP_LICENSE_AGREEMENT_WAIT,
    CLASSICSETUP_LICENSE_AGREEMENT_ACCEPT,
    CLASSICSETUP_LICENSE_AGREEMENT_BACK,
    CLASSICSETUP_LICENSE_AGREEMENT_QUIT
};

enum classicsetup_license_agreement_result
classicsetup_license_agreement_result_for_key(int key);

enum classicsetup_license_agreement_result
classicsetup_show_license_agreement(void);

#endif
