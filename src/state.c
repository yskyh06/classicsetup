#include "classicsetup/state.h"

enum classicsetup_state classicsetup_next_state(
    enum classicsetup_state state,
    enum classicsetup_event event)
{
    switch (state) {
    case CLASSICSETUP_STATE_WELCOME:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_KEYBOARD;
        }
        return CLASSICSETUP_STATE_WELCOME;
    case CLASSICSETUP_STATE_KEYBOARD:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_DISK;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_WELCOME;
        }
        return CLASSICSETUP_STATE_KEYBOARD;
    case CLASSICSETUP_STATE_DISK:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_PARTITION;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_KEYBOARD;
        }
        return CLASSICSETUP_STATE_DISK;
    case CLASSICSETUP_STATE_PARTITION:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_FORMAT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_DISK;
        }
        return CLASSICSETUP_STATE_PARTITION;
    case CLASSICSETUP_STATE_FORMAT:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_APPLY_PREVIEW;
        }
        if (event == CLASSICSETUP_EVENT_CANCEL) {
            return CLASSICSETUP_STATE_PARTITION;
        }
        return CLASSICSETUP_STATE_FORMAT;
    case CLASSICSETUP_STATE_APPLY_PREVIEW:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_APPLY_CONFIRMATION;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_FORMAT;
        }
        return CLASSICSETUP_STATE_APPLY_PREVIEW;
    case CLASSICSETUP_STATE_APPLY_CONFIRMATION:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_APPLY_RESULT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_APPLY_PREVIEW;
        }
        return CLASSICSETUP_STATE_APPLY_CONFIRMATION;
    case CLASSICSETUP_STATE_APPLY_RESULT:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_AFTER_FORMAT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_APPLY_PREVIEW;
        }
        return CLASSICSETUP_STATE_APPLY_RESULT;
    case CLASSICSETUP_STATE_AFTER_FORMAT:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_EXIT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_APPLY_RESULT;
        }
        return CLASSICSETUP_STATE_AFTER_FORMAT;
    case CLASSICSETUP_STATE_EXIT:
        return CLASSICSETUP_STATE_EXIT;
    }

    return CLASSICSETUP_STATE_EXIT;
}

enum classicsetup_state classicsetup_resolve_quit_request(
    enum classicsetup_state state,
    bool confirmed)
{
    switch (state) {
    case CLASSICSETUP_STATE_WELCOME:
    case CLASSICSETUP_STATE_KEYBOARD:
    case CLASSICSETUP_STATE_DISK:
    case CLASSICSETUP_STATE_PARTITION:
    case CLASSICSETUP_STATE_FORMAT:
    case CLASSICSETUP_STATE_APPLY_PREVIEW:
    case CLASSICSETUP_STATE_APPLY_CONFIRMATION:
    case CLASSICSETUP_STATE_APPLY_RESULT:
    case CLASSICSETUP_STATE_AFTER_FORMAT:
        return confirmed ? CLASSICSETUP_STATE_EXIT : state;
    case CLASSICSETUP_STATE_EXIT:
        return CLASSICSETUP_STATE_EXIT;
    }

    return CLASSICSETUP_STATE_EXIT;
}
