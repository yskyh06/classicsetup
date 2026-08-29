#define _POSIX_C_SOURCE 200809L

#include "classicsetup/uup.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef CLASSICSETUP_ENABLE_UUP
#define CLASSICSETUP_ENABLE_UUP 1
#endif

#ifndef CLASSICSETUP_UUP_TOOL_ROOT
#define CLASSICSETUP_UUP_TOOL_ROOT \
    "/usr/lib/classicsetup/tools/uupmediacreator"
#endif

#ifndef CLASSICSETUP_WIMLIB_EXECUTABLE
#define CLASSICSETUP_WIMLIB_EXECUTABLE "/usr/bin/wimlib-imagex"
#endif

#ifndef CLASSICSETUP_UUP_CONVERTER_ROOT
#define CLASSICSETUP_UUP_CONVERTER_ROOT CLASSICSETUP_UUP_TOOL_ROOT
#endif

#ifndef CLASSICSETUP_UUP_DOTNET_ROOT
#define CLASSICSETUP_UUP_DOTNET_ROOT ""
#endif

#ifndef CLASSICSETUP_UUP_ISO_EXTRACTOR
#define CLASSICSETUP_UUP_ISO_EXTRACTOR "/usr/bin/7z"
#endif

static const struct classicsetup_uup_tool_manifest UUP_MANIFEST = {
    .version = CLASSICSETUP_UUPMC_VERSION,
    .tag = CLASSICSETUP_UUPMC_TAG,
    .commit = CLASSICSETUP_UUPMC_COMMIT,
    .linux_x64_archive_sha256 = CLASSICSETUP_UUPMC_LINUX_X64_SHA256,
    .install_root = CLASSICSETUP_UUP_TOOL_ROOT,
    .converter_root = CLASSICSETUP_UUP_CONVERTER_ROOT,
    .download_executable = CLASSICSETUP_UUP_TOOL_ROOT "/UUPDownload",
    .converter_executable =
        CLASSICSETUP_UUP_CONVERTER_ROOT "/UUPMediaConverter",
    .wimlib_executable = CLASSICSETUP_WIMLIB_EXECUTABLE,
    .iso_extractor_executable = CLASSICSETUP_UUP_ISO_EXTRACTOR,
    .dotnet_root = CLASSICSETUP_UUP_DOTNET_ROOT,
    .converter_requires_iso = true,
    .direct_wim_output = false
};

const struct classicsetup_uup_tool_manifest *
classicsetup_uup_tool_manifest(void)
{
    return &UUP_MANIFEST;
}

void classicsetup_uup_status_reset(struct classicsetup_uup_status *status)
{
    if (status != NULL) {
        memset(status, 0, sizeof(*status));
        status->stage = CLASSICSETUP_UUP_IDLE;
    }
}

const char *classicsetup_uup_error_message(enum classicsetup_uup_error error)
{
    static const char *const messages[] = {
        "No error.",
        "The Microsoft UUP backend is disabled.",
        "The bundled UUP tool is not available.",
        "The bundled UUP runtime is not available.",
        "Windows build discovery failed.",
        "The bundled tool output format changed.",
        "The requested Windows build was not found.",
        "The requested Windows language is unavailable.",
        "The requested architecture is unavailable.",
        "The requested edition is unavailable.",
        "The Microsoft UUP service could not be reached.",
        "Windows component download failed.",
        "Downloaded Windows components could not be verified.",
        "Windows image conversion failed.",
        "The downloaded Windows component set is incomplete.",
        "The generated Windows ISO could not be verified.",
        "The ISO extraction tool is not available.",
        "The Windows image could not be read from the generated ISO.",
        "The generated install.wim was not found.",
        "The generated install.wim could not be verified.",
        "There is not enough temporary storage.",
        "The temporary workspace could not be created safely.",
        "The UUP operation was cancelled."
    };

    if ((size_t)error >= sizeof(messages) / sizeof(messages[0])) {
        return "Unknown UUP error.";
    }
    return messages[error];
}

enum classicsetup_uup_error classicsetup_uup_classify_process_failure(
    const struct classicsetup_process_result *result,
    enum classicsetup_uup_error fallback)
{
    if (result != NULL &&
        (strstr(result->output, "You must install .NET") != NULL ||
         strstr(result->output, ".NET location: Not found") != NULL ||
         strstr(result->output, "libhostfxr") != NULL)) {
        return CLASSICSETUP_UUP_ERROR_RUNTIME_MISSING;
    }
    return fallback;
}

const char *classicsetup_uup_architecture_token(
    enum classicsetup_windows_architecture architecture)
{
    switch (architecture) {
    case CLASSICSETUP_ARCH_X64:
        return "amd64";
    case CLASSICSETUP_ARCH_X86:
        return "x86";
    case CLASSICSETUP_ARCH_ARM64:
        return "arm64";
    }
    return NULL;
}

const char *classicsetup_uup_language_token(
    enum classicsetup_windows_language language)
{
    switch (language) {
    case CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN:
        return "ko-KR";
    case CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH:
        return "en-US";
    }
    return NULL;
}

const char *classicsetup_uup_edition_token(
    enum classicsetup_uup_edition edition)
{
    return edition == CLASSICSETUP_UUP_EDITION_PROFESSIONAL
               ? "Professional"
               : NULL;
}

static bool safe_token(const char *value)
{
    const unsigned char *cursor = (const unsigned char *)value;

    if (value == NULL || value[0] == '\0') {
        return false;
    }
    while (*cursor != '\0') {
        if (!isalnum(*cursor) && *cursor != '.' && *cursor != '_' &&
            *cursor != '-') {
            return false;
        }
        ++cursor;
    }
    return true;
}

bool classicsetup_uup_target_is_supported(
    const struct classicsetup_uup_target *target)
{
    return target != NULL && target->family == CLASSICSETUP_WINDOWS_11 &&
           target->language == CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN &&
           target->architecture == CLASSICSETUP_ARCH_X64 &&
           target->edition == CLASSICSETUP_UUP_EDITION_PROFESSIONAL &&
           safe_token(target->reporting_version) &&
           safe_token(target->flight_ring) &&
           safe_token(target->flighting_branch) &&
           safe_token(target->current_branch);
}

int classicsetup_uup_recommended_target(
    struct classicsetup_uup_target *target)
{
    if (target == NULL) {
        return -1;
    }
    memset(target, 0, sizeof(*target));
    target->family = CLASSICSETUP_WINDOWS_11;
    target->language = CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN;
    target->architecture = CLASSICSETUP_ARCH_X64;
    target->edition = CLASSICSETUP_UUP_EDITION_PROFESSIONAL;
    (void)snprintf(target->reporting_version,
                   sizeof(target->reporting_version), "%s",
                   "10.0.22631.1");
    (void)snprintf(target->flight_ring, sizeof(target->flight_ring),
                   "%s", "Retail");
    (void)snprintf(target->flighting_branch,
                   sizeof(target->flighting_branch), "%s", "Retail");
    (void)snprintf(target->current_branch,
                   sizeof(target->current_branch), "%s", "ni_release");
    target->exact_version = false;
    return 0;
}

int classicsetup_uup_recommended_catalog(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog)
{
#if CLASSICSETUP_ENABLE_UUP
    struct classicsetup_windows_release *release;
#endif

    if (catalog == NULL) {
        return -1;
    }
    classicsetup_source_catalog_reset(catalog);
#if !CLASSICSETUP_ENABLE_UUP
    (void)family;
    catalog->state = CLASSICSETUP_SOURCE_ERROR;
    (void)snprintf(catalog->error, sizeof(catalog->error), "%s",
                   "Automatic Windows acquisition is not available in this build.");
    return -1;
#else
    if (family != CLASSICSETUP_WINDOWS_11) {
        catalog->state = CLASSICSETUP_SOURCE_ERROR;
        (void)snprintf(catalog->error, sizeof(catalog->error), "%s",
                       "The validated automatic source currently supports Windows 11 only.");
        return -1;
    }
    release = &catalog->releases[0];
    release->family = CLASSICSETUP_WINDOWS_11;
    release->language = CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN;
    release->architecture = CLASSICSETUP_ARCH_X64;
    release->edition = CLASSICSETUP_WINDOWS_EDITION_PROFESSIONAL;
    (void)snprintf(release->release_name,
                   sizeof(release->release_name), "%s",
                   "Validated stable release");
    (void)snprintf(release->language_name,
                   sizeof(release->language_name), "%s", "Korean");
    (void)snprintf(release->architecture_token,
                   sizeof(release->architecture_token), "%s", "amd64");
    (void)snprintf(release->edition_name,
                   sizeof(release->edition_name), "%s", "Windows 11 Pro");
    catalog->release_count = 1;
    catalog->state = CLASSICSETUP_SOURCE_READY;
    return 0;
#endif
}

int classicsetup_uup_resolve_dotnet_root(
    const char *configured_root,
    const char *environment_root,
    char *resolved,
    size_t resolved_size)
{
    const char *candidate = NULL;
    struct stat info;

    if (resolved == NULL || resolved_size == 0) {
        return -1;
    }
    resolved[0] = '\0';
    if (configured_root != NULL && configured_root[0] != '\0') {
        candidate = configured_root;
    } else if (environment_root != NULL && environment_root[0] != '\0') {
        candidate = environment_root;
    } else {
        return 0;
    }
    if (candidate[0] != '/' || lstat(candidate, &info) != 0 ||
        !S_ISDIR(info.st_mode) || strlen(candidate) >= resolved_size) {
        return -1;
    }
    memcpy(resolved, candidate, strlen(candidate) + 1);
    return 0;
}

int classicsetup_uup_validate_tool(const char *path)
{
    struct stat info;

    return path != NULL && path[0] == '/' && lstat(path, &info) == 0 &&
           S_ISREG(info.st_mode) && access(path, X_OK) == 0 ? 0 : -1;
}

static int append_argument(char *arguments[CLASSICSETUP_UUP_ARG_COUNT],
                           size_t *count, const char *value)
{
    if (value == NULL || *count + 1 >= CLASSICSETUP_UUP_ARG_COUNT) {
        return -1;
    }
    arguments[(*count)++] = (char *)value;
    arguments[*count] = NULL;
    return 0;
}

int classicsetup_uup_build_discovery_argv(
    const char *executable,
    const struct classicsetup_uup_target *target,
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT])
{
    size_t count = 0;

    if (arguments == NULL || !classicsetup_uup_target_is_supported(target)) {
        return -1;
    }
    memset(arguments, 0, sizeof(char *) * CLASSICSETUP_UUP_ARG_COUNT);
    return append_argument(arguments, &count, executable) == 0 &&
                   append_argument(arguments, &count, "get-builds") == 0 &&
                   append_argument(arguments, &count, "-s") == 0 &&
                   append_argument(arguments, &count,
                                   classicsetup_uup_edition_token(
                                       target->edition)) == 0 &&
                   append_argument(arguments, &count, "-t") == 0 &&
                   append_argument(arguments, &count,
                                   classicsetup_uup_architecture_token(
                                       target->architecture)) == 0
               ? 0
               : -1;
}

int classicsetup_uup_build_download_argv(
    const char *executable,
    const struct classicsetup_uup_target *target,
    const struct classicsetup_workspace *workspace,
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT])
{
    size_t count = 0;

    if (arguments == NULL || workspace == NULL || !workspace->valid ||
        !classicsetup_uup_target_is_supported(target)) {
        return -1;
    }
    memset(arguments, 0, sizeof(char *) * CLASSICSETUP_UUP_ARG_COUNT);
#define ADD(value) \
    do { if (append_argument(arguments, &count, (value)) != 0) return -1; } while (0)
    ADD(executable);
    ADD("request-download");
    ADD("-s"); ADD(classicsetup_uup_edition_token(target->edition));
    ADD("-v"); ADD(target->reporting_version);
    ADD("-t"); ADD(classicsetup_uup_architecture_token(target->architecture));
    ADD("-r"); ADD(target->flight_ring);
    ADD("-b"); ADD(target->flighting_branch);
    ADD("-c"); ADD(target->current_branch);
    ADD("-o"); ADD(workspace->uup_path);
    ADD("-e"); ADD(classicsetup_uup_edition_token(target->edition));
    ADD("-l"); ADD(classicsetup_uup_language_token(target->language));
    if (target->exact_version) {
        ADD("-y");
    }
#undef ADD
    return 0;
}

int classicsetup_uup_build_converter_argv(
    const char *executable,
    const struct classicsetup_uup_target *target,
    const struct classicsetup_workspace *workspace,
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT])
{
    size_t count = 0;

    if (arguments == NULL || workspace == NULL || !workspace->valid ||
        workspace->uup_payload_path[0] == '\0' ||
        !classicsetup_uup_target_is_supported(target)) {
        return -1;
    }
    memset(arguments, 0, sizeof(char *) * CLASSICSETUP_UUP_ARG_COUNT);
#define ADD(value) \
    do { if (append_argument(arguments, &count, (value)) != 0) return -1; } while (0)
    ADD(executable);
    ADD("desktop-convert");
    ADD("-u"); ADD(workspace->uup_payload_path);
    ADD("-i"); ADD(workspace->iso_partial_path);
    ADD("-l"); ADD(classicsetup_uup_language_token(target->language));
    ADD("-e"); ADD(classicsetup_uup_edition_token(target->edition));
    ADD("-t"); ADD(workspace->image_path);
#undef ADD
    return 0;
}

static int copy_field(const char *begin, const char *end,
                      char *target, size_t target_size)
{
    size_t length = (size_t)(end - begin);

    if (length == 0 || length >= target_size) {
        return -1;
    }
    memcpy(target, begin, length);
    target[length] = '\0';
    return 0;
}

int classicsetup_uup_parse_builds(
    const char *output,
    struct classicsetup_uup_release *releases,
    size_t capacity,
    size_t *count)
{
    const char *line;
    size_t used = 0;

    if (output == NULL || releases == NULL || capacity == 0 || count == NULL) {
        return -1;
    }
    line = output;
    while (*line != '\0') {
        const char *line_end = strchr(line, '\n');
        const char *ring_end;
        const char *build_begin;
        const char *build_end;

        if (line_end == NULL) {
            line_end = line + strlen(line);
        }
        if (*line == '"' && (ring_end = strstr(line + 1, "\"[")) != NULL &&
            ring_end < line_end &&
            (build_begin = strstr(ring_end, "]=")) != NULL &&
            build_begin + 3 < line_end && build_begin[2] == '"' &&
            (build_end = memchr(build_begin + 3, '"',
                                (size_t)(line_end - (build_begin + 3)))) != NULL) {
            if (used >= capacity ||
                copy_field(line + 1, ring_end, releases[used].ring,
                           sizeof(releases[used].ring)) != 0 ||
                copy_field(build_begin + 3, build_end,
                           releases[used].build,
                           sizeof(releases[used].build)) != 0) {
                return -1;
            }
            ++used;
        }
        line = *line_end == '\0' ? line_end : line_end + 1;
    }
    *count = used;
    return used > 0 ? 0 : -1;
}

static void report_status(struct classicsetup_uup_status *status,
                          enum classicsetup_uup_stage stage,
                          enum classicsetup_uup_error error,
                          const char *detail,
                          classicsetup_uup_progress_callback callback,
                          void *context)
{
    if (status == NULL) {
        return;
    }
    status->stage = stage;
    status->error = error;
    (void)snprintf(status->detail, sizeof(status->detail), "%s",
                   detail != NULL ? detail : "");
    if (callback != NULL) {
        callback(status, context);
    }
}

static bool ends_with_case(const char *text, const char *suffix)
{
    size_t text_length;
    size_t suffix_length;

    if (text == NULL || suffix == NULL) {
        return false;
    }
    text_length = strlen(text);
    suffix_length = strlen(suffix);
    return text_length >= suffix_length &&
           strcasecmp(text + text_length - suffix_length, suffix) == 0;
}

static bool contains_case(const char *text, const char *needle)
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

static int inspect_payload_tree(
    const char *path,
    struct classicsetup_uup_payload_summary *summary)
{
    DIR *directory;
    struct dirent *entry;
    int failed = 0;

    directory = opendir(path);
    if (directory == NULL) {
        return -1;
    }
    while ((entry = readdir(directory)) != NULL) {
        char child[CLASSICSETUP_WORKSPACE_PATH_SIZE];
        struct stat info;
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        written = snprintf(child, sizeof(child), "%s/%s", path,
                           entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(child) ||
            lstat(child, &info) != 0 || S_ISLNK(info.st_mode)) {
            failed = 1;
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            if (inspect_payload_tree(child, summary) != 0) {
                failed = 1;
            }
            continue;
        }
        if (!S_ISREG(info.st_mode) || info.st_size < 0) {
            failed = 1;
            continue;
        }
        ++summary->file_count;
        summary->total_bytes += (unsigned long long)info.st_size;
        if (info.st_size == 0) {
            ++summary->zero_byte_count;
        }
        if (ends_with_case(entry->d_name, ".part") ||
            ends_with_case(entry->d_name, ".tmp")) {
            ++summary->temporary_file_count;
        }
        if (ends_with_case(entry->d_name, ".uupmcreplay")) {
            summary->replay_metadata_present = true;
        }
        if (strcasecmp(entry->d_name, "professional_ko-kr.esd") == 0 ||
            (contains_case(child, "editionpackages") &&
             contains_case(entry->d_name, "Professional") &&
             ends_with_case(entry->d_name, ".ESD"))) {
            summary->requested_edition_metadata_present = true;
        }
    }
    if (closedir(directory) != 0) {
        failed = 1;
    }
    return failed ? -1 : 0;
}

static int locate_payload_root(struct classicsetup_workspace *workspace)
{
    DIR *directory;
    struct dirent *entry;
    size_t candidates = 0;

    directory = opendir(workspace->uup_path);
    if (directory == NULL) {
        return -1;
    }
    while ((entry = readdir(directory)) != NULL) {
        char child[CLASSICSETUP_WORKSPACE_PATH_SIZE];
        DIR *payload_directory;
        struct dirent *payload_entry;
        struct stat info;
        bool has_metadata = false;
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        written = snprintf(child, sizeof(child), "%s/%s",
                           workspace->uup_path, entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(child) ||
            lstat(child, &info) != 0 || !S_ISDIR(info.st_mode)) {
            continue;
        }
        payload_directory = opendir(child);
        if (payload_directory == NULL) {
            continue;
        }
        while ((payload_entry = readdir(payload_directory)) != NULL) {
            char metadata[CLASSICSETUP_WORKSPACE_PATH_SIZE];

            if (!ends_with_case(payload_entry->d_name,
                                "AggregatedMetadata.cab")) {
                continue;
            }
            written = snprintf(metadata, sizeof(metadata), "%s/%s", child,
                               payload_entry->d_name);
            if (written > 0 && (size_t)written < sizeof(metadata) &&
                lstat(metadata, &info) == 0 && S_ISREG(info.st_mode)) {
                has_metadata = true;
                break;
            }
        }
        (void)closedir(payload_directory);
        if (!has_metadata) {
            continue;
        }
        if (++candidates == 1) {
            (void)snprintf(workspace->uup_payload_path,
                           sizeof(workspace->uup_payload_path), "%s",
                           child);
        }
    }
    (void)closedir(directory);
    if (candidates != 1) {
        workspace->uup_payload_path[0] = '\0';
        return -1;
    }
    return 0;
}

int classicsetup_uup_inspect_payload(
    struct classicsetup_workspace *workspace,
    struct classicsetup_uup_payload_summary *summary)
{
    if (workspace == NULL || !workspace->valid || summary == NULL) {
        return -1;
    }
    memset(summary, 0, sizeof(*summary));
    if (inspect_payload_tree(workspace->uup_path, summary) != 0 ||
        locate_payload_root(workspace) != 0 ||
        summary->file_count < CLASSICSETUP_UUP_MIN_PAYLOAD_FILES ||
        summary->total_bytes < CLASSICSETUP_UUP_MIN_PAYLOAD_BYTES ||
        summary->zero_byte_count != 0 ||
        summary->temporary_file_count != 0 ||
        !summary->replay_metadata_present ||
        !summary->requested_edition_metadata_present) {
        return -1;
    }
    return 0;
}

int classicsetup_uup_discover(
    const struct classicsetup_uup_target *target,
    struct classicsetup_uup_release *releases,
    size_t capacity,
    size_t *count,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_uup_status *status)
{
    const struct classicsetup_uup_tool_manifest *manifest =
        classicsetup_uup_tool_manifest();
    struct classicsetup_process_result result;
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT];
    int run_result;

    classicsetup_uup_status_reset(status);
    if (status == NULL || releases == NULL || count == NULL ||
        classicsetup_uup_build_discovery_argv(
            manifest->download_executable, target, arguments) != 0 ||
        classicsetup_uup_validate_tool(manifest->download_executable) != 0) {
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      CLASSICSETUP_UUP_ERROR_TOOL_NOT_AVAILABLE,
                      "Pinned UUPDownload is unavailable.", NULL, NULL);
        return -1;
    }
    status->stage = CLASSICSETUP_UUP_SEARCHING;
    run_result = classicsetup_uup_run(
        manifest->download_executable, arguments, cancel_callback,
        cancel_context, &result);
    if (run_result == 1) {
        report_status(status, CLASSICSETUP_UUP_CANCELLED,
                      CLASSICSETUP_UUP_ERROR_CANCELLED,
                      "Build discovery was cancelled.", NULL, NULL);
        return -1;
    }
    if (run_result != 0 || !result.exited || result.exit_status != 0) {
        status->child_exit_status = result.exited ? result.exit_status : -1;
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      classicsetup_uup_classify_process_failure(
                          &result,
                          CLASSICSETUP_UUP_ERROR_DISCOVERY_FAILED),
                      "Microsoft UUP build discovery failed.", NULL, NULL);
        return -1;
    }
    if (classicsetup_uup_parse_builds(
            result.output, releases, capacity, count) != 0) {
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      CLASSICSETUP_UUP_ERROR_SCHEMA_CHANGED,
                      "UUPDownload build output was not recognized.",
                      NULL, NULL);
        return -1;
    }
    report_status(status, CLASSICSETUP_UUP_COMPLETE,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Microsoft UUP builds discovered.", NULL, NULL);
    return 0;
}

static int iso_sanity(const char *path)
{
    unsigned char descriptor[2048];
    struct stat info;
    FILE *file;
    bool primary = false;
    bool bootable = false;
    size_t index;

    if (path == NULL || lstat(path, &info) != 0 ||
        !S_ISREG(info.st_mode) || info.st_size < 32774) {
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL || fseeko(file, 32768, SEEK_SET) != 0) {
        if (file != NULL) {
            (void)fclose(file);
        }
        return -1;
    }
    for (index = 0; index < 32; ++index) {
        if (fread(descriptor, 1, sizeof(descriptor), file) !=
            sizeof(descriptor) || memcmp(descriptor + 1, "CD001", 5) != 0) {
            break;
        }
        if (descriptor[0] == 1) {
            primary = true;
        } else if (descriptor[0] == 0 &&
                   memcmp(descriptor + 7,
                          "EL TORITO SPECIFICATION", 23) == 0) {
            bootable = true;
        } else if (descriptor[0] == 255) {
            break;
        }
    }
    (void)fclose(file);
    return primary && bootable ? 0 : -1;
}

int classicsetup_uup_verify_iso(const char *path)
{
    return iso_sanity(path);
}

int classicsetup_uup_download_and_build_iso(
    const struct classicsetup_uup_target *target,
    struct classicsetup_workspace *workspace,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    classicsetup_uup_progress_callback progress_callback,
    void *progress_context,
    struct classicsetup_uup_status *status,
    struct classicsetup_verified_windows_source *verified_source)
{
    const struct classicsetup_uup_tool_manifest *manifest =
        classicsetup_uup_tool_manifest();
    struct classicsetup_process_result result;
    struct classicsetup_uup_payload_summary payload;
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT];
    int run_result;

    classicsetup_uup_status_reset(status);
    if (status == NULL || workspace == NULL || !workspace->valid ||
        verified_source == NULL ||
        !classicsetup_uup_target_is_supported(target)) {
        return -1;
    }
    report_status(status, CLASSICSETUP_UUP_CHECKING_TOOL,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Checking pinned UUP tools.", progress_callback,
                  progress_context);
    if (classicsetup_uup_validate_tool(manifest->download_executable) != 0 ||
        classicsetup_uup_validate_tool(manifest->converter_executable) != 0 ||
        classicsetup_uup_validate_tool(manifest->wimlib_executable) != 0 ||
        classicsetup_uup_validate_tool(
            manifest->iso_extractor_executable) != 0) {
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      classicsetup_uup_validate_tool(
                          manifest->iso_extractor_executable) != 0
                          ? CLASSICSETUP_UUP_ERROR_ISO_EXTRACTOR_MISSING
                          : CLASSICSETUP_UUP_ERROR_TOOL_NOT_AVAILABLE,
                      "A required Windows source runtime tool is unavailable.",
                      progress_callback, progress_context);
        return -1;
    }
    status->workspace_required_bytes =
        CLASSICSETUP_UUP_WORKSPACE_RESERVE_BYTES;
    (void)snprintf(status->workspace_root,
                   sizeof(status->workspace_root), "%s",
                   workspace->root_path);
    if (classicsetup_workspace_available_bytes(
            workspace, &status->workspace_available_bytes) != 0 ||
        status->workspace_available_bytes <
            CLASSICSETUP_UUP_WORKSPACE_RESERVE_BYTES) {
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      CLASSICSETUP_UUP_ERROR_OUT_OF_SPACE,
                      "There is not enough temporary storage.",
                      progress_callback, progress_context);
        return -1;
    }
    if (classicsetup_uup_build_download_argv(
            manifest->download_executable, target, workspace,
            arguments) != 0) {
        return -1;
    }
    report_status(status, CLASSICSETUP_UUP_SEARCHING,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Searching Microsoft Windows Update.", progress_callback,
                  progress_context);
    report_status(status, CLASSICSETUP_UUP_RESOLVING,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Resolving a validated Microsoft Retail offer.",
                  progress_callback, progress_context);
    report_status(status, CLASSICSETUP_UUP_DOWNLOADING,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Downloading Microsoft UUP components.", progress_callback,
                  progress_context);
    run_result = classicsetup_uup_run(
        manifest->download_executable, arguments, cancel_callback,
        cancel_context, &result);
    if (run_result == 1) {
        classicsetup_workspace_cleanup_cancel(workspace);
        report_status(status, CLASSICSETUP_UUP_CANCELLED,
                      CLASSICSETUP_UUP_ERROR_CANCELLED,
                      "UUP download was cancelled.", progress_callback,
                      progress_context);
        return -1;
    }
    if (run_result != 0 || !result.exited || result.exit_status != 0 ||
        strstr(result.output, "Completed.") == NULL) {
        status->child_exit_status = result.exited ? result.exit_status : -1;
        classicsetup_workspace_cleanup_failure(workspace);
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      classicsetup_uup_classify_process_failure(
                          &result,
                          CLASSICSETUP_UUP_ERROR_PAYLOAD_DOWNLOAD_FAILED),
                      "Microsoft UUP component download failed.",
                      progress_callback, progress_context);
        return -1;
    }
    report_status(status, CLASSICSETUP_UUP_VERIFYING_PAYLOAD,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Verifying downloaded Windows files.",
                  progress_callback, progress_context);
    if (classicsetup_uup_inspect_payload(workspace, &payload) != 0) {
        classicsetup_workspace_cleanup_failure(workspace);
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      CLASSICSETUP_UUP_ERROR_PAYLOAD_STRUCTURE,
                      "Downloaded Windows files are incomplete.",
                      progress_callback, progress_context);
        return -1;
    }
    status->files_completed = payload.file_count;
    status->total_files = payload.file_count;
    status->bytes_received = payload.total_bytes;
    status->total_bytes = payload.total_bytes;
    if (progress_callback != NULL) {
        progress_callback(status, progress_context);
    }
    if (classicsetup_uup_build_converter_argv(
            manifest->converter_executable, target, workspace,
            arguments) != 0) {
        return -1;
    }
    report_status(status, CLASSICSETUP_UUP_BUILDING_IMAGE,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Preparing the Windows installation image.",
                  progress_callback, progress_context);
    run_result = classicsetup_uup_run(
        manifest->converter_executable, arguments, cancel_callback,
        cancel_context, &result);
    if (run_result == 1) {
        classicsetup_workspace_cleanup_cancel(workspace);
        report_status(status, CLASSICSETUP_UUP_CANCELLED,
                      CLASSICSETUP_UUP_ERROR_CANCELLED,
                      "Image conversion was cancelled.", progress_callback,
                      progress_context);
        return -1;
    }
    if (run_result != 0 || !result.exited || result.exit_status != 0 ||
        strstr(result.output, "[Done]") == NULL) {
        status->child_exit_status = result.exited ? result.exit_status : -1;
        classicsetup_workspace_cleanup_failure(workspace);
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      classicsetup_uup_classify_process_failure(
                          &result,
                          CLASSICSETUP_UUP_ERROR_CONVERSION_FAILED),
                      "UUP image conversion failed.", progress_callback,
                      progress_context);
        return -1;
    }
    report_status(status, CLASSICSETUP_UUP_VERIFYING_IMAGE,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "Checking generated ISO structure.", progress_callback,
                  progress_context);
    if (classicsetup_uup_verify_iso(workspace->iso_partial_path) != 0) {
        classicsetup_workspace_cleanup_failure(workspace);
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      CLASSICSETUP_UUP_ERROR_ISO_VERIFY_FAILED,
                      "Generated ISO failed the basic structure check.",
                      progress_callback, progress_context);
        return -1;
    }
    run_result = classicsetup_uup_extract_and_verify_image(
        target, workspace, manifest->iso_extractor_executable,
        manifest->wimlib_executable, cancel_callback, cancel_context,
        verified_source, &result);
    if (run_result != 0) {
        classicsetup_workspace_cleanup_failure(workspace);
        report_status(status,
                      run_result == 1 ? CLASSICSETUP_UUP_CANCELLED
                                      : CLASSICSETUP_UUP_FAILED,
                      run_result == 1
                          ? CLASSICSETUP_UUP_ERROR_CANCELLED
                          : CLASSICSETUP_UUP_ERROR_WIM_VERIFY_FAILED,
                      run_result == 1
                          ? "Windows image verification was cancelled."
                          : "The generated Windows image could not be verified.",
                      progress_callback, progress_context);
        return -1;
    }
    if (classicsetup_workspace_promote_verified_iso(workspace) != 0) {
        classicsetup_workspace_cleanup_failure(workspace);
        report_status(status, CLASSICSETUP_UUP_FAILED,
                      CLASSICSETUP_UUP_ERROR_ISO_VERIFY_FAILED,
                      "The verified Windows ISO could not be retained.",
                      progress_callback, progress_context);
        return -1;
    }
    (void)snprintf(verified_source->path,
                   sizeof(verified_source->path), "%s",
                   workspace->iso_final_path);
    classicsetup_workspace_cleanup_success(workspace);
    report_status(status, CLASSICSETUP_UUP_COMPLETE,
                  CLASSICSETUP_UUP_ERROR_NONE,
                  "The verified Windows image is ready.", progress_callback,
                  progress_context);
    return 0;
}

int classicsetup_uup_run(
    const char *executable,
    char *const arguments[],
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_process_result *result)
{
#if CLASSICSETUP_ENABLE_UUP
    char dotnet_root[CLASSICSETUP_WORKSPACE_PATH_SIZE];

    if (classicsetup_uup_validate_tool(executable) != 0) {
        return -1;
    }
    if (classicsetup_uup_resolve_dotnet_root(
            UUP_MANIFEST.dotnet_root, getenv("DOTNET_ROOT"),
            dotnet_root, sizeof(dotnet_root)) != 0) {
        return -1;
    }
    if (dotnet_root[0] != '\0') {
        return classicsetup_run_process_cancellable_with_environment(
            executable, arguments, "DOTNET_ROOT", dotnet_root,
            cancel_callback, cancel_context, result);
    }
    return classicsetup_run_process_cancellable(
        executable, arguments, cancel_callback, cancel_context, result);
#else
    (void)executable;
    (void)arguments;
    (void)cancel_callback;
    (void)cancel_context;
    (void)result;
    return -1;
#endif
}

int classicsetup_uup_verify_wim_signature(const char *path)
{
    static const unsigned char signature[8] = {
        'M', 'S', 'W', 'I', 'M', 0, 0, 0
    };
    unsigned char header[8];
    struct stat info;
    FILE *file;
    int valid;

    if (path == NULL || lstat(path, &info) != 0 ||
        !S_ISREG(info.st_mode) || info.st_size < CLASSICSETUP_UUP_MIN_WIM_SIZE) {
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return -1;
    }
    valid = fread(header, 1, sizeof(header), file) == sizeof(header) &&
            memcmp(header, signature, sizeof(signature)) == 0;
    (void)fclose(file);
    return valid ? 0 : -1;
}

int classicsetup_uup_verify_wim(
    const char *path,
    const char *wimlib_executable,
    struct classicsetup_process_result *result)
{
    char *arguments[4];
    const char *count_text;
    char *end;
    long image_count;

    if (classicsetup_uup_verify_wim_signature(path) != 0 ||
        classicsetup_uup_validate_tool(wimlib_executable) != 0 ||
        result == NULL) {
        return -1;
    }
    arguments[0] = (char *)wimlib_executable;
    arguments[1] = "info";
    arguments[2] = (char *)path;
    arguments[3] = NULL;
    if (classicsetup_run_process(wimlib_executable, arguments, result) != 0 ||
        !result->exited || result->exit_status != 0) {
        return -1;
    }
    count_text = strstr(result->output, "Image Count:");
    if (count_text == NULL) {
        return -1;
    }
    image_count = strtol(count_text + strlen("Image Count:"), &end, 10);
    return end != count_text + strlen("Image Count:") && image_count >= 1
               ? 0
               : -1;
}

static int copy_info_value(const char *output, const char *label,
                           char *value, size_t value_size)
{
    const char *begin = strstr(output, label);
    const char *end;

    if (begin == NULL || value == NULL || value_size == 0) {
        return -1;
    }
    begin += strlen(label);
    while (*begin == ' ' || *begin == '\t') {
        ++begin;
    }
    end = begin;
    while (*end != '\0' && *end != '\r' && *end != '\n') {
        ++end;
    }
    while (end > begin && (end[-1] == ' ' || end[-1] == '\t')) {
        --end;
    }
    return copy_field(begin, end, value, value_size);
}

int classicsetup_uup_parse_wim_info(
    const char *output,
    const char *iso_path,
    struct classicsetup_verified_windows_source *source)
{
    char name[CLASSICSETUP_SOURCE_NAME_SIZE];
    char architecture[32];
    char language[32];
    char edition[CLASSICSETUP_SOURCE_NAME_SIZE];
    char build[32];
    char revision[32];
    const char *count_text;
    char *count_end;
    long image_count;

    if (output == NULL || iso_path == NULL || source == NULL ||
        copy_info_value(output, "Name:", name, sizeof(name)) != 0 ||
        copy_info_value(output, "Architecture:", architecture,
                        sizeof(architecture)) != 0 ||
        copy_info_value(output, "Edition ID:", edition,
                        sizeof(edition)) != 0 ||
        copy_info_value(output, "Default Language:", language,
                        sizeof(language)) != 0 ||
        copy_info_value(output, "Build:", build, sizeof(build)) != 0 ||
        copy_info_value(output, "Service Pack Build:", revision,
                        sizeof(revision)) != 0) {
        return -1;
    }
    count_text = strstr(output, "Image Count:");
    if (count_text == NULL) {
        return -1;
    }
    count_text += strlen("Image Count:");
    image_count = strtol(count_text, &count_end, 10);
    if (count_end == count_text || image_count < 1 ||
        !contains_case(name, "Windows 11") ||
        strcasecmp(architecture, "x86_64") != 0 ||
        strcasecmp(language, "ko-KR") != 0 ||
        strcasecmp(edition, "Professional") != 0) {
        return -1;
    }
    memset(source, 0, sizeof(*source));
    source->backend = CLASSICSETUP_SOURCE_MICROSOFT_UUP;
    source->kind = CLASSICSETUP_VERIFIED_SOURCE_ISO;
    source->family = CLASSICSETUP_WINDOWS_11;
    source->language = CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN;
    source->architecture = CLASSICSETUP_ARCH_X64;
    (void)snprintf(source->release_name, sizeof(source->release_name),
                   "%s", name);
    (void)snprintf(source->build, sizeof(source->build), "%s.%s",
                   build, revision);
    (void)snprintf(source->edition, sizeof(source->edition), "%s",
                   edition);
    (void)snprintf(source->path, sizeof(source->path), "%s", iso_path);
    source->image_index = 1;
    source->verified = true;
    return 0;
}

int classicsetup_uup_extract_and_verify_image(
    const struct classicsetup_uup_target *target,
    struct classicsetup_workspace *workspace,
    const char *extractor_executable,
    const char *wimlib_executable,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_verified_windows_source *source,
    struct classicsetup_process_result *result)
{
    static const char *const archive_paths[] = {
        "sources/install.wim", "sources/install.esd"
    };
    char output_argument[CLASSICSETUP_WORKSPACE_PATH_SIZE + 3];
    char extracted_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char *arguments[8];
    size_t index;
    int run_result = -1;

    if (target == NULL || workspace == NULL || !workspace->valid ||
        source == NULL || result == NULL ||
        classicsetup_uup_validate_tool(extractor_executable) != 0 ||
        classicsetup_uup_validate_tool(wimlib_executable) != 0 ||
        !classicsetup_uup_target_is_supported(target) ||
        snprintf(output_argument, sizeof(output_argument), "-o%s",
                 workspace->image_path) <= 0) {
        return -1;
    }
    for (index = 0; index < sizeof(archive_paths) / sizeof(archive_paths[0]);
         ++index) {
        const char *base_name = strrchr(archive_paths[index], '/');

        base_name = base_name == NULL ? archive_paths[index] : base_name + 1;
        if (snprintf(extracted_path, sizeof(extracted_path), "%s/%s",
                     workspace->image_path, base_name) <= 0) {
            return -1;
        }
        (void)unlink(extracted_path);
        arguments[0] = (char *)extractor_executable;
        arguments[1] = "e";
        arguments[2] = "-y";
        arguments[3] = output_argument;
        arguments[4] = workspace->iso_partial_path;
        arguments[5] = (char *)archive_paths[index];
        arguments[6] = NULL;
        run_result = classicsetup_run_process_cancellable(
            extractor_executable, arguments, cancel_callback,
            cancel_context, result);
        if (run_result == 1) {
            return 1;
        }
        if (run_result == 0 && result->exited && result->exit_status == 0 &&
            classicsetup_uup_verify_wim_signature(extracted_path) == 0) {
            break;
        }
    }
    if (index == sizeof(archive_paths) / sizeof(archive_paths[0])) {
        return -1;
    }
    arguments[0] = (char *)wimlib_executable;
    arguments[1] = "verify";
    arguments[2] = extracted_path;
    arguments[3] = NULL;
    run_result = classicsetup_run_process_cancellable(
        wimlib_executable, arguments, cancel_callback, cancel_context,
        result);
    if (run_result != 0 || !result->exited || result->exit_status != 0) {
        return run_result == 1 ? 1 : -1;
    }
    arguments[1] = "info";
    run_result = classicsetup_run_process_cancellable(
        wimlib_executable, arguments, cancel_callback, cancel_context,
        result);
    if (run_result != 0 || !result->exited || result->exit_status != 0 ||
        classicsetup_uup_parse_wim_info(
            result->output, workspace->iso_partial_path, source) != 0 ||
        source->family != target->family ||
        source->language != target->language ||
        source->architecture != target->architecture ||
        strcasecmp(source->edition,
                   classicsetup_uup_edition_token(target->edition)) != 0) {
        return run_result == 1 ? 1 : -1;
    }
    return 0;
}

int classicsetup_uup_register_verified_wim(
    const struct classicsetup_uup_target *target,
    const struct classicsetup_workspace *workspace,
    struct classicsetup_verified_windows_source *source)
{
    if (target == NULL || workspace == NULL || source == NULL ||
        !workspace->valid || !workspace->verified_wim) {
        return -1;
    }
    memset(source, 0, sizeof(*source));
    source->backend = CLASSICSETUP_SOURCE_MICROSOFT_UUP;
    source->kind = CLASSICSETUP_VERIFIED_SOURCE_WIM;
    source->family = target->family;
    source->language = target->language;
    source->architecture = target->architecture;
    if (strlen(target->reporting_version) >= sizeof(source->build)) {
        return -1;
    }
    memcpy(source->build, target->reporting_version,
           strlen(target->reporting_version) + 1);
    (void)snprintf(source->edition, sizeof(source->edition), "%s",
                   classicsetup_uup_edition_token(target->edition));
    (void)snprintf(source->path, sizeof(source->path), "%s",
                   workspace->wim_final_path);
    source->image_index = 1;
    source->verified = true;
    return 0;
}
