#ifndef CLASSICSETUP_RETAIL_BROWSER_H
#define CLASSICSETUP_RETAIL_BROWSER_H

#include <stdbool.h>

#include "classicsetup/windows_source.h"

enum classicsetup_retail_browser_stage {
    CLASSICSETUP_RETAIL_BROWSER_IDLE,
    CLASSICSETUP_RETAIL_BROWSER_PREPARING_MICROSOFT_PAGE,
    CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT,
    CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_USER_DOWNLOAD_CLICK,
    CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING,
    CLASSICSETUP_RETAIL_BROWSER_VERIFYING_ISO,
    CLASSICSETUP_RETAIL_BROWSER_INSPECTING_IMAGE,
    CLASSICSETUP_RETAIL_BROWSER_COMPLETE,
    CLASSICSETUP_RETAIL_BROWSER_FAILED,
    CLASSICSETUP_RETAIL_BROWSER_CANCELLED
};

struct classicsetup_retail_browser_status {
    enum classicsetup_retail_browser_stage stage;
    bool full_page_fallback;
    bool cancel_webkit_download;
    char delivery_host[128];
};

const char *classicsetup_retail_browser_page_uri(void);

void classicsetup_retail_browser_status_reset(
    struct classicsetup_retail_browser_status *status);

int classicsetup_retail_browser_transition(
    struct classicsetup_retail_browser_status *status,
    enum classicsetup_retail_browser_stage stage);

void classicsetup_retail_browser_fallback_to_full_page(
    struct classicsetup_retail_browser_status *status);

bool classicsetup_retail_browser_navigation_is_allowed(const char *uri);

bool classicsetup_retail_browser_delivery_uri_is_allowed(const char *uri);

int classicsetup_retail_browser_capture_download(
    const char *uri,
    struct classicsetup_windows_release *release,
    struct classicsetup_retail_browser_status *status);

void classicsetup_retail_browser_clear_uri(
    struct classicsetup_windows_release *release);

#endif
