#define _POSIX_C_SOURCE 200809L

#include "classicsetup/process.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static int write_all(int descriptor, const char *data, size_t length)
{
    size_t written_total = 0;

    while (written_total < length) {
        ssize_t written = write(
            descriptor,
            data + written_total,
            length - written_total);

        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        written_total += (size_t)written;
    }
    return 0;
}

static void read_output(int descriptor, char *output, size_t output_size,
                        int *truncated)
{
    char discarded[256];
    size_t used = 0;

    while (used + 1 < output_size) {
        ssize_t count = read(descriptor, output + used, output_size - used - 1);

        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }
        if (count == 0) {
            break;
        }
        used += (size_t)count;
    }
    output[used] = '\0';

    while (read(descriptor, discarded, sizeof(discarded)) > 0) {
        if (truncated != NULL) {
            *truncated = 1;
        }
    }
}

int classicsetup_run_process_with_input(
    const char *executable,
    char *const arguments[],
    const char *input,
    struct classicsetup_process_result *result)
{
    int input_pipe[2];
    int output_pipe[2];
    int status;
    struct sigaction ignore_pipe = {0};
    struct sigaction previous_pipe;
    pid_t child;

    if (executable == NULL || arguments == NULL || arguments[0] == NULL ||
        input == NULL || result == NULL) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    if (pipe(input_pipe) != 0) {
        return -1;
    }
    if (pipe(output_pipe) != 0) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        return -1;
    }

    child = fork();
    if (child < 0) {
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        return -1;
    }
    if (child == 0) {
        close(input_pipe[1]);
        close(output_pipe[0]);
        if (dup2(input_pipe[0], STDIN_FILENO) < 0 ||
            dup2(output_pipe[1], STDOUT_FILENO) < 0 ||
            dup2(output_pipe[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(input_pipe[0]);
        close(output_pipe[1]);
        execv(executable, arguments);
        _exit(127);
    }

    close(input_pipe[0]);
    close(output_pipe[1]);
    ignore_pipe.sa_handler = SIG_IGN;
    sigemptyset(&ignore_pipe.sa_mask);
    if (sigaction(SIGPIPE, &ignore_pipe, &previous_pipe) != 0) {
        close(input_pipe[1]);
        close(output_pipe[0]);
        waitpid(child, &status, 0);
        return -1;
    }
    if (write_all(input_pipe[1], input, strlen(input)) != 0) {
        sigaction(SIGPIPE, &previous_pipe, NULL);
        close(input_pipe[1]);
        read_output(output_pipe[0], result->output, sizeof(result->output),
                    &result->output_truncated);
        close(output_pipe[0]);
        waitpid(child, &status, 0);
        return -1;
    }
    sigaction(SIGPIPE, &previous_pipe, NULL);
    close(input_pipe[1]);
    read_output(output_pipe[0], result->output, sizeof(result->output),
                &result->output_truncated);
    close(output_pipe[0]);

    while (waitpid(child, &status, 0) < 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    if (WIFEXITED(status)) {
        result->exited = 1;
        result->exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result->signaled = 1;
        result->signal_number = WTERMSIG(status);
    }
    return 0;
}

int classicsetup_run_process(
    const char *executable,
    char *const arguments[],
    struct classicsetup_process_result *result)
{
    return classicsetup_run_process_with_input(
        executable,
        arguments,
        "",
        result);
}

static ssize_t append_available_output(
    int descriptor,
    struct classicsetup_process_result *result,
    size_t *used)
{
    char chunk[256];
    ssize_t count;

    do {
        count = read(descriptor, chunk, sizeof(chunk));
    } while (count < 0 && errno == EINTR);
    if (count > 0) {
        size_t capacity = sizeof(result->output) - 1;
        size_t incoming = (size_t)count;

        {
            size_t required = *used + incoming;

            if (required > capacity) {
                size_t discard = required - capacity;

                result->output_truncated = 1;
                memmove(result->output, result->output + discard,
                        *used - discard);
                *used -= discard;
            }
            memcpy(result->output + *used, chunk, incoming);
            *used += incoming;
        }
        result->output[*used] = '\0';
    }
    return count;
}

static int run_process_cancellable_internal(
    const char *executable,
    char *const arguments[],
    const char *environment_name,
    const char *environment_value,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_process_result *result)
{
    int output_pipe[2];
    int status = 0;
    int child_done = 0;
    int cancel_sent = 0;
    int cancel_polls = 0;
    pid_t child;
    size_t used = 0;

    if (executable == NULL || arguments == NULL || arguments[0] == NULL ||
        result == NULL || pipe(output_pipe) != 0) {
        return -1;
    }
    memset(result, 0, sizeof(*result));
    child = fork();
    if (child < 0) {
        close(output_pipe[0]);
        close(output_pipe[1]);
        return -1;
    }
    if (child == 0) {
        (void)setpgid(0, 0);
        close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0 ||
            dup2(output_pipe[1], STDERR_FILENO) < 0) {
            _exit(126);
        }
        close(output_pipe[1]);
        if (environment_name != NULL && environment_value != NULL &&
            setenv(environment_name, environment_value, 1) != 0) {
            _exit(126);
        }
        execv(executable, arguments);
        _exit(127);
    }
    (void)setpgid(child, child);
    close(output_pipe[1]);
    (void)fcntl(output_pipe[0], F_SETFL,
                fcntl(output_pipe[0], F_GETFL, 0) | O_NONBLOCK);

    while (!child_done) {
        struct pollfd descriptor = {output_pipe[0], POLLIN | POLLHUP, 0};
        pid_t waited;

        (void)poll(&descriptor, 1, 100);
        if ((descriptor.revents & (POLLIN | POLLHUP)) != 0) {
            (void)append_available_output(output_pipe[0], result, &used);
        }
        if (!cancel_sent && cancel_callback != NULL &&
            cancel_callback(cancel_context)) {
            (void)kill(-child, SIGTERM);
            cancel_sent = 1;
        }
        if (cancel_sent && ++cancel_polls >= 20) {
            (void)kill(-child, SIGKILL);
        }
        do {
            waited = waitpid(child, &status, WNOHANG);
        } while (waited < 0 && errno == EINTR);
        if (waited == child) {
            child_done = 1;
        } else if (waited < 0) {
            close(output_pipe[0]);
            return -1;
        }
    }
    while (poll(&(struct pollfd){output_pipe[0], POLLIN | POLLHUP, 0},
                1, 0) > 0) {
        if (append_available_output(output_pipe[0], result, &used) <= 0) {
            break;
        }
    }
    close(output_pipe[0]);
    if (WIFEXITED(status)) {
        result->exited = 1;
        result->exit_status = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        result->signaled = 1;
        result->signal_number = WTERMSIG(status);
    }
    return cancel_sent ? 1 : 0;
}

int classicsetup_run_process_cancellable(
    const char *executable,
    char *const arguments[],
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_process_result *result)
{
    return run_process_cancellable_internal(
        executable, arguments, NULL, NULL, cancel_callback,
        cancel_context, result);
}

int classicsetup_run_process_cancellable_with_environment(
    const char *executable,
    char *const arguments[],
    const char *environment_name,
    const char *environment_value,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_process_result *result)
{
    if (environment_name == NULL || environment_name[0] == '\0' ||
        strchr(environment_name, '=') != NULL || environment_value == NULL) {
        return -1;
    }
    return run_process_cancellable_internal(
        executable, arguments, environment_name, environment_value,
        cancel_callback, cancel_context, result);
}
