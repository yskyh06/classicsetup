#include "classicsetup/gui.h"

#include <stdio.h>
#include <string.h>

void classicsetup_gui_session_reset(
    struct classicsetup_gui_session *session)
{
    classicsetup_gui_session_reset_for_entry(
        session,
        CLASSICSETUP_GUI_ENTRY_RECOMMENDED);
}

void classicsetup_gui_session_reset_for_entry(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_entry_mode entry_mode)
{
    if (session == NULL) {
        return;
    }
    memset(session, 0, sizeof(*session));
    session->entry_mode = entry_mode;
    session->page = entry_mode == CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN
                        ? CLASSICSETUP_GUI_PAGE_NETWORK
                        : CLASSICSETUP_GUI_PAGE_DISK;
    session->windows_version = CLASSICSETUP_GUI_WINDOWS_11;
    classicsetup_network_snapshot_reset(&session->network);
    classicsetup_source_catalog_reset(&session->source_catalog);
    classicsetup_download_status_reset(&session->download);
}

enum classicsetup_gui_page classicsetup_gui_page_next(
    enum classicsetup_gui_page page)
{
    if (page < CLASSICSETUP_GUI_PAGE_DISK ||
        page >= CLASSICSETUP_GUI_PAGE_SUMMARY) {
        return CLASSICSETUP_GUI_PAGE_SUMMARY;
    }
    return (enum classicsetup_gui_page)(page + 1);
}

enum classicsetup_gui_page classicsetup_gui_page_back(
    enum classicsetup_gui_page page)
{
    if (page <= CLASSICSETUP_GUI_PAGE_DISK ||
        page >= CLASSICSETUP_GUI_PAGE_COUNT) {
        return CLASSICSETUP_GUI_PAGE_DISK;
    }
    return (enum classicsetup_gui_page)(page - 1);
}

enum classicsetup_gui_back_action classicsetup_gui_page_back_for_entry(
    enum classicsetup_gui_entry_mode entry_mode,
    enum classicsetup_gui_page page,
    enum classicsetup_gui_page *destination)
{
    if (destination == NULL) {
        return CLASSICSETUP_GUI_BACK_TO_TUI;
    }
    if (page == CLASSICSETUP_GUI_PAGE_DISK ||
        (entry_mode == CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN &&
         page == CLASSICSETUP_GUI_PAGE_NETWORK) ||
        page < CLASSICSETUP_GUI_PAGE_DISK ||
        page >= CLASSICSETUP_GUI_PAGE_COUNT) {
        *destination = page;
        return CLASSICSETUP_GUI_BACK_TO_TUI;
    }
    *destination = classicsetup_gui_page_back(page);
    return CLASSICSETUP_GUI_BACK_TO_PAGE;
}

int classicsetup_gui_select_disk(
    struct classicsetup_gui_session *session,
    size_t index)
{
    if (session == NULL || index >= session->assessment_count ||
        !session->assessments[index].selectable ||
        session->firmware != CLASSICSETUP_FIRMWARE_UEFI) {
        return -1;
    }
    session->selected_disk_index = index;
    session->has_selected_disk = true;
    return 0;
}

int classicsetup_gui_set_windows_version(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_windows_version version)
{
    if (session == NULL ||
        (version != CLASSICSETUP_GUI_WINDOWS_11 &&
         version != CLASSICSETUP_GUI_WINDOWS_10)) {
        return -1;
    }
    if (session->download.state != CLASSICSETUP_DOWNLOAD_NOT_STARTED) {
        return -1;
    }
    session->windows_version = version;
    session->has_selected_release = false;
    session->selected_release_index = 0;
    classicsetup_source_catalog_reset(&session->source_catalog);
    return 0;
}

int classicsetup_gui_select_release(
    struct classicsetup_gui_session *session,
    size_t index)
{
    if (session == NULL ||
        session->source_catalog.state != CLASSICSETUP_SOURCE_READY ||
        index >= session->source_catalog.release_count ||
        session->download.state != CLASSICSETUP_DOWNLOAD_NOT_STARTED) {
        return -1;
    }
    session->selected_release_index = index;
    session->has_selected_release = true;
    return 0;
}

bool classicsetup_gui_summary_is_ready(
    const struct classicsetup_gui_session *session)
{
    return session != NULL &&
           classicsetup_network_can_continue(&session->network) &&
           session->has_selected_release &&
           session->options_placeholder &&
           classicsetup_download_is_ready(
               &session->download, &session->workspace);
}

void classicsetup_gui_network_presentation_reset(
    struct classicsetup_gui_network_presentation *presentation)
{
    if (presentation != NULL) {
        memset(presentation, 0, sizeof(*presentation));
    }
}

int classicsetup_gui_network_presentation_add_ethernet(
    struct classicsetup_gui_network_presentation *presentation,
    const char *display_name,
    bool connected)
{
    struct classicsetup_gui_ethernet_entry *entry;

    if (presentation == NULL || display_name == NULL ||
        display_name[0] == '\0' ||
        presentation->ethernet_count >= CLASSICSETUP_GUI_MAX_ETHERNET) {
        return -1;
    }
    entry = &presentation->ethernet[presentation->ethernet_count++];
    (void)snprintf(
        entry->display_name,
        sizeof(entry->display_name),
        "%s",
        display_name);
    entry->connected = connected;
    return 0;
}

void classicsetup_gui_network_presentation_from_snapshot(
    struct classicsetup_gui_network_presentation *presentation,
    const struct classicsetup_network_snapshot *snapshot)
{
    classicsetup_gui_network_presentation_reset(presentation);
    if (presentation == NULL || snapshot == NULL ||
        !snapshot->ethernet_available) {
        return;
    }
    (void)classicsetup_gui_network_presentation_add_ethernet(
        presentation,
        "Local Area Connection",
        snapshot->ethernet_connected);
}

int classicsetup_gui_session_init_advanced_plan(
    struct classicsetup_gui_session *session,
    const struct classicsetup_config *config)
{
    if (session == NULL || config == NULL ||
        config->setup_mode != CLASSICSETUP_SETUP_ADVANCED ||
        !config->advanced_storage_plan_ready ||
        !config->has_selected_disk || !config->has_partition_plan ||
        !config->has_selected_plan_target ||
        !config->selected_format_plan.valid) {
        return -1;
    }
    classicsetup_gui_session_reset_for_entry(
        session,
        CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN);
    session->advanced_plan_prepared = true;
    session->has_prepared_disk = true;
    session->prepared_disk = config->selected_disk;
    session->prepared_install_mode = config->install_mode;
    session->prepared_partition_plan = config->partition_plan;
    session->prepared_format_plan = config->selected_format_plan;
    memcpy(
        session->prepared_role_format_plans,
        config->role_format_plans,
        sizeof(session->prepared_role_format_plans));
    session->has_prepared_apply_plan = config->has_apply_plan;
    if (config->has_apply_plan) {
        session->prepared_apply_plan = config->apply_plan;
    }
    return 0;
}
