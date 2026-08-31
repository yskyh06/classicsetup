#include "classicsetup/download.h"

#include <stdlib.h>
#include <string.h>

void classicsetup_download_status_reset(
    struct classicsetup_download_status *status)
{
    if (status == NULL) {
        return;
    }
    memset(status, 0, sizeof(*status));
    status->state = CLASSICSETUP_DOWNLOAD_NOT_STARTED;
}

bool classicsetup_download_is_ready(
    const struct classicsetup_download_status *status,
    const struct classicsetup_workspace *workspace)
{
    return status != NULL && workspace != NULL && workspace->valid &&
           workspace->verified_iso &&
           status->state == CLASSICSETUP_DOWNLOAD_COMPLETE &&
           status->error == CLASSICSETUP_DOWNLOAD_ERROR_NONE;
}

bool classicsetup_download_keep_failed_image_enabled(void)
{
    const char *value = getenv("CLASSICSETUP_KEEP_FAILED_IMAGE");

    return value != NULL && strcmp(value, "1") == 0;
}
