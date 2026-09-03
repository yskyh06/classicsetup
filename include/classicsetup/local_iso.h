#ifndef CLASSICSETUP_LOCAL_ISO_H
#define CLASSICSETUP_LOCAL_ISO_H

#include <stdatomic.h>
#include <stddef.h>

#include "classicsetup/download.h"
#include "classicsetup/windows_source.h"
#include "classicsetup/workspace.h"

enum {
    CLASSICSETUP_LOCAL_ISO_MAX_FILES = 32,
    CLASSICSETUP_LOCAL_ISO_NAME_SIZE = 256,
    CLASSICSETUP_LOCAL_ISO_PATH_SIZE = 4096
};

struct classicsetup_local_iso_entry {
    char name[CLASSICSETUP_LOCAL_ISO_NAME_SIZE];
    char path[CLASSICSETUP_LOCAL_ISO_PATH_SIZE];
    unsigned long long size;
};

struct classicsetup_local_iso_catalog {
    struct classicsetup_local_iso_entry
        entries[CLASSICSETUP_LOCAL_ISO_MAX_FILES];
    size_t count;
    char directory[CLASSICSETUP_LOCAL_ISO_PATH_SIZE];
    char error[CLASSICSETUP_SOURCE_ERROR_SIZE];
};

void classicsetup_local_iso_catalog_reset(
    struct classicsetup_local_iso_catalog *catalog);

int classicsetup_local_iso_default_directory(
    char *directory, size_t directory_size);

int classicsetup_local_iso_scan(
    const char *directory,
    struct classicsetup_local_iso_catalog *catalog);

int classicsetup_local_iso_verify(
    const struct classicsetup_windows_release *release,
    const char *iso_path,
    struct classicsetup_workspace *workspace,
    atomic_bool *cancel_requested,
    classicsetup_download_progress_callback progress,
    void *progress_data,
    struct classicsetup_download_status *status,
    struct classicsetup_verified_windows_source *verified_source);

#endif
