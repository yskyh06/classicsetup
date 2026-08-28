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
                        char hash[CLASSICSETUP_SOURCE_HASH_SIZE])
{
    char marker[128];
    const char *entry;
    size_t index;

    if (snprintf(marker, sizeof(marker), "<td>%s 64-bit</td><td>",
                 language) <= 0 ||
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
        (void)snprintf(release.architecture,
                       sizeof(release.architecture), "%s", "x86_64");
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
            landing_hash(landing_html, language,
                         release.expected_sha256) == 0;
        catalog->releases[catalog->release_count++] = release;
        cursor = end + 1;
    }
    if (catalog->release_count == 0) {
        return -1;
    }
    catalog->state = CLASSICSETUP_SOURCE_READY;
    return 0;
}

int classicsetup_windows_source_parse_download(
    const char *download_json,
    struct classicsetup_windows_release *release)
{
    const char *options;
    const char *cursor;

    if (download_json == NULL || release == NULL ||
        strstr(download_json, "\"Errors\":[{") != NULL ||
        (options = strstr(download_json, "\"ProductDownloadOptions\"")) ==
            NULL) {
        return -1;
    }
    cursor = options;
    while ((cursor = strchr(cursor, '{')) != NULL) {
        const char *end = strchr(cursor, '}');
        char type[64];
        char uri[CLASSICSETUP_SOURCE_URI_SIZE];

        if (end == NULL) {
            return -1;
        }
        if (object_string(cursor, end, "\"DownloadType\":", type,
                          sizeof(type)) == 0 &&
            (strstr(type, "64") != NULL || strstr(type, "x64") != NULL) &&
            object_string(cursor, end, "\"Uri\":", uri,
                          sizeof(uri)) == 0 &&
            classicsetup_windows_source_uri_is_official(uri)) {
            (void)snprintf(release->download_uri,
                           sizeof(release->download_uri), "%s", uri);
            release->resolved = true;
            return 0;
        }
        cursor = end + 1;
    }
    return -1;
}
