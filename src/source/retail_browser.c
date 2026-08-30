#include "classicsetup/retail_browser.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool stage_is_terminal(enum classicsetup_retail_browser_stage stage)
{
    return stage == CLASSICSETUP_RETAIL_BROWSER_COMPLETE ||
           stage == CLASSICSETUP_RETAIL_BROWSER_FAILED ||
           stage == CLASSICSETUP_RETAIL_BROWSER_CANCELLED;
}

static bool transition_is_allowed(
    enum classicsetup_retail_browser_stage from,
    enum classicsetup_retail_browser_stage to)
{
    if (to == CLASSICSETUP_RETAIL_BROWSER_FAILED ||
        to == CLASSICSETUP_RETAIL_BROWSER_CANCELLED) {
        return !stage_is_terminal(from);
    }
    switch (from) {
    case CLASSICSETUP_RETAIL_BROWSER_IDLE:
        return to == CLASSICSETUP_RETAIL_BROWSER_PREPARING_MICROSOFT_PAGE;
    case CLASSICSETUP_RETAIL_BROWSER_PREPARING_MICROSOFT_PAGE:
        return to == CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT;
    case CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT:
        return to ==
               CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_USER_DOWNLOAD_CLICK;
    case CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_USER_DOWNLOAD_CLICK:
        return to == CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING;
    case CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING:
        return to == CLASSICSETUP_RETAIL_BROWSER_VERIFYING_ISO;
    case CLASSICSETUP_RETAIL_BROWSER_VERIFYING_ISO:
        return to == CLASSICSETUP_RETAIL_BROWSER_INSPECTING_IMAGE;
    case CLASSICSETUP_RETAIL_BROWSER_INSPECTING_IMAGE:
        return to == CLASSICSETUP_RETAIL_BROWSER_COMPLETE;
    case CLASSICSETUP_RETAIL_BROWSER_COMPLETE:
    case CLASSICSETUP_RETAIL_BROWSER_FAILED:
    case CLASSICSETUP_RETAIL_BROWSER_CANCELLED:
        return false;
    }
    return false;
}

static int uri_host(
    const char *uri,
    char *host,
    size_t host_size,
    const char **path)
{
    const char *begin;
    const char *end;
    size_t length;

    if (uri == NULL || host == NULL || host_size == 0 ||
        strncmp(uri, "https://", 8) != 0) {
        return -1;
    }
    begin = uri + 8;
    end = strpbrk(begin, "/?#");
    length = end != NULL ? (size_t)(end - begin) : strlen(begin);
    if (length == 0 || length >= host_size ||
        memchr(begin, '@', length) != NULL ||
        memchr(begin, ':', length) != NULL) {
        return -1;
    }
    memcpy(host, begin, length);
    host[length] = '\0';
    if (path != NULL) {
        *path = end != NULL && *end == '/' ? end : NULL;
    }
    return 0;
}

static bool host_is_microsoft_page(const char *host)
{
    return strcasecmp(host, "microsoft.com") == 0 ||
           strcasecmp(host, "www.microsoft.com") == 0;
}

static bool path_has_iso_suffix(const char *path)
{
    const char *end;
    const char *cursor;

    if (path == NULL) {
        return false;
    }
    end = strpbrk(path, "?#");
    if (end == NULL) {
        end = path + strlen(path);
    }
    if ((size_t)(end - path) < 4) {
        return false;
    }
    cursor = end - 4;
    return strncasecmp(cursor, ".iso", 4) == 0;
}

const char *classicsetup_retail_browser_page_uri(
    enum classicsetup_windows_language language)
{
    return language == CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH
               ? "https://www.microsoft.com/en-us/software-download/windows11"
               : "https://www.microsoft.com/ko-kr/software-download/windows11";
}

bool classicsetup_retail_browser_should_show_webview(
    const struct classicsetup_retail_browser_status *status)
{
    return status != NULL &&
           (status->full_page_fallback ||
            status->stage ==
                CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_USER_DOWNLOAD_CLICK);
}

void classicsetup_retail_browser_status_reset(
    struct classicsetup_retail_browser_status *status)
{
    if (status != NULL) {
        memset(status, 0, sizeof(*status));
        status->stage = CLASSICSETUP_RETAIL_BROWSER_IDLE;
    }
}

int classicsetup_retail_browser_transition(
    struct classicsetup_retail_browser_status *status,
    enum classicsetup_retail_browser_stage stage)
{
    if (status == NULL || !transition_is_allowed(status->stage, stage)) {
        return -1;
    }
    status->stage = stage;
    return 0;
}

void classicsetup_retail_browser_fallback_to_full_page(
    struct classicsetup_retail_browser_status *status)
{
    if (status == NULL || stage_is_terminal(status->stage)) {
        return;
    }
    status->full_page_fallback = true;
    status->stage = CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT;
}

bool classicsetup_retail_browser_navigation_is_allowed(const char *uri)
{
    char host[128];

    if (uri_host(uri, host, sizeof(host), NULL) != 0) {
        return false;
    }
    return host_is_microsoft_page(host) ||
           strcasecmp(host, "vlscppe.microsoft.com") == 0 ||
           strcasecmp(host, "ov-df.microsoft.com") == 0 ||
           strcasecmp(host, "software.download.prss.microsoft.com") == 0;
}

bool classicsetup_retail_browser_delivery_uri_is_allowed(const char *uri)
{
    char host[128];
    const char *path = NULL;

    return uri_host(uri, host, sizeof(host), &path) == 0 &&
           strcasecmp(host, "software.download.prss.microsoft.com") == 0 &&
           path_has_iso_suffix(path);
}

int classicsetup_retail_browser_capture_download(
    const char *uri,
    struct classicsetup_windows_release *release,
    struct classicsetup_retail_browser_status *status)
{
    char host[128];
    const char *path = NULL;

    if (release == NULL || status == NULL ||
        (status->stage !=
             CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_USER_DOWNLOAD_CLICK &&
         !(status->full_page_fallback &&
           status->stage ==
               CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT)) ||
        !classicsetup_retail_browser_delivery_uri_is_allowed(uri) ||
        uri_host(uri, host, sizeof(host), &path) != 0 || path == NULL ||
        strlen(uri) >= sizeof(release->download_uri)) {
        return -1;
    }
    (void)snprintf(release->download_uri,
                   sizeof(release->download_uri), "%s", uri);
    release->resolved = true;
    (void)snprintf(status->delivery_host,
                   sizeof(status->delivery_host), "%s", host);
    status->cancel_webkit_download = true;
    status->stage = CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING;
    return 0;
}

void classicsetup_retail_browser_clear_uri(
    struct classicsetup_windows_release *release)
{
    if (release != NULL) {
        memset(release->download_uri, 0, sizeof(release->download_uri));
        release->resolved = false;
    }
}
