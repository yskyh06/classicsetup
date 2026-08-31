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

#ifndef CLASSICSETUP_CONFIGURED_WORKSPACE_ROOT
#define CLASSICSETUP_CONFIGURED_WORKSPACE_ROOT ""
#endif

#define CLASSICSETUP_WORKSPACE_ENV "CLASSICSETUP_WORKSPACE_ROOT"

static int set_path(char *target, size_t target_size,
                    const char *root, const char *name)
{
    int written = snprintf(target, target_size, "%s/%s", root, name);

    return written > 0 && (size_t)written < target_size ? 0 : -1;
}

static int path_is_owned(const struct classicsetup_workspace *workspace,
                         const char *path);

static void remove_file(const struct classicsetup_workspace *workspace,
                        const char *path)
{
    if (path_is_owned(workspace, path)) {
        (void)unlink(path);
    }
}

static int path_is_owned(const struct classicsetup_workspace *workspace,
                         const char *path)
{
    size_t root_length;
    const char *component;

    if (workspace == NULL || path == NULL || !workspace->valid) {
        return 0;
    }
    root_length = strlen(workspace->root_path);
    if (root_length == 0 || strncmp(path, workspace->root_path,
                                    root_length) != 0 ||
        path[root_length] != '/') {
        return 0;
    }
    component = path + root_length + 1;
    while (*component != '\0') {
        const char *end = strchr(component, '/');
        size_t length = end == NULL ? strlen(component)
                                    : (size_t)(end - component);

        if (length == 0 || (length == 1 && component[0] == '.') ||
            (length == 2 && component[0] == '.' && component[1] == '.')) {
            return 0;
        }
        if (end == NULL) {
            break;
        }
        component = end + 1;
    }
    return 1;
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

static void diagnostics_reset(
    struct classicsetup_workspace_diagnostics *diagnostics,
    unsigned long long required_bytes)
{
    if (diagnostics != NULL) {
        memset(diagnostics, 0, sizeof(*diagnostics));
        diagnostics->required_bytes = required_bytes;
    }
}

static int base_directory_is_safe(const char *path)
{
    struct stat info;

    return path != NULL && path[0] == '/' &&
           lstat(path, &info) == 0 && S_ISDIR(info.st_mode) &&
           !S_ISLNK(info.st_mode) && access(path, W_OK | X_OK) == 0;
}

static int private_parent_path(const char *base_path,
                               char *path, size_t path_size)
{
    int written = snprintf(path, path_size, "%s/classicsetup-%lu",
                           base_path, (unsigned long)geteuid());

    return written > 0 && (size_t)written < path_size ? 0 : -1;
}

static int ensure_private_parent(const char *path)
{
    struct stat info;

    if (mkdir(path, S_IRWXU) == 0) {
        return chmod(path, S_IRWXU);
    }
    if (errno != EEXIST || lstat(path, &info) != 0 ||
        !S_ISDIR(info.st_mode) || S_ISLNK(info.st_mode) ||
        info.st_uid != geteuid() || (info.st_mode & 0777) != 0700) {
        return -1;
    }
    return 0;
}

static int available_at(const char *path, unsigned long long *available)
{
    struct statvfs info;

    if (path == NULL || available == NULL || statvfs(path, &info) != 0) {
        return -1;
    }
    *available = (unsigned long long)info.f_bavail *
                 (unsigned long long)info.f_frsize;
    return 0;
}

bool classicsetup_workspace_capacity_allows(
    unsigned long long available_bytes,
    unsigned long long required_bytes)
{
    return available_bytes >= required_bytes;
}

static int initialize_workspace_paths(
    struct classicsetup_workspace *workspace, const char *root)
{
    return set_path(workspace->iso_final_path,
                    sizeof(workspace->iso_final_path), root,
                    "windows.iso") == 0 &&
           set_path(workspace->iso_partial_path,
                    sizeof(workspace->iso_partial_path), root,
                    "windows.iso.part") == 0 &&
           set_path(workspace->metadata_path,
                    sizeof(workspace->metadata_path), root,
                    "source-metadata.tmp") == 0 &&
           set_path(workspace->debug_path,
                    sizeof(workspace->debug_path), root,
                    "debug.tmp") == 0 &&
           set_path(workspace->uup_path,
                    sizeof(workspace->uup_path), root, "uup") == 0 &&
           set_path(workspace->image_path,
                    sizeof(workspace->image_path), root, "image") == 0 &&
           set_path(workspace->wim_final_path,
                    sizeof(workspace->wim_final_path), root,
                    "install.wim") == 0 &&
           set_path(workspace->wim_partial_path,
                    sizeof(workspace->wim_partial_path), root,
                    "install.wim.part") == 0 &&
           set_path(workspace->iso_debug_path,
                    sizeof(workspace->iso_debug_path), root,
                    "windows-uup-debug.iso") == 0
               ? 0 : -1;
}

int classicsetup_workspace_create_in(
    struct classicsetup_workspace *workspace,
    const char *base_path,
    unsigned long long required_bytes,
    struct classicsetup_workspace_diagnostics *diagnostics)
{
    char private_parent[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char template_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    unsigned long long available = 0;
    struct stat info;
    char *root;
    int written;

    diagnostics_reset(diagnostics, required_bytes);
    if (workspace == NULL) {
        return CLASSICSETUP_WORKSPACE_CREATE_ERROR;
    }
    memset(workspace, 0, sizeof(*workspace));
    if (!base_directory_is_safe(base_path) ||
        private_parent_path(base_path, private_parent,
                            sizeof(private_parent)) != 0 ||
        ensure_private_parent(private_parent) != 0 ||
        available_at(private_parent, &available) != 0) {
        return CLASSICSETUP_WORKSPACE_CREATE_ERROR;
    }
    if (diagnostics != NULL) {
        (void)snprintf(diagnostics->root_path,
                       sizeof(diagnostics->root_path), "%s", base_path);
        diagnostics->available_bytes = available;
    }
    if (!classicsetup_workspace_capacity_allows(available, required_bytes)) {
        return CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE;
    }
    written = snprintf(template_path, sizeof(template_path),
                       "%s/workspace-XXXXXX", private_parent);
    if (written <= 0 || (size_t)written >= sizeof(template_path)) {
        return CLASSICSETUP_WORKSPACE_CREATE_ERROR;
    }
    root = mkdtemp(template_path);
    if (root == NULL || lstat(root, &info) != 0 || !S_ISDIR(info.st_mode) ||
        S_ISLNK(info.st_mode) || info.st_uid != geteuid() ||
        chmod(root, S_IRWXU) != 0 ||
        snprintf(workspace->base_path, sizeof(workspace->base_path), "%s",
                 base_path) <= 0 ||
        snprintf(workspace->root_path, sizeof(workspace->root_path), "%s",
                 root) <= 0) {
        if (root != NULL) {
            (void)rmdir(root);
        }
        memset(workspace, 0, sizeof(*workspace));
        return CLASSICSETUP_WORKSPACE_CREATE_ERROR;
    }
    workspace->valid = true;
    if (initialize_workspace_paths(workspace, root) != 0 ||
        create_private_directory(workspace->uup_path) != 0 ||
        create_private_directory(workspace->image_path) != 0 ||
        classicsetup_workspace_available_bytes(workspace, &available) != 0 ||
        !classicsetup_workspace_capacity_allows(available, required_bytes)) {
        if (workspace->uup_path[0] != '\0') {
            (void)remove_tree(workspace, workspace->uup_path);
        }
        if (workspace->image_path[0] != '\0') {
            (void)remove_tree(workspace, workspace->image_path);
        }
        (void)rmdir(root);
        memset(workspace, 0, sizeof(*workspace));
        if (diagnostics != NULL) {
            diagnostics->available_bytes = available;
        }
        return !classicsetup_workspace_capacity_allows(
                   available, required_bytes)
                   ? CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE
                   : CLASSICSETUP_WORKSPACE_CREATE_ERROR;
    }
    if (diagnostics != NULL) {
        (void)snprintf(diagnostics->root_path,
                       sizeof(diagnostics->root_path), "%s", root);
        diagnostics->available_bytes = available;
    }
    return CLASSICSETUP_WORKSPACE_CREATE_OK;
}

int classicsetup_workspace_create_for_reserve(
    struct classicsetup_workspace *workspace,
    unsigned long long required_bytes,
    struct classicsetup_workspace_diagnostics *diagnostics)
{
    static const char *const fallback_roots[] = {"/var/tmp", "/tmp"};
    const char *environment_root = getenv(CLASSICSETUP_WORKSPACE_ENV);
    const char *configured_root = CLASSICSETUP_CONFIGURED_WORKSPACE_ROOT;
    struct classicsetup_workspace_diagnostics candidate_diagnostics;
    struct classicsetup_workspace_diagnostics no_space_diagnostics;
    size_t index;
    bool saw_no_space = false;
    int result;

    if (workspace == NULL) {
        return CLASSICSETUP_WORKSPACE_CREATE_ERROR;
    }
    diagnostics_reset(&no_space_diagnostics, required_bytes);
    if (environment_root != NULL && environment_root[0] != '\0') {
        return classicsetup_workspace_create_in(
            workspace, environment_root, required_bytes, diagnostics);
    }
    if (configured_root[0] != '\0') {
        return classicsetup_workspace_create_in(
            workspace, configured_root, required_bytes, diagnostics);
    }
    for (index = 0;
         index < sizeof(fallback_roots) / sizeof(fallback_roots[0]); ++index) {
        result = classicsetup_workspace_create_in(
            workspace, fallback_roots[index], required_bytes,
            &candidate_diagnostics);
        if (result == CLASSICSETUP_WORKSPACE_CREATE_OK) {
            if (diagnostics != NULL) {
                *diagnostics = candidate_diagnostics;
            }
            return result;
        }
        if (result == CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE) {
            saw_no_space = true;
            no_space_diagnostics = candidate_diagnostics;
        }
    }
    if (diagnostics != NULL) {
        *diagnostics = no_space_diagnostics;
    }
    return saw_no_space ? CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE
                        : CLASSICSETUP_WORKSPACE_CREATE_ERROR;
}

int classicsetup_workspace_create(struct classicsetup_workspace *workspace)
{
    return classicsetup_workspace_create_for_reserve(workspace, 0, NULL);
}

int classicsetup_workspace_format_diagnostics(
    const struct classicsetup_workspace_diagnostics *diagnostics,
    char *output,
    size_t output_size)
{
    int written;

    if (diagnostics == NULL || output == NULL || output_size == 0 ||
        diagnostics->root_path[0] == '\0') {
        return -1;
    }
    written = snprintf(
        output, output_size,
        "workspace_root=%s available_bytes=%llu available_gib=%.2f "
        "required_bytes=%llu required_gib=%.2f",
        diagnostics->root_path, diagnostics->available_bytes,
        (double)diagnostics->available_bytes / (1024.0 * 1024.0 * 1024.0),
        diagnostics->required_bytes,
        (double)diagnostics->required_bytes / (1024.0 * 1024.0 * 1024.0));
    return written > 0 && (size_t)written < output_size ? 0 : -1;
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
    return classicsetup_workspace_capacity_allows(
        available, expected_size + overhead_bytes);
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

int classicsetup_workspace_retain_completed_iso(
    struct classicsetup_workspace *workspace)
{
    struct stat info;

    if (workspace == NULL || !workspace->valid || workspace->verified_iso ||
        workspace->diagnostic_iso_retained ||
        lstat(workspace->iso_partial_path, &info) != 0 ||
        !S_ISREG(info.st_mode) || S_ISLNK(info.st_mode) || info.st_size <= 0 ||
        rename(workspace->iso_partial_path, workspace->iso_final_path) != 0) {
        return -1;
    }
    workspace->diagnostic_iso_retained = true;
    return 0;
}

void classicsetup_workspace_cleanup_cancel(
    struct classicsetup_workspace *workspace)
{
    if (workspace == NULL || !workspace->valid) {
        return;
    }
    remove_file(workspace, workspace->iso_partial_path);
    remove_file(workspace, workspace->metadata_path);
    remove_file(workspace, workspace->debug_path);
    remove_file(workspace, workspace->wim_partial_path);
    remove_file(workspace, workspace->iso_debug_path);
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
    remove_file(workspace, workspace->iso_partial_path);
    remove_file(workspace, workspace->metadata_path);
    remove_file(workspace, workspace->debug_path);
    remove_file(workspace, workspace->wim_partial_path);
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
    if (!keep_iso && !workspace->diagnostic_iso_retained) {
        remove_file(workspace, workspace->iso_final_path);
        workspace->verified_iso = false;
    }
    remove_file(workspace, workspace->wim_final_path);
    workspace->verified_wim = false;
    remove_file(workspace, workspace->iso_debug_path);
    (void)remove_tree(workspace, workspace->uup_path);
    (void)remove_tree(workspace, workspace->image_path);
    if (!workspace->verified_iso && !workspace->verified_wim &&
        !workspace->diagnostic_iso_retained) {
        (void)rmdir(workspace->root_path);
        memset(workspace, 0, sizeof(*workspace));
    }
}
