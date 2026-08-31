#define _POSIX_C_SOURCE 200809L

#include "classicsetup/retail.h"

#include <ctype.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef CLASSICSETUP_PWSH_PATH
#define CLASSICSETUP_PWSH_PATH ""
#endif
#ifndef CLASSICSETUP_FIDO_LINUX_SCRIPT
#define CLASSICSETUP_FIDO_LINUX_SCRIPT \
    "/usr/lib/classicsetup/tools/fido/fido-linux.ps1"
#endif
#ifndef CLASSICSETUP_FIDO_LINUX_SHA256
#define CLASSICSETUP_FIDO_LINUX_SHA256 ""
#endif
#ifndef CLASSICSETUP_FIDO_INSTALLED_SCRIPT
#define CLASSICSETUP_FIDO_INSTALLED_SCRIPT \
    "/usr/lib/classicsetup/tools/fido/fido-linux.ps1"
#endif
#ifndef CLASSICSETUP_FIDO_BUILD_RELATIVE_DIR
#define CLASSICSETUP_FIDO_BUILD_RELATIVE_DIR \
    "lib/classicsetup/tools/fido"
#endif
#ifndef CLASSICSETUP_FIDO_INSTALL_RELATIVE_DIR
#define CLASSICSETUP_FIDO_INSTALL_RELATIVE_DIR \
    "lib/classicsetup/tools/fido"
#endif
#ifndef CLASSICSETUP_WIMLIB_EXECUTABLE
#define CLASSICSETUP_WIMLIB_EXECUTABLE "/usr/bin/wimlib-imagex"
#endif
#ifndef CLASSICSETUP_UUP_ISO_EXTRACTOR
#define CLASSICSETUP_UUP_ISO_EXTRACTOR "/usr/bin/7z"
#endif

enum { RETAIL_ARG_COUNT = 22 };

static void report(struct classicsetup_retail_status *status,
                   enum classicsetup_retail_stage stage,
                   enum classicsetup_retail_error error,
                   const char *detail,
                   classicsetup_retail_progress_callback progress,
                   void *context)
{
    status->stage = stage;
    status->error = error;
    (void)snprintf(status->detail, sizeof(status->detail), "%s",
                   detail != NULL ? detail : "");
    if (progress != NULL) {
        progress(status, context);
    }
}

void classicsetup_retail_status_reset(struct classicsetup_retail_status *status)
{
    if (status != NULL) {
        memset(status, 0, sizeof(*status));
        status->stage = CLASSICSETUP_RETAIL_IDLE;
    }
}

const char *classicsetup_retail_error_message(enum classicsetup_retail_error error)
{
    static const char *const messages[] = {
        "No Retail source error.",
        "PowerShell is not available.",
        "The pinned Microsoft source resolver is missing.",
        "The Microsoft source resolver failed integrity validation.",
        "ClassicSetup could not obtain an official Windows download.",
        "Microsoft did not provide a usable download link.",
        "The returned download link did not meet source policy.",
        "The downloaded Windows ISO could not be read.",
        "The Windows installation image metadata could not be read.",
        "The Windows image does not match the selected source.",
        "Windows source resolution was cancelled."
    };

    return (size_t)error < sizeof(messages) / sizeof(messages[0])
               ? messages[error] : "The Microsoft Retail source failed.";
}

int classicsetup_retail_recommended_catalog(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog)
{
    struct classicsetup_windows_release *release;

    if (catalog == NULL) {
        return -1;
    }
    classicsetup_source_catalog_reset(catalog);
    if (family != CLASSICSETUP_WINDOWS_11) {
        catalog->state = CLASSICSETUP_SOURCE_ERROR;
        (void)snprintf(catalog->error, sizeof(catalog->error), "%s",
                       "Only the validated Windows 11 Retail source is enabled.");
        return -1;
    }
    for (catalog->release_count = 0; catalog->release_count < 2;
         ++catalog->release_count) {
        release = &catalog->releases[catalog->release_count];
        release->family = CLASSICSETUP_WINDOWS_11;
        release->language = catalog->release_count == 0
                                ? CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN
                                : CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH;
        release->architecture = CLASSICSETUP_ARCH_X64;
        release->edition = CLASSICSETUP_WINDOWS_EDITION_PROFESSIONAL;
        (void)snprintf(release->release_name,
                       sizeof(release->release_name), "%s",
                       "Latest stable Retail");
        (void)snprintf(release->language_name,
                       sizeof(release->language_name), "%s",
                       release->language ==
                               CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN
                           ? "Korean" : "English");
        (void)snprintf(release->architecture_token,
                       sizeof(release->architecture_token), "%s", "x64");
        (void)snprintf(release->edition_name,
                       sizeof(release->edition_name), "%s",
                       "Windows 11 Home/Pro/Edu");
    }
    catalog->state = CLASSICSETUP_SOURCE_READY;
    return 0;
}

static int executable_file(const char *path)
{
    struct stat info;

    return path != NULL && path[0] == '/' && stat(path, &info) == 0 &&
           S_ISREG(info.st_mode) && access(path, X_OK) == 0 ? 0 : -1;
}

static int copy_path(const char *path, char *output, size_t output_size)
{
    int written;

    if (path == NULL || output == NULL || output_size == 0) {
        return -1;
    }
    written = snprintf(output, output_size, "%s", path);
    return written > 0 && (size_t)written < output_size ? 0 : -1;
}

int classicsetup_retail_resolve_pwsh(
    const char *configured_path, char *resolved, size_t resolved_size)
{
    const char *override = getenv("CLASSICSETUP_PWSH");
    const char *path;
    const char *cursor;
    static const char *const known[] = {
        "/opt/microsoft/powershell/7/pwsh"
    };
    size_t index;

    if (override != NULL && override[0] != '\0') {
        return executable_file(override) == 0
                   ? copy_path(override, resolved, resolved_size) : -1;
    }
    path = getenv("PATH");
    cursor = path;
    while (cursor != NULL && *cursor != '\0') {
        const char *end = strchr(cursor, ':');
        size_t length = end != NULL ? (size_t)(end - cursor) : strlen(cursor);
        char candidate[CLASSICSETUP_WORKSPACE_PATH_SIZE];
        int written;

        if (length > 0) {
            written = snprintf(candidate, sizeof(candidate), "%.*s/pwsh",
                               (int)length, cursor);
            if (written > 0 && (size_t)written < sizeof(candidate) &&
                executable_file(candidate) == 0) {
                return copy_path(candidate, resolved, resolved_size);
            }
        }
        cursor = end != NULL ? end + 1 : NULL;
    }
    if (configured_path != NULL && configured_path[0] != '\0' &&
        executable_file(configured_path) == 0) {
        return copy_path(configured_path, resolved, resolved_size);
    }
    for (index = 0; index < sizeof(known) / sizeof(known[0]); ++index) {
        if (executable_file(known[index]) == 0) {
            return copy_path(known[index], resolved, resolved_size);
        }
    }
    return -1;
}

int classicsetup_retail_validate_script(
    const char *path, const char *expected_sha256)
{
    unsigned char buffer[65536];
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    char text[65];
    EVP_MD_CTX *context = NULL;
    struct stat info;
    FILE *file = NULL;
    size_t count;
    size_t index;
    int valid = -1;

    if (path == NULL || path[0] != '/' || expected_sha256 == NULL ||
        strlen(expected_sha256) != 64 || lstat(path, &info) != 0 ||
        !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) ||
        (file = fopen(path, "rb")) == NULL ||
        (context = EVP_MD_CTX_new()) == NULL ||
        EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) {
        goto done;
    }
    while ((count = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(context, buffer, count) != 1) {
            goto done;
        }
    }
    if (ferror(file) || EVP_DigestFinal_ex(context, digest, &digest_size) != 1 ||
        digest_size != 32) {
        goto done;
    }
    for (index = 0; index < digest_size; ++index) {
        (void)snprintf(text + index * 2, 3, "%02x", digest[index]);
    }
    text[64] = '\0';
    valid = strcasecmp(text, expected_sha256) == 0 ? 0 : -1;
done:
    if (file != NULL) {
        (void)fclose(file);
    }
    EVP_MD_CTX_free(context);
    return valid;
}

int classicsetup_retail_build_fido_argv(
    const char *pwsh, const char *script,
    const struct classicsetup_windows_release *release,
    char *arguments[], size_t argument_count)
{
    const char *architecture;
    const char *language;

    if (pwsh == NULL || script == NULL || release == NULL ||
        arguments == NULL || argument_count < RETAIL_ARG_COUNT ||
        release->family != CLASSICSETUP_WINDOWS_11 ||
        (release->language != CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN &&
         release->language != CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH) ||
        release->architecture != CLASSICSETUP_ARCH_X64) {
        return -1;
    }
    architecture = classicsetup_windows_architecture_label(
        release->architecture);
    language = release->language == CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN
                   ? "Korean" : "English";
    arguments[0] = (char *)pwsh;
    arguments[1] = "-NoLogo";
    arguments[2] = "-NoProfile";
    arguments[3] = "-NonInteractive";
    arguments[4] = "-File";
    arguments[5] = (char *)script;
    arguments[6] = "-Win";
    arguments[7] = "Windows 11";
    arguments[8] = "-Rel";
    arguments[9] = "Latest";
    arguments[10] = "-Ed";
    arguments[11] = "Home/Pro/Edu";
    arguments[12] = "-Lang";
    arguments[13] = (char *)language;
    arguments[14] = "-Arch";
    arguments[15] = (char *)architecture;
    arguments[16] = "-GetUrl";
    arguments[17] = NULL;
    return 0;
}

static bool cancel_requested(void *context)
{
    return context != NULL && atomic_load((atomic_bool *)context);
}

static int executable_relative_script(char *resolved, size_t resolved_size)
{
    char executable[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char *separator;
    ssize_t length = readlink("/proc/self/exe", executable,
                              sizeof(executable) - 1);
    int written;
    static const char *const formats[] = {
        "%s/" CLASSICSETUP_FIDO_BUILD_RELATIVE_DIR "/fido-linux.ps1",
        "%s/../" CLASSICSETUP_FIDO_INSTALL_RELATIVE_DIR "/fido-linux.ps1"
    };
    size_t index;

    if (length <= 0 || (size_t)length >= sizeof(executable)) {
        return -1;
    }
    executable[length] = '\0';
    separator = strrchr(executable, '/');
    if (separator == NULL) {
        return -1;
    }
    *separator = '\0';
    for (index = 0; index < sizeof(formats) / sizeof(formats[0]); ++index) {
        written = snprintf(resolved, resolved_size, formats[index], executable);
        if (written > 0 && (size_t)written < resolved_size &&
            access(resolved, R_OK) == 0) {
            return 0;
        }
    }
    return -1;
}

int classicsetup_retail_resolve_script(
    char *resolved, size_t resolved_size)
{
    const char *override = getenv("CLASSICSETUP_FIDO_SCRIPT");
    const char *candidates[] = {
        CLASSICSETUP_FIDO_INSTALLED_SCRIPT,
        CLASSICSETUP_FIDO_LINUX_SCRIPT
    };
    size_t index;

    if (override != NULL && override[0] != '\0') {
        return override[0] == '/' &&
                       copy_path(override, resolved, resolved_size) == 0
                   ? 0 : -1;
    }
    if (executable_relative_script(resolved, resolved_size) == 0) {
        return 0;
    }
    for (index = 0; index < sizeof(candidates) / sizeof(candidates[0]);
         ++index) {
        if (candidates[index][0] == '/' &&
            access(candidates[index], R_OK) == 0 &&
            copy_path(candidates[index], resolved, resolved_size) == 0) {
            return 0;
        }
    }
    return copy_path(CLASSICSETUP_FIDO_INSTALLED_SCRIPT,
                     resolved, resolved_size);
}

static int parse_signed_url(char *output,
                            struct classicsetup_windows_release *release,
                            struct classicsetup_retail_status *status)
{
    char *begin = output;
    char *end;
    const char *host;
    const char *host_end;
    size_t host_length;

    while (*begin != '\0' && isspace((unsigned char)*begin)) {
        ++begin;
    }
    end = begin + strlen(begin);
    while (end > begin && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    if (*begin == '\0' || strchr(begin, '\n') != NULL ||
        strlen(begin) >= sizeof(release->download_uri)) {
        return -1;
    }
    if (!classicsetup_windows_source_uri_is_official(begin)) {
        return -2;
    }
    (void)snprintf(release->download_uri, sizeof(release->download_uri),
                   "%s", begin);
    release->resolved = true;
    host = begin + strlen("https://");
    host_end = strpbrk(host, "/?#");
    host_length = host_end != NULL ? (size_t)(host_end - host) : strlen(host);
    if (host_length >= sizeof(status->delivery_host)) {
        return -2;
    }
    memcpy(status->delivery_host, host, host_length);
    status->delivery_host[host_length] = '\0';
    return 0;
}

int classicsetup_retail_resolve(
    struct classicsetup_windows_release *release,
    atomic_bool *cancel_flag,
    classicsetup_retail_progress_callback progress,
    void *progress_context,
    struct classicsetup_retail_status *status)
{
    char pwsh[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char script[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char *arguments[RETAIL_ARG_COUNT];
    struct classicsetup_process_result result;
    int run_result;
    int parse_result;

    if (release == NULL || status == NULL) {
        return -1;
    }
    classicsetup_retail_status_reset(status);
    release->resolved = false;
    memset(release->download_uri, 0, sizeof(release->download_uri));
    report(status, CLASSICSETUP_RETAIL_CHECKING_SOURCE,
           CLASSICSETUP_RETAIL_ERROR_NONE, "Checking Microsoft source tools...",
           progress, progress_context);
    if (classicsetup_retail_resolve_pwsh(CLASSICSETUP_PWSH_PATH, pwsh,
                                         sizeof(pwsh)) != 0) {
        report(status, CLASSICSETUP_RETAIL_FAILED,
               CLASSICSETUP_RETAIL_ERROR_PWSH_MISSING,
               classicsetup_retail_error_message(
                   CLASSICSETUP_RETAIL_ERROR_PWSH_MISSING),
               progress, progress_context);
        return -1;
    }
    if (classicsetup_retail_resolve_script(script, sizeof(script)) != 0 ||
        access(script, R_OK) != 0) {
        report(status, CLASSICSETUP_RETAIL_FAILED,
               CLASSICSETUP_RETAIL_ERROR_SCRIPT_MISSING,
               classicsetup_retail_error_message(
                   CLASSICSETUP_RETAIL_ERROR_SCRIPT_MISSING),
               progress, progress_context);
        return -1;
    }
    if (classicsetup_retail_validate_script(
            script, CLASSICSETUP_FIDO_LINUX_SHA256) != 0) {
        report(status, CLASSICSETUP_RETAIL_FAILED,
               CLASSICSETUP_RETAIL_ERROR_SCRIPT_HASH,
               classicsetup_retail_error_message(
                   CLASSICSETUP_RETAIL_ERROR_SCRIPT_HASH),
               progress, progress_context);
        return -1;
    }
    if (classicsetup_retail_build_fido_argv(
            pwsh, script, release, arguments,
            sizeof(arguments) / sizeof(arguments[0])) != 0) {
        return -1;
    }
    report(status, CLASSICSETUP_RETAIL_DISCOVERING,
           CLASSICSETUP_RETAIL_ERROR_NONE, "Discovering Windows releases...",
           progress, progress_context);
    report(status, CLASSICSETUP_RETAIL_RESOLVING_LINK,
           CLASSICSETUP_RETAIL_ERROR_NONE,
           "Requesting an official Microsoft download...",
           progress, progress_context);
    run_result = classicsetup_run_process_cancellable(
        pwsh, arguments, cancel_requested, cancel_flag, &result);
    if (run_result == 1) {
        memset(result.output, 0, sizeof(result.output));
        report(status, CLASSICSETUP_RETAIL_CANCELLED,
               CLASSICSETUP_RETAIL_ERROR_CANCELLED,
               classicsetup_retail_error_message(
                   CLASSICSETUP_RETAIL_ERROR_CANCELLED),
               progress, progress_context);
        return -1;
    }
    status->child_exit_status = result.exited ? result.exit_status : -1;
    if (run_result != 0 || !result.exited || result.exit_status != 0) {
        memset(result.output, 0, sizeof(result.output));
        report(status, CLASSICSETUP_RETAIL_FAILED,
               CLASSICSETUP_RETAIL_ERROR_PROCESS,
               classicsetup_retail_error_message(
                   CLASSICSETUP_RETAIL_ERROR_PROCESS),
               progress, progress_context);
        return -1;
    }
    parse_result = parse_signed_url(result.output, release, status);
    memset(result.output, 0, sizeof(result.output));
    if (parse_result != 0) {
        report(status, CLASSICSETUP_RETAIL_FAILED,
               parse_result == -2 ? CLASSICSETUP_RETAIL_ERROR_POLICY
                                  : CLASSICSETUP_RETAIL_ERROR_NO_LINK,
               classicsetup_retail_error_message(
                   parse_result == -2 ? CLASSICSETUP_RETAIL_ERROR_POLICY
                                      : CLASSICSETUP_RETAIL_ERROR_NO_LINK),
               progress, progress_context);
        return -1;
    }
    return 0;
}

static int tool_available(const char *path)
{
    return executable_file(path);
}

static int copy_info(const char *output, const char *label,
                     char *value, size_t value_size)
{
    const char *begin = strstr(output, label);
    const char *end;
    size_t length;

    if (begin == NULL || value == NULL || value_size == 0) {
        return -1;
    }
    begin += strlen(label);
    while (*begin == ' ' || *begin == '\t') {
        ++begin;
    }
    end = strpbrk(begin, "\r\n");
    if (end == NULL) {
        end = begin + strlen(begin);
    }
    while (end > begin && isspace((unsigned char)end[-1])) {
        --end;
    }
    length = (size_t)(end - begin);
    if (length == 0 || length >= value_size) {
        return -1;
    }
    memcpy(value, begin, length);
    value[length] = '\0';
    return 0;
}

static int contains_case_insensitive(const char *text, const char *needle)
{
    size_t needle_length;

    if (text == NULL || needle == NULL || *needle == '\0') {
        return 0;
    }
    needle_length = strlen(needle);
    while (*text != '\0') {
        if (strncasecmp(text, needle, needle_length) == 0) {
            return 1;
        }
        ++text;
    }
    return 0;
}

int classicsetup_retail_parse_wim_metadata(
    const struct classicsetup_windows_release *release,
    const char *output,
    const char *iso_path,
    struct classicsetup_verified_windows_source *source)
{
    char name[CLASSICSETUP_SOURCE_NAME_SIZE] = "";
    char architecture[32] = "";
    char language[32] = "";
    char edition[CLASSICSETUP_SOURCE_NAME_SIZE] = "";
    char build[32] = "";
    char revision[32] = "";
    const char *expected_arch = release->architecture == CLASSICSETUP_ARCH_X64
                                    ? "x86_64"
                                    : release->architecture == CLASSICSETUP_ARCH_X86
                                          ? "x86" : "ARM64";

    if (copy_info(output, "Name:", name, sizeof(name)) != 0 ||
        copy_info(output, "Architecture:", architecture,
                  sizeof(architecture)) != 0 ||
        strcasecmp(architecture, expected_arch) != 0) {
        return -1;
    }
    (void)copy_info(output, "Default Language:", language, sizeof(language));
    if (language[0] != '\0') {
        if (release->language == CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN &&
            strcasecmp(language, "ko-KR") != 0 &&
            strcasecmp(language, "Korean") != 0) {
            return -1;
        }
        if (release->language == CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH &&
            strcasecmp(language, "en-US") != 0 &&
            strcasecmp(language, "English") != 0) {
            return -1;
        }
    }
    if ((release->family == CLASSICSETUP_WINDOWS_11 &&
         !contains_case_insensitive(name, "Windows 11")) ||
        (release->family == CLASSICSETUP_WINDOWS_10 &&
         !contains_case_insensitive(name, "Windows 10"))) {
        return -1;
    }
    (void)copy_info(output, "Edition ID:", edition, sizeof(edition));
    (void)copy_info(output, "Build:", build, sizeof(build));
    (void)copy_info(output, "Service Pack Build:", revision,
                    sizeof(revision));
    memset(source, 0, sizeof(*source));
    source->backend = CLASSICSETUP_SOURCE_MICROSOFT_RETAIL;
    source->kind = CLASSICSETUP_VERIFIED_SOURCE_ISO;
    source->family = release->family;
    source->language = release->language;
    source->architecture = release->architecture;
    (void)snprintf(source->release_name, sizeof(source->release_name), "%s",
                   name);
    if (build[0] != '\0') {
        if (revision[0] != '\0') {
            (void)snprintf(source->build, sizeof(source->build), "%s.%s",
                           build, revision);
        } else {
            (void)snprintf(source->build, sizeof(source->build), "%s", build);
        }
    }
    (void)snprintf(source->edition, sizeof(source->edition), "%s",
                   edition[0] != '\0' ? edition : "Multiple editions");
    (void)snprintf(source->path, sizeof(source->path), "%s", iso_path);
    source->image_index = 1;
    source->verified = true;
    return 0;
}

int classicsetup_retail_inspect_iso(
    const struct classicsetup_windows_release *release,
    struct classicsetup_workspace *workspace,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_verified_windows_source *source,
    struct classicsetup_process_result *result,
    struct classicsetup_retail_inspection_diagnostics *diagnostics)
{
    static const char *const members[] = {
        "sources/install.wim", "sources/install.esd"
    };
    char output_option[CLASSICSETUP_WORKSPACE_PATH_SIZE + 3];
    char image_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char *arguments[8];
    const char *count_text;
    char *count_end;
    size_t index;
    long image_count;
    int run_result;

    if (diagnostics != NULL) {
        memset(diagnostics, 0, sizeof(*diagnostics));
        diagnostics->stage = CLASSICSETUP_RETAIL_INSPECTION_PREREQUISITES;
        diagnostics->extractor_exit_status = -1;
        diagnostics->wimlib_exit_status = -1;
    }
    if (release == NULL || workspace == NULL || !workspace->valid ||
        source == NULL || result == NULL ||
        tool_available(CLASSICSETUP_UUP_ISO_EXTRACTOR) != 0 ||
        tool_available(CLASSICSETUP_WIMLIB_EXECUTABLE) != 0 ||
        snprintf(output_option, sizeof(output_option), "-o%s",
                 workspace->image_path) <= 0) {
        return -1;
    }
    for (index = 0; index < sizeof(members) / sizeof(members[0]); ++index) {
        const char *name = strrchr(members[index], '/');

        name = name != NULL ? name + 1 : members[index];
        if (snprintf(image_path, sizeof(image_path), "%s/%s",
                     workspace->image_path, name) <= 0) {
            return -1;
        }
        (void)unlink(image_path);
        if (diagnostics != NULL) {
            diagnostics->stage = index == 0
                ? CLASSICSETUP_RETAIL_INSPECTION_EXTRACT_WIM
                : CLASSICSETUP_RETAIL_INSPECTION_EXTRACT_ESD;
        }
        arguments[0] = (char *)CLASSICSETUP_UUP_ISO_EXTRACTOR;
        arguments[1] = "e";
        arguments[2] = "-y";
        arguments[3] = output_option;
        arguments[4] = workspace->iso_partial_path;
        arguments[5] = (char *)members[index];
        arguments[6] = NULL;
        run_result = classicsetup_run_process_cancellable(
            CLASSICSETUP_UUP_ISO_EXTRACTOR, arguments, cancel_callback,
            cancel_context, result);
        if (diagnostics != NULL) {
            diagnostics->extractor_exit_status =
                result->exited ? result->exit_status : -1;
            diagnostics->output_truncated = result->output_truncated != 0;
        }
        if (run_result == 1) {
            return 1;
        }
        if (run_result == 0 && result->exited && result->exit_status == 0 &&
            access(image_path, R_OK) == 0) {
            if (diagnostics != NULL) {
                diagnostics->install_wim_found = index == 0;
                diagnostics->install_esd_found = index == 1;
            }
            break;
        }
    }
    if (index == sizeof(members) / sizeof(members[0])) {
        return -1;
    }
    arguments[0] = (char *)CLASSICSETUP_WIMLIB_EXECUTABLE;
    arguments[1] = "info";
    arguments[2] = image_path;
    arguments[3] = NULL;
    if (diagnostics != NULL) {
        diagnostics->stage = CLASSICSETUP_RETAIL_INSPECTION_IMAGE_COUNT;
    }
    run_result = classicsetup_run_process_cancellable(
        CLASSICSETUP_WIMLIB_EXECUTABLE, arguments, cancel_callback,
        cancel_context, result);
    if (diagnostics != NULL) {
        diagnostics->wimlib_exit_status =
            result->exited ? result->exit_status : -1;
        diagnostics->output_truncated = result->output_truncated != 0;
    }
    if (run_result != 0 || !result->exited || result->exit_status != 0 ||
        (count_text = strstr(result->output, "Image Count:")) == NULL) {
        (void)unlink(image_path);
        return run_result == 1 ? 1 : -1;
    }
    count_text += strlen("Image Count:");
    image_count = strtol(count_text, &count_end, 10);
    if (count_end == count_text || image_count < 1) {
        (void)unlink(image_path);
        return -1;
    }
    if (diagnostics != NULL) {
        diagnostics->image_count = image_count;
        diagnostics->stage = CLASSICSETUP_RETAIL_INSPECTION_IMAGE_METADATA;
    }
    arguments[3] = "1";
    arguments[4] = NULL;
    run_result = classicsetup_run_process_cancellable(
        CLASSICSETUP_WIMLIB_EXECUTABLE, arguments, cancel_callback,
        cancel_context, result);
    if (diagnostics != NULL) {
        diagnostics->wimlib_exit_status =
            result->exited ? result->exit_status : -1;
        diagnostics->output_truncated = result->output_truncated != 0;
        (void)copy_info(result->output, "Architecture:",
                        diagnostics->architecture,
                        sizeof(diagnostics->architecture));
        (void)copy_info(result->output, "Default Language:",
                        diagnostics->language,
                        sizeof(diagnostics->language));
    }
    if (run_result != 0 || !result->exited || result->exit_status != 0 ||
        classicsetup_retail_parse_wim_metadata(
            release, result->output, workspace->iso_final_path, source) != 0) {
        (void)unlink(image_path);
        return run_result == 1 ? 1 : -1;
    }
    (void)unlink(image_path);
    if (diagnostics != NULL) {
        diagnostics->stage = CLASSICSETUP_RETAIL_INSPECTION_COMPLETE;
    }
    return 0;
}
