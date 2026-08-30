#include "classicsetup/download.h"

#include <stdio.h>
#include <string.h>

int classicsetup_verify_source_file(
    const char *path, unsigned long long expected_size,
    const char *expected_sha256, enum classicsetup_download_error *error)
{
    (void)path;
    (void)expected_size;
    (void)expected_sha256;
    if (error != NULL) {
        *error = CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE;
    }
    return -1;
}

int classicsetup_download_windows_iso(
    const struct classicsetup_windows_release *release,
    struct classicsetup_workspace *workspace,
    atomic_bool *cancel_requested,
    classicsetup_download_progress_callback progress,
    void *progress_data,
    struct classicsetup_download_status *status,
    struct classicsetup_verified_windows_source *verified_source)
{
    (void)release;
    (void)workspace;
    (void)cancel_requested;
    if (verified_source != NULL) {
        memset(verified_source, 0, sizeof(*verified_source));
    }
    if (status != NULL) {
        classicsetup_download_status_reset(status);
        status->state = CLASSICSETUP_DOWNLOAD_FAILED;
        status->error = CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE;
        (void)snprintf(status->message, sizeof(status->message), "%s",
                       "Windows download support is unavailable.");
        if (progress != NULL) {
            progress(status, progress_data);
        }
    }
    return -1;
}
