#include "classicsetup/windows_source.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static const char *find_json_key(const char *begin, const char *end,
                                 const char *key)
{
    const char *found = begin;

    while (found != NULL && found < end) {
        found = strstr(found, key);
        if (found != NULL && found < end) {
            return found + strlen(key);
        }
    }
    return NULL;
}

static int copy_json_string(const char *value, const char *end,
                            char *target, size_t target_size)
{
    size_t used = 0;

    if (value == NULL || value >= end || *value != '"' ||
        target == NULL || target_size == 0) {
        return -1;
    }
    ++value;
    while (value < end && *value != '"') {
        char output;

        if (*value == '\\') {
            ++value;
            if (value >= end) {
                return -1;
            }
            if (*value == 'u') {
                if (end - value < 5 || strncmp(value, "u0026", 5) != 0) {
                    return -1;
                }
                output = '&';
                value += 5;
            } else if (*value == '"' || *value == '\\' || *value == '/') {
                output = *value++;
            } else {
                return -1;
            }
        } else {
            output = *value++;
        }
        if (used + 1 >= target_size) {
            return -1;
        }
        target[used++] = output;
    }
    if (value >= end || *value != '"') {
        return -1;
    }
    target[used] = '\0';
    return 0;
}

static int object_string(const char *begin, const char *end,
                         const char *key, char *target, size_t target_size)
{
    const char *value = find_json_key(begin, end, key);

    if (value == NULL) {
        return -1;
    }
    while (value < end && isspace((unsigned char)*value)) {
        ++value;
    }
    return copy_json_string(value, end, target, target_size);
}

static int object_string_any(const char *begin, const char *end,
                             const char *const *keys, size_t key_count,
                             char *target, size_t target_size)
{
    size_t index;

    for (index = 0; index < key_count; ++index) {
        if (object_string(begin, end, keys[index], target, target_size) ==
            0) {
            return 0;
        }
    }
    return -1;
}

static bool contains_case_insensitive(const char *text, const char *needle)
{
    size_t needle_length;

    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return false;
    }
    needle_length = strlen(needle);
    while (*text != '\0') {
        if (strncasecmp(text, needle, needle_length) == 0) {
            return true;
        }
        ++text;
    }
    return false;
}

static const char *find_case_insensitive(const char *text,
                                         const char *needle)
{
    size_t needle_length;

    if (text == NULL || needle == NULL || needle[0] == '\0') {
        return NULL;
    }
    needle_length = strlen(needle);
    while (*text != '\0') {
        if (strncasecmp(text, needle, needle_length) == 0) {
            return text;
        }
        ++text;
    }
    return NULL;
}

static bool response_has_error_number(const char *response, const char *key,
                                       long expected)
{
    const char *cursor = response;

    while ((cursor = find_case_insensitive(cursor, key)) != NULL) {
        char *endptr;
        long value;

        cursor += strlen(key);
        while (*cursor != '\0' && (isspace((unsigned char)*cursor) ||
                                   *cursor == ':')) {
            ++cursor;
        }
        if (*cursor == '"') {
            ++cursor;
        }
        value = strtol(cursor, &endptr, 10);
        if (endptr != cursor && value == expected) {
            return true;
        }
        if (*cursor == '\0') {
            break;
        }
    }
    return false;
}

static const char *first_nonspace(const char *text)
{
    if (text == NULL) {
        return NULL;
    }
    while (*text != '\0' && isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static void set_response_kind(
    const char *response,
    struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    const char *first = first_nonspace(response);

    if (diagnostics == NULL) {
        return;
    }
    diagnostics->response_is_json = first != NULL &&
                                    (*first == '{' || *first == '[');
    diagnostics->response_is_html = first != NULL && *first == '<';
    if (!diagnostics->response_is_html && response != NULL) {
        diagnostics->response_is_html =
            find_case_insensitive(response, "<html") != NULL ||
            find_case_insensitive(response, "<!doctype") != NULL;
    }
}

void classicsetup_source_resolve_diagnostics_reset(
    struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    if (diagnostics != NULL) {
        memset(diagnostics, 0, sizeof(*diagnostics));
        diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_NONE;
    }
}

const char *classicsetup_source_resolve_error_message(
    enum classicsetup_source_resolve_error error)
{
    switch (error) {
    case CLASSICSETUP_SOURCE_RESOLVE_NONE:
        return "No source resolution error.";
    case CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR:
        return "The Microsoft source request failed at the network layer.";
    case CLASSICSETUP_SOURCE_RESOLVE_HTTP_ERROR:
        return "The Microsoft source endpoint returned an HTTP error.";
    case CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED:
        return "Microsoft requires an active source-download session.";
    case CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED:
        return "The Microsoft source response schema changed.";
    case CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED:
        return "Microsoft temporarily rate limited the source request.";
    case CLASSICSETUP_SOURCE_RESOLVE_NO_LINK:
        return "Microsoft did not provide a usable download link.";
    case CLASSICSETUP_SOURCE_RESOLVE_POLICY_REJECTED:
        return "The returned link did not meet the official-source policy.";
    case CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED:
        return "The Microsoft source redirect was not accepted.";
    case CLASSICSETUP_SOURCE_RESOLVE_MALFORMED:
        return "The Microsoft source response was malformed.";
    }
    return "The Microsoft source request failed.";
}

static const char *resolve_error_name(
    enum classicsetup_source_resolve_error error)
{
    switch (error) {
    case CLASSICSETUP_SOURCE_RESOLVE_NONE:
        return "none";
    case CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR:
        return "network-error";
    case CLASSICSETUP_SOURCE_RESOLVE_HTTP_ERROR:
        return "http-error";
    case CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED:
        return "session-required";
    case CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED:
        return "schema-changed";
    case CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED:
        return "rate-limited";
    case CLASSICSETUP_SOURCE_RESOLVE_NO_LINK:
        return "no-link";
    case CLASSICSETUP_SOURCE_RESOLVE_POLICY_REJECTED:
        return "policy-rejected";
    case CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED:
        return "redirect-rejected";
    case CLASSICSETUP_SOURCE_RESOLVE_MALFORMED:
        return "malformed";
    }
    return "unknown";
}

static const char *parser_stage_name(
    enum classicsetup_source_parser_stage stage)
{
    switch (stage) {
    case CLASSICSETUP_SOURCE_PARSER_NONE:
        return "none";
    case CLASSICSETUP_SOURCE_PARSER_LANDING:
        return "landing";
    case CLASSICSETUP_SOURCE_PARSER_SKU:
        return "sku";
    case CLASSICSETUP_SOURCE_PARSER_LINKS:
        return "links";
    }
    return "unknown";
}

int classicsetup_source_resolve_format_diagnostics(
    const struct classicsetup_source_resolve_diagnostics *diagnostics,
    char *output,
    size_t output_size)
{
    int written;

    if (diagnostics == NULL || output == NULL || output_size == 0) {
        return -1;
    }
    written = snprintf(
        output, output_size,
        "stage=%s error=%s http=%ld redirects=%ld bytes=%zu "
        "content-type=%s endpoint=%s product=%s sku=%s language=%s "
        "architecture=%s session=%d cookies=%d json=%d html=%d field=%d "
        "link=%d",
        parser_stage_name(diagnostics->parser_stage),
        resolve_error_name(diagnostics->error),
        diagnostics->http_status, diagnostics->redirect_count,
        diagnostics->response_bytes,
        diagnostics->content_type[0] == '\0' ? "<none>"
                                               : diagnostics->content_type,
        diagnostics->endpoint[0] == '\0' ? "<none>"
                                           : diagnostics->endpoint,
        diagnostics->product_edition_id[0] == '\0'
            ? "<none>"
            : diagnostics->product_edition_id,
        diagnostics->sku_id[0] == '\0' ? "<none>" : diagnostics->sku_id,
        diagnostics->language[0] == '\0' ? "<none>"
                                          : diagnostics->language,
        diagnostics->architecture[0] == '\0' ? "<none>"
                                              : diagnostics->architecture,
        diagnostics->session_id_present ? 1 : 0,
        diagnostics->cookie_engine_enabled ? 1 : 0,
        diagnostics->response_is_json ? 1 : 0,
        diagnostics->response_is_html ? 1 : 0,
        diagnostics->expected_field_present ? 1 : 0,
        diagnostics->link_present ? 1 : 0);
    return written >= 0 && (size_t)written < output_size ? 0 : -1;
}

enum classicsetup_source_resolve_error
classicsetup_source_resolve_classify_http(long http_status, bool redirected)
{
    if (http_status == 429) {
        return CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED;
    }
    if (http_status >= 300 && http_status < 400) {
        return CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED;
    }
    if (http_status >= 400) {
        return CLASSICSETUP_SOURCE_RESOLVE_HTTP_ERROR;
    }
    if (http_status == 0) {
        return CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR;
    }
    if (redirected && http_status < 200) {
        return CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED;
    }
    return CLASSICSETUP_SOURCE_RESOLVE_NONE;
}

static int landing_edition_id(const char *html, char *id, size_t id_size)
{
    const char *select = strstr(html, "id=\"product-edition\"");
    const char *option;
    const char *value;
    size_t length = 0;

    if (select == NULL ||
        (option = strstr(select, "<option value=\"")) == NULL ||
        (option = strstr(option + 1, "<option value=\"")) == NULL) {
        return -1;
    }
    value = option + strlen("<option value=\"");
    while (isdigit((unsigned char)value[length])) {
        ++length;
    }
    if (length == 0 || length >= id_size || value[length] != '"') {
        return -1;
    }
    memcpy(id, value, length);
    id[length] = '\0';
    return 0;
}

static int landing_hash(const char *html, const char *language,
                        const char *architecture_name,
                        char hash[CLASSICSETUP_SOURCE_HASH_SIZE])
{
    char marker[128];
    const char *entry;
    size_t index;

    if (snprintf(marker, sizeof(marker), "<td>%s %s</td><td>",
                 language, architecture_name) <= 0 ||
        (entry = strstr(html, marker)) == NULL) {
        return -1;
    }
    entry += strlen(marker);
    for (index = 0; index < 64; ++index) {
        if (!isxdigit((unsigned char)entry[index])) {
            return -1;
        }
        hash[index] = (char)toupper((unsigned char)entry[index]);
    }
    hash[64] = '\0';
    return 0;
}

const char *classicsetup_windows_architecture_label(
    enum classicsetup_windows_architecture architecture)
{
    switch (architecture) {
    case CLASSICSETUP_ARCH_X64:
        return "x64";
    case CLASSICSETUP_ARCH_X86:
        return "x86 (32-bit)";
    case CLASSICSETUP_ARCH_ARM64:
        return "ARM64";
    }
    return "Unknown";
}

const char *classicsetup_windows_architecture_token(
    enum classicsetup_windows_architecture architecture)
{
    switch (architecture) {
    case CLASSICSETUP_ARCH_X64:
        return "x64";
    case CLASSICSETUP_ARCH_X86:
        return "x86";
    case CLASSICSETUP_ARCH_ARM64:
        return "arm64";
    }
    return "";
}

bool classicsetup_windows_architecture_is_native(
    enum classicsetup_windows_architecture architecture)
{
#if defined(__aarch64__)
    return architecture == CLASSICSETUP_ARCH_ARM64;
#elif defined(__x86_64__) || defined(_M_X64)
    return architecture == CLASSICSETUP_ARCH_X64;
#elif defined(__i386__) || defined(_M_IX86)
    return architecture == CLASSICSETUP_ARCH_X86;
#else
    (void)architecture;
    return false;
#endif
}

void classicsetup_source_catalog_reset(
    struct classicsetup_source_catalog *catalog)
{
    if (catalog == NULL) {
        return;
    }
    memset(catalog, 0, sizeof(*catalog));
    catalog->state = CLASSICSETUP_SOURCE_IDLE;
}

bool classicsetup_windows_source_uri_is_official(const char *uri)
{
    const char *host;
    const char *end;
    size_t host_length;
    static const char suffix[] = ".microsoft.com";

    if (uri == NULL || strncmp(uri, "https://", 8) != 0) {
        return false;
    }
    host = uri + 8;
    end = strpbrk(host, "/?#");
    host_length = end != NULL ? (size_t)(end - host) : strlen(host);
    if (host_length == strlen("microsoft.com") &&
        strncasecmp(host, "microsoft.com", host_length) == 0) {
        return true;
    }
    return host_length > strlen(suffix) &&
           strncasecmp(host + host_length - strlen(suffix), suffix,
                       strlen(suffix)) == 0;
}

int classicsetup_windows_source_sanitize_uri(
    const char *uri, char *sanitized, size_t sanitized_size)
{
    const char *end;
    size_t length;

    if (!classicsetup_windows_source_uri_is_official(uri) ||
        sanitized == NULL || sanitized_size == 0) {
        return -1;
    }
    end = strpbrk(uri, "?#");
    length = end != NULL ? (size_t)(end - uri) : strlen(uri);
    if (length >= sanitized_size) {
        return -1;
    }
    memcpy(sanitized, uri, length);
    sanitized[length] = '\0';
    return 0;
}

int classicsetup_windows_source_parse_catalog(
    enum classicsetup_windows_family family,
    const char *landing_html,
    const char *sku_json,
    struct classicsetup_source_catalog *catalog)
{
    char edition_id[CLASSICSETUP_SOURCE_ID_SIZE];
    const char *cursor;

    if (landing_html == NULL || sku_json == NULL || catalog == NULL ||
        (family != CLASSICSETUP_WINDOWS_11 &&
         family != CLASSICSETUP_WINDOWS_10) ||
        strstr(sku_json, "\"Errors\":[{") != NULL ||
        landing_edition_id(landing_html, edition_id,
                           sizeof(edition_id)) != 0) {
        return -1;
    }
    classicsetup_source_catalog_reset(catalog);
    catalog->state = CLASSICSETUP_SOURCE_DISCOVERING;
    cursor = strstr(sku_json, "\"Skus\":[");
    if (cursor == NULL) {
        return -1;
    }
    while ((cursor = strchr(cursor, '{')) != NULL &&
           catalog->release_count < CLASSICSETUP_SOURCE_MAX_RELEASES) {
        const char *end = strchr(cursor, '}');
        char language[CLASSICSETUP_SOURCE_NAME_SIZE];
        struct classicsetup_windows_release release = {0};
        char x86_hash[CLASSICSETUP_SOURCE_HASH_SIZE];

        if (end == NULL || object_string(cursor, end, "\"Language\":",
                                         language, sizeof(language)) != 0) {
            break;
        }
        if (strcmp(language, "Korean") != 0 &&
            strcmp(language, "English") != 0) {
            cursor = end + 1;
            continue;
        }
        release.family = family;
        release.language = strcmp(language, "Korean") == 0
                               ? CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN
                               : CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH;
        (void)snprintf(release.language_name,
                       sizeof(release.language_name), "%s", language);
        release.architecture = CLASSICSETUP_ARCH_X64;
        (void)snprintf(release.architecture_token,
                       sizeof(release.architecture_token), "%s",
                       classicsetup_windows_architecture_token(
                           release.architecture));
        (void)snprintf(release.product_edition_id,
                       sizeof(release.product_edition_id), "%s", edition_id);
        if (object_string(cursor, end, "\"Id\":", release.sku_id,
                          sizeof(release.sku_id)) != 0 ||
            object_string(cursor, end, "\"ProductDisplayName\":",
                          release.release_name,
                          sizeof(release.release_name)) != 0) {
            return -1;
        }
        release.official_hash_available =
            landing_hash(landing_html, language, "64-bit",
                         release.expected_sha256) == 0;
        catalog->releases[catalog->release_count++] = release;
        if (family == CLASSICSETUP_WINDOWS_10 &&
            catalog->release_count < CLASSICSETUP_SOURCE_MAX_RELEASES &&
            landing_hash(
                landing_html, language, "32-bit", x86_hash) == 0) {
            release.architecture = CLASSICSETUP_ARCH_X86;
            (void)snprintf(
                release.architecture_token,
                sizeof(release.architecture_token), "%s",
                classicsetup_windows_architecture_token(
                    release.architecture));
            (void)snprintf(
                release.expected_sha256,
                sizeof(release.expected_sha256), "%s", x86_hash);
            release.official_hash_available = true;
            catalog->releases[catalog->release_count++] = release;
        }
        cursor = end + 1;
    }
    if (catalog->release_count == 0) {
        return -1;
    }
    catalog->state = CLASSICSETUP_SOURCE_READY;
    return 0;
}

static bool architecture_matches_text(
    enum classicsetup_windows_architecture architecture, const char *text)
{
    if (text == NULL) {
        return false;
    }
    if (architecture == CLASSICSETUP_ARCH_ARM64) {
        return contains_case_insensitive(text, "arm64");
    }
    if (architecture == CLASSICSETUP_ARCH_X86) {
        return (contains_case_insensitive(text, "32") ||
                contains_case_insensitive(text, "x86")) &&
               !contains_case_insensitive(text, "x64") &&
               !contains_case_insensitive(text, "arm64");
    }
    return contains_case_insensitive(text, "64") &&
           !contains_case_insensitive(text, "arm64");
}

static int copy_html_attribute(const char *begin, const char *end,
                               const char *attribute, char *target,
                               size_t target_size)
{
    const char *value = find_case_insensitive(begin, attribute);
    char quote;
    size_t used = 0;

    if (value == NULL || value >= end ||
        (value = strchr(value, '=')) == NULL || value >= end) {
        return -1;
    }
    ++value;
    while (value < end && isspace((unsigned char)*value)) {
        ++value;
    }
    if (value >= end || (*value != '\'' && *value != '"')) {
        return -1;
    }
    quote = *value++;
    while (value < end && *value != quote) {
        if (value + 5 <= end && strncasecmp(value, "&amp;", 5) == 0) {
            if (used + 1 >= target_size) {
                return -1;
            }
            target[used++] = '&';
            value += 5;
        } else {
            if (used + 1 >= target_size) {
                return -1;
            }
            target[used++] = *value++;
        }
    }
    if (value >= end || *value != quote || target_size == 0) {
        return -1;
    }
    target[used] = '\0';
    return 0;
}

static int parse_html_download(
    const char *response, struct classicsetup_windows_release *release,
    struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    const char *cursor = response;
    bool saw_uri = false;
    static const char *const attributes[] = {
        "href", "data-uri", "data-download-url"
    };

    while ((cursor = find_case_insensitive(cursor, "<a")) != NULL) {
        const char *end = strchr(cursor, '>');
        char uri[CLASSICSETUP_SOURCE_URI_SIZE];
        char type[256];
        const char *near_end;
        size_t index;

        if (end == NULL) {
            if (diagnostics != NULL) {
                diagnostics->error =
                    CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
            }
            return -1;
        }
        near_end = end + 257;
        if (strlen(end) < 257) {
            near_end = end + strlen(end);
        }
        if (copy_html_attribute(cursor, end, attributes[0], uri,
                                sizeof(uri)) != 0) {
            for (index = 1; index < sizeof(attributes) / sizeof(attributes[0]);
                 ++index) {
                if (copy_html_attribute(cursor, end, attributes[index], uri,
                                        sizeof(uri)) == 0) {
                    break;
                }
            }
            if (index == sizeof(attributes) / sizeof(attributes[0])) {
                cursor = end + 1;
                continue;
            }
        }
        saw_uri = true;
        if (!classicsetup_windows_source_uri_is_official(uri)) {
            cursor = end + 1;
            continue;
        }
        (void)snprintf(type, sizeof(type), "%.*s", (int)(near_end - cursor),
                       cursor);
        if (architecture_matches_text(release->architecture, type)) {
            (void)snprintf(release->download_uri,
                           sizeof(release->download_uri), "%s", uri);
            release->resolved = true;
            if (diagnostics != NULL) {
                diagnostics->link_present = true;
            }
            return 0;
        }
        cursor = end + 1;
    }
    if (diagnostics != NULL && saw_uri) {
        diagnostics->error =
            CLASSICSETUP_SOURCE_RESOLVE_POLICY_REJECTED;
    }
    return -1;
}

int classicsetup_windows_source_parse_download_diagnostic(
    const char *download_response,
    struct classicsetup_windows_release *release,
    struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    const char *options;
    const char *cursor;
    const char *first;
    bool saw_uri = false;
    bool saw_matching_type = false;
    struct classicsetup_source_resolve_diagnostics local_diagnostics;
    static const char *const uri_keys[] = {
        "\"Uri\":", "\"Url\":", "\"URL\":", "\"DownloadUrl\":"
    };

    if (diagnostics == NULL) {
        diagnostics = &local_diagnostics;
    }
    classicsetup_source_resolve_diagnostics_reset(diagnostics);
    diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LINKS;
    diagnostics->response_bytes = download_response == NULL
                                      ? 0
                                      : strlen(download_response);
    set_response_kind(download_response, diagnostics);
    if (release == NULL || download_response == NULL) {
        if (diagnostics != NULL) {
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        }
        return -1;
    }
    release->resolved = false;
    release->download_uri[0] = '\0';
    first = first_nonspace(download_response);
    if (first == NULL || first[0] == '\0') {
        if (diagnostics != NULL) {
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        }
        return -1;
    }
    if (contains_case_insensitive(download_response, "sentinelreject") ||
        response_has_error_number(download_response, "\"Type\"", 8)) {
        if (diagnostics != NULL) {
            diagnostics->error =
                CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED;
        }
        return -1;
    }
    if (contains_case_insensitive(download_response, "toomanyrequests") ||
        contains_case_insensitive(download_response, "ratelimit") ||
        response_has_error_number(download_response, "\"Type\"", 429)) {
        if (diagnostics != NULL) {
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED;
        }
        return -1;
    }
    if (diagnostics != NULL && diagnostics->response_is_html) {
        if (contains_case_insensitive(download_response, "consent") ||
            contains_case_insensitive(download_response, "captcha") ||
            contains_case_insensitive(download_response, "sentinel")) {
            diagnostics->error =
                CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED;
        }
        if (parse_html_download(download_response, release, diagnostics) == 0) {
            return 0;
        }
        if (diagnostics->error == CLASSICSETUP_SOURCE_RESOLVE_NONE) {
            diagnostics->error =
                CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED;
        }
        return -1;
    }
    options = strstr(download_response, "\"ProductDownloadOptions\"");
    if (diagnostics != NULL) {
        diagnostics->expected_field_present = options != NULL;
    }
    if (options == NULL) {
        if (strchr(first, '{') != NULL &&
            strrchr(download_response, '}') == NULL) {
            if (diagnostics != NULL) {
                diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
            }
        } else if (diagnostics != NULL) {
            diagnostics->error =
                CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED;
        }
        return -1;
    }
    cursor = options;
    while ((cursor = strchr(cursor, '{')) != NULL) {
        const char *end = strchr(cursor, '}');
        char type[64];
        char uri[CLASSICSETUP_SOURCE_URI_SIZE];

        if (end == NULL) {
            if (diagnostics != NULL) {
                diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
            }
            return -1;
        }
        if (object_string(cursor, end, "\"DownloadType\":", type,
                          sizeof(type)) == 0 &&
            architecture_matches_text(release->architecture, type)) {
            saw_matching_type = true;
            if (object_string_any(cursor, end, uri_keys,
                                  sizeof(uri_keys) / sizeof(uri_keys[0]), uri,
                                  sizeof(uri)) == 0) {
                saw_uri = true;
                if (!classicsetup_windows_source_uri_is_official(uri)) {
                    cursor = end + 1;
                    continue;
                }
                (void)snprintf(release->download_uri,
                               sizeof(release->download_uri), "%s", uri);
                release->resolved = true;
                if (diagnostics != NULL) {
                    diagnostics->link_present = true;
                    diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_NONE;
                }
                return 0;
            }
        }
        cursor = end + 1;
    }
    if (diagnostics != NULL) {
        diagnostics->error = saw_uri
                                ? CLASSICSETUP_SOURCE_RESOLVE_POLICY_REJECTED
                                : saw_matching_type
                                    ? CLASSICSETUP_SOURCE_RESOLVE_MALFORMED
                                    : CLASSICSETUP_SOURCE_RESOLVE_NO_LINK;
    }
    return -1;
}

int classicsetup_windows_source_parse_download(
    const char *download_json,
    struct classicsetup_windows_release *release)
{
    return classicsetup_windows_source_parse_download_diagnostic(
        download_json, release, NULL);
}
