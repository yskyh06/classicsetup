#define _POSIX_C_SOURCE 200809L

#include "classicsetup/download.h"
#include "classicsetup/retail.h"

#include <curl/curl.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static const unsigned long long DOWNLOAD_SPACE_FALLBACK =
    8ULL * 1024ULL * 1024ULL * 1024ULL;
static const unsigned long long DOWNLOAD_SPACE_OVERHEAD =
    512ULL * 1024ULL * 1024ULL;

struct transfer_context {
    FILE *file;
    atomic_bool *cancel_requested;
    classicsetup_download_progress_callback progress;
    void *progress_data;
    struct classicsetup_download_status *status;
    struct timespec started;
    struct timespec last_update;
};

static double elapsed_seconds(const struct timespec *start,
                              const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}

static void notify(struct transfer_context *context)
{
    if (context->progress != NULL) {
        context->progress(context->status, context->progress_data);
    }
}

static size_t write_file(char *data, size_t size, size_t count,
                         void *user_data)
{
    struct transfer_context *context = user_data;

    if (size != 0 && count > SIZE_MAX / size) {
        return 0;
    }
    return fwrite(data, 1, size * count, context->file);
}

static int transfer_progress(void *user_data, curl_off_t total,
                             curl_off_t current, curl_off_t upload_total,
                             curl_off_t upload_current)
{
    struct transfer_context *context = user_data;
    struct timespec now;
    double elapsed;

    (void)upload_total;
    (void)upload_current;
    if (context->cancel_requested != NULL &&
        atomic_load(context->cancel_requested)) {
        return 1;
    }
    (void)clock_gettime(CLOCK_MONOTONIC, &now);
    context->status->bytes_received = current > 0
                                          ? (unsigned long long)current : 0;
    context->status->total_bytes = total > 0
                                       ? (unsigned long long)total : 0;
    context->status->progress_fraction = total > 0
        ? (double)current / (double)total : 0.0;
    elapsed = elapsed_seconds(&context->started, &now);
    context->status->bytes_per_second = elapsed > 0.0
        ? (double)context->status->bytes_received / elapsed : 0.0;
    if (elapsed_seconds(&context->last_update, &now) >= 0.2) {
        context->last_update = now;
        notify(context);
    }
    return 0;
}

static int file_sha256(const char *path, char output[65])
{
    unsigned char buffer[65536];
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_size = 0;
    EVP_MD_CTX *context = NULL;
    FILE *file = NULL;
    size_t read_size;
    size_t index;
    int result = -1;

    file = fopen(path, "rb");
    context = EVP_MD_CTX_new();
    if (file == NULL || context == NULL ||
        EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1) {
        goto done;
    }
    while ((read_size = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        if (EVP_DigestUpdate(context, buffer, read_size) != 1) {
            goto done;
        }
    }
    if (ferror(file) ||
        EVP_DigestFinal_ex(context, digest, &digest_size) != 1 ||
        digest_size != 32) {
        goto done;
    }
    for (index = 0; index < digest_size; ++index) {
        (void)snprintf(output + index * 2, 3, "%02X", digest[index]);
    }
    output[64] = '\0';
    result = 0;
done:
    if (file != NULL) {
        (void)fclose(file);
    }
    EVP_MD_CTX_free(context);
    return result;
}

int classicsetup_verify_source_file(
    const char *path, unsigned long long expected_size,
    const char *expected_sha256, enum classicsetup_download_error *error)
{
    struct stat info;
    unsigned char descriptor[6];
    char actual_hash[65];
    FILE *file;

    if (error != NULL) {
        *error = CLASSICSETUP_DOWNLOAD_ERROR_ISO;
    }
    if (path == NULL || stat(path, &info) != 0 || !S_ISREG(info.st_mode)) {
        return -1;
    }
    if (expected_size != 0 &&
        (unsigned long long)info.st_size != expected_size) {
        if (error != NULL) {
            *error = CLASSICSETUP_DOWNLOAD_ERROR_SIZE;
        }
        return -1;
    }
    file = fopen(path, "rb");
    if (file == NULL || fseeko(file, 32768, SEEK_SET) != 0 ||
        fread(descriptor, 1, sizeof(descriptor), file) != sizeof(descriptor)) {
        if (file != NULL) {
            (void)fclose(file);
        }
        return -1;
    }
    (void)fclose(file);
    if (memcmp(descriptor + 1, "CD001", 5) != 0) {
        return -1;
    }
    if (expected_sha256 != NULL && expected_sha256[0] != '\0' &&
        (file_sha256(path, actual_hash) != 0 ||
         strcasecmp(actual_hash, expected_sha256) != 0)) {
        if (error != NULL) {
            *error = CLASSICSETUP_DOWNLOAD_ERROR_HASH;
        }
        return -1;
    }
    if (error != NULL) {
        *error = CLASSICSETUP_DOWNLOAD_ERROR_NONE;
    }
    return 0;
}

static void fail_status(struct classicsetup_download_status *status,
                        enum classicsetup_download_error error,
                        const char *message)
{
    status->state = error == CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED
                        ? CLASSICSETUP_DOWNLOAD_CANCELLED
                        : CLASSICSETUP_DOWNLOAD_FAILED;
    status->error = error;
    (void)snprintf(status->message, sizeof(status->message), "%s", message);
}

static bool download_cancel_callback(void *context)
{
    return context != NULL && atomic_load((atomic_bool *)context);
}

int classicsetup_download_windows_iso(
    const struct classicsetup_windows_release *release,
    struct classicsetup_workspace *workspace,
    atomic_bool *cancel_requested,
    classicsetup_download_progress_callback progress,
    void *progress_data,
    struct classicsetup_download_status *status,
    struct classicsetup_verified_windows_source *verified_source)
{
    struct transfer_context context = {0};
    unsigned long long required_size;
    unsigned long long available = 0;
    enum classicsetup_download_error verify_error;
    CURL *curl = NULL;
    CURLcode curl_result;
    long response_code = 0;
    char *effective_uri = NULL;
    int file_descriptor = -1;
    int result = -1;
    bool curl_initialized = false;
    struct classicsetup_process_result inspect_result;
    int inspect_status;

    if (release == NULL || workspace == NULL || !workspace->valid ||
        status == NULL || verified_source == NULL || !release->resolved ||
        !classicsetup_windows_source_uri_is_official(release->download_uri)) {
        return -1;
    }
    classicsetup_download_status_reset(status);
    memset(verified_source, 0, sizeof(*verified_source));
    status->state = CLASSICSETUP_DOWNLOAD_PREPARING;
    required_size = release->expected_size != 0
                        ? release->expected_size : DOWNLOAD_SPACE_FALLBACK;
    if (!classicsetup_workspace_has_space(workspace, required_size,
                                          DOWNLOAD_SPACE_OVERHEAD,
                                          &available)) {
        (void)available;
        fail_status(status, CLASSICSETUP_DOWNLOAD_ERROR_OUT_OF_SPACE,
                    "Not enough temporary storage space.");
        goto done;
    }
    classicsetup_workspace_cleanup_cancel(workspace);
    file_descriptor = open(workspace->iso_partial_path,
                           O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (file_descriptor < 0 ||
        (context.file = fdopen(file_descriptor, "wb")) == NULL) {
        if (file_descriptor >= 0 && context.file == NULL) {
            (void)close(file_descriptor);
        }
        fail_status(status, CLASSICSETUP_DOWNLOAD_ERROR_WRITE,
                    "The temporary download file could not be created.");
        goto done;
    }
    file_descriptor = -1;
    context.cancel_requested = cancel_requested;
    context.progress = progress;
    context.progress_data = progress_data;
    context.status = status;
    (void)clock_gettime(CLOCK_MONOTONIC, &context.started);
    context.last_update = context.started;
    status->state = CLASSICSETUP_DOWNLOAD_DOWNLOADING;
    (void)snprintf(status->message, sizeof(status->message), "%s",
                   "Downloading Windows...");
    notify(&context);
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        fail_status(status, CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE,
                    "The download backend is unavailable.");
        goto done;
    }
    curl_initialized = true;
    if ((curl = curl_easy_init()) == NULL ||
        curl_easy_setopt(curl, CURLOPT_URL, release->download_uri) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https") != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https") != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_file) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
                         transfer_progress) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &context) != CURLE_OK ||
        curl_easy_setopt(curl, CURLOPT_USERAGENT,
                         "ClassicSetup/11 ISO downloader") != CURLE_OK) {
        fail_status(status, CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE,
                    "The download backend is unavailable.");
        goto done;
    }
    curl_result = curl_easy_perform(curl);
    (void)curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    (void)curl_easy_getinfo(curl, CURLINFO_EFFECTIVE_URL, &effective_uri);
    if (fflush(context.file) != 0 || fsync(fileno(context.file)) != 0) {
        fail_status(status, CLASSICSETUP_DOWNLOAD_ERROR_WRITE,
                    "The downloaded data could not be saved.");
        goto done;
    }
    (void)fclose(context.file);
    context.file = NULL;
    if (curl_result == CURLE_ABORTED_BY_CALLBACK && cancel_requested != NULL &&
        atomic_load(cancel_requested)) {
        fail_status(status, CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED,
                    "Download cancelled.");
        goto done;
    }
    if (curl_result != CURLE_OK || response_code < 200 || response_code >= 300 ||
        !classicsetup_windows_source_uri_is_official(effective_uri)) {
        fail_status(
            status,
            CLASSICSETUP_DOWNLOAD_ERROR_HTTP,
            curl_result == CURLE_SSL_CONNECT_ERROR ||
                    curl_result == CURLE_PEER_FAILED_VERIFICATION ||
                    curl_result == CURLE_SSL_CACERT_BADFILE
                ? "Secure connection to Microsoft could not be established."
                : "ClassicSetup could not obtain an official Windows download.");
        goto done;
    }
    status->state = CLASSICSETUP_DOWNLOAD_VERIFYING;
    (void)snprintf(status->message, sizeof(status->message), "%s",
                   "Verifying the Windows image...");
    notify(&context);
    if (classicsetup_verify_source_file(
            workspace->iso_partial_path, release->expected_size,
            release->official_hash_available ? release->expected_sha256 : "",
            &verify_error) != 0) {
        fail_status(status, verify_error,
                    "The downloaded Windows image could not be verified.");
        goto done;
    }
    (void)snprintf(status->message, sizeof(status->message), "%s",
                   "Inspecting the Windows installation image...");
    notify(&context);
    inspect_status = classicsetup_retail_inspect_iso(
        release, workspace, download_cancel_callback, cancel_requested,
        verified_source, &inspect_result);
    if (inspect_status != 0) {
        fail_status(status,
                    inspect_status == 1
                        ? CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED
                        : CLASSICSETUP_DOWNLOAD_ERROR_ISO,
                    inspect_status == 1
                        ? "Download cancelled."
                        : "The Windows installation image metadata could not be read.");
        goto done;
    }
    if (classicsetup_workspace_promote_verified_iso(workspace) != 0) {
        fail_status(status, CLASSICSETUP_DOWNLOAD_ERROR_WRITE,
                    "The verified Windows image could not be finalized.");
        goto done;
    }
    classicsetup_workspace_cleanup_success(workspace);
    (void)snprintf(verified_source->path, sizeof(verified_source->path), "%s",
                   workspace->iso_final_path);
    status->state = CLASSICSETUP_DOWNLOAD_COMPLETE;
    status->error = CLASSICSETUP_DOWNLOAD_ERROR_NONE;
    status->progress_fraction = 1.0;
    (void)snprintf(
        status->message, sizeof(status->message), "%s",
        release->official_hash_available
            ? "Download completed and verified using an official SHA-256 hash."
            : "Windows installation image verified.");
    result = 0;
done:
    if (context.file != NULL) {
        (void)fclose(context.file);
    }
    if (curl != NULL) {
        curl_easy_cleanup(curl);
    }
    if (curl_initialized) {
        curl_global_cleanup();
    }
    if (result != 0) {
        memset(verified_source, 0, sizeof(*verified_source));
        if (status != NULL &&
            status->state == CLASSICSETUP_DOWNLOAD_CANCELLED) {
            classicsetup_workspace_cleanup_cancel(workspace);
        } else {
            classicsetup_workspace_cleanup_failure(workspace);
        }
    }
    if (progress != NULL && status != NULL) {
        progress(status, progress_data);
    }
    return result;
}
