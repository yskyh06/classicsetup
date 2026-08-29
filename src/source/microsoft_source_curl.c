#include "classicsetup/windows_source.h"

#include <curl/curl.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

enum { MAX_RESPONSE_BYTES = 4 * 1024 * 1024 };

struct response_buffer {
    char *data;
    size_t size;
};

enum fetch_result {
    FETCH_OK,
    FETCH_NETWORK_ERROR,
    FETCH_HTTP_ERROR,
    FETCH_EMPTY
};

struct fetch_info {
    long http_status;
    long redirect_count;
    CURLcode curl_result;
    size_t response_bytes;
    bool effective_uri_official;
    char content_type[CLASSICSETUP_SOURCE_CONTENT_TYPE_SIZE];
    char endpoint[CLASSICSETUP_SOURCE_ENDPOINT_SIZE];
};

static void copy_safe_diagnostic_text(char *target, size_t target_size,
                                      const char *source);

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

static bool response_has_type(const char *response, long expected)
{
    const char *cursor = response;

    while ((cursor = strstr(cursor, "\"Type\"")) != NULL) {
        char *endptr;
        long value;

        cursor += strlen("\"Type\"");
        while (*cursor != '\0' &&
               (isspace((unsigned char)*cursor) || *cursor == ':')) {
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

static bool query_token_is_safe(const char *value)
{
    size_t index;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    for (index = 0; value[index] != '\0'; ++index) {
        unsigned char character = (unsigned char)value[index];

        if (!(isalnum(character) || character == '-' || character == '_' ||
              character == '.' || character == '~')) {
            return false;
        }
    }
    return true;
}

static void set_response_kind_diagnostic(
    struct classicsetup_source_resolve_diagnostics *diagnostics,
    const struct response_buffer *response,
    const struct fetch_info *info)
{
    const char *body;

    if (diagnostics == NULL) {
        return;
    }
    diagnostics->response_is_json =
        info != NULL && strstr(info->content_type, "json") != NULL;
    diagnostics->response_is_html =
        info != NULL && strstr(info->content_type, "html") != NULL;
    if (response == NULL || response->data == NULL) {
        return;
    }
    body = response->data;
    while (*body != '\0' && isspace((unsigned char)*body)) {
        ++body;
    }
    if (*body == '{' || *body == '[') {
        diagnostics->response_is_json = true;
    } else if (*body == '<') {
        diagnostics->response_is_html = true;
    }
}

static void copy_request_context(
    struct classicsetup_source_resolve_diagnostics *diagnostics,
    const struct classicsetup_windows_release *release)
{
    if (diagnostics == NULL || release == NULL) {
        return;
    }
    copy_safe_diagnostic_text(diagnostics->product_edition_id,
                              sizeof(diagnostics->product_edition_id),
                              release->product_edition_id);
    copy_safe_diagnostic_text(diagnostics->sku_id,
                              sizeof(diagnostics->sku_id),
                              release->sku_id);
    copy_safe_diagnostic_text(diagnostics->language,
                              sizeof(diagnostics->language),
                              release->language_name);
    copy_safe_diagnostic_text(
        diagnostics->architecture, sizeof(diagnostics->architecture),
        release->architecture_token[0] != '\0'
            ? release->architecture_token
            : classicsetup_windows_architecture_token(
                  release->architecture));
    diagnostics->session_id_present = true;
    diagnostics->cookie_engine_enabled = true;
}

static void copy_safe_diagnostic_text(char *target, size_t target_size,
                                      const char *source)
{
    size_t index = 0;

    if (target == NULL || target_size == 0) {
        return;
    }
    if (source != NULL) {
        while (source[index] != '\0' && index + 1 < target_size) {
            unsigned char character = (unsigned char)source[index];

            if (isalnum(character) || character == '/' || character == '-' ||
                character == '+' || character == '.' || character == ';' ||
                character == '=' || character == ' ') {
                target[index] = (char)character;
            } else {
                target[index] = '?';
            }
            ++index;
        }
    }
    target[index] = '\0';
}

#ifndef NDEBUG
static void maybe_report_diagnostics(
    const struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    const char *enabled = getenv("CLASSICSETUP_SOURCE_DIAGNOSTIC");
    char message[1024];

    if (enabled == NULL || strcmp(enabled, "1") != 0 || diagnostics == NULL) {
        return;
    }
    if (classicsetup_source_resolve_format_diagnostics(
            diagnostics, message, sizeof(message)) == 0) {
        (void)fprintf(stderr, "classicsetup-source: %s\n", message);
    }
}
#else
static void maybe_report_diagnostics(
    const struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    (void)diagnostics;
}
#endif

static const char *landing_uri(enum classicsetup_windows_family family)
{
    return family == CLASSICSETUP_WINDOWS_11
               ? "https://www.microsoft.com/en-us/software-download/windows11"
               : "https://www.microsoft.com/en-us/software-download/windows10ISO";
}

static size_t append_response(char *data, size_t size, size_t count,
                              void *user_data)
{
    struct response_buffer *buffer = user_data;
    size_t bytes = size * count;
    char *resized;

    if (size != 0 && bytes / size != count) {
        return 0;
    }
    if (buffer->size > MAX_RESPONSE_BYTES ||
        bytes > MAX_RESPONSE_BYTES - buffer->size) {
        return 0;
    }
    resized = realloc(buffer->data, buffer->size + bytes + 1);
    if (resized == NULL) {
        return 0;
    }
    buffer->data = resized;
    memcpy(buffer->data + buffer->size, data, bytes);
    buffer->size += bytes;
    buffer->data[buffer->size] = '\0';
    return bytes;
}

static void reset_response(struct response_buffer *buffer)
{
    free(buffer->data);
    memset(buffer, 0, sizeof(*buffer));
}

static enum fetch_result fetch(CURL *curl, const char *uri,
                               const char *referer,
                               struct response_buffer *buffer,
                               struct fetch_info *info)
{
    long response_code = 0;
    long redirect_count = 0;
    const char *content_type = NULL;
    const char *effective_uri = NULL;
    struct curl_slist *headers = NULL;
    CURLcode result;

    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
    reset_response(buffer);
    headers = curl_slist_append(headers, "Accept: application/json");
    if (headers != NULL) {
        struct curl_slist *with_origin = curl_slist_append(
            headers, "Origin: https://www.microsoft.com");

        if (with_origin == NULL) {
            curl_slist_free_all(headers);
            return FETCH_NETWORK_ERROR;
        }
        headers = with_origin;
    }
    if (headers == NULL || curl_easy_setopt(curl, CURLOPT_URL, uri) !=
                                 CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_REFERER, referer) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_response) !=
            CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer) != CURLE_OK) {
        curl_slist_free_all(headers);
        return FETCH_NETWORK_ERROR;
    }
    result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    (void)curl_easy_getinfo(curl, CURLINFO_REDIRECT_COUNT, &redirect_count);
    (void)curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    (void)curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_uri);
    curl_slist_free_all(headers);
    if (info != NULL) {
        info->http_status = response_code;
        info->redirect_count = redirect_count;
        info->curl_result = result;
        info->response_bytes = buffer->size;
        if (content_type != NULL) {
            copy_safe_diagnostic_text(info->content_type,
                                      sizeof(info->content_type),
                                      content_type);
        }
        if (effective_uri != NULL &&
            classicsetup_windows_source_sanitize_uri(
                effective_uri, info->endpoint, sizeof(info->endpoint)) == 0) {
            info->effective_uri_official = true;
        } else if (effective_uri != NULL) {
            (void)snprintf(info->endpoint, sizeof(info->endpoint), "%s",
                           "<non-official>");
        }
    }
    if (result != CURLE_OK) {
        return FETCH_NETWORK_ERROR;
    }
    if (response_code < 200 || response_code >= 300) {
        return FETCH_HTTP_ERROR;
    }
    return buffer->data != NULL ? FETCH_OK : FETCH_EMPTY;
}

static int configure(CURL *curl)
{
    return curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L) == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https") == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https") == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L) == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L) == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "") == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_USERAGENT,
                            "ClassicSetup/11 source discovery") == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 20L) == CURLE_OK &&
           curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L) == CURLE_OK ? 0 : -1;
}

static int extract_edition_id(const char *html, char *id, size_t id_size)
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
    while (value[length] >= '0' && value[length] <= '9') {
        ++length;
    }
    if (length == 0 || length >= id_size || value[length] != '"') {
        return -1;
    }
    memcpy(id, value, length);
    id[length] = '\0';
    return 0;
}

static void make_session_id(char output[37])
{
    unsigned long long value = (unsigned long long)time(NULL);
    unsigned long random_value = (unsigned long)clock();

    (void)snprintf(output, 37,
                   "%08lx-%04lx-4%03lx-8%03lx-%012llx",
                   random_value & 0xffffffffUL,
                   (random_value >> 8) & 0xffffUL,
                   random_value & 0xfffUL,
                   (random_value >> 4) & 0xfffUL,
                   value & 0xffffffffffffULL);
}

static void copy_fetch_diagnostics(
    struct classicsetup_source_resolve_diagnostics *diagnostics,
    const struct fetch_info *info)
{
    if (diagnostics == NULL || info == NULL) {
        return;
    }
    diagnostics->http_status = info->http_status;
    diagnostics->redirect_count = info->redirect_count;
    diagnostics->response_bytes = info->response_bytes;
    (void)snprintf(diagnostics->content_type,
                   sizeof(diagnostics->content_type), "%s",
                   info->content_type);
    (void)snprintf(diagnostics->endpoint, sizeof(diagnostics->endpoint),
                   "%s", info->endpoint);
    if (info->redirect_count > 0 && !info->effective_uri_official) {
        diagnostics->error =
            CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED;
    }
}

static void set_fetch_error(
    struct classicsetup_source_resolve_diagnostics *diagnostics,
    enum fetch_result result, const struct fetch_info *info)
{
    if (diagnostics == NULL) {
        return;
    }
    copy_fetch_diagnostics(diagnostics, info);
    diagnostics->response_is_json =
        strstr(diagnostics->content_type, "json") != NULL;
    diagnostics->response_is_html =
        strstr(diagnostics->content_type, "html") != NULL;
    if (diagnostics->error == CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED) {
        return;
    }
    if (result == FETCH_NETWORK_ERROR) {
        diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR;
    } else if (result == FETCH_HTTP_ERROR) {
        diagnostics->error = classicsetup_source_resolve_classify_http(
            diagnostics->http_status, diagnostics->redirect_count > 0);
    } else if (result == FETCH_EMPTY) {
        diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
    }
}

/*
 * A 2xx response is not necessarily a successful API response.  The
 * connector can return a consent/attestation page or an Errors object with
 * HTTP 200.  Classify only stable markers; never log or expose its body.
 */
static enum classicsetup_source_resolve_error classify_success_body(
    const struct response_buffer *response,
    const struct fetch_info *info)
{
    const char *body;

    if (response == NULL || response->data == NULL || response->size == 0) {
        return CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
    }
    body = response->data;
    if (contains_case_insensitive(body, "sentinelreject") ||
        response_has_type(body, 8) ||
        (info != NULL && strstr(info->content_type, "html") != NULL &&
         (contains_case_insensitive(body, "captcha") ||
          contains_case_insensitive(body, "consent")))) {
        return CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED;
    }
    if (contains_case_insensitive(body, "toomanyrequests") ||
        contains_case_insensitive(body, "ratelimit") ||
        response_has_type(body, 429)) {
        return CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED;
    }
    return CLASSICSETUP_SOURCE_RESOLVE_NONE;
}

static const char *language_token(
    enum classicsetup_windows_language language)
{
    return language == CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN ? "Korean"
                                                              : "English";
}

static int fetch_landing_and_skus(
    CURL *curl, enum classicsetup_windows_family family,
    const char *session, struct response_buffer *landing,
    struct response_buffer *skus, struct fetch_info *landing_info,
    struct fetch_info *sku_info)
{
    char edition[CLASSICSETUP_SOURCE_ID_SIZE];
    char uri[1024];
    const char *page = landing_uri(family);
    int written;

    if (fetch(curl, page, page, landing, landing_info) != FETCH_OK ||
        extract_edition_id(landing->data, edition, sizeof(edition)) != 0 ||
        !query_token_is_safe(edition) || !query_token_is_safe(session)) {
        return -1;
    }
    written = snprintf(
        uri, sizeof(uri),
        "https://www.microsoft.com/software-download-connector/api/"
        "getskuinformationbyproductedition?profile=606624d44113&"
        "ProductEditionId=%s&SKU=undefined&friendlyFileName=undefined&"
        "Locale=en-US&sessionID=%s",
        edition, session);
    return written > 0 && (size_t)written < sizeof(uri)
               ? fetch(curl, uri, page, skus, sku_info) == FETCH_OK ? 0 : -1
               : -1;
}

int classicsetup_microsoft_source_discover(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog)
{
    struct response_buffer landing = {0};
    struct response_buffer skus = {0};
    char session[37];
    CURL *curl;
    int result = -1;

    if (catalog == NULL ||
        (family != CLASSICSETUP_WINDOWS_11 &&
         family != CLASSICSETUP_WINDOWS_10)) {
        return -1;
    }
    classicsetup_source_catalog_reset(catalog);
    catalog->state = CLASSICSETUP_SOURCE_DISCOVERING;
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        catalog->state = CLASSICSETUP_SOURCE_ERROR;
        return -1;
    }
    make_session_id(session);
    curl = curl_easy_init();
    if (curl != NULL && configure(curl) == 0 &&
        fetch_landing_and_skus(curl, family, session, &landing, &skus, NULL,
                               NULL) == 0 &&
        classicsetup_windows_source_parse_catalog(
            family, landing.data, skus.data, catalog) == 0) {
        result = 0;
    }
    if (result != 0) {
        catalog->state = CLASSICSETUP_SOURCE_ERROR;
        (void)snprintf(catalog->error, sizeof(catalog->error), "%s",
                       "Official Microsoft source discovery failed.");
    }
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
    reset_response(&landing);
    reset_response(&skus);
    curl_global_cleanup();
    return result;
}

int classicsetup_microsoft_source_resolve_with_diagnostics(
    struct classicsetup_windows_release *release,
    struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    struct response_buffer landing = {0};
    struct response_buffer skus = {0};
    struct response_buffer links = {0};
    char session[37];
    char uri[1024];
    struct classicsetup_source_catalog current_catalog = {0};
    struct classicsetup_source_resolve_diagnostics parsed = {0};
    struct fetch_info landing_info = {0};
    struct fetch_info sku_info = {0};
    struct fetch_info links_info = {0};
    struct classicsetup_source_resolve_diagnostics local_diagnostics;
    const struct classicsetup_windows_release *current = NULL;
    const char *page;
    enum classicsetup_source_resolve_error body_error;
    enum fetch_result fetch_result;
    char edition[CLASSICSETUP_SOURCE_ID_SIZE];
    size_t index;
    CURL *curl;
    int written;
    int result = -1;

    if (diagnostics == NULL) {
        diagnostics = &local_diagnostics;
    }
    classicsetup_source_resolve_diagnostics_reset(diagnostics);
    if (release == NULL || release->product_edition_id[0] == '\0' ||
        release->sku_id[0] == '\0') {
        diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        maybe_report_diagnostics(diagnostics);
        return -1;
    }
    copy_request_context(diagnostics, release);
    if (!query_token_is_safe(release->product_edition_id) ||
        !query_token_is_safe(release->sku_id)) {
        diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        maybe_report_diagnostics(diagnostics);
        return -1;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR;
        maybe_report_diagnostics(diagnostics);
        return -1;
    }
    make_session_id(session);
    curl = curl_easy_init();
    page = landing_uri(release->family);
    if (curl == NULL || configure(curl) != 0) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LANDING;
            diagnostics->error =
                CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR;
        }
        goto done;
    }
    fetch_result = fetch(curl, page, page, &landing, &landing_info);
    if (fetch_result != FETCH_OK) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LANDING;
            set_fetch_error(diagnostics, fetch_result, &landing_info);
        }
        goto done;
    }
    set_response_kind_diagnostic(diagnostics, &landing, &landing_info);
    body_error = classify_success_body(&landing, &landing_info);
    if (body_error != CLASSICSETUP_SOURCE_RESOLVE_NONE) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LANDING;
            copy_fetch_diagnostics(diagnostics, &landing_info);
            diagnostics->error = body_error;
        }
        goto done;
    }
    if (extract_edition_id(landing.data, edition, sizeof(edition)) != 0) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LANDING;
            copy_fetch_diagnostics(diagnostics, &landing_info);
            diagnostics->error =
                CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED;
        }
        goto done;
    }
    if (!query_token_is_safe(edition)) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LANDING;
            copy_fetch_diagnostics(diagnostics, &landing_info);
            set_response_kind_diagnostic(diagnostics, &landing, &landing_info);
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        }
        goto done;
    }
    copy_safe_diagnostic_text(diagnostics->product_edition_id,
                              sizeof(diagnostics->product_edition_id),
                              edition);
    written = snprintf(
        uri, sizeof(uri),
        "https://www.microsoft.com/software-download-connector/api/"
        "getskuinformationbyproductedition?profile=606624d44113&"
        "ProductEditionId=%s&SKU=undefined&friendlyFileName=undefined&"
        "Locale=en-us&sessionID=%s",
        edition, session);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_SKU;
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        }
        goto done;
    }
    fetch_result = fetch(curl, uri, page, &skus, &sku_info);
    if (fetch_result != FETCH_OK) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_SKU;
            set_fetch_error(diagnostics, fetch_result, &sku_info);
        }
        goto done;
    }
    set_response_kind_diagnostic(diagnostics, &skus, &sku_info);
    body_error = classify_success_body(&skus, &sku_info);
    if (body_error != CLASSICSETUP_SOURCE_RESOLVE_NONE) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_SKU;
            copy_fetch_diagnostics(diagnostics, &sku_info);
            diagnostics->error = body_error;
        }
        goto done;
    }
    if (classicsetup_windows_source_parse_catalog(
            release->family, landing.data, skus.data,
            &current_catalog) != 0) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_SKU;
            copy_fetch_diagnostics(diagnostics, &sku_info);
            diagnostics->error =
                CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED;
        }
        goto done;
    }
    for (index = 0; index < current_catalog.release_count; ++index) {
        const struct classicsetup_windows_release *candidate =
            &current_catalog.releases[index];

        if (candidate->language == release->language &&
            candidate->architecture == release->architecture &&
            strcmp(candidate->release_name, release->release_name) == 0) {
            current = candidate;
            break;
        }
    }
    if (current == NULL) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_SKU;
            copy_fetch_diagnostics(diagnostics, &sku_info);
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_NO_LINK;
        }
        goto done;
    }
    if (!query_token_is_safe(current->product_edition_id) ||
        !query_token_is_safe(current->sku_id)) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LINKS;
            copy_fetch_diagnostics(diagnostics, &sku_info);
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        }
        goto done;
    }
    copy_safe_diagnostic_text(diagnostics->sku_id,
                              sizeof(diagnostics->sku_id), current->sku_id);
    written = snprintf(
        uri, sizeof(uri),
        "https://www.microsoft.com/software-download-connector/api/"
        "GetProductDownloadLinksBySku?profile=606624d44113&"
        "ProductEditionId=%s&SKU=%s&friendlyFileName=undefined&"
        "Locale=en-us&language=%s&sessionID=%s",
        current->product_edition_id, current->sku_id,
        language_token(current->language), session);
    if (written <= 0 || (size_t)written >= sizeof(uri)) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LINKS;
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_MALFORMED;
        }
        goto done;
    }
    fetch_result = fetch(curl, uri, page, &links, &links_info);
    if (fetch_result != FETCH_OK) {
        if (diagnostics != NULL) {
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LINKS;
            set_fetch_error(diagnostics, fetch_result, &links_info);
        }
        goto done;
    }
    set_response_kind_diagnostic(diagnostics, &links, &links_info);
    if (classicsetup_windows_source_parse_download_diagnostic(
            links.data, release, &parsed) == 0) {
        if (diagnostics != NULL) {
            copy_fetch_diagnostics(diagnostics, &links_info);
            diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LINKS;
            diagnostics->response_is_json = parsed.response_is_json;
            diagnostics->response_is_html = parsed.response_is_html;
            diagnostics->expected_field_present =
                parsed.expected_field_present;
            diagnostics->link_present = parsed.link_present;
            diagnostics->error = CLASSICSETUP_SOURCE_RESOLVE_NONE;
        }
        result = 0;
    } else if (diagnostics != NULL) {
        copy_fetch_diagnostics(diagnostics, &links_info);
        diagnostics->parser_stage = CLASSICSETUP_SOURCE_PARSER_LINKS;
        diagnostics->response_is_json = parsed.response_is_json;
        diagnostics->response_is_html = parsed.response_is_html;
        diagnostics->expected_field_present = parsed.expected_field_present;
        diagnostics->link_present = parsed.link_present;
        diagnostics->error = parsed.error;
    }
done:
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
    reset_response(&landing);
    reset_response(&skus);
    reset_response(&links);
    curl_global_cleanup();
    maybe_report_diagnostics(diagnostics);
    return result;
}

int classicsetup_microsoft_source_resolve(
    struct classicsetup_windows_release *release)
{
    struct classicsetup_source_resolve_diagnostics diagnostics;

    return classicsetup_microsoft_source_resolve_with_diagnostics(
        release, &diagnostics);
}
