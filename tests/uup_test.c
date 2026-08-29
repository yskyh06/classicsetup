#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#include "classicsetup/uup.h"

static struct classicsetup_uup_target poc_target(void)
{
    struct classicsetup_uup_target target = {0};

    assert(classicsetup_uup_recommended_target(&target) == 0);
    return target;
}

static void test_manifest_and_model(void)
{
    const struct classicsetup_uup_tool_manifest *manifest =
        classicsetup_uup_tool_manifest();
    struct classicsetup_uup_target target = poc_target();
    struct classicsetup_process_result process = {0};

    assert(strcmp(manifest->version, "3.1.9.3") == 0);
    assert(strcmp(manifest->commit,
                  "e0c4ce00dc5415bb0441e599aa9a86a2f6021707") == 0);
    assert(strlen(manifest->linux_x64_archive_sha256) == 64);
    assert(manifest->converter_requires_iso);
    assert(!manifest->direct_wim_output);
    assert(strcmp(target.reporting_version, "10.0.22631.1") == 0);
    assert(strcmp(target.flight_ring, "Retail") == 0);
    assert(classicsetup_uup_target_is_supported(&target));
    assert(strcmp(classicsetup_uup_architecture_token(
                      CLASSICSETUP_ARCH_X64), "amd64") == 0);
    assert(strcmp(classicsetup_uup_architecture_token(
                      CLASSICSETUP_ARCH_X86), "x86") == 0);
    assert(strcmp(classicsetup_uup_architecture_token(
                      CLASSICSETUP_ARCH_ARM64), "arm64") == 0);
    assert(strcmp(classicsetup_uup_language_token(
                      CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN), "ko-KR") == 0);
    target.current_branch[0] = ';';
    assert(!classicsetup_uup_target_is_supported(&target));
    (void)strcpy(process.output, "You must install .NET to run this application.");
    assert(classicsetup_uup_classify_process_failure(
               &process, CLASSICSETUP_UUP_ERROR_DISCOVERY_FAILED) ==
           CLASSICSETUP_UUP_ERROR_RUNTIME_MISSING);
    process.output[0] = '\0';
    assert(classicsetup_uup_classify_process_failure(
               &process, CLASSICSETUP_UUP_ERROR_DISCOVERY_FAILED) ==
           CLASSICSETUP_UUP_ERROR_DISCOVERY_FAILED);
}

static int has_argument(char *arguments[], const char *value)
{
    size_t index;

    for (index = 0; arguments[index] != NULL; ++index) {
        if (strcmp(arguments[index], value) == 0) {
            return 1;
        }
    }
    return 0;
}

static void test_argv_and_parser(void)
{
    struct classicsetup_workspace workspace;
    struct classicsetup_uup_target target = poc_target();
    struct classicsetup_uup_release releases[4];
    char *arguments[CLASSICSETUP_UUP_ARG_COUNT];
    size_t count = 0;

    assert(classicsetup_workspace_create(&workspace) == 0);
    assert(classicsetup_uup_build_discovery_argv(
               "/tool/UUPDownload", &target, arguments) == 0);
    assert(strcmp(arguments[0], "/tool/UUPDownload") == 0);
    assert(has_argument(arguments, "get-builds"));
    assert(has_argument(arguments, "amd64"));
    assert(classicsetup_uup_build_download_argv(
               "/tool/UUPDownload", &target, &workspace, arguments) == 0);
    assert(has_argument(arguments, "request-download"));
    assert(has_argument(arguments, workspace.uup_path));
    assert(has_argument(arguments, "Professional"));
    assert(has_argument(arguments, "ko-KR"));
    assert(strlen(workspace.uup_path) + strlen("/validated-payload") + 1 <
           sizeof(workspace.uup_payload_path));
    (void)strcpy(workspace.uup_payload_path, workspace.uup_path);
    (void)strcat(workspace.uup_payload_path, "/validated-payload");
    assert(classicsetup_uup_build_converter_argv(
               "/tool/UUPMediaConverter", &target, &workspace,
               arguments) == 0);
    assert(has_argument(arguments, "desktop-convert"));
    assert(has_argument(arguments, workspace.uup_payload_path));
    assert(has_argument(arguments, workspace.iso_partial_path));
    assert(has_argument(arguments, workspace.image_path));

    memset(releases, 0, sizeof(releases));
    assert(classicsetup_uup_parse_builds(
               "logo\n\"Release Preview\"[0]=\"10.0.26100.1\"\n"
               "\"Retail (VB)\"[0]=\"10.0.19045.1\"\n",
               releases, 4, &count) == 0);
    assert(count == 2);
    assert(strcmp(releases[0].ring, "Release Preview") == 0);
    assert(strcmp(releases[0].build, "10.0.26100.1") == 0);
    assert(classicsetup_uup_parse_builds(
               "unexpected output", releases, 4, &count) != 0);
    classicsetup_workspace_cleanup_after_install(&workspace, false);
}

static void write_sparse_file(const char *path, off_t size)
{
    int descriptor = open(path, O_CREAT | O_WRONLY | O_TRUNC, S_IRUSR | S_IWUSR);

    assert(descriptor >= 0);
    assert(ftruncate(descriptor, size) == 0);
    assert(close(descriptor) == 0);
}

static void test_recommended_catalog_and_payload(void)
{
    struct classicsetup_source_catalog catalog;
    struct classicsetup_workspace workspace;
    struct classicsetup_uup_payload_summary summary;
    char payload[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    size_t index;

#if CLASSICSETUP_ENABLE_UUP
    assert(classicsetup_uup_recommended_catalog(
               CLASSICSETUP_WINDOWS_11, &catalog) == 0);
    assert(catalog.state == CLASSICSETUP_SOURCE_READY);
    assert(catalog.release_count == 1);
    assert(catalog.releases[0].edition ==
           CLASSICSETUP_WINDOWS_EDITION_PROFESSIONAL);
    assert(strcmp(catalog.releases[0].edition_name,
                  "Windows 11 Pro") == 0);
#else
    assert(classicsetup_uup_recommended_catalog(
               CLASSICSETUP_WINDOWS_11, &catalog) != 0);
#endif
    assert(classicsetup_workspace_create(&workspace) == 0);
    assert(snprintf(payload, sizeof(payload), "%s/%s", workspace.uup_path,
                    "10.0.26200.9168_amd64fre") > 0);
    assert(mkdir(payload, S_IRWXU) == 0);
    assert(snprintf(path, sizeof(path), "%s/%s", payload,
                    "test.AggregatedMetadata.cab") > 0);
    write_sparse_file(path, 1);
    assert(snprintf(path, sizeof(path), "%s/%s", payload,
                    "Windows 11.uupmcreplay") > 0);
    write_sparse_file(path, CLASSICSETUP_UUP_MIN_PAYLOAD_BYTES);
    assert(snprintf(path, sizeof(path), "%s/%s", payload,
                    "professional_ko-kr.esd") > 0);
    write_sparse_file(path, 1);
    for (index = 0; index < 5; ++index) {
        assert(snprintf(path, sizeof(path), "%s/payload-%zu.esd",
                        payload, index) > 0);
        write_sparse_file(path, 1);
    }
    assert(classicsetup_uup_inspect_payload(&workspace, &summary) == 0);
    assert(summary.file_count == CLASSICSETUP_UUP_MIN_PAYLOAD_FILES);
    assert(summary.total_bytes >= CLASSICSETUP_UUP_MIN_PAYLOAD_BYTES);
    assert(strcmp(workspace.uup_payload_path, payload) == 0);
    classicsetup_workspace_cleanup_after_install(&workspace, false);
}

static void test_dotnet_runtime_resolution(void)
{
    struct classicsetup_workspace workspace;
    char resolved[CLASSICSETUP_WORKSPACE_PATH_SIZE];

    assert(classicsetup_workspace_create(&workspace) == 0);
    assert(classicsetup_uup_resolve_dotnet_root(
               workspace.root_path, "/not-used", resolved,
               sizeof(resolved)) == 0);
    assert(strcmp(resolved, workspace.root_path) == 0);
    assert(classicsetup_uup_resolve_dotnet_root(
               "", workspace.root_path, resolved, sizeof(resolved)) == 0);
    assert(strcmp(resolved, workspace.root_path) == 0);
    assert(classicsetup_uup_resolve_dotnet_root(
               "", "", resolved, sizeof(resolved)) == 0);
    assert(resolved[0] == '\0');
    assert(classicsetup_uup_resolve_dotnet_root(
               "relative/runtime", "", resolved, sizeof(resolved)) != 0);
    classicsetup_workspace_cleanup_after_install(&workspace, false);
}

static void write_bootable_iso_fixture(const char *path)
{
    unsigned char descriptor[2048] = {0};
    FILE *file = fopen(path, "wb");

    assert(file != NULL);
    assert(fseeko(file, 16 * 2048, SEEK_SET) == 0);
    descriptor[0] = 1;
    memcpy(descriptor + 1, "CD001", 5);
    assert(fwrite(descriptor, 1, sizeof(descriptor), file) ==
           sizeof(descriptor));
    memset(descriptor, 0, sizeof(descriptor));
    memcpy(descriptor + 1, "CD001", 5);
    memcpy(descriptor + 7, "EL TORITO SPECIFICATION", 23);
    assert(fwrite(descriptor, 1, sizeof(descriptor), file) ==
           sizeof(descriptor));
    memset(descriptor, 0, sizeof(descriptor));
    descriptor[0] = 255;
    memcpy(descriptor + 1, "CD001", 5);
    assert(fwrite(descriptor, 1, sizeof(descriptor), file) ==
           sizeof(descriptor));
    assert(fclose(file) == 0);
}

static void test_iso_and_actual_image_metadata(const char *self_path)
{
    struct classicsetup_workspace workspace;
    struct classicsetup_uup_target target = poc_target();
    struct classicsetup_verified_windows_source source;
    struct classicsetup_process_result result;

    assert(classicsetup_workspace_create(&workspace) == 0);
    write_bootable_iso_fixture(workspace.iso_partial_path);
    assert(classicsetup_uup_verify_iso(workspace.iso_partial_path) == 0);
    assert(classicsetup_uup_verify_iso("/does/not/exist.iso") != 0);
    assert(classicsetup_uup_extract_and_verify_image(
               &target, &workspace, self_path, self_path, NULL, NULL,
               &source, &result) == 0);
    assert(source.verified);
    assert(source.backend ==
           CLASSICSETUP_SOURCE_MICROSOFT_UUP);
    assert(source.kind == CLASSICSETUP_VERIFIED_SOURCE_ISO);
    assert(strcmp(source.build, "26100.1") == 0);
    assert(strcmp(source.edition, "Professional") == 0);
    assert(strstr(source.build, "26200") == NULL);
    classicsetup_workspace_cleanup_after_install(&workspace, false);
}

static void write_wim(const char *path, bool valid)
{
    unsigned char data[CLASSICSETUP_UUP_MIN_WIM_SIZE] = {0};
    FILE *file;

    if (valid) {
        memcpy(data, "MSWIM\0\0\0", 8);
    }
    file = fopen(path, "wb");
    assert(file != NULL);
    assert(fwrite(data, 1, sizeof(data), file) == sizeof(data));
    assert(fclose(file) == 0);
    assert(chmod(path, S_IRUSR | S_IWUSR) == 0);
}

static void test_workspace_wim_and_cleanup(const char *self_path)
{
    struct classicsetup_workspace workspace;
    struct classicsetup_uup_target target = poc_target();
    struct classicsetup_verified_windows_source source;
    struct classicsetup_process_result result;
    struct stat info;
    char nested[CLASSICSETUP_WORKSPACE_PATH_SIZE];

    assert(classicsetup_workspace_create(&workspace) == 0);
    assert(stat(workspace.uup_path, &info) == 0 && S_ISDIR(info.st_mode));
    assert((info.st_mode & 0777) == 0700);
    assert(strlen(workspace.uup_path) + strlen("/payload.tmp") + 1 <
           sizeof(nested));
    (void)strcpy(nested, workspace.uup_path);
    (void)strcat(nested, "/payload.tmp");
    write_wim(nested, false);
    assert(classicsetup_workspace_cleanup_uup_intermediates(&workspace) == 0);
    assert(access(nested, F_OK) != 0);

    write_wim(workspace.wim_partial_path, false);
    assert(classicsetup_uup_verify_wim_signature(
               workspace.wim_partial_path) != 0);
    assert(unlink(workspace.wim_partial_path) == 0);
    assert(classicsetup_uup_verify_wim_signature(
               workspace.wim_partial_path) != 0);
    write_wim(workspace.wim_partial_path, true);
    assert(classicsetup_uup_verify_wim_signature(
               workspace.wim_partial_path) == 0);
    assert(classicsetup_uup_verify_wim(
               workspace.wim_partial_path, self_path, &result) == 0);
    assert(classicsetup_workspace_promote_verified_wim(&workspace) == 0);
    assert(classicsetup_uup_register_verified_wim(
               &target, &workspace, &source) == 0);
    assert(source.backend ==
           CLASSICSETUP_SOURCE_MICROSOFT_UUP);
    assert(source.kind == CLASSICSETUP_VERIFIED_SOURCE_WIM);
    assert(source.verified && source.image_index == 1);
    assert(strcmp(source.edition, "Professional") == 0);
    assert(strcmp(source.path, workspace.wim_final_path) == 0);
    classicsetup_workspace_cleanup_after_install(&workspace, false);
    assert(!workspace.valid);
}

static bool cancel_now(void *context)
{
    bool *called = context;

    *called = true;
    return true;
}

static void test_shell_free_process(const char *self_path)
{
    struct classicsetup_process_result result;
    char *arguments[] = {(char *)self_path, "--wait", NULL};
    bool called = false;

    assert(classicsetup_uup_validate_tool(self_path) == 0);
#if CLASSICSETUP_ENABLE_UUP
    assert(classicsetup_uup_run(self_path, arguments, cancel_now,
                                &called, &result) == 1);
    assert(called);
    assert(result.signaled || result.exited);
#else
    assert(classicsetup_uup_run(self_path, arguments, cancel_now,
                                &called, &result) == -1);
    assert(!called);
#endif
}

static void test_process_tail_capture(const char *self_path)
{
    struct classicsetup_process_result result;
    char *arguments[] = {(char *)self_path, "--tail", NULL};

    assert(classicsetup_run_process_cancellable(
               self_path, arguments, NULL, NULL, &result) == 0);
    assert(result.exited && result.exit_status == 0);
    assert(strstr(result.output, "Completed.") != NULL);
}

int main(int argc, char **argv)
{
    char self_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    ssize_t length;

    if (argc > 1 && strcmp(argv[1], "info") == 0) {
        (void)puts(
            "WIM Information:\n"
            "Image Count: 1\n"
            "Name: Windows 11 Pro\n"
            "Architecture: x86_64\n"
            "Edition ID: Professional\n"
            "Default Language: ko-KR\n"
            "Build: 26100\n"
            "Service Pack Build: 1");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "verify") == 0) {
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "e") == 0 && argc > 5) {
        char output[CLASSICSETUP_WORKSPACE_PATH_SIZE];
        const char *name = strstr(argv[5], "install.wim") != NULL
                               ? "install.wim" : "install.esd";

        assert(strncmp(argv[3], "-o", 2) == 0);
        assert(snprintf(output, sizeof(output), "%s/%s", argv[3] + 2,
                        name) > 0);
        write_wim(output, true);
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--tail") == 0) {
        size_t index;

        for (index = 0; index < 4096; ++index) {
            (void)putchar('x');
        }
        (void)puts("\nCompleted.");
        return 0;
    }
    if (argc > 1 && strcmp(argv[1], "--wait") == 0) {
        const struct timespec delay = {5, 0};
        (void)nanosleep(&delay, NULL);
        return 0;
    }
    length = readlink("/proc/self/exe", self_path, sizeof(self_path) - 1);
    assert(length > 0 && (size_t)length < sizeof(self_path));
    self_path[length] = '\0';

    test_manifest_and_model();
    test_argv_and_parser();
    test_recommended_catalog_and_payload();
    test_dotnet_runtime_resolution();
    test_workspace_wim_and_cleanup(self_path);
    test_iso_and_actual_image_metadata(self_path);
    test_shell_free_process(self_path);
    test_process_tail_capture(self_path);
    (void)puts("uup backend tests passed");
    return 0;
}
