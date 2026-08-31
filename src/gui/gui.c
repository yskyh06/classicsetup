#include "classicsetup/gui.h"

#include <stdio.h>
#include <string.h>

#ifndef CLASSICSETUP_ENABLE_UUP
#define CLASSICSETUP_ENABLE_UUP 1
#endif

static void reset_retail_acquisition(
    struct classicsetup_gui_session *session)
{
    session->retail_fido_attempted = false;
    session->retail_webview_started = false;
    classicsetup_retail_status_reset(&session->retail_status);
    classicsetup_retail_browser_status_reset(
        &session->retail_browser_status);
}

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
    session->source_backend = CLASSICSETUP_SOURCE_MICROSOFT_RETAIL;
    session->retail_source_mode = CLASSICSETUP_GUI_RETAIL_AUTOMATIC;
    classicsetup_network_snapshot_reset(&session->network);
    classicsetup_source_catalog_reset(&session->source_catalog);
    classicsetup_download_status_reset(&session->download);
    classicsetup_uup_status_reset(&session->uup_status);
    classicsetup_retail_status_reset(&session->retail_status);
    classicsetup_retail_browser_status_reset(
        &session->retail_browser_status);
}

int classicsetup_gui_set_retail_source_mode(
    struct classicsetup_gui_session *session,
    enum classicsetup_gui_retail_source_mode mode)
{
    if (session == NULL ||
        mode < CLASSICSETUP_GUI_RETAIL_AUTOMATIC ||
        mode > CLASSICSETUP_GUI_RETAIL_CUSTOM ||
        classicsetup_gui_source_change_requirement(session) !=
            CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return -1;
    }
    session->retail_source_mode = mode;
    reset_retail_acquisition(session);
    return 0;
}

bool classicsetup_gui_retail_try_fido_once(
    struct classicsetup_gui_session *session)
{
    if (session == NULL ||
        session->retail_source_mode !=
            CLASSICSETUP_GUI_RETAIL_AUTOMATIC ||
        session->retail_fido_attempted) {
        return false;
    }
    session->retail_fido_attempted = true;
    return true;
}

bool classicsetup_gui_retail_start_webview_once(
    struct classicsetup_gui_session *session)
{
    if (session == NULL || session->retail_webview_started ||
        (session->retail_source_mode ==
             CLASSICSETUP_GUI_RETAIL_AUTOMATIC &&
         !session->retail_fido_attempted) ||
        (session->retail_source_mode !=
             CLASSICSETUP_GUI_RETAIL_AUTOMATIC &&
         session->retail_source_mode !=
             CLASSICSETUP_GUI_RETAIL_MICROSOFT_PAGE)) {
        return false;
    }
    session->retail_webview_started = true;
    return true;
}

int classicsetup_gui_restart_retail_acquisition(
    struct classicsetup_gui_session *session)
{
    if (session == NULL ||
        classicsetup_gui_source_change_requirement(session) !=
            CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED ||
        (session->download.state != CLASSICSETUP_DOWNLOAD_CANCELLED &&
         session->download.state != CLASSICSETUP_DOWNLOAD_FAILED)) {
        return -1;
    }
    reset_retail_acquisition(session);
    classicsetup_download_status_reset(&session->download);
    return 0;
}

int classicsetup_gui_set_source_backend(
    struct classicsetup_gui_session *session,
    enum classicsetup_source_backend backend)
{
    if (session == NULL ||
        (backend != CLASSICSETUP_SOURCE_MICROSOFT_RETAIL &&
         backend != CLASSICSETUP_SOURCE_MICROSOFT_UUP &&
         backend != CLASSICSETUP_SOURCE_EXISTING_ISO) ||
        classicsetup_gui_source_change_requirement(session) !=
            CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return -1;
    }
#if !CLASSICSETUP_ENABLE_UUP
    if (backend == CLASSICSETUP_SOURCE_MICROSOFT_UUP) {
        return -1;
    }
#endif
    if (session->source_backend == backend) {
        return 0;
    }
    session->source_backend = backend;
    session->retail_source_mode = CLASSICSETUP_GUI_RETAIL_AUTOMATIC;
    reset_retail_acquisition(session);
    session->has_selected_release = false;
    session->has_selected_release_name = false;
    session->has_selected_language = false;
    session->has_selected_architecture = false;
    session->selected_release_name[0] = '\0';
    session->selected_release_index = 0;
    classicsetup_source_catalog_reset(&session->source_catalog);
    classicsetup_source_resolve_diagnostics_reset(
        &session->source_diagnostics);
    return 0;
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
    if (classicsetup_gui_source_change_requirement(session) !=
        CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return -1;
    }
    session->windows_version = version;
    reset_retail_acquisition(session);
    session->has_selected_release = false;
    session->has_selected_release_name = false;
    session->has_selected_language = false;
    session->has_selected_architecture = false;
    session->selected_release_name[0] = '\0';
    session->selected_release_index = 0;
    classicsetup_source_catalog_reset(&session->source_catalog);
    classicsetup_source_resolve_diagnostics_reset(
        &session->source_diagnostics);
    return 0;
}

int classicsetup_gui_select_release(
    struct classicsetup_gui_session *session,
    size_t index)
{
    if (session == NULL ||
        session->source_catalog.state != CLASSICSETUP_SOURCE_READY ||
        index >= session->source_catalog.release_count ||
        classicsetup_gui_source_change_requirement(session) !=
            CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return -1;
    }
    session->selected_release_index = index;
    session->has_selected_release = true;
    (void)snprintf(
        session->selected_release_name,
        sizeof(session->selected_release_name),
        "%s",
        session->source_catalog.releases[index].release_name);
    session->has_selected_release_name = true;
    session->selected_language =
        session->source_catalog.releases[index].language;
    session->has_selected_language = true;
    session->selected_architecture =
        session->source_catalog.releases[index].architecture;
    session->has_selected_architecture = true;
    reset_retail_acquisition(session);
    return 0;
}

static bool candidate_matches_release(
    const struct classicsetup_windows_release *candidate,
    const char *release_name)
{
    return candidate != NULL && release_name != NULL &&
           strcmp(candidate->release_name, release_name) == 0;
}

int classicsetup_gui_select_release_name(
    struct classicsetup_gui_session *session,
    const char *release_name)
{
    size_t index;
    bool found = false;

    if (session == NULL || release_name == NULL || release_name[0] == '\0' ||
        session->source_catalog.state != CLASSICSETUP_SOURCE_READY ||
        classicsetup_gui_source_change_requirement(session) !=
            CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return -1;
    }
    for (index = 0; index < session->source_catalog.release_count; ++index) {
        if (candidate_matches_release(
                &session->source_catalog.releases[index], release_name)) {
            found = true;
            break;
        }
    }
    if (!found) {
        return -1;
    }
    (void)snprintf(
        session->selected_release_name,
        sizeof(session->selected_release_name), "%s", release_name);
    session->has_selected_release_name = true;
    session->has_selected_language = false;
    session->has_selected_architecture = false;
    session->has_selected_release = false;
    session->selected_release_index = 0;
    reset_retail_acquisition(session);
    classicsetup_source_resolve_diagnostics_reset(
        &session->source_diagnostics);
    return 0;
}

int classicsetup_gui_select_language(
    struct classicsetup_gui_session *session,
    enum classicsetup_windows_language language)
{
    size_t index;
    bool found = false;

    if (session == NULL || !session->has_selected_release_name ||
        classicsetup_gui_source_change_requirement(session) !=
            CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return -1;
    }
    for (index = 0; index < session->source_catalog.release_count; ++index) {
        const struct classicsetup_windows_release *candidate =
            &session->source_catalog.releases[index];

        if (candidate_matches_release(
                candidate, session->selected_release_name) &&
            candidate->language == language) {
            found = true;
            break;
        }
    }
    if (!found) {
        return -1;
    }
    session->selected_language = language;
    session->has_selected_language = true;
    session->has_selected_architecture = false;
    session->has_selected_release = false;
    session->selected_release_index = 0;
    reset_retail_acquisition(session);
    classicsetup_source_resolve_diagnostics_reset(
        &session->source_diagnostics);
    return 0;
}

int classicsetup_gui_select_architecture(
    struct classicsetup_gui_session *session,
    enum classicsetup_windows_architecture architecture)
{
    size_t index;

    if (session == NULL || !session->has_selected_release_name ||
        !session->has_selected_language ||
        classicsetup_gui_source_change_requirement(session) !=
            CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return -1;
    }
    for (index = 0; index < session->source_catalog.release_count; ++index) {
        const struct classicsetup_windows_release *candidate =
            &session->source_catalog.releases[index];

        if (candidate_matches_release(
                candidate, session->selected_release_name) &&
            candidate->language == session->selected_language &&
            candidate->architecture == architecture) {
            session->selected_architecture = architecture;
            session->has_selected_architecture = true;
            session->selected_release_index = index;
            session->has_selected_release = true;
            reset_retail_acquisition(session);
            classicsetup_source_resolve_diagnostics_reset(
                &session->source_diagnostics);
            return 0;
        }
    }
    session->has_selected_architecture = false;
    session->has_selected_release = false;
    classicsetup_source_resolve_diagnostics_reset(
        &session->source_diagnostics);
    return -1;
}

bool classicsetup_gui_source_selection_is_valid(
    const struct classicsetup_gui_session *session)
{
    if (session == NULL || !session->has_selected_release_name ||
        !session->has_selected_language ||
        !session->has_selected_architecture ||
        !session->has_selected_release ||
        session->selected_release_index >=
            session->source_catalog.release_count) {
        return false;
    }
    return candidate_matches_release(
               &session->source_catalog.releases[
                   session->selected_release_index],
               session->selected_release_name) &&
           session->source_catalog.releases[
               session->selected_release_index].language ==
               session->selected_language &&
           session->source_catalog.releases[
               session->selected_release_index].architecture ==
               session->selected_architecture;
}

enum classicsetup_gui_source_change_requirement
classicsetup_gui_source_change_requirement(
    const struct classicsetup_gui_session *session)
{
    if (session == NULL) {
        return CLASSICSETUP_GUI_SOURCE_CHANGE_CANCEL_DOWNLOAD;
    }
    if (session->download.state == CLASSICSETUP_DOWNLOAD_PREPARING ||
        session->download.state == CLASSICSETUP_DOWNLOAD_DOWNLOADING ||
        session->download.state == CLASSICSETUP_DOWNLOAD_VERIFYING) {
        return CLASSICSETUP_GUI_SOURCE_CHANGE_CANCEL_DOWNLOAD;
    }
    if (session->workspace.valid && session->workspace.verified_iso) {
        return CLASSICSETUP_GUI_SOURCE_CHANGE_DISCARD_VERIFIED;
    }
    return CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED;
}

void classicsetup_gui_discard_downloaded_source(
    struct classicsetup_gui_session *session)
{
    size_t index;

    if (session == NULL) {
        return;
    }
    if (session->workspace.valid) {
        classicsetup_workspace_cleanup_after_install(
            &session->workspace, false);
    }
    classicsetup_download_status_reset(&session->download);
    classicsetup_uup_status_reset(&session->uup_status);
    classicsetup_retail_status_reset(&session->retail_status);
    classicsetup_retail_browser_status_reset(
        &session->retail_browser_status);
    memset(&session->verified_source, 0,
           sizeof(session->verified_source));
    classicsetup_source_resolve_diagnostics_reset(
        &session->source_diagnostics);
    for (index = 0; index < session->source_catalog.release_count; ++index) {
        memset(session->source_catalog.releases[index].download_uri, 0,
               sizeof(session->source_catalog.releases[index].download_uri));
        session->source_catalog.releases[index].resolved = false;
    }
}

bool classicsetup_gui_summary_is_ready(
    const struct classicsetup_gui_session *session)
{
    return session != NULL &&
           classicsetup_network_can_continue(&session->network) &&
           classicsetup_gui_source_selection_is_valid(session) &&
           session->options_placeholder &&
           classicsetup_download_is_ready(
               &session->download, &session->workspace) &&
           session->verified_source.verified &&
           session->verified_source.kind ==
               CLASSICSETUP_VERIFIED_SOURCE_ISO;
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
