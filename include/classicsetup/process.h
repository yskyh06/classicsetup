#ifndef CLASSICSETUP_PROCESS_H
#define CLASSICSETUP_PROCESS_H

#include <stddef.h>

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

int classicsetup_run_process_with_input(
    const char *executable,
    char *const arguments[],
    const char *input,
    struct classicsetup_process_result *result);

int classicsetup_run_process(
    const char *executable,
    char *const arguments[],
    struct classicsetup_process_result *result);

#endif
