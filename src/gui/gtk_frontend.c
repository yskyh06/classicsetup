#include "classicsetup/gui.h"

#include <gtk/gtk.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#ifndef CLASSICSETUP_GUI_CSS_PATH
#define CLASSICSETUP_GUI_CSS_PATH "src/gui/classicsetup.css"
#endif

struct classicsetup_gui_runtime {
    struct classicsetup_gui_session *session;
    GtkApplication *application;
    GtkWidget *window;
    GtkWidget *stack;
    GtkWidget *back_button;
    GtkWidget *next_button;
    GtkWidget *step_rows[CLASSICSETUP_GUI_PAGE_COUNT];
    GtkWidget *step_indicators[CLASSICSETUP_GUI_PAGE_COUNT];
    GtkWidget *disk_status;
    GtkWidget *summary_disk;
    GtkWidget *summary_version;
    GtkWidget *summary_source;
    GtkWidget *summary_verification;
    GtkWidget *network_status;
    GtkWidget *internet_status;
    GtkWidget *ethernet_list;
    GtkWidget *wifi_list;
    GtkWidget *password_label;
    GtkWidget *password_entry;
    GtkWidget *connect_button;
    GtkWidget *refresh_button;
    GtkWidget *network_spinner;
    GtkWidget *source_status;
    GtkWidget *windows11_button;
    GtkWidget *windows10_button;
    GtkWidget *release_dropdown;
    GtkStringList *release_model;
    GtkWidget *download_status;
    GtkWidget *download_detail;
    GtkWidget *download_progress;
    GtkWidget *download_start_button;
    GtkWidget *download_cancel_button;
    GtkWidget *download_release;
    GtkWidget *summary_network;
    GtkWidget *summary_options;
    GtkWidget *summary_notice;
    bool source_task_active;
    bool download_task_active;
    bool exit_pending;
    int pending_result;
    atomic_bool download_cancel_requested;
    struct classicsetup_network_controller network_controller;
    bool network_controller_ready;
    bool has_selected_wifi;
    size_t selected_wifi_index;
    int result;
};

struct download_task_result {
    struct classicsetup_windows_release release;
    struct classicsetup_workspace workspace;
    struct classicsetup_download_status status;
};

struct progress_event {
    struct classicsetup_gui_runtime *runtime;
    struct classicsetup_download_status status;
};

static void add_classic_label(
    GtkWidget *box,
    const char *text,
    const char *css_class,
    gboolean wrap)
{
    GtkWidget *label = gtk_label_new(text);

    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(label), wrap);
    if (wrap) {
        gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
        gtk_widget_set_hexpand(label, TRUE);
    }
    if (css_class != NULL) {
        gtk_widget_add_css_class(label, css_class);
    }
    gtk_box_append(GTK_BOX(box), label);
}

static GtkWidget *build_page_base(
    const char *title,
    const char *description)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    gtk_widget_add_css_class(box, "classicsetup-dialog-content");
    add_classic_label(box, title, "classic-title", TRUE);
    add_classic_label(box, description, "classic-subtitle", TRUE);
    gtk_box_append(GTK_BOX(box), separator);
    return box;
}

static GtkWidget *build_status_block(
    const char *text,
    const char *css_class)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);

    gtk_widget_add_css_class(box, "classic-status");
    if (css_class != NULL) {
        gtk_widget_add_css_class(box, css_class);
    }
    add_classic_label(box, text, NULL, TRUE);
    return box;
}

static GtkWidget *make_scrollable(GtkWidget *content)
{
    GtkWidget *scroll = gtk_scrolled_window_new();

    gtk_scrolled_window_set_policy(
        GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), content);
    gtk_widget_set_hexpand(scroll, TRUE);
    gtk_widget_set_vexpand(scroll, TRUE);
    return scroll;
}

static const char *page_step_name(enum classicsetup_gui_page page)
{
    static const char *names[CLASSICSETUP_GUI_PAGE_COUNT] = {
        "Disk preparation",
        "Connect to the Internet",
        "Select Windows version",
        "Download Windows",
        "Installation options",
        "Ready to continue"
    };

    return page >= CLASSICSETUP_GUI_PAGE_DISK &&
                   page < CLASSICSETUP_GUI_PAGE_COUNT
               ? names[page]
               : "Windows Setup";
}

static GtkWidget *build_sidebar_step(
    struct classicsetup_gui_runtime *runtime,
    enum classicsetup_gui_page page)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *indicator = gtk_label_new("•");
    GtkWidget *label = gtk_label_new(page_step_name(page));

    gtk_widget_add_css_class(row, "classicsetup-progress-step");
    gtk_widget_add_css_class(row, "classicsetup-progress-step-pending");
    gtk_widget_add_css_class(indicator, "classicsetup-progress-indicator");
    gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_box_append(GTK_BOX(row), indicator);
    gtk_box_append(GTK_BOX(row), label);
    runtime->step_rows[page] = row;
    runtime->step_indicators[page] = indicator;
    return row;
}

static GtkWidget *build_sidebar(struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    enum classicsetup_gui_page page;

    gtk_widget_add_css_class(sidebar, "classicsetup-progress-pane");
    gtk_widget_set_size_request(sidebar, 230, -1);
    add_classic_label(sidebar, "Windows Setup", "classic-sidebar-title", FALSE);
    if (runtime->session->entry_mode ==
        CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN) {
        gtk_box_append(
            GTK_BOX(sidebar),
            build_status_block(
                "Storage configuration is ready. No disk changes have been applied yet.",
                "classic-sidebar-note"));
    } else {
        add_classic_label(
            sidebar,
            "Recommended installation",
            "classic-sidebar-subtitle",
            TRUE);
    }
    add_classic_label(sidebar, "Setup progress", "classic-step-heading", FALSE);
    for (page = CLASSICSETUP_GUI_PAGE_DISK;
         page < CLASSICSETUP_GUI_PAGE_COUNT; ++page) {
        gtk_box_append(GTK_BOX(sidebar), build_sidebar_step(runtime, page));
    }
    return sidebar;
}

static gboolean page_is_complete(
    const struct classicsetup_gui_runtime *runtime,
    enum classicsetup_gui_page page)
{
    switch (page) {
    case CLASSICSETUP_GUI_PAGE_DISK:
        return runtime->session->entry_mode ==
                       CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN
                   ? runtime->session->advanced_plan_prepared
                   : runtime->session->has_selected_disk;
    case CLASSICSETUP_GUI_PAGE_NETWORK:
        return classicsetup_network_has_connection(
            &runtime->session->network);
    case CLASSICSETUP_GUI_PAGE_WINDOWS_VERSION:
        return runtime->session->has_selected_release;
    case CLASSICSETUP_GUI_PAGE_DOWNLOAD:
        return classicsetup_download_is_ready(
            &runtime->session->download, &runtime->session->workspace);
    case CLASSICSETUP_GUI_PAGE_OPTIONS:
        return runtime->session->options_placeholder &&
               runtime->session->page > CLASSICSETUP_GUI_PAGE_OPTIONS;
    case CLASSICSETUP_GUI_PAGE_SUMMARY:
    case CLASSICSETUP_GUI_PAGE_COUNT:
        return false;
    }
    return false;
}

static void update_sidebar_progress(
    struct classicsetup_gui_runtime *runtime)
{
    enum classicsetup_gui_page page;

    for (page = CLASSICSETUP_GUI_PAGE_DISK;
         page < CLASSICSETUP_GUI_PAGE_COUNT; ++page) {
        GtkWidget *row = runtime->step_rows[page];
        GtkWidget *indicator = runtime->step_indicators[page];

        if (row == NULL || indicator == NULL) {
            continue;
        }
        gtk_widget_remove_css_class(
            row, "classicsetup-progress-step-current");
        gtk_widget_remove_css_class(
            row, "classicsetup-progress-step-done");
        gtk_widget_remove_css_class(
            row, "classicsetup-progress-step-pending");
        if (page == runtime->session->page) {
            gtk_widget_add_css_class(
                row, "classicsetup-progress-step-current");
            gtk_label_set_text(GTK_LABEL(indicator), "●");
        } else if (page_is_complete(runtime, page)) {
            gtk_widget_add_css_class(
                row, "classicsetup-progress-step-done");
            gtk_label_set_text(GTK_LABEL(indicator), "✓");
        } else {
            gtk_widget_add_css_class(
                row, "classicsetup-progress-step-pending");
            gtk_label_set_text(GTK_LABEL(indicator), "○");
        }
    }
}

static void format_size(
    unsigned long long bytes,
    char *text,
    size_t text_size)
{
    if (bytes >= 1099511627776ULL) {
        (void)snprintf(
            text,
            text_size,
            "%.1f TiB",
            (double)bytes / 1099511627776.0);
    } else if (bytes >= 1073741824ULL) {
        (void)snprintf(
            text,
            text_size,
            "%.1f GiB",
            (double)bytes / 1073741824.0);
    } else {
        (void)snprintf(
            text,
            text_size,
            "%.1f MiB",
            (double)bytes / 1048576.0);
    }
}

static gboolean disk_is_selectable(
    const struct classicsetup_gui_runtime *runtime,
    size_t index)
{
    return classicsetup_recommended_assessment_is_selectable(
        &runtime->session->assessments[index],
        runtime->session->firmware);
}

static void update_navigation(struct classicsetup_gui_runtime *runtime);
static void update_download_page(struct classicsetup_gui_runtime *runtime);

static void finish_pending_exit(struct classicsetup_gui_runtime *runtime)
{
    if (runtime->exit_pending && !runtime->source_task_active &&
        !runtime->download_task_active) {
        runtime->result = runtime->pending_result;
        g_application_quit(G_APPLICATION(runtime->application));
    }
}

static void request_gui_exit(
    struct classicsetup_gui_runtime *runtime,
    int result)
{
    if (runtime->source_task_active || runtime->download_task_active) {
        runtime->exit_pending = true;
        runtime->pending_result = result;
        atomic_store(&runtime->download_cancel_requested, true);
        if (runtime->window != NULL) {
            gtk_widget_set_sensitive(runtime->window, FALSE);
        }
        return;
    }
    runtime->result = result;
    g_application_quit(G_APPLICATION(runtime->application));
}

static enum classicsetup_windows_family selected_family(
    const struct classicsetup_gui_session *session)
{
    return session->windows_version == CLASSICSETUP_GUI_WINDOWS_10
               ? CLASSICSETUP_WINDOWS_10
               : CLASSICSETUP_WINDOWS_11;
}

static void source_discovery_worker(
    GTask *task, gpointer source_object, gpointer task_data,
    GCancellable *cancellable)
{
    struct classicsetup_source_catalog *catalog = g_new0(
        struct classicsetup_source_catalog, 1);
    enum classicsetup_windows_family family =
        (enum classicsetup_windows_family)GPOINTER_TO_INT(task_data);

    (void)source_object;
    (void)cancellable;
    if (classicsetup_microsoft_source_discover(family, catalog) != 0) {
        g_task_return_pointer(task, catalog, g_free);
        return;
    }
    g_task_return_pointer(task, catalog, g_free);
}

static void update_source_controls(
    struct classicsetup_gui_runtime *runtime)
{
    const struct classicsetup_source_catalog *catalog =
        &runtime->session->source_catalog;
    size_t index;

    if (runtime->source_status == NULL || runtime->release_model == NULL) {
        return;
    }
    gtk_string_list_splice(
        runtime->release_model,
        0,
        g_list_model_get_n_items(G_LIST_MODEL(runtime->release_model)),
        NULL);
    for (index = 0; index < catalog->release_count; ++index) {
        char line[256];
        const struct classicsetup_windows_release *release =
            &catalog->releases[index];

        (void)snprintf(line, sizeof(line), "%s — %s — x64",
                       release->release_name, release->language_name);
        gtk_string_list_append(runtime->release_model, line);
    }
    if (catalog->state == CLASSICSETUP_SOURCE_DISCOVERING) {
        gtk_label_set_text(GTK_LABEL(runtime->source_status),
                           "Loading official Microsoft releases...");
    } else if (catalog->state == CLASSICSETUP_SOURCE_ERROR) {
        gtk_label_set_text(GTK_LABEL(runtime->source_status), catalog->error);
    } else if (catalog->state == CLASSICSETUP_SOURCE_READY) {
        gtk_label_set_text(
            GTK_LABEL(runtime->source_status),
            "Choose the Windows image language. Editions are selected later from the ISO.");
    } else {
        gtk_label_set_text(GTK_LABEL(runtime->source_status),
                           "Release discovery has not started.");
    }
    gtk_widget_set_sensitive(
        runtime->release_dropdown,
        catalog->state == CLASSICSETUP_SOURCE_READY &&
            runtime->session->download.state ==
                CLASSICSETUP_DOWNLOAD_NOT_STARTED);
    if (catalog->state == CLASSICSETUP_SOURCE_READY &&
        catalog->release_count > 0 &&
        !runtime->session->has_selected_release) {
        gtk_drop_down_set_selected(GTK_DROP_DOWN(runtime->release_dropdown), 0);
        (void)classicsetup_gui_select_release(runtime->session, 0);
    }
    update_navigation(runtime);
}

static void source_discovery_finished(
    GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    struct classicsetup_source_catalog *catalog;
    GError *error = NULL;

    (void)source_object;
    catalog = g_task_propagate_pointer(G_TASK(result), &error);
    runtime->source_task_active = false;
    if (catalog != NULL) {
        runtime->session->source_catalog = *catalog;
        g_free(catalog);
    } else {
        classicsetup_source_catalog_reset(&runtime->session->source_catalog);
        runtime->session->source_catalog.state = CLASSICSETUP_SOURCE_ERROR;
        (void)snprintf(runtime->session->source_catalog.error,
                       sizeof(runtime->session->source_catalog.error), "%s",
                       "Official Microsoft source discovery failed.");
    }
    if (error != NULL) {
        g_error_free(error);
    }
    update_source_controls(runtime);
    finish_pending_exit(runtime);
}

static void start_source_discovery(
    struct classicsetup_gui_runtime *runtime)
{
    GTask *task;

    if (runtime->source_task_active || runtime->download_task_active) {
        return;
    }
    runtime->session->has_selected_release = false;
    classicsetup_source_catalog_reset(&runtime->session->source_catalog);
    runtime->session->source_catalog.state =
        CLASSICSETUP_SOURCE_DISCOVERING;
    runtime->source_task_active = true;
    update_source_controls(runtime);
    task = g_task_new(NULL, NULL, source_discovery_finished, runtime);
    g_task_set_task_data(
        task,
        GINT_TO_POINTER((int)selected_family(runtime->session)),
        NULL);
    g_task_run_in_thread(task, source_discovery_worker);
    g_object_unref(task);
}

static gboolean progress_on_main(gpointer user_data)
{
    struct progress_event *event = user_data;

    event->runtime->session->download = event->status;
    update_download_page(event->runtime);
    g_free(event);
    return G_SOURCE_REMOVE;
}

static void download_progress_from_worker(
    const struct classicsetup_download_status *status,
    void *user_data)
{
    struct progress_event *event = g_new0(struct progress_event, 1);

    event->runtime = user_data;
    event->status = *status;
    g_main_context_invoke(NULL, progress_on_main, event);
}

static void download_worker(
    GTask *task, gpointer source_object, gpointer task_data,
    GCancellable *cancellable)
{
    struct classicsetup_gui_runtime *runtime = task_data;
    struct download_task_result *result = g_new0(
        struct download_task_result, 1);

    (void)source_object;
    (void)cancellable;
    result->release = runtime->session->source_catalog.releases[
        runtime->session->selected_release_index];
    classicsetup_download_status_reset(&result->status);
    result->status.state = CLASSICSETUP_DOWNLOAD_PREPARING;
    if (classicsetup_microsoft_source_resolve(&result->release) != 0) {
        result->status.state = CLASSICSETUP_DOWNLOAD_FAILED;
        result->status.error = CLASSICSETUP_DOWNLOAD_ERROR_SOURCE;
        (void)snprintf(result->status.message,
                       sizeof(result->status.message), "%s",
                       "Microsoft did not provide a usable download link.");
    } else if (classicsetup_workspace_create(&result->workspace) != 0) {
        result->status.state = CLASSICSETUP_DOWNLOAD_FAILED;
        result->status.error = CLASSICSETUP_DOWNLOAD_ERROR_WRITE;
        (void)snprintf(result->status.message,
                       sizeof(result->status.message), "%s",
                       "The temporary workspace could not be created.");
    } else {
        (void)classicsetup_download_windows_iso(
            &result->release,
            &result->workspace,
            &runtime->download_cancel_requested,
            download_progress_from_worker,
            runtime,
            &result->status);
    }
    g_task_return_pointer(task, result, g_free);
}

static void download_finished(
    GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    struct download_task_result *task_result;
    GError *error = NULL;

    (void)source_object;
    task_result = g_task_propagate_pointer(G_TASK(result), &error);
    runtime->download_task_active = false;
    if (task_result != NULL) {
        memset(task_result->release.download_uri, 0,
               sizeof(task_result->release.download_uri));
        task_result->release.resolved = false;
        runtime->session->source_catalog.releases[
            runtime->session->selected_release_index] = task_result->release;
        runtime->session->download = task_result->status;
        runtime->session->workspace = task_result->workspace;
        g_free(task_result);
    } else {
        runtime->session->download.state = CLASSICSETUP_DOWNLOAD_FAILED;
        runtime->session->download.error =
            CLASSICSETUP_DOWNLOAD_ERROR_BACKEND_UNAVAILABLE;
        (void)snprintf(runtime->session->download.message,
                       sizeof(runtime->session->download.message), "%s",
                       "The download worker failed.");
    }
    if (error != NULL) {
        g_error_free(error);
    }
    update_download_page(runtime);
    finish_pending_exit(runtime);
}

static void start_download(struct classicsetup_gui_runtime *runtime)
{
    GTask *task;

    if (runtime->download_task_active || runtime->source_task_active ||
        !runtime->session->has_selected_release) {
        return;
    }
    if (runtime->session->workspace.valid &&
        !runtime->session->workspace.verified_iso) {
        classicsetup_workspace_cleanup_after_install(
            &runtime->session->workspace, false);
    }
    classicsetup_download_status_reset(&runtime->session->download);
    runtime->session->download.state = CLASSICSETUP_DOWNLOAD_PREPARING;
    atomic_store(&runtime->download_cancel_requested, false);
    runtime->download_task_active = true;
    update_download_page(runtime);
    task = g_task_new(NULL, NULL, download_finished, runtime);
    g_task_set_task_data(task, runtime, NULL);
    g_task_run_in_thread(task, download_worker);
    g_object_unref(task);
}

static void on_disk_row_selected(
    GtkListBox *list,
    GtkListBoxRow *row,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    size_t index;

    (void)list;
    if (row == NULL) {
        return;
    }
    index = GPOINTER_TO_SIZE(
        g_object_get_data(G_OBJECT(row), "classicsetup-disk-index"));
    if (classicsetup_gui_select_disk(runtime->session, index) == 0) {
        gtk_label_set_text(
            GTK_LABEL(runtime->disk_status),
            "This disk is selected for the recommended workflow.");
    } else {
        gtk_label_set_text(
            GTK_LABEL(runtime->disk_status),
            "This disk is not available for automatic installation.");
    }
    update_navigation(runtime);
}

static GtkWidget *build_disk_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_page_base(
        "Select a disk for Windows",
        "Choose the disk that ClassicSetup should prepare. Only targets approved by the existing safety policy are available.");
    GtkWidget *list = gtk_list_box_new();
    size_t index;

    gtk_widget_add_css_class(list, "classic-list");
    gtk_list_box_set_selection_mode(
        GTK_LIST_BOX(list),
        GTK_SELECTION_SINGLE);
    gtk_widget_set_vexpand(list, TRUE);
    if (runtime->session->scan_failed ||
        runtime->session->assessment_count == 0) {
        add_classic_label(
            box,
            runtime->session->scan_failed
                ? "Disk information could not be read safely."
                : "No disks were found.",
            "classic-muted",
            TRUE);
    } else {
        for (index = 0;
             index < runtime->session->assessment_count;
             ++index) {
            GtkWidget *row = gtk_list_box_row_new();
            GtkWidget *row_box = gtk_box_new(
                GTK_ORIENTATION_VERTICAL,
                2);
            char size[32];
            char line[256];
            const struct classicsetup_disk_assessment *assessment =
                &runtime->session->assessments[index];

            format_size(
                assessment->disk.size_bytes,
                size,
                sizeof(size));
            (void)snprintf(
                line,
                sizeof(line),
                "%s    %s",
                assessment->disk.model,
                size);
            add_classic_label(row_box, line, NULL, FALSE);
            add_classic_label(
                row_box,
                assessment->presentation,
                "classic-muted",
                TRUE);
            add_classic_label(
                row_box,
                classicsetup_recommended_policy_reason(
                    assessment->disk_class,
                    runtime->session->firmware),
                "classic-muted",
                TRUE);
            (void)snprintf(
                line,
                sizeof(line),
                "Device: %s",
                assessment->disk.device_path);
            add_classic_label(row_box, line, "classic-muted", FALSE);
            gtk_widget_add_css_class(row, "classic-disk-row");
            gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
            g_object_set_data(
                G_OBJECT(row),
                "classicsetup-disk-index",
                GSIZE_TO_POINTER(index));
            gtk_widget_set_sensitive(
                row,
                disk_is_selectable(runtime, index));
            gtk_list_box_append(GTK_LIST_BOX(list), row);
        }
    }
    g_signal_connect(
        list,
        "row-selected",
        G_CALLBACK(on_disk_row_selected),
        runtime);
    gtk_box_append(GTK_BOX(box), list);
    runtime->disk_status = gtk_label_new(
        "Select an available disk to continue.");
    gtk_label_set_xalign(GTK_LABEL(runtime->disk_status), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->disk_status), TRUE);
    gtk_box_append(GTK_BOX(box), runtime->disk_status);
    return box;
}

static GtkWidget *build_placeholder_page(
    const char *title,
    const char *description)
{
    return build_page_base(title, description);
}

static void clear_list_rows(GtkWidget *list)
{
    GtkWidget *child;

    while ((child = gtk_widget_get_first_child(list)) != NULL) {
        gtk_list_box_remove(GTK_LIST_BOX(list), child);
    }
}

static void update_network_page(
    struct classicsetup_gui_runtime *runtime)
{
    const struct classicsetup_network_snapshot *snapshot =
        &runtime->session->network;
    struct classicsetup_gui_network_presentation presentation;
    char line[256];
    size_t index;
    gboolean busy = snapshot->state == CLASSICSETUP_NETWORK_SCANNING ||
                    snapshot->state == CLASSICSETUP_NETWORK_CONNECTING;

    if (runtime->network_status == NULL) {
        return;
    }
    if (classicsetup_network_has_connection(snapshot)) {
        gtk_label_set_text(
            GTK_LABEL(runtime->network_status),
            "Network connection is ready.");
        gtk_label_set_text(
            GTK_LABEL(runtime->internet_status),
            classicsetup_network_can_continue(snapshot)
                ? "Internet access is verified."
                : "Internet access has not been verified yet. It will be checked again before downloading Windows.");
    } else {
        gtk_label_set_text(
            GTK_LABEL(runtime->network_status), snapshot->status);
        gtk_label_set_text(
            GTK_LABEL(runtime->internet_status),
            "Connect a wired or wireless network to continue.");
    }
    classicsetup_gui_network_presentation_from_snapshot(
        &presentation, snapshot);
    clear_list_rows(runtime->ethernet_list);
    for (index = 0; index < presentation.ethernet_count; ++index) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        add_classic_label(
            row_box,
            presentation.ethernet[index].display_name,
            NULL,
            TRUE);
        add_classic_label(
            row_box,
            presentation.ethernet[index].connected
                ? "Connected"
                : "Cable not connected",
            "classic-muted",
            TRUE);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
        gtk_widget_add_css_class(row, "classicsetup-network-row");
        gtk_widget_set_sensitive(row, FALSE);
        gtk_list_box_append(GTK_LIST_BOX(runtime->ethernet_list), row);
    }
    if (presentation.ethernet_count == 0) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(
            "No wired network adapter was detected.");

        gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
        gtk_label_set_wrap(GTK_LABEL(label), TRUE);
        gtk_widget_add_css_class(label, "classic-muted");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_widget_set_sensitive(row, FALSE);
        gtk_list_box_append(GTK_LIST_BOX(runtime->ethernet_list), row);
    }
    clear_list_rows(runtime->wifi_list);
    for (index = 0; index < snapshot->wifi_count; ++index) {
        const struct classicsetup_wifi_network *network =
            &snapshot->wifi[index];
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        GtkWidget *ssid_label;
        GtkWidget *detail_label;

        ssid_label = gtk_label_new(network->ssid);
        gtk_label_set_xalign(GTK_LABEL(ssid_label), 0.0F);
        gtk_widget_set_hexpand(ssid_label, TRUE);
        (void)snprintf(
            line,
            sizeof(line),
            "%d%%  %s%s",
            network->signal_strength,
            network->enterprise ? "Enterprise (unsupported)" :
                (network->secured ? "Secured" : "Open"),
            network->connected ? "  Connected" : "");
        detail_label = gtk_label_new(line);
        gtk_widget_add_css_class(detail_label, "classic-muted");
        gtk_box_append(GTK_BOX(row_box), ssid_label);
        gtk_box_append(GTK_BOX(row_box), detail_label);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), row_box);
        gtk_widget_add_css_class(row, "classicsetup-network-row");
        g_object_set_data(
            G_OBJECT(row),
            "classicsetup-wifi-index",
            GSIZE_TO_POINTER(index));
        gtk_widget_set_sensitive(row, !network->enterprise && !busy);
        gtk_list_box_append(GTK_LIST_BOX(runtime->wifi_list), row);
    }
    if (!snapshot->wifi_available && !busy) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new(
            "No wireless adapter was detected.");

        gtk_label_set_xalign(GTK_LABEL(label), 0.0F);
        gtk_widget_add_css_class(label, "classic-muted");
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), label);
        gtk_widget_set_sensitive(row, FALSE);
        gtk_list_box_append(GTK_LIST_BOX(runtime->wifi_list), row);
    }
    gtk_widget_set_sensitive(runtime->refresh_button, !busy);
    gtk_widget_set_sensitive(
        runtime->connect_button,
        runtime->has_selected_wifi && !busy);
    gtk_widget_set_sensitive(runtime->password_entry, !busy);
    gtk_widget_set_visible(
        runtime->password_label,
        runtime->has_selected_wifi &&
            snapshot->wifi[runtime->selected_wifi_index].secured);
    gtk_widget_set_visible(
        runtime->password_entry,
        runtime->has_selected_wifi &&
            snapshot->wifi[runtime->selected_wifi_index].secured);
    if (busy) {
        gtk_spinner_start(GTK_SPINNER(runtime->network_spinner));
    } else {
        gtk_spinner_stop(GTK_SPINNER(runtime->network_spinner));
    }
    update_navigation(runtime);
}

static void network_snapshot_changed(
    const struct classicsetup_network_snapshot *snapshot,
    void *user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    runtime->session->network = *snapshot;
    runtime->has_selected_wifi = false;
    runtime->selected_wifi_index = 0;
    update_network_page(runtime);
}

static void on_wifi_row_selected(
    GtkListBox *list,
    GtkListBoxRow *row,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    size_t index;

    (void)list;
    if (row == NULL) {
        runtime->has_selected_wifi = false;
        gtk_widget_set_visible(runtime->password_label, FALSE);
        gtk_widget_set_visible(runtime->password_entry, FALSE);
        gtk_widget_set_sensitive(runtime->connect_button, FALSE);
        return;
    }
    index = GPOINTER_TO_SIZE(
        g_object_get_data(G_OBJECT(row), "classicsetup-wifi-index"));
    if (index >= runtime->session->network.wifi_count ||
        runtime->session->network.wifi[index].enterprise) {
        return;
    }
    runtime->selected_wifi_index = index;
    runtime->has_selected_wifi = true;
    gtk_editable_set_text(GTK_EDITABLE(runtime->password_entry), "");
    gtk_widget_set_visible(
        runtime->password_label,
        runtime->session->network.wifi[index].secured);
    gtk_widget_set_visible(
        runtime->password_entry,
        runtime->session->network.wifi[index].secured);
    gtk_widget_set_sensitive(runtime->connect_button, TRUE);
}

static void on_network_refresh_clicked(GtkButton *button, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)button;
    runtime->has_selected_wifi = false;
    (void)classicsetup_network_controller_refresh(
        &runtime->network_controller);
}

static void on_network_connect_clicked(GtkButton *button, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    const struct classicsetup_wifi_network *network;
    const char *password;

    (void)button;
    if (!runtime->has_selected_wifi ||
        runtime->selected_wifi_index >= runtime->session->network.wifi_count) {
        return;
    }
    network = &runtime->session->network.wifi[runtime->selected_wifi_index];
    password = gtk_editable_get_text(GTK_EDITABLE(runtime->password_entry));
    if (classicsetup_network_controller_connect_wifi(
            &runtime->network_controller,
            network,
            password) != 0) {
        gtk_label_set_text(
            GTK_LABEL(runtime->network_status),
            "Could not start the connection attempt.");
    }
    gtk_editable_set_text(GTK_EDITABLE(runtime->password_entry), "");
}

static GtkWidget *build_network_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_page_base(
        "Connect to a network",
        "Choose a wired or wireless connection. Internet access is checked separately before Windows is downloaded.");
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *status_text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    add_classic_label(
        box, "Wired connections", "classic-section-title", FALSE);
    runtime->ethernet_list = gtk_list_box_new();
    gtk_widget_add_css_class(
        runtime->ethernet_list, "classicsetup-network-list");
    gtk_list_box_set_selection_mode(
        GTK_LIST_BOX(runtime->ethernet_list), GTK_SELECTION_NONE);
    gtk_box_append(GTK_BOX(box), runtime->ethernet_list);
    add_classic_label(
        box, "Wireless networks", "classic-section-title", FALSE);
    runtime->wifi_list = gtk_list_box_new();
    gtk_widget_add_css_class(
        runtime->wifi_list, "classicsetup-network-list");
    gtk_list_box_set_selection_mode(
        GTK_LIST_BOX(runtime->wifi_list),
        GTK_SELECTION_SINGLE);
    gtk_widget_set_vexpand(runtime->wifi_list, TRUE);
    g_signal_connect(
        runtime->wifi_list,
        "row-selected",
        G_CALLBACK(on_wifi_row_selected),
        runtime);
    gtk_box_append(GTK_BOX(box), runtime->wifi_list);
    runtime->password_label = gtk_label_new("Network password:");
    gtk_label_set_xalign(GTK_LABEL(runtime->password_label), 0.0F);
    runtime->password_entry = gtk_password_entry_new();
    gtk_password_entry_set_show_peek_icon(
        GTK_PASSWORD_ENTRY(runtime->password_entry),
        FALSE);
    gtk_widget_set_visible(runtime->password_label, FALSE);
    gtk_widget_set_visible(runtime->password_entry, FALSE);
    gtk_box_append(GTK_BOX(box), runtime->password_label);
    gtk_box_append(GTK_BOX(box), runtime->password_entry);
    runtime->refresh_button = gtk_button_new_with_label("Refresh");
    runtime->connect_button = gtk_button_new_with_label("Connect");
    g_signal_connect(
        runtime->refresh_button,
        "clicked",
        G_CALLBACK(on_network_refresh_clicked),
        runtime);
    g_signal_connect(
        runtime->connect_button,
        "clicked",
        G_CALLBACK(on_network_connect_clicked),
        runtime);
    gtk_box_append(GTK_BOX(actions), runtime->refresh_button);
    gtk_box_append(GTK_BOX(actions), runtime->connect_button);
    gtk_box_append(GTK_BOX(box), actions);
    runtime->network_spinner = gtk_spinner_new();
    runtime->network_status = gtk_label_new("Network service is not available.");
    runtime->internet_status = gtk_label_new(
        "Connect a wired or wireless network to continue.");
    gtk_label_set_xalign(GTK_LABEL(runtime->network_status), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->internet_status), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->network_status), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->internet_status), TRUE);
    gtk_widget_add_css_class(runtime->internet_status, "classic-muted");
    gtk_box_append(GTK_BOX(status_box), runtime->network_spinner);
    gtk_box_append(GTK_BOX(status_text), runtime->network_status);
    gtk_box_append(GTK_BOX(status_text), runtime->internet_status);
    gtk_widget_set_hexpand(status_text, TRUE);
    gtk_box_append(GTK_BOX(status_box), status_text);
    gtk_widget_add_css_class(status_box, "classicsetup-status-box");
    gtk_box_append(GTK_BOX(box), status_box);
    return box;
}

static void on_version_toggled(
    GtkCheckButton *button,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    int value;

    if (!gtk_check_button_get_active(button)) {
        return;
    }
    value = GPOINTER_TO_INT(
        g_object_get_data(G_OBJECT(button), "classicsetup-version"));
    if (classicsetup_gui_set_windows_version(
            runtime->session,
            (enum classicsetup_gui_windows_version)value) == 0) {
        start_source_discovery(runtime);
    }
}

static void on_release_selected(
    GObject *object, GParamSpec *parameter, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));

    (void)parameter;
    if (selected != GTK_INVALID_LIST_POSITION) {
        (void)classicsetup_gui_select_release(runtime->session, selected);
    }
    update_navigation(runtime);
}

static GtkWidget *build_windows_version_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Select Windows Version",
        "ClassicSetup discovers currently available consumer ISO sources from Microsoft's official download service.");
    GtkWidget *windows11 = gtk_check_button_new_with_label("Windows 11");
    GtkWidget *windows10 = gtk_check_button_new_with_label("Windows 10");

    runtime->windows11_button = windows11;
    runtime->windows10_button = windows10;

    gtk_check_button_set_group(GTK_CHECK_BUTTON(windows10),
                               GTK_CHECK_BUTTON(windows11));
    gtk_check_button_set_active(
        GTK_CHECK_BUTTON(runtime->session->windows_version ==
                                 CLASSICSETUP_GUI_WINDOWS_10
                             ? windows10
                             : windows11),
        TRUE);
    g_object_set_data(
        G_OBJECT(windows11),
        "classicsetup-version",
        GINT_TO_POINTER(CLASSICSETUP_GUI_WINDOWS_11));
    g_object_set_data(
        G_OBJECT(windows10),
        "classicsetup-version",
        GINT_TO_POINTER(CLASSICSETUP_GUI_WINDOWS_10));
    g_signal_connect(
        windows11,
        "toggled",
        G_CALLBACK(on_version_toggled),
        runtime);
    g_signal_connect(
        windows10,
        "toggled",
        G_CALLBACK(on_version_toggled),
        runtime);
    gtk_box_append(GTK_BOX(box), windows11);
    gtk_box_append(GTK_BOX(box), windows10);
    runtime->source_status = gtk_label_new(
        "Release discovery has not started.");
    gtk_label_set_xalign(GTK_LABEL(runtime->source_status), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->source_status), TRUE);
    gtk_box_append(GTK_BOX(box), runtime->source_status);
    runtime->release_model = gtk_string_list_new(NULL);
    runtime->release_dropdown = gtk_drop_down_new(
        G_LIST_MODEL(runtime->release_model), NULL);
    gtk_widget_set_sensitive(runtime->release_dropdown, FALSE);
    g_signal_connect(runtime->release_dropdown, "notify::selected",
                     G_CALLBACK(on_release_selected), runtime);
    gtk_box_append(GTK_BOX(box), runtime->release_dropdown);
    return box;
}

static void on_download_start_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    start_download(user_data);
}

static void on_download_cancel_clicked(GtkButton *button, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)button;
    atomic_store(&runtime->download_cancel_requested, true);
    gtk_label_set_text(GTK_LABEL(runtime->download_status),
                       "Cancelling download...");
}

static void update_download_page(struct classicsetup_gui_runtime *runtime)
{
    const struct classicsetup_download_status *status =
        &runtime->session->download;
    char detail[256];
    gboolean active = runtime->download_task_active;

    if (runtime->download_status == NULL) {
        return;
    }
    if (runtime->download_release != NULL &&
        runtime->session->has_selected_release) {
        const struct classicsetup_windows_release *release =
            &runtime->session->source_catalog.releases[
                runtime->session->selected_release_index];

        (void)snprintf(detail, sizeof(detail), "%s\n%s  |  %s",
                       release->release_name, release->language_name,
                       release->architecture);
        gtk_label_set_text(GTK_LABEL(runtime->download_release), detail);
    }
    gtk_label_set_text(
        GTK_LABEL(runtime->download_status),
        status->message[0] != '\0' ? status->message
                                   : "Ready to download the selected Windows image.");
    if (status->total_bytes != 0) {
        (void)snprintf(
            detail, sizeof(detail), "%.2f GiB / %.2f GiB    %.1f MiB/s",
            (double)status->bytes_received / 1073741824.0,
            (double)status->total_bytes / 1073741824.0,
            status->bytes_per_second / 1048576.0);
    } else {
        (void)snprintf(detail, sizeof(detail), "%.2f GiB received",
                       (double)status->bytes_received / 1073741824.0);
    }
    gtk_label_set_text(GTK_LABEL(runtime->download_detail), detail);
    gtk_progress_bar_set_fraction(
        GTK_PROGRESS_BAR(runtime->download_progress),
        status->progress_fraction >= 0.0 &&
                status->progress_fraction <= 1.0
            ? status->progress_fraction : 0.0);
    gtk_widget_set_sensitive(
        runtime->download_start_button,
        runtime->session->has_selected_release && !active &&
        status->state != CLASSICSETUP_DOWNLOAD_COMPLETE);
    gtk_widget_set_sensitive(runtime->download_cancel_button, active);
    if (runtime->windows11_button != NULL) {
        gtk_widget_set_sensitive(runtime->windows11_button,
                                 !active && status->state ==
                                     CLASSICSETUP_DOWNLOAD_NOT_STARTED);
        gtk_widget_set_sensitive(runtime->windows10_button,
                                 !active && status->state ==
                                     CLASSICSETUP_DOWNLOAD_NOT_STARTED);
    }
    if (runtime->release_dropdown != NULL) {
        gtk_widget_set_sensitive(
            runtime->release_dropdown,
            !active && status->state == CLASSICSETUP_DOWNLOAD_NOT_STARTED &&
                runtime->session->source_catalog.state ==
                    CLASSICSETUP_SOURCE_READY);
    }
    update_navigation(runtime);
}

static GtkWidget *build_download_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Download Windows",
        "The selected multi-edition x64 ISO will be downloaded from Microsoft and verified before use.");
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    runtime->download_release = gtk_label_new("Windows source not selected");
    gtk_label_set_xalign(GTK_LABEL(runtime->download_release), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->download_release), TRUE);
    gtk_widget_add_css_class(runtime->download_release, "classic-source-summary");
    runtime->download_status = gtk_label_new(
        "Ready to download the selected Windows image.");
    gtk_label_set_xalign(GTK_LABEL(runtime->download_status), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->download_status), TRUE);
    runtime->download_progress = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(
        GTK_PROGRESS_BAR(runtime->download_progress), TRUE);
    runtime->download_detail = gtk_label_new("0 bytes received");
    gtk_label_set_xalign(GTK_LABEL(runtime->download_detail), 0.0F);
    runtime->download_start_button = gtk_button_new_with_label("Download");
    runtime->download_cancel_button =
        gtk_button_new_with_label("Cancel Download");
    g_signal_connect(runtime->download_start_button, "clicked",
                     G_CALLBACK(on_download_start_clicked), runtime);
    g_signal_connect(runtime->download_cancel_button, "clicked",
                     G_CALLBACK(on_download_cancel_clicked), runtime);
    gtk_box_append(GTK_BOX(actions), runtime->download_start_button);
    gtk_box_append(GTK_BOX(actions), runtime->download_cancel_button);
    gtk_box_append(GTK_BOX(box), runtime->download_release);
    gtk_box_append(GTK_BOX(box), runtime->download_status);
    gtk_box_append(GTK_BOX(box), runtime->download_progress);
    gtk_box_append(GTK_BOX(box), runtime->download_detail);
    gtk_box_append(GTK_BOX(box), actions);
    add_classic_label(
        box,
        "You can configure installation options while Windows downloads. Leaving this page does not cancel the transfer.",
        "classic-muted", TRUE);
    return box;
}

static GtkWidget *build_options_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Installation Options",
        "Locale, account, privacy, compatibility, and cleanup options are placeholders only.");
    GtkWidget *check = gtk_check_button_new_with_label(
        "Use recommended settings (placeholder)");
    static const char *categories[] = {
        "Locale and region",
        "Account setup",
        "Privacy preferences",
        "Windows 11 compatibility",
        "Online account options",
        "Debloat and cleanup"
    };
    size_t index;

    gtk_check_button_set_active(GTK_CHECK_BUTTON(check), TRUE);
    runtime->session->options_placeholder = true;
    gtk_box_append(GTK_BOX(box), check);
    add_classic_label(
        box, "Planned option categories", "classic-section-title", FALSE);
    for (index = 0; index < G_N_ELEMENTS(categories); ++index) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        GtkWidget *name = gtk_label_new(categories[index]);
        GtkWidget *state = gtk_label_new("Not configured in this milestone");

        gtk_widget_add_css_class(row, "classic-option-row");
        gtk_label_set_xalign(GTK_LABEL(name), 0.0F);
        gtk_widget_set_hexpand(name, TRUE);
        gtk_widget_add_css_class(state, "classic-muted");
        gtk_box_append(GTK_BOX(row), name);
        gtk_box_append(GTK_BOX(row), state);
        gtk_box_append(GTK_BOX(box), row);
    }
    return box;
}

static GtkWidget *build_summary_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Ready to Continue",
        "The installation engine connection will be enabled in a later milestone.");

    runtime->summary_disk = gtk_label_new("Target disk: not selected");
    runtime->summary_version = gtk_label_new("Windows version: not selected");
    runtime->summary_source = gtk_label_new("Windows source: not ready");
    runtime->summary_verification = gtk_label_new(
        "Download verification: not complete");
    runtime->summary_network = gtk_label_new("Network: not ready");
    runtime->summary_options = gtk_label_new(
        "Installation options: recommended placeholder settings");
    runtime->summary_notice = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_disk), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_version), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_source), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_verification), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_network), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_options), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_notice), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_disk), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_network), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_version), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_source), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_verification), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_options), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_notice), TRUE);
    gtk_widget_add_css_class(runtime->summary_disk, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_network, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_version, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_source, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_verification, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_options, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_notice, "classic-status");
    gtk_widget_add_css_class(runtime->summary_notice, "classic-warning");
    gtk_widget_set_visible(runtime->summary_notice, FALSE);
    gtk_box_append(GTK_BOX(box), runtime->summary_disk);
    gtk_box_append(GTK_BOX(box), runtime->summary_network);
    gtk_box_append(GTK_BOX(box), runtime->summary_version);
    gtk_box_append(GTK_BOX(box), runtime->summary_source);
    gtk_box_append(GTK_BOX(box), runtime->summary_verification);
    gtk_box_append(GTK_BOX(box), runtime->summary_options);
    gtk_box_append(GTK_BOX(box), runtime->summary_notice);
    add_classic_label(
        box,
        "Storage and image installation remain deferred until a later milestone.",
        "classic-muted",
        TRUE);
    return box;
}

static void update_summary(struct classicsetup_gui_runtime *runtime)
{
    char line[256];

    if (runtime->session->entry_mode ==
            CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN &&
        runtime->session->has_prepared_disk) {
        const struct classicsetup_disk_info *disk =
            &runtime->session->prepared_disk;

        (void)snprintf(
            line,
            sizeof(line),
            "Planned target disk: %.100s (%.100s)",
            disk->model,
            disk->device_path);
        gtk_label_set_text(GTK_LABEL(runtime->summary_disk), line);
    } else if (runtime->session->has_selected_disk) {
        const struct classicsetup_disk_info *disk =
            &runtime->session->assessments[
                runtime->session->selected_disk_index].disk;
        (void)snprintf(
            line,
            sizeof(line),
            "Target disk: %.100s (%.100s)",
            disk->model,
            disk->device_path);
        gtk_label_set_text(GTK_LABEL(runtime->summary_disk), line);
    }
    gtk_label_set_text(
        GTK_LABEL(runtime->summary_version),
        runtime->session->windows_version == CLASSICSETUP_GUI_WINDOWS_10
            ? "Windows family: Windows 10"
            : "Windows family: Windows 11");
    gtk_label_set_text(
        GTK_LABEL(runtime->summary_network),
        classicsetup_network_can_continue(&runtime->session->network)
            ? "Network: connected to the Internet"
            : "Network: Internet connection required");
    if (runtime->session->has_selected_release) {
        const struct classicsetup_windows_release *release =
            &runtime->session->source_catalog.releases[
                runtime->session->selected_release_index];

        (void)snprintf(line, sizeof(line), "Windows source: %.140s / %.60s",
                       release->release_name, release->language_name);
    } else {
        (void)snprintf(line, sizeof(line), "%s",
                       "Windows source: not selected");
    }
    gtk_label_set_text(GTK_LABEL(runtime->summary_source), line);
    gtk_label_set_text(
        GTK_LABEL(runtime->summary_verification),
        classicsetup_download_is_ready(
            &runtime->session->download, &runtime->session->workspace)
            ? "Download verification: complete"
            : "Download verification: not complete");
}

static void update_navigation(struct classicsetup_gui_runtime *runtime)
{
    gboolean can_next = TRUE;
    gboolean summary = runtime->session->page == CLASSICSETUP_GUI_PAGE_SUMMARY;

    if (runtime->session->page == CLASSICSETUP_GUI_PAGE_DISK) {
        can_next = runtime->session->has_selected_disk;
    } else if (runtime->session->page == CLASSICSETUP_GUI_PAGE_NETWORK) {
        can_next = classicsetup_network_has_connection(
            &runtime->session->network);
    } else if (runtime->session->page ==
               CLASSICSETUP_GUI_PAGE_WINDOWS_VERSION) {
        can_next = runtime->session->has_selected_release;
    } else if (runtime->session->page == CLASSICSETUP_GUI_PAGE_SUMMARY) {
        can_next = classicsetup_gui_summary_is_ready(runtime->session);
    }

    gtk_widget_set_sensitive(runtime->back_button, TRUE);
    gtk_widget_set_sensitive(runtime->next_button, can_next);
    gtk_button_set_label(
        GTK_BUTTON(runtime->next_button),
        summary ? "Install" : "Next");
    update_sidebar_progress(runtime);
    if (summary) {
        update_summary(runtime);
    }
}

static void set_page(
    struct classicsetup_gui_runtime *runtime,
    enum classicsetup_gui_page page)
{
    static const char *names[CLASSICSETUP_GUI_PAGE_COUNT] = {
        "disk", "network", "version", "download", "options", "summary"
    };

    runtime->session->page = page;
    gtk_stack_set_visible_child_name(
        GTK_STACK(runtime->stack),
        names[page]);
    if (page == CLASSICSETUP_GUI_PAGE_NETWORK &&
        runtime->network_controller_ready &&
        !runtime->network_controller.busy) {
        (void)classicsetup_network_controller_refresh(
            &runtime->network_controller);
    }
    if (page == CLASSICSETUP_GUI_PAGE_WINDOWS_VERSION &&
        runtime->session->source_catalog.state == CLASSICSETUP_SOURCE_IDLE) {
        start_source_discovery(runtime);
    }
    if (page == CLASSICSETUP_GUI_PAGE_DOWNLOAD) {
        update_download_page(runtime);
    }
    update_navigation(runtime);
}

static void on_back_clicked(
    GtkButton *button,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    enum classicsetup_gui_page destination;

    (void)button;
    if (classicsetup_gui_page_back_for_entry(
            runtime->session->entry_mode,
            runtime->session->page,
            &destination) == CLASSICSETUP_GUI_BACK_TO_TUI) {
        request_gui_exit(runtime, CLASSICSETUP_GUI_BACK);
        return;
    }
    set_page(runtime, destination);
}

static void on_next_clicked(
    GtkButton *button,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)button;
    if (runtime->session->page == CLASSICSETUP_GUI_PAGE_DISK &&
        !runtime->session->has_selected_disk) {
        gtk_label_set_text(
            GTK_LABEL(runtime->disk_status),
            "Choose an available disk before continuing.");
        return;
    }
    if (runtime->session->page == CLASSICSETUP_GUI_PAGE_SUMMARY) {
        if (classicsetup_gui_summary_is_ready(runtime->session)) {
            gtk_label_set_text(
                GTK_LABEL(runtime->summary_notice),
                "The Windows installation engine is not connected yet. No disk or filesystem changes were made.");
            gtk_widget_set_visible(runtime->summary_notice, TRUE);
        }
        return;
    }
    set_page(
        runtime,
        classicsetup_gui_page_next(runtime->session->page));
}

static gboolean on_window_close(
    GtkWindow *window,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)window;
    request_gui_exit(runtime, CLASSICSETUP_GUI_BACK);
    return TRUE;
}

static void load_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    GError *error = NULL;
    GdkDisplay *display = gdk_display_get_default();

    gtk_css_provider_load_from_path(
        provider,
        CLASSICSETUP_GUI_CSS_PATH);
    if (display != NULL) {
        gtk_style_context_add_provider_for_display(
            display,
            GTK_STYLE_PROVIDER(provider),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    }
    if (error != NULL) {
        g_error_free(error);
    }
    g_object_unref(provider);
}

static void activate(
    GtkApplication *application,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    GtkWidget *root;
    GtkWidget *header;
    GtkWidget *body;
    GtkWidget *sidebar;
    GtkWidget *dialog_stage;
    GtkWidget *dialog;
    GtkWidget *dialog_header;
    GtkWidget *footer;
    GtkWidget *footer_spacer;
    GtkWidget *header_text;
    GtkWidget *page;

    load_css();
    runtime->application = application;
    runtime->window = gtk_application_window_new(application);
    gtk_window_set_title(
        GTK_WINDOW(runtime->window),
        runtime->session->entry_mode ==
                CLASSICSETUP_GUI_ENTRY_ADVANCED_PLAN
            ? "ClassicSetup - Windows setup"
            : "ClassicSetup - Recommended installation");
    gtk_window_set_default_size(GTK_WINDOW(runtime->window), 1180, 760);
    gtk_widget_set_size_request(runtime->window, 940, 620);
    g_signal_connect(
        runtime->window,
        "close-request",
        G_CALLBACK(on_window_close),
        runtime);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(root, "classicsetup-background");
    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(header, "classicsetup-background-header");
    header_text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    add_classic_label(header_text, "ClassicSetup", "classic-brand", FALSE);
    add_classic_label(
        header_text,
        "Windows Setup",
        "classic-header-subtitle",
        FALSE);
    gtk_box_append(GTK_BOX(header), header_text);
    gtk_box_append(GTK_BOX(root), header);

    body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_vexpand(body, TRUE);
    sidebar = build_sidebar(runtime);
    gtk_box_append(GTK_BOX(body), sidebar);

    dialog_stage = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(dialog_stage, "classicsetup-dialog-stage");
    gtk_widget_set_hexpand(dialog_stage, TRUE);
    gtk_widget_set_vexpand(dialog_stage, TRUE);
    dialog = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(dialog, "classicsetup-dialog");
    gtk_widget_set_size_request(dialog, 660, 520);
    gtk_widget_set_halign(dialog, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(dialog, GTK_ALIGN_CENTER);
    dialog_header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(
        dialog_header, "classicsetup-dialog-header");
    add_classic_label(
        dialog_header,
        "ClassicSetup Windows Setup",
        "classicsetup-dialog-caption",
        FALSE);
    gtk_box_append(GTK_BOX(dialog), dialog_header);

    runtime->stack = gtk_stack_new();
    gtk_stack_set_transition_type(
        GTK_STACK(runtime->stack),
        GTK_STACK_TRANSITION_TYPE_NONE);
    page = build_disk_page(runtime);
    gtk_stack_add_named(
        GTK_STACK(runtime->stack), make_scrollable(page), "disk");
    page = build_network_page(runtime);
    gtk_stack_add_named(
        GTK_STACK(runtime->stack), make_scrollable(page), "network");
    page = build_windows_version_page(runtime);
    gtk_stack_add_named(
        GTK_STACK(runtime->stack), make_scrollable(page), "version");
    page = build_download_page(runtime);
    gtk_stack_add_named(
        GTK_STACK(runtime->stack), make_scrollable(page), "download");
    page = build_options_page(runtime);
    gtk_stack_add_named(
        GTK_STACK(runtime->stack), make_scrollable(page), "options");
    page = build_summary_page(runtime);
    gtk_stack_add_named(
        GTK_STACK(runtime->stack), make_scrollable(page), "summary");
    gtk_widget_set_hexpand(runtime->stack, TRUE);
    gtk_widget_set_vexpand(runtime->stack, TRUE);
    gtk_box_append(GTK_BOX(dialog), runtime->stack);

    footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(footer, "classicsetup-dialog-footer");
    runtime->back_button = gtk_button_new_with_label("Back");
    runtime->next_button = gtk_button_new_with_label("Next");
    footer_spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_hexpand(footer_spacer, TRUE);
    gtk_widget_set_halign(runtime->next_button, GTK_ALIGN_END);
    g_signal_connect(
        runtime->back_button,
        "clicked",
        G_CALLBACK(on_back_clicked),
        runtime);
    g_signal_connect(
        runtime->next_button,
        "clicked",
        G_CALLBACK(on_next_clicked),
        runtime);
    gtk_box_append(GTK_BOX(footer), runtime->back_button);
    gtk_box_append(GTK_BOX(footer), footer_spacer);
    gtk_box_append(GTK_BOX(footer), runtime->next_button);
    gtk_box_append(GTK_BOX(dialog), footer);
    gtk_box_append(GTK_BOX(dialog_stage), dialog);
    gtk_box_append(GTK_BOX(body), dialog_stage);
    gtk_box_append(GTK_BOX(root), body);
    gtk_window_set_child(GTK_WINDOW(runtime->window), root);
    {
        struct classicsetup_network_backend backend = {0};

        if (classicsetup_network_manager_backend_create(&backend) == 0) {
            classicsetup_network_controller_init(
                &runtime->network_controller,
                backend,
                network_snapshot_changed,
                runtime);
            runtime->network_controller_ready = true;
        }
    }
    update_network_page(runtime);
    update_source_controls(runtime);
    update_download_page(runtime);
    set_page(runtime, runtime->session->page);
    gtk_window_present(GTK_WINDOW(runtime->window));
}

int classicsetup_gui_run(
    struct classicsetup_gui_session *session)
{
    struct classicsetup_gui_runtime runtime = {
        .session = session,
        .result = CLASSICSETUP_GUI_ERROR
    };
    GtkApplication *application;
    int argc = 0;
    char **argv = NULL;

    if (session == NULL) {
        return CLASSICSETUP_GUI_ERROR;
    }
    atomic_init(&runtime.download_cancel_requested, false);
    application = gtk_application_new(
        "org.classicsetup.Setup",
        G_APPLICATION_DEFAULT_FLAGS);
    if (application == NULL) {
        return CLASSICSETUP_GUI_ERROR;
    }
    g_signal_connect(
        application,
        "activate",
        G_CALLBACK(activate),
        &runtime);
    (void)g_application_run(
        G_APPLICATION(application),
        argc,
        argv);
    if (runtime.network_controller_ready) {
        classicsetup_network_controller_destroy(
            &runtime.network_controller);
    }
    if (session->workspace.valid && !session->workspace.verified_iso) {
        classicsetup_workspace_cleanup_after_install(
            &session->workspace, false);
    } else if (session->workspace.valid) {
        classicsetup_workspace_cleanup_success(&session->workspace);
    }
    g_object_unref(application);
    return runtime.result;
}
