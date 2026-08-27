#define _POSIX_C_SOURCE 200809L

#include "classicsetup/process.h"

#include <errno.h>
#include <signal.h>
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

static void read_output(int descriptor, char *output, size_t output_size)
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
        read_output(output_pipe[0], result->output, sizeof(result->output));
        close(output_pipe[0]);
        waitpid(child, &status, 0);
        return -1;
    }
    sigaction(SIGPIPE, &previous_pipe, NULL);
    close(input_pipe[1]);
    read_output(output_pipe[0], result->output, sizeof(result->output));
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
