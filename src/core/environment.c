#include "classicsetup/environment.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

enum {
    ENVIRONMENT_TEXT_SIZE = 512,
    ENVIRONMENT_PATH_SIZE = 512
};

static int read_text_file(const char *path, char *text, size_t text_size)
{
    FILE *file;
    size_t length;

    if (path == NULL || text == NULL || text_size == 0) {
        return -1;
    }
    file = fopen(path, "r");
    if (file == NULL) {
        return -1;
    }
    if (fgets(text, (int)text_size, file) == NULL) {
        fclose(file);
        return -1;
    }
    fclose(file);

    length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) {
        text[--length] = '\0';
    }
    return 0;
}

static int contains_case_insensitive(const char *text, const char *needle)
{
    size_t needle_length = strlen(needle);
    size_t index;

    if (needle_length == 0) {
        return 1;
    }
    for (; *text != '\0'; ++text) {
        for (index = 0; index < needle_length; ++index) {
            if (text[index] == '\0' ||
                tolower((unsigned char)text[index]) !=
                    tolower((unsigned char)needle[index])) {
                break;
            }
        }
        if (index == needle_length) {
            return 1;
        }
    }
    return 0;
}

static int file_contains_environment_marker(
    const char *path,
    const char *marker)
{
    char text[ENVIRONMENT_TEXT_SIZE];

    return read_text_file(path, text, sizeof(text)) == 0 &&
           contains_case_insensitive(text, marker);
}

static int dmi_contains_marker(const char *dmi_path, const char *marker)
{
    const char *files[] = {
        "product_name",
        "sys_vendor",
        "board_vendor"
    };
    char path[ENVIRONMENT_PATH_SIZE];
    size_t index;

    for (index = 0; index < sizeof(files) / sizeof(files[0]); ++index) {
        int written = snprintf(
            path,
            sizeof(path),
            "%s/%s",
            dmi_path,
            files[index]);

        if (written >= 0 && (size_t)written < sizeof(path) &&
            file_contains_environment_marker(path, marker)) {
            return 1;
        }
    }
    return 0;
}

int classicsetup_detect_environment_from(
    const char *version_path,
    const char *osrelease_path,
    const char *dmi_path,
    enum classicsetup_environment *environment)
{
    char version[ENVIRONMENT_TEXT_SIZE];
    char osrelease[ENVIRONMENT_TEXT_SIZE];
    int version_read;
    int osrelease_read;

    if (version_path == NULL || osrelease_path == NULL || dmi_path == NULL ||
        environment == NULL) {
        return -1;
    }

    *environment = CLASSICSETUP_ENV_UNKNOWN;
    version_read = read_text_file(version_path, version, sizeof(version));
    osrelease_read = read_text_file(
        osrelease_path,
        osrelease,
        sizeof(osrelease));
    if (version_read != 0 || osrelease_read != 0) {
        return 0;
    }
    if ((version_read == 0 &&
         (contains_case_insensitive(version, "microsoft") ||
          contains_case_insensitive(version, "wsl"))) ||
        (osrelease_read == 0 &&
         (contains_case_insensitive(osrelease, "microsoft") ||
          contains_case_insensitive(osrelease, "wsl")))) {
        *environment = CLASSICSETUP_ENV_WSL;
        return 0;
    }
    if (dmi_contains_marker(dmi_path, "virtualbox")) {
        *environment = CLASSICSETUP_ENV_VIRTUALBOX;
        return 0;
    }
    if (dmi_contains_marker(dmi_path, "vmware")) {
        *environment = CLASSICSETUP_ENV_VMWARE;
        return 0;
    }
    return 0;
}

int classicsetup_detect_environment(
    enum classicsetup_environment *environment)
{
    return classicsetup_detect_environment_from(
        "/proc/version",
        "/proc/sys/kernel/osrelease",
        "/sys/class/dmi/id",
        environment);
}

int classicsetup_environment_allows_apply(
    enum classicsetup_environment environment)
{
    return environment == CLASSICSETUP_ENV_VIRTUALBOX ||
           environment == CLASSICSETUP_ENV_VMWARE;
}

int classicsetup_destructive_unlock_enabled(const char *value)
{
    return value != NULL && strcmp(value, "YES") == 0;
}

const char *classicsetup_environment_name(
    enum classicsetup_environment environment)
{
    switch (environment) {
    case CLASSICSETUP_ENV_WSL:
        return "WSL";
    case CLASSICSETUP_ENV_VIRTUALBOX:
        return "VirtualBox";
    case CLASSICSETUP_ENV_VMWARE:
        return "VMware";
    case CLASSICSETUP_ENV_UNKNOWN:
        return "Unknown or bare metal";
    }
    return "Invalid";
}
