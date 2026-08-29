#ifndef CLASSICSETUP_UUP_H
#define CLASSICSETUP_UUP_H

#include <stdbool.h>
#include <stddef.h>

#include "classicsetup/process.h"
#include "classicsetup/windows_source.h"
#include "classicsetup/workspace.h"

#define CLASSICSETUP_UUPMC_VERSION "3.1.9.3"
#define CLASSICSETUP_UUPMC_TAG "v3.1.9.3"
#define CLASSICSETUP_UUPMC_COMMIT \
    "e0c4ce00dc5415bb0441e599aa9a86a2f6021707"
#define CLASSICSETUP_UUPMC_LINUX_X64_SHA256 \
    "4a73e28321d893e4fed5f0e774702722995930e4864c6965e78e586d19803ce9"
#define CLASSICSETUP_UUP_WORKSPACE_RESERVE_BYTES \
    (24ULL * 1024ULL * 1024ULL * 1024ULL)

enum {
    CLASSICSETUP_UUP_TEXT_SIZE = 128,
    CLASSICSETUP_UUP_ARG_COUNT = 24,
    CLASSICSETUP_UUP_MIN_WIM_SIZE = 64,
    CLASSICSETUP_UUP_MIN_PAYLOAD_FILES = 8,
    CLASSICSETUP_UUP_MIN_PAYLOAD_BYTES = 64 * 1024 * 1024
};

enum classicsetup_uup_stage {
    CLASSICSETUP_UUP_IDLE,
    CLASSICSETUP_UUP_CHECKING_TOOL,
    CLASSICSETUP_UUP_SEARCHING,
    CLASSICSETUP_UUP_RESOLVING,
    CLASSICSETUP_UUP_DOWNLOADING,
    CLASSICSETUP_UUP_VERIFYING_PAYLOAD,
    CLASSICSETUP_UUP_BUILDING_IMAGE,
    CLASSICSETUP_UUP_VERIFYING_IMAGE,
    CLASSICSETUP_UUP_COMPLETE,
    CLASSICSETUP_UUP_CANCELLED,
    CLASSICSETUP_UUP_FAILED
};

enum classicsetup_uup_error {
    CLASSICSETUP_UUP_ERROR_NONE,
    CLASSICSETUP_UUP_ERROR_DISABLED,
    CLASSICSETUP_UUP_ERROR_TOOL_NOT_AVAILABLE,
    CLASSICSETUP_UUP_ERROR_RUNTIME_MISSING,
    CLASSICSETUP_UUP_ERROR_DISCOVERY_FAILED,
    CLASSICSETUP_UUP_ERROR_SCHEMA_CHANGED,
    CLASSICSETUP_UUP_ERROR_BUILD_NOT_FOUND,
    CLASSICSETUP_UUP_ERROR_LANGUAGE_NOT_AVAILABLE,
    CLASSICSETUP_UUP_ERROR_ARCH_NOT_AVAILABLE,
    CLASSICSETUP_UUP_ERROR_EDITION_NOT_AVAILABLE,
    CLASSICSETUP_UUP_ERROR_NETWORK_FAILED,
    CLASSICSETUP_UUP_ERROR_PAYLOAD_DOWNLOAD_FAILED,
    CLASSICSETUP_UUP_ERROR_PAYLOAD_VERIFY_FAILED,
    CLASSICSETUP_UUP_ERROR_CONVERSION_FAILED,
    CLASSICSETUP_UUP_ERROR_PAYLOAD_STRUCTURE,
    CLASSICSETUP_UUP_ERROR_ISO_VERIFY_FAILED,
    CLASSICSETUP_UUP_ERROR_ISO_EXTRACTOR_MISSING,
    CLASSICSETUP_UUP_ERROR_ISO_EXTRACT_FAILED,
    CLASSICSETUP_UUP_ERROR_WIM_NOT_FOUND,
    CLASSICSETUP_UUP_ERROR_WIM_VERIFY_FAILED,
    CLASSICSETUP_UUP_ERROR_OUT_OF_SPACE,
    CLASSICSETUP_UUP_ERROR_WORKSPACE_FAILED,
    CLASSICSETUP_UUP_ERROR_CANCELLED
};

enum classicsetup_uup_edition {
    CLASSICSETUP_UUP_EDITION_PROFESSIONAL
};

struct classicsetup_uup_tool_manifest {
    const char *version;
    const char *tag;
    const char *commit;
    const char *linux_x64_archive_sha256;
    const char *install_root;
    const char *converter_root;
    const char *download_executable;
    const char *converter_executable;
    const char *wimlib_executable;
    const char *iso_extractor_executable;
    const char *dotnet_root;
    bool converter_requires_iso;
    bool direct_wim_output;
};

struct classicsetup_uup_target {
    enum classicsetup_windows_family family;
    enum classicsetup_windows_language language;
    enum classicsetup_windows_architecture architecture;
    enum classicsetup_uup_edition edition;
    char reporting_version[CLASSICSETUP_UUP_TEXT_SIZE];
    char flight_ring[CLASSICSETUP_UUP_TEXT_SIZE];
    char flighting_branch[CLASSICSETUP_UUP_TEXT_SIZE];
    char current_branch[CLASSICSETUP_UUP_TEXT_SIZE];
    bool exact_version;
};

struct classicsetup_uup_release {
    char ring[CLASSICSETUP_UUP_TEXT_SIZE];
    char build[CLASSICSETUP_UUP_TEXT_SIZE];
};

struct classicsetup_uup_status {
    enum classicsetup_uup_stage stage;
    enum classicsetup_uup_error error;
    unsigned long long bytes_received;
    unsigned long long total_bytes;
    size_t files_completed;
    size_t total_files;
    unsigned long long workspace_available_bytes;
    unsigned long long workspace_required_bytes;
    int child_exit_status;
    char detail[CLASSICSETUP_UUP_TEXT_SIZE];
    char workspace_root[CLASSICSETUP_WORKSPACE_PATH_SIZE];
};

struct classicsetup_uup_payload_summary {
    size_t file_count;
    unsigned long long total_bytes;
    size_t zero_byte_count;
    size_t temporary_file_count;
    bool replay_metadata_present;
    bool requested_edition_metadata_present;
};

typedef void (*classicsetup_uup_progress_callback)(
    const struct classicsetup_uup_status *status,
    void *context);

const struct classicsetup_uup_tool_manifest *
classicsetup_uup_tool_manifest(void);

void classicsetup_uup_status_reset(struct classicsetup_uup_status *status);
const char *classicsetup_uup_error_message(enum classicsetup_uup_error error);
enum classicsetup_uup_error classicsetup_uup_classify_process_failure(
    const struct classicsetup_process_result *result,
    enum classicsetup_uup_error fallback);
const char *classicsetup_uup_architecture_token(
    enum classicsetup_windows_architecture architecture);
const char *classicsetup_uup_language_token(
    enum classicsetup_windows_language language);
const char *classicsetup_uup_edition_token(
    enum classicsetup_uup_edition edition);

bool classicsetup_uup_target_is_supported(
    const struct classicsetup_uup_target *target);
int classicsetup_uup_recommended_target(
    struct classicsetup_uup_target *target);
int classicsetup_uup_recommended_catalog(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog);
int classicsetup_uup_resolve_dotnet_root(
    const char *configured_root,
    const char *environment_root,
    char *resolved,
    size_t resolved_size);
int classicsetup_uup_validate_tool(const char *path);

int classicsetup_uup_build_discovery_argv(
    const char *executable,
    const struct classicsetup_uup_target *target,
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT]);
int classicsetup_uup_build_download_argv(
    const char *executable,
    const struct classicsetup_uup_target *target,
    const struct classicsetup_workspace *workspace,
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT]);
int classicsetup_uup_build_converter_argv(
    const char *executable,
    const struct classicsetup_uup_target *target,
    const struct classicsetup_workspace *workspace,
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT]);

int classicsetup_uup_parse_builds(
    const char *output,
    struct classicsetup_uup_release *releases,
    size_t capacity,
    size_t *count);

int classicsetup_uup_discover(
    const struct classicsetup_uup_target *target,
    struct classicsetup_uup_release *releases,
    size_t capacity,
    size_t *count,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_uup_status *status);

int classicsetup_uup_download_and_build_iso(
    const struct classicsetup_uup_target *target,
    struct classicsetup_workspace *workspace,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    classicsetup_uup_progress_callback progress_callback,
    void *progress_context,
    struct classicsetup_uup_status *status,
    struct classicsetup_verified_windows_source *verified_source);

int classicsetup_uup_inspect_payload(
    struct classicsetup_workspace *workspace,
    struct classicsetup_uup_payload_summary *summary);
int classicsetup_uup_extract_and_verify_image(
    const struct classicsetup_uup_target *target,
    struct classicsetup_workspace *workspace,
    const char *extractor_executable,
    const char *wimlib_executable,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_verified_windows_source *source,
    struct classicsetup_process_result *result);
int classicsetup_uup_parse_wim_info(
    const char *output,
    const char *iso_path,
    struct classicsetup_verified_windows_source *source);
int classicsetup_uup_verify_iso(const char *path);

int classicsetup_uup_run(
    const char *executable,
    char *const arguments[],
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_process_result *result);

int classicsetup_uup_verify_wim_signature(const char *path);
int classicsetup_uup_verify_wim(
    const char *path,
    const char *wimlib_executable,
    struct classicsetup_process_result *result);

int classicsetup_uup_register_verified_wim(
    const struct classicsetup_uup_target *target,
    const struct classicsetup_workspace *workspace,
    struct classicsetup_verified_windows_source *source);

#endif
