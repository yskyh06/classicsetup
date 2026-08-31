#define _POSIX_C_SOURCE 200809L

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "classicsetup/download.h"
#include "classicsetup/retail.h"
#include "classicsetup/windows_source.h"
#include "classicsetup/workspace.h"

static const char LANDING[] =
    "<select id=\"product-edition\"><option value=\"null\">Select</option>"
    "<option value=\"3321\">Windows</option></select>"
    "<tr><td>Korean 64-bit</td><td>"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "</td></tr><tr><td>English 64-bit</td><td>"
    "BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB"
    "</td></tr><tr><td>Korean 32-bit</td><td>"
    "CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC"
    "</td></tr><tr><td>English 32-bit</td><td>"
    "DDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDDD"
    "</td></tr>";

static const char SKUS[] =
    "{\"ValidationContainer\":{\"Errors\":[]},\"Skus\":["
    "{\"Id\":\"20\",\"Language\":\"Korean\","
    "\"LocalizedLanguage\":\"Korean\","
    "\"ProductDisplayName\":\"Windows 11 25H2\"},"
    "{\"Id\":\"21\",\"Language\":\"English\","
    "\"LocalizedLanguage\":\"English\","
    "\"ProductDisplayName\":\"Windows 11 25H2\"}]}";

static void test_source_mapping_and_policy(void)
{
    struct classicsetup_source_catalog catalog;
    struct classicsetup_source_catalog windows10_catalog;
    char sanitized[256];

    assert(classicsetup_windows_source_parse_catalog(
               CLASSICSETUP_WINDOWS_11, LANDING, SKUS, &catalog) == 0);
    assert(catalog.state == CLASSICSETUP_SOURCE_READY);
    assert(catalog.release_count == 2);
    assert(catalog.releases[0].language ==
           CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN);
    assert(strcmp(catalog.releases[0].product_edition_id, "3321") == 0);
    assert(strcmp(catalog.releases[0].sku_id, "20") == 0);
    assert(catalog.releases[0].official_hash_available);
    assert(catalog.releases[0].architecture == CLASSICSETUP_ARCH_X64);
    assert(strcmp(catalog.releases[0].architecture_token, "x64") == 0);
    assert(strcmp(classicsetup_windows_architecture_label(
                      CLASSICSETUP_ARCH_X86),
                  "x86 (32-bit)") == 0);
    assert(strcmp(classicsetup_windows_architecture_token(
                      CLASSICSETUP_ARCH_ARM64),
                  "arm64") == 0);
    assert(classicsetup_windows_source_parse_catalog(
               CLASSICSETUP_WINDOWS_10,
               LANDING, SKUS, &windows10_catalog) == 0);
    assert(windows10_catalog.release_count == 4);
    assert(windows10_catalog.releases[0].architecture ==
           CLASSICSETUP_ARCH_X64);
    assert(windows10_catalog.releases[1].architecture ==
           CLASSICSETUP_ARCH_X86);
    assert(windows10_catalog.releases[1].official_hash_available);
    assert(classicsetup_windows_source_parse_download(
               "{\"ProductDownloadOptions\":[{\"DownloadType\":"
               "\"64-bit\",\"Uri\":\"https://software.download.prss."
               "microsoft.com/path/file.iso?token=secret\\u0026x=1\"}]}",
               &catalog.releases[0]) == 0);
    assert(catalog.releases[0].resolved);
    assert(classicsetup_windows_source_uri_is_official(
        catalog.releases[0].download_uri));
    assert(classicsetup_windows_source_sanitize_uri(
               catalog.releases[0].download_uri,
               sanitized, sizeof(sanitized)) == 0);
    assert(strchr(sanitized, '?') == NULL);
    assert(strstr(sanitized, "secret") == NULL);
    assert(!classicsetup_windows_source_uri_is_official("http://microsoft.com/a"));
    assert(!classicsetup_windows_source_uri_is_official("https://microsoft.com.evil/a"));
    assert(classicsetup_windows_source_parse_download(
               "{\"ProductDownloadOptions\":[{\"DownloadType\":"
               "\"64-bit\",\"Uri\":\"https://mirror.invalid/a.iso\"}]}",
               &catalog.releases[1]) != 0);
    catalog.releases[1].architecture = CLASSICSETUP_ARCH_X86;
    assert(classicsetup_windows_source_parse_download(
               "{\"ProductDownloadOptions\":["
               "{\"DownloadType\":\"ARM64\",\"Uri\":"
               "\"https://software.download.prss.microsoft.com/arm.iso\"},"
               "{\"DownloadType\":\"32-bit\",\"Uri\":"
               "\"https://software.download.prss.microsoft.com/x86.iso\"}]}",
               &catalog.releases[1]) == 0);
    assert(strstr(catalog.releases[1].download_uri, "x86.iso") != NULL);
    catalog.releases[1].resolved = false;
    catalog.releases[1].download_uri[0] = '\0';
    catalog.releases[1].architecture = CLASSICSETUP_ARCH_ARM64;
    assert(classicsetup_windows_source_parse_download(
               "{\"ProductDownloadOptions\":["
               "{\"DownloadType\":\"ARM64\",\"Uri\":"
               "\"https://software.download.prss.microsoft.com/arm.iso\"}]}",
               &catalog.releases[1]) == 0);
    assert(strstr(catalog.releases[1].download_uri, "arm.iso") != NULL);
    catalog.releases[1].resolved = false;
    catalog.releases[1].download_uri[0] = '\0';
    catalog.releases[1].architecture = CLASSICSETUP_ARCH_X86;
    assert(classicsetup_windows_source_parse_download(
               "{\"ProductDownloadOptions\":[{\"DownloadType\":"
               "\"ARM64\",\"Uri\":\"https://software.download.prss."
               "microsoft.com/arm.iso\"}]}",
               &catalog.releases[1]) != 0);
    assert(classicsetup_windows_source_parse_catalog(
               CLASSICSETUP_WINDOWS_11, LANDING,
               "{\"Errors\":[{\"Type\":8}]}", &catalog) != 0);
}

static void test_source_resolve_diagnostics(void)
{
    struct classicsetup_windows_release release = {0};
    struct classicsetup_source_resolve_diagnostics diagnostics;
    char formatted[1024];

    release.architecture = CLASSICSETUP_ARCH_X64;
    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"ProductDownloadOptions\":[{\"DownloadType\":"
               "\"64-bit\",\"Uri\":\"https://software.download."
               "prss.microsoft.com/windows.iso?token=hidden\"}]}",
               &release, &diagnostics) == 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_NONE);
    assert(diagnostics.response_is_json);
    assert(diagnostics.expected_field_present);
    assert(diagnostics.link_present);
    assert(strstr(release.download_uri, "token=hidden") != NULL);
    assert(classicsetup_windows_source_sanitize_uri(
               release.download_uri, diagnostics.endpoint,
               sizeof(diagnostics.endpoint)) == 0);
    diagnostics.session_id_present = true;
    diagnostics.cookie_engine_enabled = true;
    (void)snprintf(diagnostics.product_edition_id,
                   sizeof(diagnostics.product_edition_id), "%s", "3321");
    (void)snprintf(diagnostics.sku_id, sizeof(diagnostics.sku_id), "%s",
                   "20");
    assert(classicsetup_source_resolve_format_diagnostics(
               &diagnostics, formatted, sizeof(formatted)) == 0);
    assert(strstr(formatted, "stage=links") != NULL);
    assert(strstr(formatted, "error=none") != NULL);
    assert(strstr(formatted, "session=1") != NULL);
    assert(strstr(formatted, "cookies=1") != NULL);
    assert(strstr(formatted, "token") == NULL);
    assert(strstr(formatted, "hidden") == NULL);
    assert(strchr(formatted, '?') == NULL);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"ProductDownloadLinks\":[{\"url\":\"https://"
               "software.download.prss.microsoft.com/windows.iso\"}]}",
               &release, &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_SCHEMA_CHANGED);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"ProductDownloadOptions\":[{\"DownloadType\":"
               "\"64-bit\",\"Uri\":\"https://mirror.invalid/"
               "windows.iso\"}]}",
               &release, &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_POLICY_REJECTED);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"ProductDownloadOptions\":[{\"DownloadType\":"
               "\"32-bit\",\"Uri\":\"https://software.download."
               "prss.microsoft.com/windows.iso\"}]}",
               &release, &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_NO_LINK);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"Errors\":[{\"Type\":8,\"Key\":\"SentinelReject\"}]}",
               &release, &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"Errors\":[{\"Type\":429,\"Key\":\"TooManyRequests\"}]}",
               &release, &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "<html><body>Consent is required for this session.</body>"
               "</html>",
               &release, &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"ProductDownloadOptions\":[]}", &release,
               &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_NO_LINK);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "{\"ProductDownloadOptions\":[{\"DownloadType\":"
               "\"64-bit\"",
               &release, &diagnostics) != 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_MALFORMED);

    assert(classicsetup_windows_source_parse_download_diagnostic(
               "<html><body><a data-download-type=\"64-bit\" href=\""
               "https://software.download.prss.microsoft.com/windows.iso?"
               "signed=secret\">Download</a></body></html>",
               &release, &diagnostics) == 0);
    assert(diagnostics.error == CLASSICSETUP_SOURCE_RESOLVE_NONE);
    assert(diagnostics.response_is_html);
    assert(diagnostics.link_present);
    release.resolved = false;
    release.download_uri[0] = '\0';
    assert(classicsetup_windows_source_parse_download(
               "<a data-download-type=\"64-bit\" href=\"https://"
               "software.download.prss.microsoft.com/windows.iso\">"
               "Download</a>",
               &release) == 0);

    assert(classicsetup_source_resolve_classify_http(403, false) ==
           CLASSICSETUP_SOURCE_RESOLVE_HTTP_ERROR);
    assert(classicsetup_source_resolve_classify_http(429, false) ==
           CLASSICSETUP_SOURCE_RESOLVE_RATE_LIMITED);
    assert(classicsetup_source_resolve_classify_http(302, true) ==
           CLASSICSETUP_SOURCE_RESOLVE_REDIRECT_REJECTED);
    assert(classicsetup_source_resolve_classify_http(0, false) ==
           CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR);
    assert(strcmp(classicsetup_source_resolve_error_message(
                      CLASSICSETUP_SOURCE_RESOLVE_SESSION_REQUIRED),
                  "Microsoft requires an active source-download session.") ==
           0);
}

static void write_iso_fixture(const char *path, int valid)
{
    unsigned char zero[32768] = {0};
    FILE *file = fopen(path, "wb");
    const unsigned char descriptor[6] = {
        1, valid ? 'C' : 'B', 'D', '0', '0', '1'
    };

    assert(file != NULL);
    assert(fwrite(zero, 1, sizeof(zero), file) == sizeof(zero));
    assert(fwrite(descriptor, 1, sizeof(descriptor), file) ==
           sizeof(descriptor));
    assert(fclose(file) == 0);
}

static void test_workspace_and_verification(void)
{
    struct classicsetup_workspace first;
    struct classicsetup_workspace second;
    struct classicsetup_download_status status;
    unsigned long long available = 0;
    struct stat info;
#if CLASSICSETUP_DOWNLOAD_ENABLED
    enum classicsetup_download_error error;
#endif

    assert(classicsetup_workspace_create(&first) == 0);
    assert(classicsetup_workspace_create(&second) == 0);
    assert(strcmp(first.root_path, second.root_path) != 0);
    assert(stat(first.root_path, &info) == 0);
    assert((info.st_mode & 0777) == 0700);
    assert(classicsetup_workspace_available_bytes(&first, &available) == 0);
    assert(available > 0);
    write_iso_fixture(first.iso_partial_path, 1);
#if CLASSICSETUP_DOWNLOAD_ENABLED
    assert(classicsetup_verify_source_file(
               first.iso_partial_path, 32774, "", &error) == 0);
    assert(error == CLASSICSETUP_DOWNLOAD_ERROR_NONE);
    assert(classicsetup_verify_source_file(
               first.iso_partial_path, 1, "", &error) != 0);
    assert(error == CLASSICSETUP_DOWNLOAD_ERROR_SIZE);
    assert(classicsetup_verify_source_file(
               first.iso_partial_path, 32774,
               "0000000000000000000000000000000000000000000000000000000000000000",
               &error) != 0);
    assert(error == CLASSICSETUP_DOWNLOAD_ERROR_HASH);
#endif
    assert(classicsetup_workspace_promote_verified_iso(&first) == 0);
    assert(access(first.iso_partial_path, F_OK) != 0);
    assert(access(first.iso_final_path, F_OK) == 0);
    classicsetup_download_status_reset(&status);
    status.state = CLASSICSETUP_DOWNLOAD_COMPLETE;
    assert(classicsetup_download_is_ready(&status, &first));
    classicsetup_workspace_cleanup_after_install(&first, false);
    assert(!first.valid);
    write_iso_fixture(second.iso_partial_path, 0);
    classicsetup_workspace_cleanup_cancel(&second);
    assert(access(second.iso_partial_path, F_OK) != 0);
    classicsetup_workspace_cleanup_after_install(&second, false);
}

static void test_workspace_root_policy(void)
{
    char base_template[] = "/tmp/classicsetup-root-test-XXXXXX";
    char link_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char victim_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char saved_uup_path[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char private_parent[CLASSICSETUP_WORKSPACE_PATH_SIZE];
    char diagnostics_text[512];
    struct classicsetup_workspace workspace;
    struct classicsetup_workspace_diagnostics diagnostics;
    const char *base = mkdtemp(base_template);
    FILE *victim;
    int written;

    assert(base != NULL);
    assert(!classicsetup_workspace_capacity_allows(
        27ULL * 1024ULL * 1024ULL * 1024ULL / 10ULL,
        24ULL * 1024ULL * 1024ULL * 1024ULL));
    assert(classicsetup_workspace_capacity_allows(
        50ULL * 1024ULL * 1024ULL * 1024ULL,
        24ULL * 1024ULL * 1024ULL * 1024ULL));
    assert(chmod(base, 0700) == 0);
    assert(setenv("CLASSICSETUP_WORKSPACE_ROOT", base, 1) == 0);
    assert(classicsetup_workspace_create_for_reserve(
               &workspace, 1, &diagnostics) ==
           CLASSICSETUP_WORKSPACE_CREATE_OK);
    assert(strcmp(workspace.base_path, base) == 0);
    assert(strncmp(workspace.root_path, base, strlen(base)) == 0);
    assert(strcmp(diagnostics.root_path, workspace.root_path) == 0);
    assert(diagnostics.available_bytes >= 1);
    assert(diagnostics.required_bytes == 1);
    assert(classicsetup_workspace_format_diagnostics(
               &diagnostics, diagnostics_text,
               sizeof(diagnostics_text)) == 0);
    assert(strstr(diagnostics_text, "workspace_root=") != NULL);
    assert(strstr(diagnostics_text, "available_bytes=") != NULL);
    assert(strstr(diagnostics_text, "required_bytes=1") != NULL);

    written = snprintf(victim_path, sizeof(victim_path), "%s/victim", base);
    assert(written > 0 && (size_t)written < sizeof(victim_path));
    victim = fopen(victim_path, "wb");
    assert(victim != NULL);
    assert(fputs("keep", victim) >= 0);
    assert(fclose(victim) == 0);
    (void)strcpy(saved_uup_path, workspace.uup_path);
    (void)snprintf(workspace.uup_path, sizeof(workspace.uup_path), "%s",
                   victim_path);
    classicsetup_workspace_cleanup_cancel(&workspace);
    assert(access(victim_path, F_OK) == 0);
    (void)strcpy(workspace.uup_path, saved_uup_path);
    classicsetup_workspace_cleanup_after_install(&workspace, false);
    assert(unlink(victim_path) == 0);

    assert(classicsetup_workspace_create_in(
               &workspace, base, ULLONG_MAX, &diagnostics) ==
           CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE);
    assert(diagnostics.available_bytes < diagnostics.required_bytes);
    assert(setenv("CLASSICSETUP_WORKSPACE_ROOT", "relative/path", 1) == 0);
    assert(classicsetup_workspace_create_for_reserve(
               &workspace, 0, &diagnostics) ==
           CLASSICSETUP_WORKSPACE_CREATE_ERROR);

    written = snprintf(link_path, sizeof(link_path), "%s-link", base);
    assert(written > 0 && (size_t)written < sizeof(link_path));
    assert(symlink(base, link_path) == 0);
    assert(setenv("CLASSICSETUP_WORKSPACE_ROOT", link_path, 1) == 0);
    assert(classicsetup_workspace_create_for_reserve(
               &workspace, 0, &diagnostics) ==
           CLASSICSETUP_WORKSPACE_CREATE_ERROR);
    assert(unlink(link_path) == 0);
    assert(unsetenv("CLASSICSETUP_WORKSPACE_ROOT") == 0);

    written = snprintf(private_parent, sizeof(private_parent),
                       "%s/classicsetup-%lu", base,
                       (unsigned long)geteuid());
    assert(written > 0 && (size_t)written < sizeof(private_parent));
    assert(rmdir(private_parent) == 0);
    assert(rmdir(base) == 0);
}

static void test_download_backend_fallback(void)
{
#if !CLASSICSETUP_DOWNLOAD_ENABLED
    struct classicsetup_windows_release release = {0};
    struct classicsetup_workspace workspace = {0};
    struct classicsetup_download_status status;
    struct classicsetup_verified_windows_source verified_source;
    atomic_bool cancel_requested;

    atomic_init(&cancel_requested, false);
    assert(classicsetup_download_windows_iso(
               &release, &workspace, &cancel_requested,
               NULL, NULL, &status, &verified_source) != 0);
    assert(status.state == CLASSICSETUP_DOWNLOAD_FAILED);
    assert(status.error ==
           CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE);
#endif
}

static void test_retail_model_and_resolver(void)
{
#if CLASSICSETUP_DOWNLOAD_ENABLED
    struct classicsetup_source_catalog catalog;
    struct classicsetup_retail_status status;
    struct classicsetup_windows_release release;
    struct classicsetup_verified_windows_source verified_source;
    atomic_bool cancel_requested;
    char *arguments[22];
    char directory[] = "/tmp/classicsetup-retail-test-XXXXXX";
    char resolver[PATH_MAX];
    char wrong_script[PATH_MAX];
    char resolved_script[PATH_MAX];
    FILE *file;

    assert(classicsetup_retail_recommended_catalog(
               CLASSICSETUP_WINDOWS_11, &catalog) == 0);
    assert(catalog.release_count == 2);
    release = catalog.releases[0];
    assert(release.architecture == CLASSICSETUP_ARCH_X64);
    assert(release.language == CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN);
    assert(classicsetup_retail_build_fido_argv(
               "/usr/bin/pwsh", CLASSICSETUP_TEST_FIDO_SCRIPT,
               &release, arguments,
               sizeof(arguments) / sizeof(arguments[0])) == 0);
    assert(strcmp(arguments[1], "-NoLogo") == 0);
    assert(strcmp(arguments[5], CLASSICSETUP_TEST_FIDO_SCRIPT) == 0);
    assert(strcmp(arguments[15], "x64") == 0);
    assert(arguments[17] == NULL);
    release = catalog.releases[1];
    assert(release.language == CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH);
    assert(classicsetup_retail_build_fido_argv(
               "/usr/bin/pwsh", CLASSICSETUP_TEST_FIDO_SCRIPT,
               &release, arguments,
               sizeof(arguments) / sizeof(arguments[0])) == 0);
    assert(strcmp(arguments[13], "English") == 0);
    assert(classicsetup_retail_parse_wim_metadata(
               &release,
               "Name: Windows 11 Pro\nArchitecture: x86_64\n"
               "Default Language: en-US\n",
               "/private/windows.iso", &verified_source) == 0);
    release = catalog.releases[0];
    assert(classicsetup_retail_validate_script(
               CLASSICSETUP_TEST_FIDO_SCRIPT,
               CLASSICSETUP_TEST_FIDO_SHA256) == 0);
    assert(unsetenv("CLASSICSETUP_FIDO_SCRIPT") == 0);
    assert(classicsetup_retail_resolve_script(
               resolved_script, sizeof(resolved_script)) == 0);
    assert(strstr(resolved_script,
                  "/lib/classicsetup/tools/fido/fido-linux.ps1") != NULL);
    assert(classicsetup_retail_validate_script(
               resolved_script, CLASSICSETUP_TEST_FIDO_SHA256) == 0);

    assert(mkdtemp(directory) != NULL);
    assert(snprintf(resolver, sizeof(resolver), "%s/pwsh", directory) > 0);
    assert(snprintf(wrong_script, sizeof(wrong_script), "%s/wrong.ps1",
                    directory) > 0);
    file = fopen(resolver, "w");
    assert(file != NULL);
    assert(fputs("#!/bin/sh\nprintf '%s\\n' "
                 "'https://software.download.prss.microsoft.com/test.iso?token=secret'\n",
                 file) >= 0);
    assert(fclose(file) == 0);
    assert(chmod(resolver, 0700) == 0);

    assert(setenv("CLASSICSETUP_PWSH", "/does/not/exist/pwsh", 1) == 0);
    release = catalog.releases[0];
    atomic_init(&cancel_requested, false);
    assert(classicsetup_retail_resolve(
               &release, &cancel_requested, NULL, NULL, &status) != 0);
    assert(status.error == CLASSICSETUP_RETAIL_ERROR_PWSH_MISSING);

    assert(setenv("CLASSICSETUP_PWSH", resolver, 1) == 0);
    assert(setenv("CLASSICSETUP_FIDO_SCRIPT", "/does/not/exist/fido.ps1", 1) == 0);
    release = catalog.releases[0];
    assert(classicsetup_retail_resolve(
               &release, &cancel_requested, NULL, NULL, &status) != 0);
    assert(status.error == CLASSICSETUP_RETAIL_ERROR_SCRIPT_MISSING);

    file = fopen(wrong_script, "w");
    assert(file != NULL);
    assert(fputs("not the pinned resolver\n", file) >= 0);
    assert(fclose(file) == 0);
    assert(setenv("CLASSICSETUP_FIDO_SCRIPT", wrong_script, 1) == 0);
    release = catalog.releases[0];
    assert(classicsetup_retail_resolve(
               &release, &cancel_requested, NULL, NULL, &status) != 0);
    assert(status.error == CLASSICSETUP_RETAIL_ERROR_SCRIPT_HASH);

    assert(setenv("CLASSICSETUP_FIDO_SCRIPT",
                  CLASSICSETUP_TEST_FIDO_SCRIPT, 1) == 0);
    file = fopen(resolver, "w");
    assert(file != NULL);
    assert(fputs("#!/bin/sh\nexit 9\n", file) >= 0);
    assert(fclose(file) == 0);
    release = catalog.releases[0];
    assert(classicsetup_retail_resolve(
               &release, &cancel_requested, NULL, NULL, &status) != 0);
    assert(status.error == CLASSICSETUP_RETAIL_ERROR_PROCESS);
    assert(status.child_exit_status == 9);

    file = fopen(resolver, "w");
    assert(file != NULL);
    assert(fputs("#!/bin/sh\nprintf 'first\\nsecond\\n'\n", file) >= 0);
    assert(fclose(file) == 0);
    release = catalog.releases[0];
    assert(classicsetup_retail_resolve(
               &release, &cancel_requested, NULL, NULL, &status) != 0);
    assert(status.error == CLASSICSETUP_RETAIL_ERROR_NO_LINK);

    file = fopen(resolver, "w");
    assert(file != NULL);
    assert(fputs("#!/bin/sh\nprintf '%s\\n' "
                 "'https://software.download.prss.microsoft.com/test.iso?token=secret'\n",
                 file) >= 0);
    assert(fclose(file) == 0);
    release = catalog.releases[0];
    assert(classicsetup_retail_resolve(
               &release, &cancel_requested, NULL, NULL, &status) == 0);
    assert(release.resolved);
    assert(strcmp(status.delivery_host,
                  "software.download.prss.microsoft.com") == 0);
    assert(strstr(status.detail, "secret") == NULL);
    assert(classicsetup_retail_parse_wim_metadata(
               &release,
               "Name: Windows 11 Pro\n"
               "Architecture: x86_64\n"
               "Default Language: ko-KR\n"
               "Edition ID: Professional\n"
               "Build: 26100\n"
               "Service Pack Build: 1\n",
               "/private/windows.iso", &verified_source) == 0);
    assert(verified_source.verified);
    assert(strcmp(verified_source.build, "26100.1") == 0);
    assert(strcmp(verified_source.edition, "Professional") == 0);
    assert(verified_source.architecture == CLASSICSETUP_ARCH_X64);
    assert(classicsetup_retail_parse_wim_metadata(
               &release,
               "Name: Windows 11 Pro\nArchitecture: ARM64\n"
               "Default Language: ko-KR\n",
               "/private/windows.iso", &verified_source) != 0);
    memset(release.download_uri, 0, sizeof(release.download_uri));
    release.resolved = false;
    assert(unsetenv("CLASSICSETUP_PWSH") == 0);
    assert(unsetenv("CLASSICSETUP_FIDO_SCRIPT") == 0);
    assert(unlink(wrong_script) == 0);
    assert(unlink(resolver) == 0);
    assert(rmdir(directory) == 0);
#endif
}

int main(void)
{
    test_source_mapping_and_policy();
    test_source_resolve_diagnostics();
    test_workspace_and_verification();
    test_workspace_root_policy();
    test_download_backend_fallback();
    test_retail_model_and_resolver();
    return 0;
}
