#define _XOPEN_SOURCE 700

#include "classicsetup/local_iso.h"

#include "classicsetup/retail.h"

#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef CLASSICSETUP_INSTALLED_ISO_DIRECTORY
#define CLASSICSETUP_INSTALLED_ISO_DIRECTORY "/usr/share/classicsetup/iso"
#endif

static bool has_iso_extension(const char *name)
{
    size_t length = name != NULL ? strlen(name) : 0;

    return length > 4 && strcasecmp(name + length - 4, ".iso") == 0;
}

static int compare_entries(const void *left, const void *right)
{
    const struct classicsetup_local_iso_entry *a = left;
    const struct classicsetup_local_iso_entry *b = right;

    return strcasecmp(a->name, b->name);
}

void classicsetup_local_iso_catalog_reset(
    struct classicsetup_local_iso_catalog *catalog)
{
    if (catalog != NULL) {
        memset(catalog, 0, sizeof(*catalog));
    }
}

int classicsetup_local_iso_default_directory(
    char *directory, size_t directory_size)
{
    const char *configured = getenv("CLASSICSETUP_ISO_DIRECTORY");
    char executable[CLASSICSETUP_LOCAL_ISO_PATH_SIZE];
    char candidate[CLASSICSETUP_LOCAL_ISO_PATH_SIZE];
    char resolved[CLASSICSETUP_LOCAL_ISO_PATH_SIZE];
    char *separator;
    ssize_t length;

    if (directory == NULL || directory_size == 0) {
        return -1;
    }
    if (configured != NULL && configured[0] == '/' &&
        realpath(configured, resolved) != NULL) {
        return snprintf(directory, directory_size, "%s", resolved) > 0
                   ? 0 : -1;
    }
    length = readlink("/proc/self/exe", executable, sizeof(executable) - 1);
    if (length > 0 && (size_t)length < sizeof(executable)) {
        executable[length] = '\0';
        separator = strrchr(executable, '/');
        if (separator != NULL) {
            *separator = '\0';
            if (snprintf(candidate, sizeof(candidate), "%s/../iso",
                         executable) > 0 &&
                realpath(candidate, resolved) != NULL) {
                return snprintf(directory, directory_size, "%s", resolved) > 0
                           ? 0 : -1;
            }
        }
    }
    if (realpath(CLASSICSETUP_INSTALLED_ISO_DIRECTORY, resolved) != NULL) {
        return snprintf(directory, directory_size, "%s", resolved) > 0
                   ? 0 : -1;
    }
    directory[0] = '\0';
    return -1;
}

int classicsetup_local_iso_scan(
    const char *directory,
    struct classicsetup_local_iso_catalog *catalog)
{
    DIR *stream;
    struct dirent *entry;
    char resolved_directory[CLASSICSETUP_LOCAL_ISO_PATH_SIZE];

    if (directory == NULL || catalog == NULL) {
        return -1;
    }
    classicsetup_local_iso_catalog_reset(catalog);
    if (realpath(directory, resolved_directory) == NULL ||
        (stream = opendir(resolved_directory)) == NULL) {
        (void)snprintf(catalog->error, sizeof(catalog->error),
                       "The ClassicSetup ISO folder could not be opened.");
        return -1;
    }
    (void)snprintf(catalog->directory, sizeof(catalog->directory), "%s",
                   resolved_directory);
    while ((entry = readdir(stream)) != NULL) {
        struct stat info;
        struct classicsetup_local_iso_entry *item;
        char path[CLASSICSETUP_LOCAL_ISO_PATH_SIZE];

        if (!has_iso_extension(entry->d_name) ||
            catalog->count >= CLASSICSETUP_LOCAL_ISO_MAX_FILES ||
            snprintf(path, sizeof(path), "%s/%s", resolved_directory,
                     entry->d_name) <= 0 ||
            lstat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
            continue;
        }
        item = &catalog->entries[catalog->count++];
        (void)snprintf(item->name, sizeof(item->name), "%s", entry->d_name);
        (void)snprintf(item->path, sizeof(item->path), "%s", path);
        item->size = (unsigned long long)info.st_size;
    }
    (void)closedir(stream);
    qsort(catalog->entries, catalog->count, sizeof(catalog->entries[0]),
          compare_entries);
    if (catalog->count == 0) {
        (void)snprintf(catalog->error, sizeof(catalog->error),
                       "No Windows ISO files were found in %.150s.",
                       catalog->directory);
        return -1;
    }
    return 0;
}

static bool local_cancelled(void *context)
{
    return context != NULL && atomic_load((atomic_bool *)context);
}

static void notify(
    classicsetup_download_progress_callback progress,
    void *progress_data,
    const struct classicsetup_download_status *status)
{
    if (progress != NULL) {
        progress(status, progress_data);
    }
}

int classicsetup_local_iso_verify(
    const struct classicsetup_windows_release *release,
    const char *iso_path,
    struct classicsetup_workspace *workspace,
    atomic_bool *cancel_requested,
    classicsetup_download_progress_callback progress,
    void *progress_data,
    struct classicsetup_download_status *status,
    struct classicsetup_verified_windows_source *verified_source)
{
    struct stat info;
    enum classicsetup_download_error verify_error;
    struct classicsetup_process_result process_result;
    struct classicsetup_retail_inspection_diagnostics diagnostics;
    int inspect_result;

    if (release == NULL || iso_path == NULL || workspace == NULL ||
        !workspace->valid || status == NULL || verified_source == NULL ||
        lstat(iso_path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return -1;
    }
    classicsetup_download_status_reset(status);
    memset(verified_source, 0, sizeof(*verified_source));
    status->state = CLASSICSETUP_DOWNLOAD_VERIFYING;
    status->bytes_received = (unsigned long long)info.st_size;
    status->total_bytes = (unsigned long long)info.st_size;
    status->progress_fraction = 1.0;
    (void)snprintf(status->message, sizeof(status->message), "%s",
                   "Verifying the selected Windows ISO...");
    notify(progress, progress_data, status);
    if (cancel_requested != NULL && atomic_load(cancel_requested)) {
        status->state = CLASSICSETUP_DOWNLOAD_CANCELLED;
        status->error = CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED;
        (void)snprintf(status->message, sizeof(status->message), "%s",
                       "ISO verification cancelled.");
        classicsetup_workspace_cleanup_cancel(workspace);
        notify(progress, progress_data, status);
        return -1;
    }
    if (classicsetup_verify_source_file(
            iso_path, 0, "", &verify_error) != 0) {
        status->state = CLASSICSETUP_DOWNLOAD_FAILED;
        status->error = verify_error;
        (void)snprintf(status->message, sizeof(status->message), "%s",
                       "The selected file is not a readable Windows ISO.");
        classicsetup_workspace_cleanup_failure(workspace);
        notify(progress, progress_data, status);
        return -1;
    }
    (void)snprintf(status->message, sizeof(status->message), "%s",
                   "Reading Windows image metadata...");
    notify(progress, progress_data, status);
    inspect_result = classicsetup_retail_inspect_iso_path(
        release, workspace, iso_path, iso_path, local_cancelled,
        cancel_requested, verified_source, &process_result, &diagnostics);
    if (inspect_result != 0) {
        status->state = inspect_result == 1
                            ? CLASSICSETUP_DOWNLOAD_CANCELLED
                            : CLASSICSETUP_DOWNLOAD_FAILED;
        status->error = inspect_result == 1
                            ? CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED
                            : CLASSICSETUP_DOWNLOAD_ERROR_ISO;
        (void)snprintf(status->message, sizeof(status->message), "%s",
                       inspect_result == 1
                           ? "ISO verification cancelled."
                           : "The selected ISO metadata could not be read or does not match the selected Windows options.");
        memset(verified_source, 0, sizeof(*verified_source));
        classicsetup_workspace_cleanup_failure(workspace);
        notify(progress, progress_data, status);
        return -1;
    }
    verified_source->backend = CLASSICSETUP_SOURCE_EXISTING_ISO;
    status->state = CLASSICSETUP_DOWNLOAD_COMPLETE;
    status->error = CLASSICSETUP_DOWNLOAD_ERROR_NONE;
    (void)snprintf(status->message, sizeof(status->message), "%s",
                   "The existing Windows ISO is ready to use.");
    classicsetup_workspace_cleanup_success(workspace);
    notify(progress, progress_data, status);
    return 0;
}
