#ifndef CLASSICSETUP_WORKSPACE_H
#define CLASSICSETUP_WORKSPACE_H

#include <stdbool.h>
#include <stddef.h>

enum { CLASSICSETUP_WORKSPACE_PATH_SIZE = 4096 };

enum classicsetup_artifact_type {
    CLASSICSETUP_ARTIFACT_TEMPORARY,
    CLASSICSETUP_ARTIFACT_PARTIAL_DOWNLOAD,
    CLASSICSETUP_ARTIFACT_CACHE,
    CLASSICSETUP_ARTIFACT_SOURCE_ISO,
    CLASSICSETUP_ARTIFACT_DEBUG
};

struct classicsetup_workspace {
    bool valid;
    bool verified_iso;
    char root_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char iso_final_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char iso_partial_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char metadata_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char debug_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
};

int classicsetup_workspace_create(struct classicsetup_workspace *workspace);

int classicsetup_workspace_available_bytes(
    const struct classicsetup_workspace *workspace,
    unsigned long long *available_bytes);

bool classicsetup_workspace_has_space(
    const struct classicsetup_workspace *workspace,
    unsigned long long expected_size,
    unsigned long long overhead_bytes,
    unsigned long long *available_bytes);

int classicsetup_workspace_promote_verified_iso(
    struct classicsetup_workspace *workspace);

void classicsetup_workspace_cleanup_cancel(
    struct classicsetup_workspace *workspace);

void classicsetup_workspace_cleanup_failure(
    struct classicsetup_workspace *workspace);

void classicsetup_workspace_cleanup_success(
    struct classicsetup_workspace *workspace);

void classicsetup_workspace_cleanup_after_install(
    struct classicsetup_workspace *workspace,
    bool keep_iso);

#endif
