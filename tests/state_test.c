#include <assert.h>
#include <stddef.h>

#include "classicsetup/state.h"

static void test_continue_flow(void)
{
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_WELCOME,
               CLASSICSETUP_EVENT_CONTINUE) == CLASSICSETUP_STATE_KEYBOARD);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_KEYBOARD,
               CLASSICSETUP_EVENT_CONTINUE) ==
           CLASSICSETUP_STATE_INSTALL_MODE);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_INSTALL_MODE,
               CLASSICSETUP_EVENT_CONTINUE) == CLASSICSETUP_STATE_DISK);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_DISK,
               CLASSICSETUP_EVENT_CONTINUE) == CLASSICSETUP_STATE_PARTITION);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_PARTITION,
               CLASSICSETUP_EVENT_CONTINUE) == CLASSICSETUP_STATE_FORMAT);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_FORMAT,
               CLASSICSETUP_EVENT_CONTINUE) ==
           CLASSICSETUP_STATE_APPLY_PREVIEW);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_PREVIEW,
               CLASSICSETUP_EVENT_CONTINUE) ==
           CLASSICSETUP_STATE_APPLY_CONFIRMATION);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_CONFIRMATION,
               CLASSICSETUP_EVENT_CONTINUE) ==
           CLASSICSETUP_STATE_APPLY_RESULT);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_RESULT,
               CLASSICSETUP_EVENT_CONTINUE) ==
           CLASSICSETUP_STATE_AFTER_FORMAT);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_AFTER_FORMAT,
               CLASSICSETUP_EVENT_CONTINUE) == CLASSICSETUP_STATE_EXIT);
}

static void test_cancel_policy(void)
{
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_WELCOME,
               CLASSICSETUP_EVENT_CANCEL) == CLASSICSETUP_STATE_WELCOME);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_KEYBOARD,
               CLASSICSETUP_EVENT_CANCEL) == CLASSICSETUP_STATE_KEYBOARD);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_INSTALL_MODE,
               CLASSICSETUP_EVENT_CANCEL) ==
           CLASSICSETUP_STATE_INSTALL_MODE);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_DISK,
               CLASSICSETUP_EVENT_CANCEL) == CLASSICSETUP_STATE_DISK);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_PARTITION,
               CLASSICSETUP_EVENT_CANCEL) == CLASSICSETUP_STATE_PARTITION);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_FORMAT,
               CLASSICSETUP_EVENT_CANCEL) == CLASSICSETUP_STATE_PARTITION);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_AFTER_FORMAT,
               CLASSICSETUP_EVENT_CANCEL) ==
           CLASSICSETUP_STATE_AFTER_FORMAT);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_PREVIEW,
               CLASSICSETUP_EVENT_CANCEL) ==
           CLASSICSETUP_STATE_APPLY_PREVIEW);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_CONFIRMATION,
               CLASSICSETUP_EVENT_CANCEL) ==
           CLASSICSETUP_STATE_APPLY_CONFIRMATION);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_RESULT,
               CLASSICSETUP_EVENT_CANCEL) ==
           CLASSICSETUP_STATE_APPLY_RESULT);
}

static void test_back_policy(void)
{
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_WELCOME,
               CLASSICSETUP_EVENT_BACK) == CLASSICSETUP_STATE_WELCOME);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_KEYBOARD,
               CLASSICSETUP_EVENT_BACK) == CLASSICSETUP_STATE_WELCOME);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_INSTALL_MODE,
               CLASSICSETUP_EVENT_BACK) == CLASSICSETUP_STATE_KEYBOARD);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_DISK,
               CLASSICSETUP_EVENT_BACK) ==
           CLASSICSETUP_STATE_INSTALL_MODE);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_PARTITION,
               CLASSICSETUP_EVENT_BACK) == CLASSICSETUP_STATE_DISK);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_FORMAT,
               CLASSICSETUP_EVENT_BACK) == CLASSICSETUP_STATE_FORMAT);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_PREVIEW,
               CLASSICSETUP_EVENT_BACK) == CLASSICSETUP_STATE_FORMAT);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_CONFIRMATION,
               CLASSICSETUP_EVENT_BACK) ==
           CLASSICSETUP_STATE_APPLY_PREVIEW);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_APPLY_RESULT,
               CLASSICSETUP_EVENT_BACK) ==
           CLASSICSETUP_STATE_APPLY_PREVIEW);
    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_AFTER_FORMAT,
               CLASSICSETUP_EVENT_BACK) ==
           CLASSICSETUP_STATE_APPLY_RESULT);
}

static void test_quit_request_policy(void)
{
    enum classicsetup_state states[] = {
        CLASSICSETUP_STATE_WELCOME,
        CLASSICSETUP_STATE_KEYBOARD,
        CLASSICSETUP_STATE_INSTALL_MODE,
        CLASSICSETUP_STATE_DISK,
        CLASSICSETUP_STATE_PARTITION,
        CLASSICSETUP_STATE_FORMAT,
        CLASSICSETUP_STATE_APPLY_PREVIEW,
        CLASSICSETUP_STATE_APPLY_CONFIRMATION,
        CLASSICSETUP_STATE_APPLY_RESULT,
        CLASSICSETUP_STATE_AFTER_FORMAT
    };
    size_t index;

    for (index = 0; index < sizeof(states) / sizeof(states[0]); ++index) {
        assert(classicsetup_next_state(
                   states[index],
                   CLASSICSETUP_EVENT_QUIT_REQUEST) == states[index]);
        assert(classicsetup_resolve_quit_request(states[index], false) ==
               states[index]);
        assert(classicsetup_resolve_quit_request(states[index], true) ==
               CLASSICSETUP_STATE_EXIT);
    }
}

int main(void)
{
    test_continue_flow();
    test_cancel_policy();
    test_back_policy();
    test_quit_request_policy();

    assert(classicsetup_next_state(
               CLASSICSETUP_STATE_EXIT,
               CLASSICSETUP_EVENT_CONTINUE) == CLASSICSETUP_STATE_EXIT);
    assert(classicsetup_next_state(
               (enum classicsetup_state)999,
               CLASSICSETUP_EVENT_CONTINUE) == CLASSICSETUP_STATE_EXIT);
    assert(classicsetup_resolve_quit_request(
               (enum classicsetup_state)999,
               false) == CLASSICSETUP_STATE_EXIT);
    return 0;
}
