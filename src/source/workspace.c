#define _POSIX_C_SOURCE 200809L

#include "classicsetup/workspace.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

static int set_path(char *target, size_t target_size,
                    const char *root, const char *name)
{
    int written = snprintf(target, target_size, "%s/%s", root, name);

    return written > 0 && (size_t)written < target_size ? 0 : -1;
}

static void remove_file(const char *path)
{
    if (path != NULL && path[0] != '\0') {
        (void)unlink(path);
    }
}

int classicsetup_workspace_create(struct classicsetup_workspace *workspace)
{
    char template_path[] = "/tmp/classicsetup-XXXXXX";
    char *root;

    if (workspace == NULL) {
        return -1;
    }
    memset(workspace, 0, sizeof(*workspace));
    root = mkdtemp(template_path);
    if (root == NULL || chmod(root, S_IRWXU) != 0 ||
        snprintf(workspace->root_path, sizeof(workspace->root_path),
                 "%s", root) < 0 ||
        set_path(workspace->iso_final_path,
                 sizeof(workspace->iso_final_path), root,
                 "windows.iso") != 0 ||
        set_path(workspace->iso_partial_path,
                 sizeof(workspace->iso_partial_path), root,
                 "windows.iso.part") != 0 ||
        set_path(workspace->metadata_path,
                 sizeof(workspace->metadata_path), root,
                 "source-metadata.tmp") != 0 ||
        set_path(workspace->debug_path,
                 sizeof(workspace->debug_path), root,
                 "debug.tmp") != 0) {
        if (root != NULL) {
            (void)rmdir(root);
        }
        memset(workspace, 0, sizeof(*workspace));
        return -1;
    }
    workspace->valid = true;
    return 0;
}

int classicsetup_workspace_available_bytes(
    const struct classicsetup_workspace *workspace,
    unsigned long long *available_bytes)
{
    struct statvfs info;

    if (workspace == NULL || !workspace->valid || available_bytes == NULL ||
        statvfs(workspace->root_path, &info) != 0) {
        return -1;
    }
    *available_bytes = (unsigned long long)info.f_bavail *
                       (unsigned long long)info.f_frsize;
    return 0;
}

bool classicsetup_workspace_has_space(
    const struct classicsetup_workspace *workspace,
    unsigned long long expected_size,
    unsigned long long overhead_bytes,
    unsigned long long *available_bytes)
{
    unsigned long long available;

    if (classicsetup_workspace_available_bytes(workspace, &available) != 0) {
        return false;
    }
    if (available_bytes != NULL) {
        *available_bytes = available;
    }
    if (expected_size > ULLONG_MAX - overhead_bytes) {
        return false;
    }
    return available >= expected_size + overhead_bytes;
}

int classicsetup_workspace_promote_verified_iso(
    struct classicsetup_workspace *workspace)
{
    if (workspace == NULL || !workspace->valid || workspace->verified_iso ||
        rename(workspace->iso_partial_path, workspace->iso_final_path) != 0) {
        return -1;
    }
    workspace->verified_iso = true;
    return 0;
}

void classicsetup_workspace_cleanup_cancel(
    struct classicsetup_workspace *workspace)
{
    if (workspace == NULL || !workspace->valid) {
        return;
    }
    remove_file(workspace->iso_partial_path);
    remove_file(workspace->metadata_path);
    remove_file(workspace->debug_path);
}

void classicsetup_workspace_cleanup_failure(
    struct classicsetup_workspace *workspace)
{
    classicsetup_workspace_cleanup_cancel(workspace);
}

void classicsetup_workspace_cleanup_success(
    struct classicsetup_workspace *workspace)
{
    if (workspace == NULL || !workspace->valid) {
        return;
    }
    remove_file(workspace->iso_partial_path);
    remove_file(workspace->metadata_path);
    remove_file(workspace->debug_path);
}

void classicsetup_workspace_cleanup_after_install(
    struct classicsetup_workspace *workspace,
    bool keep_iso)
{
    if (workspace == NULL || !workspace->valid) {
        return;
    }
    classicsetup_workspace_cleanup_success(workspace);
    if (!keep_iso) {
        remove_file(workspace->iso_final_path);
        workspace->verified_iso = false;
    }
    if (!workspace->verified_iso) {
        (void)rmdir(workspace->root_path);
        memset(workspace, 0, sizeof(*workspace));
    }
}
