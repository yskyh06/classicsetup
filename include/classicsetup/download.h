#ifndef CLASSICSETUP_DOWNLOAD_H
#define CLASSICSETUP_DOWNLOAD_H

#include <stdatomic.h>

#include "classicsetup/windows_source.h"
#include "classicsetup/workspace.h"

enum classicsetup_download_state {
    CLASSICSETUP_DOWNLOAD_NOT_STARTED,
    CLASSICSETUP_DOWNLOAD_PREPARING,
    CLASSICSETUP_DOWNLOAD_DOWNLOADING,
    CLASSICSETUP_DOWNLOAD_VERIFYING,
    CLASSICSETUP_DOWNLOAD_COMPLETE,
    CLASSICSETUP_DOWNLOAD_CANCELLED,
    CLASSICSETUP_DOWNLOAD_FAILED
};

enum classicsetup_download_error {
    CLASSICSETUP_DOWNLOAD_ERROR_NONE,
    CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE,
    CLASSICSETUP_DOWNLOAD_ERROR_SOURCE,
    CLASSICSETUP_DOWNLOAD_ERROR_OUT_OF_SPACE,
    CLASSICSETUP_DOWNLOAD_ERROR_HTTP,
    CLASSICSETUP_DOWNLOAD_ERROR_WRITE,
    CLASSICSETUP_DOWNLOAD_ERROR_SIZE,
    CLASSICSETUP_DOWNLOAD_ERROR_HASH,
    CLASSICSETUP_DOWNLOAD_ERROR_ISO,
    CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED
};

struct classicsetup_download_status {
    enum classicsetup_download_state state;
    enum classicsetup_download_error error;
    unsigned long long bytes_received;
    unsigned long long total_bytes;
    double progress_fraction;
    double bytes_per_second;
    char message[CLASSICSETUP_SOURCE_ERROR_SIZE];
};

typedef void (*classicsetup_download_progress_callback)(
    const struct classicsetup_download_status *status,
    void *user_data);

void classicsetup_download_status_reset(
    struct classicsetup_download_status *status);

bool classicsetup_download_is_ready(
    const struct classicsetup_download_status *status,
    const struct classicsetup_workspace *workspace);

int classicsetup_verify_source_file(
    const char *path,
    unsigned long long expected_size,
    const char *expected_sha256,
    enum classicsetup_download_error *error);

int classicsetup_download_windows_iso(
    const struct classicsetup_windows_release *release,
    struct classicsetup_workspace *workspace,
    atomic_bool *cancel_requested,
    classicsetup_download_progress_callback progress,
    void *progress_data,
    struct classicsetup_download_status *status);

#endif
