#include "classicsetup/state.h"

enum classicsetup_state classicsetup_next_state(
    enum classicsetup_state state,
    enum classicsetup_event event)
{
    return classicsetup_next_state_for_setup_mode(
        state,
        event,
        CLASSICSETUP_SETUP_RECOMMENDED);
}

enum classicsetup_state classicsetup_next_state_for_setup_mode(
    enum classicsetup_state state,
    enum classicsetup_event event,
    enum classicsetup_setup_mode setup_mode)
{
    switch (state) {
    case CLASSICSETUP_STATE_WELCOME:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_LICENSE_AGREEMENT;
        }
        return CLASSICSETUP_STATE_WELCOME;
    case CLASSICSETUP_STATE_LICENSE_AGREEMENT:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_SETUP_MODE;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_WELCOME;
        }
        return CLASSICSETUP_STATE_LICENSE_AGREEMENT;
    case CLASSICSETUP_STATE_SETUP_MODE:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return setup_mode == CLASSICSETUP_SETUP_ADVANCED
                       ? CLASSICSETUP_STATE_KEYBOARD
                       : CLASSICSETUP_STATE_RECOMMENDED_GUI_TRANSITION;
        }
        return CLASSICSETUP_STATE_SETUP_MODE;
    case CLASSICSETUP_STATE_RECOMMENDED_GUI_TRANSITION:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_RECOMMENDED_DISK;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_SETUP_MODE;
        }
        return CLASSICSETUP_STATE_RECOMMENDED_GUI_TRANSITION;
    case CLASSICSETUP_STATE_KEYBOARD:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return setup_mode == CLASSICSETUP_SETUP_ADVANCED
                       ? CLASSICSETUP_STATE_INSTALL_MODE
                       : CLASSICSETUP_STATE_RECOMMENDED_DISK;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_SETUP_MODE;
        }
        return CLASSICSETUP_STATE_KEYBOARD;
    case CLASSICSETUP_STATE_RECOMMENDED_DISK:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_NETWORK;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_SETUP_MODE;
        }
        return CLASSICSETUP_STATE_RECOMMENDED_DISK;
    case CLASSICSETUP_STATE_NETWORK:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_WINDOWS_VERSION;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_RECOMMENDED_DISK;
        }
        return CLASSICSETUP_STATE_NETWORK;
    case CLASSICSETUP_STATE_WINDOWS_VERSION:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_WINDOWS_DOWNLOAD;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_NETWORK;
        }
        return CLASSICSETUP_STATE_WINDOWS_VERSION;
    case CLASSICSETUP_STATE_WINDOWS_DOWNLOAD:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_INSTALL_OPTIONS;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_WINDOWS_VERSION;
        }
        return CLASSICSETUP_STATE_WINDOWS_DOWNLOAD;
    case CLASSICSETUP_STATE_INSTALL_OPTIONS:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_INSTALL_SUMMARY;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_WINDOWS_DOWNLOAD;
        }
        return CLASSICSETUP_STATE_INSTALL_OPTIONS;
    case CLASSICSETUP_STATE_INSTALL_SUMMARY:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_RECOMMENDED_RESULT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_INSTALL_OPTIONS;
        }
        return CLASSICSETUP_STATE_INSTALL_SUMMARY;
    case CLASSICSETUP_STATE_RECOMMENDED_RESULT:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_NEXT_STAGE;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_INSTALL_SUMMARY;
        }
        return CLASSICSETUP_STATE_RECOMMENDED_RESULT;
    case CLASSICSETUP_STATE_INSTALL_MODE:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_DISK;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_KEYBOARD;
        }
        return CLASSICSETUP_STATE_INSTALL_MODE;
    case CLASSICSETUP_STATE_DISK:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_PARTITION;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_INSTALL_MODE;
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
            return CLASSICSETUP_STATE_FORMAT_APPLY_PREVIEW;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_APPLY_PREVIEW;
        }
        return CLASSICSETUP_STATE_APPLY_RESULT;
    case CLASSICSETUP_STATE_FORMAT_APPLY_PREVIEW:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_FORMAT_APPLY_CONFIRMATION;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_APPLY_RESULT;
        }
        return CLASSICSETUP_STATE_FORMAT_APPLY_PREVIEW;
    case CLASSICSETUP_STATE_FORMAT_APPLY_CONFIRMATION:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_FORMAT_APPLY_RESULT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_FORMAT_APPLY_PREVIEW;
        }
        return CLASSICSETUP_STATE_FORMAT_APPLY_CONFIRMATION;
    case CLASSICSETUP_STATE_FORMAT_APPLY_RESULT:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_AFTER_FORMAT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_FORMAT_APPLY_PREVIEW;
        }
        return CLASSICSETUP_STATE_FORMAT_APPLY_RESULT;
    case CLASSICSETUP_STATE_AFTER_FORMAT:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_NEXT_STAGE;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return CLASSICSETUP_STATE_FORMAT_APPLY_RESULT;
        }
        return CLASSICSETUP_STATE_AFTER_FORMAT;
    case CLASSICSETUP_STATE_NEXT_STAGE:
        if (event == CLASSICSETUP_EVENT_CONTINUE) {
            return CLASSICSETUP_STATE_EXIT;
        }
        if (event == CLASSICSETUP_EVENT_BACK) {
            return setup_mode == CLASSICSETUP_SETUP_ADVANCED
                       ? CLASSICSETUP_STATE_AFTER_FORMAT
                       : CLASSICSETUP_STATE_RECOMMENDED_RESULT;
        }
        return CLASSICSETUP_STATE_NEXT_STAGE;
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
    case CLASSICSETUP_STATE_LICENSE_AGREEMENT:
    case CLASSICSETUP_STATE_SETUP_MODE:
    case CLASSICSETUP_STATE_RECOMMENDED_GUI_TRANSITION:
    case CLASSICSETUP_STATE_KEYBOARD:
    case CLASSICSETUP_STATE_RECOMMENDED_DISK:
    case CLASSICSETUP_STATE_NETWORK:
    case CLASSICSETUP_STATE_WINDOWS_VERSION:
    case CLASSICSETUP_STATE_WINDOWS_DOWNLOAD:
    case CLASSICSETUP_STATE_INSTALL_OPTIONS:
    case CLASSICSETUP_STATE_INSTALL_SUMMARY:
    case CLASSICSETUP_STATE_RECOMMENDED_RESULT:
    case CLASSICSETUP_STATE_INSTALL_MODE:
    case CLASSICSETUP_STATE_DISK:
    case CLASSICSETUP_STATE_PARTITION:
    case CLASSICSETUP_STATE_FORMAT:
    case CLASSICSETUP_STATE_APPLY_PREVIEW:
    case CLASSICSETUP_STATE_APPLY_CONFIRMATION:
    case CLASSICSETUP_STATE_APPLY_RESULT:
    case CLASSICSETUP_STATE_FORMAT_APPLY_PREVIEW:
    case CLASSICSETUP_STATE_FORMAT_APPLY_CONFIRMATION:
    case CLASSICSETUP_STATE_FORMAT_APPLY_RESULT:
    case CLASSICSETUP_STATE_AFTER_FORMAT:
    case CLASSICSETUP_STATE_NEXT_STAGE:
        return confirmed ? CLASSICSETUP_STATE_EXIT : state;
    case CLASSICSETUP_STATE_EXIT:
        return CLASSICSETUP_STATE_EXIT;
    }

    return CLASSICSETUP_STATE_EXIT;
}
