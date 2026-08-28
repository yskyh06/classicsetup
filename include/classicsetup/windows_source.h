#ifndef CLASSICSETUP_WINDOWS_SOURCE_H
#define CLASSICSETUP_WINDOWS_SOURCE_H

#include <stdbool.h>
#include <stddef.h>

enum {
    CLASSICSETUP_SOURCE_MAX_RELEASES = 32,
    CLASSICSETUP_SOURCE_NAME_SIZE = 96,
    CLASSICSETUP_SOURCE_ID_SIZE = 32,
    CLASSICSETUP_SOURCE_URI_SIZE = 2048,
    CLASSICSETUP_SOURCE_HASH_SIZE = 65,
    CLASSICSETUP_SOURCE_ERROR_SIZE = 192
};

enum classicsetup_windows_family {
    CLASSICSETUP_WINDOWS_11,
    CLASSICSETUP_WINDOWS_10
};

enum classicsetup_windows_language {
    CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN,
    CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH
};

enum classicsetup_windows_architecture {
    CLASSICSETUP_ARCH_X64,
    CLASSICSETUP_ARCH_X86,
    CLASSICSETUP_ARCH_ARM64
};

enum classicsetup_source_state {
    CLASSICSETUP_SOURCE_IDLE,
    CLASSICSETUP_SOURCE_DISCOVERING,
    CLASSICSETUP_SOURCE_READY,
    CLASSICSETUP_SOURCE_ERROR
};

struct classicsetup_windows_release {
    enum classicsetup_windows_family family;
    enum classicsetup_windows_language language;
    enum classicsetup_windows_architecture architecture;
    char release_name[CLASSICSETUP_SOURCE_NAME_SIZE];
    char language_name[CLASSICSETUP_SOURCE_NAME_SIZE];
    char architecture_token[16];
    char product_edition_id[CLASSICSETUP_SOURCE_ID_SIZE];
    char sku_id[CLASSICSETUP_SOURCE_ID_SIZE];
    char expected_sha256[CLASSICSETUP_SOURCE_HASH_SIZE];
    char download_uri[CLASSICSETUP_SOURCE_URI_SIZE];
    unsigned long long expected_size;
    bool official_hash_available;
    bool resolved;
};

struct classicsetup_source_catalog {
    enum classicsetup_source_state state;
    struct classicsetup_windows_release
        releases[CLASSICSETUP_SOURCE_MAX_RELEASES];
    size_t release_count;
    char error[CLASSICSETUP_SOURCE_ERROR_SIZE];
};

void classicsetup_source_catalog_reset(
    struct classicsetup_source_catalog *catalog);

const char *classicsetup_windows_architecture_label(
    enum classicsetup_windows_architecture architecture);

const char *classicsetup_windows_architecture_token(
    enum classicsetup_windows_architecture architecture);

bool classicsetup_windows_architecture_is_native(
    enum classicsetup_windows_architecture architecture);

bool classicsetup_windows_source_uri_is_official(const char *uri);

int classicsetup_windows_source_sanitize_uri(
    const char *uri,
    char *sanitized,
    size_t sanitized_size);

int classicsetup_windows_source_parse_catalog(
    enum classicsetup_windows_family family,
    const char *landing_html,
    const char *sku_json,
    struct classicsetup_source_catalog *catalog);

int classicsetup_windows_source_parse_download(
    const char *download_json,
    struct classicsetup_windows_release *release);

int classicsetup_microsoft_source_discover(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog);

int classicsetup_microsoft_source_resolve(
    struct classicsetup_windows_release *release);

#endif
