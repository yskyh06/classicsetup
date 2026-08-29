#define _POSIX_C_SOURCE 200809L

#include "classicsetup/workspace.h"

#include <limits.h>
#include <dirent.h>
#include <errno.h>
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

static int path_is_owned(const struct classicsetup_workspace *workspace,
                         const char *path)
{
    size_t root_length;

    if (workspace == NULL || path == NULL || !workspace->valid) {
        return 0;
    }
    root_length = strlen(workspace->root_path);
    return root_length > 0 && strncmp(path, workspace->root_path,
                                      root_length) == 0 &&
           path[root_length] == '/';
}

static int remove_tree(const struct classicsetup_workspace *workspace,
                       const char *path)
{
    struct stat info;
    DIR *directory;
    struct dirent *entry;
    int failed = 0;

    if (!path_is_owned(workspace, path)) {
        return -1;
    }
    if (lstat(path, &info) != 0) {
        return errno == ENOENT ? 0 : -1;
    }
    if (!S_ISDIR(info.st_mode)) {
        return unlink(path);
    }
    directory = opendir(path);
    if (directory == NULL) {
        return -1;
    }
    while ((entry = readdir(directory)) != NULL) {
        char child[CLASSICSETUP_WORKSPACE_PATH_SIZE];
        int written;

        if (strcmp(entry->d_name, ".") == 0 ||
            strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        written = snprintf(child, sizeof(child), "%s/%s", path,
                           entry->d_name);
        if (written <= 0 || (size_t)written >= sizeof(child) ||
            remove_tree(workspace, child) != 0) {
            failed = 1;
        }
    }
    if (closedir(directory) != 0) {
        failed = 1;
    }
    if (rmdir(path) != 0) {
        failed = 1;
    }
    return failed ? -1 : 0;
}

static int create_private_directory(const char *path)
{
    return mkdir(path, S_IRWXU) == 0 && chmod(path, S_IRWXU) == 0 ? 0 : -1;
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
                 "%s", root) < 0) {
        if (root != NULL) {
            (void)rmdir(root);
        }
        memset(workspace, 0, sizeof(*workspace));
        return -1;
    }
    workspace->valid = true;
    if (
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
                 "debug.tmp") != 0 ||
        set_path(workspace->uup_path,
                 sizeof(workspace->uup_path), root, "uup") != 0 ||
        set_path(workspace->image_path,
                 sizeof(workspace->image_path), root, "image") != 0 ||
        set_path(workspace->wim_final_path,
                 sizeof(workspace->wim_final_path), root,
                 "install.wim") != 0 ||
        set_path(workspace->wim_partial_path,
                 sizeof(workspace->wim_partial_path), root,
                 "install.wim.part") != 0 ||
        set_path(workspace->iso_debug_path,
                 sizeof(workspace->iso_debug_path), root,
                 "windows-uup-debug.iso") != 0 ||
        create_private_directory(workspace->uup_path) != 0 ||
        create_private_directory(workspace->image_path) != 0) {
        if (root != NULL) {
            if (workspace->uup_path[0] != '\0') {
                (void)remove_tree(workspace, workspace->uup_path);
            }
            if (workspace->image_path[0] != '\0') {
                (void)remove_tree(workspace, workspace->image_path);
            }
            (void)rmdir(root);
        }
        memset(workspace, 0, sizeof(*workspace));
        return -1;
    }
    return 0;
}

int classicsetup_workspace_promote_verified_wim(
    struct classicsetup_workspace *workspace)
{
    if (workspace == NULL || !workspace->valid || workspace->verified_wim ||
        rename(workspace->wim_partial_path, workspace->wim_final_path) != 0) {
        return -1;
    }
    workspace->verified_wim = true;
    return 0;
}

int classicsetup_workspace_cleanup_uup_intermediates(
    struct classicsetup_workspace *workspace)
{
    int failed = 0;

    if (workspace == NULL || !workspace->valid) {
        return -1;
    }
    if (remove_tree(workspace, workspace->uup_path) != 0 ||
        remove_tree(workspace, workspace->image_path) != 0) {
        failed = 1;
    }
    if (create_private_directory(workspace->uup_path) != 0 ||
        create_private_directory(workspace->image_path) != 0) {
        failed = 1;
    }
    return failed ? -1 : 0;
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
    remove_file(workspace->wim_partial_path);
    remove_file(workspace->iso_debug_path);
    (void)classicsetup_workspace_cleanup_uup_intermediates(workspace);
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
    remove_file(workspace->wim_partial_path);
    (void)classicsetup_workspace_cleanup_uup_intermediates(workspace);
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
    remove_file(workspace->wim_final_path);
    workspace->verified_wim = false;
    remove_file(workspace->iso_debug_path);
    (void)remove_tree(workspace, workspace->uup_path);
    (void)remove_tree(workspace, workspace->image_path);
    if (!workspace->verified_iso && !workspace->verified_wim) {
        (void)rmdir(workspace->root_path);
        memset(workspace, 0, sizeof(*workspace));
    }
}
