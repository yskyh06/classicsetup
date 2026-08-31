#include "classicsetup/retail.h"

#include <string.h>

void classicsetup_retail_status_reset(struct classicsetup_retail_status *status)
{
    if (status != NULL) {
        memset(status, 0, sizeof(*status));
    }
}

const char *classicsetup_retail_error_message(enum classicsetup_retail_error error)
{
    (void)error;
    return "The Microsoft Retail source backend is unavailable.";
}

int classicsetup_retail_recommended_catalog(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog)
{
    (void)family;
    if (catalog != NULL) {
        classicsetup_source_catalog_reset(catalog);
        catalog->state = CLASSICSETUP_SOURCE_ERROR;
    }
    return -1;
}

int classicsetup_retail_resolve_pwsh(
    const char *configured_path, char *resolved, size_t resolved_size)
{
    (void)configured_path;
    (void)resolved;
    (void)resolved_size;
    return -1;
}

int classicsetup_retail_validate_script(
    const char *path, const char *expected_sha256)
{
    (void)path;
    (void)expected_sha256;
    return -1;
}

int classicsetup_retail_resolve_script(
    char *resolved, size_t resolved_size)
{
    (void)resolved;
    (void)resolved_size;
    return -1;
}

int classicsetup_retail_build_fido_argv(
    const char *pwsh, const char *script,
    const struct classicsetup_windows_release *release,
    char *arguments[], size_t argument_count)
{
    (void)pwsh;
    (void)script;
    (void)release;
    (void)arguments;
    (void)argument_count;
    return -1;
}

int classicsetup_retail_resolve(
    struct classicsetup_windows_release *release,
    atomic_bool *cancel_requested,
    classicsetup_retail_progress_callback progress,
    void *progress_context,
    struct classicsetup_retail_status *status)
{
    (void)release;
    (void)cancel_requested;
    (void)progress;
    (void)progress_context;
    classicsetup_retail_status_reset(status);
    return -1;
}

int classicsetup_retail_inspect_iso(
    const struct classicsetup_windows_release *release,
    struct classicsetup_workspace *workspace,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_verified_windows_source *source,
    struct classicsetup_process_result *result)
{
    (void)release;
    (void)workspace;
    (void)cancel_callback;
    (void)cancel_context;
    (void)source;
    (void)result;
    return -1;
}

int classicsetup_retail_parse_wim_metadata(
    const struct classicsetup_windows_release *release,
    const char *wimlib_output,
    const char *iso_path,
    struct classicsetup_verified_windows_source *source)
{
    (void)release;
    (void)wimlib_output;
    (void)iso_path;
    (void)source;
    return -1;
}
