#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "classicsetup/download.h"
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

static void test_download_backend_fallback(void)
{
#if !CLASSICSETUP_DOWNLOAD_ENABLED
    struct classicsetup_windows_release release = {0};
    struct classicsetup_workspace workspace = {0};
    struct classicsetup_download_status status;
    atomic_bool cancel_requested;

    atomic_init(&cancel_requested, false);
    assert(classicsetup_download_windows_iso(
               &release, &workspace, &cancel_requested,
               NULL, NULL, &status) != 0);
    assert(status.state == CLASSICSETUP_DOWNLOAD_FAILED);
    assert(status.error ==
           CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE);
#endif
}

int main(void)
{
    test_source_mapping_and_policy();
    test_workspace_and_verification();
    test_download_backend_fallback();
    return 0;
}
