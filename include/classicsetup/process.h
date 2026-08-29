#ifndef CLASSICSETUP_PROCESS_H
#define CLASSICSETUP_PROCESS_H

#include <stddef.h>
#include <stdbool.h>

enum {
    CLASSICSETUP_PROCESS_OUTPUT_SIZE = 2048
};

struct classicsetup_process_result {
    int exited;
    int exit_status;
    int signaled;
    int signal_number;
    char output[CLASSICSETUP_PROCESS_OUTPUT_SIZE];
};

typedef bool (*classicsetup_process_cancel_callback)(void *context);

int classicsetup_run_process_with_input(
    const char *executable,
    char *const arguments[],
    const char *input,
    struct classicsetup_process_result *result);

int classicsetup_run_process(
    const char *executable,
    char *const arguments[],
    struct classicsetup_process_result *result);

int classicsetup_run_process_cancellable(
    const char *executable,
    char *const arguments[],
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_process_result *result);

int classicsetup_run_process_cancellable_with_environment(
    const char *executable,
    char *const arguments[],
    const char *environment_name,
    const char *environment_value,
    classicsetup_process_cancel_callback cancel_callback,
    void *cancel_context,
    struct classicsetup_process_result *result);

#endif
