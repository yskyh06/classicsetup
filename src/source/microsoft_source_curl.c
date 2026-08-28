#include "classicsetup/windows_source.h"

#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { MAX_RESPONSE_BYTES = 4 * 1024 * 1024 };

struct response_buffer {
    char *data;
    size_t size;
};

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

static int fetch(CURL *curl, const char *uri, const char *referer,
                 struct response_buffer *buffer)
{
    long response_code = 0;
    CURLcode result;

    reset_response(buffer);
    if (curl_easy_setopt(curl, CURLOPT_URL, uri) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_REFERER, referer) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, append_response) !=
            CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer) != CURLE_OK) {
        return -1;
    }
    result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    return result == CURLE_OK && response_code >= 200 &&
           response_code < 300 && buffer->data != NULL ? 0 : -1;
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

static int fetch_landing_and_skus(
    CURL *curl, enum classicsetup_windows_family family,
    const char *session, struct response_buffer *landing,
    struct response_buffer *skus)
{
    char edition[CLASSICSETUP_SOURCE_ID_SIZE];
    char uri[1024];
    const char *page = landing_uri(family);
    int written;

    if (fetch(curl, page, page, landing) != 0 ||
        extract_edition_id(landing->data, edition, sizeof(edition)) != 0) {
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
               ? fetch(curl, uri, page, skus)
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
        fetch_landing_and_skus(curl, family, session, &landing, &skus) == 0 &&
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

int classicsetup_microsoft_source_resolve(
    struct classicsetup_windows_release *release)
{
    struct response_buffer landing = {0};
    struct response_buffer skus = {0};
    struct response_buffer links = {0};
    char session[37];
    char uri[1024];
    struct classicsetup_source_catalog current_catalog;
    const struct classicsetup_windows_release *current = NULL;
    size_t index;
    CURL *curl;
    int written;
    int result = -1;

    if (release == NULL || release->product_edition_id[0] == '\0' ||
        release->sku_id[0] == '\0') {
        return -1;
    }
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        return -1;
    }
    make_session_id(session);
    curl = curl_easy_init();
    if (curl == NULL || configure(curl) != 0 ||
        fetch_landing_and_skus(curl, release->family, session,
                               &landing, &skus) != 0) {
        goto done;
    }
    if (classicsetup_windows_source_parse_catalog(
            release->family, landing.data, skus.data,
            &current_catalog) != 0) {
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
        goto done;
    }
    written = snprintf(
        uri, sizeof(uri),
        "https://www.microsoft.com/software-download-connector/api/"
        "GetProductDownloadLinksBySku?profile=606624d44113&"
        "ProductEditionId=%s&SKU=%s&friendlyFileName=undefined&"
        "Locale=en-US&sessionID=%s",
        current->product_edition_id, current->sku_id, session);
    if (written > 0 && (size_t)written < sizeof(uri) &&
        fetch(curl, uri, landing_uri(release->family), &links) == 0 &&
        classicsetup_windows_source_parse_download(
            links.data, release) == 0) {
        result = 0;
    }
done:
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
    reset_response(&landing);
    reset_response(&skus);
    reset_response(&links);
    curl_global_cleanup();
    return result;
}
