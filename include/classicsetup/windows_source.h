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
    CLASSICSETUP_SOURCE_ERROR_SIZE = 192,
    CLASSICSETUP_SOURCE_CONTENT_TYPE_SIZE = 96,
    CLASSICSETUP_SOURCE_ENDPOINT_SIZE = 192
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

enum classicsetup_windows_edition {
    CLASSICSETUP_WINDOWS_EDITION_PROFESSIONAL
};

enum classicsetup_source_backend {
    CLASSICSETUP_SOURCE_MICROSOFT_RETAIL,
    CLASSICSETUP_SOURCE_MICROSOFT_UUP,
    CLASSICSETUP_SOURCE_EXISTING_ISO
};

enum classicsetup_verified_source_kind {
    CLASSICSETUP_VERIFIED_SOURCE_NONE,
    CLASSICSETUP_VERIFIED_SOURCE_ISO,
    CLASSICSETUP_VERIFIED_SOURCE_WIM
};

struct classicsetup_verified_windows_source {
    enum classicsetup_source_backend backend;
    enum classicsetup_verified_source_kind kind;
    enum classicsetup_windows_family family;
    enum classicsetup_windows_language language;
    enum classicsetup_windows_architecture architecture;
    char release_name[CLASSICSETUP_SOURCE_NAME_SIZE];
    char build[CLASSICSETUP_SOURCE_NAME_SIZE];
    char edition[CLASSICSETUP_SOURCE_NAME_SIZE];
    char path[4096];
    unsigned int image_index;
    bool verified;
};

enum classicsetup_source_state {
    CLASSICSETUP_SOURCE_IDLE,
    CLASSICSETUP_SOURCE_DISCOVERING,
    CLASSICSETUP_SOURCE_READY,
    CLASSICSETUP_SOURCE_ERROR
};

enum classicsetup_source_resolve_error {
    CLASSICSETUP_SOURCE_RESOLVE_NONE,
    CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR,
    CLASSICSETUP_SOURCE_RESOLVE_HTTP_ERROR,
    CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED,
    CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED,
    CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED,
    CLASSICSETUP_SOURCE_RESOLVE_NO_LINK,
    CLASSICSETUP_SOURCE_RESOLVE_POLICY_REJECTED,
    CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED,
    CLASSICSETUP_SOURCE_RESOLVE_MALFORMED
};

enum classicsetup_source_parser_stage {
    CLASSICSETUP_SOURCE_PARSER_NONE,
    CLASSICSETUP_SOURCE_PARSER_LANDING,
    CLASSICSETUP_SOURCE_PARSER_SKU,
    CLASSICSETUP_SOURCE_PARSER_LINKS
};

struct classicsetup_source_resolve_diagnostics {
    enum classicsetup_source_resolve_error error;
    enum classicsetup_source_parser_stage parser_stage;
    long http_status;
    long redirect_count;
    size_t response_bytes;
    char content_type[CLASSICSETUP_SOURCE_CONTENT_TYPE_SIZE];
    char endpoint[CLASSICSETUP_SOURCE_ENDPOINT_SIZE];
    char product_edition_id[CLASSICSETUP_SOURCE_ID_SIZE];
    char sku_id[CLASSICSETUP_SOURCE_ID_SIZE];
    char language[CLASSICSETUP_SOURCE_NAME_SIZE];
    char architecture[16];
    bool response_is_json;
    bool response_is_html;
    bool expected_field_present;
    bool link_present;
    bool session_id_present;
    bool cookie_engine_enabled;
};

struct classicsetup_windows_release {
    enum classicsetup_windows_family family;
    enum classicsetup_windows_language language;
    enum classicsetup_windows_architecture architecture;
    enum classicsetup_windows_edition edition;
    char release_name[CLASSICSETUP_SOURCE_NAME_SIZE];
    char language_name[CLASSICSETUP_SOURCE_NAME_SIZE];
    char architecture_token[16];
    char edition_name[CLASSICSETUP_SOURCE_NAME_SIZE];
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

void classicsetup_source_resolve_diagnostics_reset(
    struct classicsetup_source_resolve_diagnostics *diagnostics);

const char *classicsetup_source_resolve_error_message(
    enum classicsetup_source_resolve_error error);

int classicsetup_source_resolve_format_diagnostics(
    const struct classicsetup_source_resolve_diagnostics *diagnostics,
    char *output,
    size_t output_size);

enum classicsetup_source_resolve_error
classicsetup_source_resolve_classify_http(long http_status,
                                          bool redirected);

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

int classicsetup_windows_source_parse_download_diagnostic(
    const char *download_response,
    struct classicsetup_windows_release *release,
    struct classicsetup_source_resolve_diagnostics *diagnostics);

int classicsetup_microsoft_source_discover(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog);

int classicsetup_microsoft_source_resolve(
    struct classicsetup_windows_release *release);

int classicsetup_microsoft_source_resolve_with_diagnostics(
    struct classicsetup_windows_release *release,
    struct classicsetup_source_resolve_diagnostics *diagnostics);

#endif
