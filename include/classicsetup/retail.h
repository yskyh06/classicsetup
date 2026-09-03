#ifndef CLASSICSETUP_RETAIL_H
#define CLASSICSETUP_RETAIL_H

#include <stdatomic.h>

#include "classicsetup/process.h"
#include "classicsetup/windows_source.h"
#include "classicsetup/workspace.h"

enum classicsetup_retail_stage {
    CLASSICSETUP_RETAIL_IDLE,
    CLASSICSETUP_RETAIL_CHECKING_SOURCE,
    CLASSICSETUP_RETAIL_DISCOVERING,
    CLASSICSETUP_RETAIL_RESOLVING_LINK,
    CLASSICSETUP_RETAIL_DOWNLOADING,
    CLASSICSETUP_RETAIL_VERIFYING_ISO,
    CLASSICSETUP_RETAIL_INSPECTING_IMAGE,
    CLASSICSETUP_RETAIL_COMPLETE,
    CLASSICSETUP_RETAIL_FAILED,
    CLASSICSETUP_RETAIL_CANCELLED
};

enum classicsetup_retail_error {
    CLASSICSETUP_RETAIL_ERROR_NONE,
    CLASSICSETUP_RETAIL_ERROR_PWSH_MISSING,
    CLASSICSETUP_RETAIL_ERROR_SCRIPT_MISSING,
    CLASSICSETUP_RETAIL_ERROR_SCRIPT_HASH,
    CLASSICSETUP_RETAIL_ERROR_PROCESS,
    CLASSICSETUP_RETAIL_ERROR_NO_LINK,
    CLASSICSETUP_RETAIL_ERROR_POLICY,
    CLASSICSETUP_RETAIL_ERROR_ISO,
    CLASSICSETUP_RETAIL_ERROR_IMAGE,
    CLASSICSETUP_RETAIL_ERROR_MISMATCH,
    CLASSICSETUP_RETAIL_ERROR_CANCELLED
};

struct classicsetup_retail_status {
    enum classicsetup_retail_stage stage;
    enum classicsetup_retail_error error;
    int child_exit_status;
    char delivery_host[128];
    char detail[CLASSICSETUP_SOURCE_ERROR_SIZE];
};

enum classicsetup_retail_inspection_stage {
    CLASSICSETUP_RETAIL_INSPECTION_NONE,
    CLASSICSETUP_RETAIL_INSPECTION_PREREQUISITES,
    CLASSICSETUP_RETAIL_INSPECTION_EXTRACT_WIM,
    CLASSICSETUP_RETAIL_INSPECTION_EXTRACT_ESD,
    CLASSICSETUP_RETAIL_INSPECTION_IMAGE_COUNT,
    CLASSICSETUP_RETAIL_INSPECTION_IMAGE_METADATA,
    CLASSICSETUP_RETAIL_INSPECTION_COMPLETE
};

struct classicsetup_retail_inspection_diagnostics {
    enum classicsetup_retail_inspection_stage stage;
    bool install_wim_found;
    bool install_esd_found;
    bool output_truncated;
    int extractor_exit_status;
    int wimlib_exit_status;
    long image_count;
    char architecture[32];
    char language[32];
};

typedef void (*classicsetup_retail_progress_callback)(
    const struct classicsetup_retail_status *status,
    void *context);

void classicsetup_retail_status_reset(
    struct classicsetup_retail_status *status);

const char *classicsetup_retail_error_message(
    enum classicsetup_retail_error error);

int classicsetup_retail_recommended_catalog(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog);

int classicsetup_retail_resolve_pwsh(
    const char *configured_path,
    char *resolved,
    size_t resolved_size);

int classicsetup_retail_validate_script(
    const char *path,
    const char *expected_sha256);

int classicsetup_retail_resolve_script(
    char *resolved,
    size_t resolved_size);

int classicsetup_retail_build_fido_argv(
    const char *pwsh,
    const char *script,
    const struct classicsetup_windows_release *release,
    char *arguments[],
    size_t argument_count);

int classicsetup_retail_resolve(
    struct classicsetup_windows_release *release,
    atomic_bool *cancel_requested,
    classicsetup_retail_progress_callback progress,
    void *progress_context,
    struct classicsetup_retail_status *status);

int classicsetup_retail_inspect_iso(
    const struct classicsetup_windows_release *release,
    struct classicsetup_workspace *workspace,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_verified_windows_source *source,
    struct classicsetup_process_result *result,
    struct classicsetup_retail_inspection_diagnostics *diagnostics);

int classicsetup_retail_inspect_iso_path(
    const struct classicsetup_windows_release *release,
    struct classicsetup_workspace *workspace,
    const char *iso_path,
    const char *verified_path,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_verified_windows_source *source,
    struct classicsetup_process_result *result,
    struct classicsetup_retail_inspection_diagnostics *diagnostics);

int classicsetup_retail_parse_wim_metadata(
    const struct classicsetup_windows_release *release,
    const char *wimlib_output,
    const char *iso_path,
    struct classicsetup_verified_windows_source *source);

#endif
