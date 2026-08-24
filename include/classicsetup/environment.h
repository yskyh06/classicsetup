#ifndef CLASSICSETUP_ENVIRONMENT_H
#define CLASSICSETUP_ENVIRONMENT_H

enum classicsetup_environment {
    CLASSICSETUP_ENV_UNKNOWN,
    CLASSICSETUP_ENV_WSL,
    CLASSICSETUP_ENV_VIRTUALBOX,
    CLASSICSETUP_ENV_VMWARE
};

int classicsetup_detect_environment(
    enum classicsetup_environment *environment);

int classicsetup_detect_environment_from(
    const char *version_path,
    const char *osrelease_path,
    const char *dmi_path,
    enum classicsetup_environment *environment);

int classicsetup_environment_allows_apply(
    enum classicsetup_environment environment);

int classicsetup_destructive_unlock_enabled(const char *value);

const char *classicsetup_environment_name(
    enum classicsetup_environment environment);

#endif
