#include "classicsetup/gui.h"

#include <gtk/gtk.h>
#include <ctype.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

#ifndef CLASSICSETUP_ENABLE_WEBKIT_RETAIL
#define CLASSICSETUP_ENABLE_WEBKIT_RETAIL 0
#endif

#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
#include <webkit/webkit.h>
#endif

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
    GtkWidget *summary_release;
    GtkWidget *summary_language;
    GtkWidget *summary_architecture;
    GtkWidget *summary_storage;
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
    GtkWidget *language_dropdown;
    GtkStringList *language_model;
    GtkWidget *architecture_dropdown;
    GtkStringList *architecture_model;
    GtkWidget *edition_value;
    GtkWidget *change_source_button;
    GtkWidget *retail_source_buttons[4];
    char release_choices[CLASSICSETUP_SOURCE_MAX_RELEASES]
                        [CLASSICSETUP_SOURCE_NAME_SIZE];
    size_t release_choice_count;
    enum classicsetup_windows_language
        language_choices[CLASSICSETUP_SOURCE_MAX_RELEASES];
    size_t language_choice_count;
    enum classicsetup_windows_architecture architecture_choices[3];
    size_t architecture_choice_count;
    bool updating_source_controls;
    bool source_change_pending;
    GtkWidget *download_status;
    GtkWidget *download_detail;
    GtkWidget *download_size;
    GtkWidget *download_rate;
    GtkWidget *download_eta;
    GtkWidget *download_progress;
    GtkWidget *download_start_button;
    GtkWidget *download_cancel_button;
    GtkWidget *download_release;
    GtkWidget *retail_browser_box;
    GtkWidget *retail_preparing_box;
    GtkWidget *retail_preparing_selection;
    GtkWidget *retail_manual_notice;
    GtkWidget *retail_retry_button;
    GtkWidget *retail_existing_iso_button;
    GtkWidget *sidebar_download_box;
    GtkWidget *sidebar_download_title;
    GtkWidget *sidebar_download_progress;
    GtkWidget *sidebar_download_detail;
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
    WebKitWebView *retail_web_view;
    WebKitNetworkSession *retail_network_session;
    guint retail_automation_timer;
    unsigned int retail_automation_attempts;
    char retail_expected_sha256[65];
#endif
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
    struct classicsetup_source_resolve_diagnostics resolve_diagnostics;
    struct classicsetup_uup_status uup_status;
    struct classicsetup_retail_status retail_status;
    struct classicsetup_verified_windows_source verified_source;
};

struct download_task_request {
    struct classicsetup_gui_runtime *runtime;
    struct classicsetup_windows_release release;
    enum classicsetup_source_backend backend;
    bool pre_resolved;
};

struct source_task_request {
    enum classicsetup_windows_family family;
    enum classicsetup_source_backend backend;
};

struct progress_event {
    struct classicsetup_gui_runtime *runtime;
    struct classicsetup_download_status status;
};

struct uup_progress_event {
    struct classicsetup_gui_runtime *runtime;
    struct classicsetup_uup_status status;
};

struct retail_progress_event {
    struct classicsetup_gui_runtime *runtime;
    struct classicsetup_retail_status status;
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
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *header_text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    GtkWidget *icon = gtk_label_new("i");
    GtkWidget *separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);

    gtk_widget_add_css_class(box, "classicsetup-dialog-content");
    gtk_widget_add_css_class(header, "classicsetup-page-header");
    gtk_widget_add_css_class(icon, "classicsetup-page-icon");
    gtk_widget_set_hexpand(header_text, TRUE);
    add_classic_label(header_text, title, "classic-title", TRUE);
    add_classic_label(header_text, description, "classic-subtitle", TRUE);
    gtk_box_append(GTK_BOX(header), header_text);
    gtk_box_append(GTK_BOX(header), icon);
    gtk_box_append(GTK_BOX(box), header);
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
    {
        GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        runtime->sidebar_download_box =
            gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
        runtime->sidebar_download_title = gtk_label_new("Windows download");
        runtime->sidebar_download_progress = gtk_progress_bar_new();
        runtime->sidebar_download_detail = gtk_label_new("");
        gtk_widget_set_vexpand(spacer, TRUE);
        gtk_widget_add_css_class(runtime->sidebar_download_box,
                                 "classic-sidebar-download");
        gtk_label_set_xalign(GTK_LABEL(runtime->sidebar_download_title), 0.0F);
        gtk_label_set_xalign(GTK_LABEL(runtime->sidebar_download_detail), 0.0F);
        gtk_label_set_wrap(GTK_LABEL(runtime->sidebar_download_detail), TRUE);
        gtk_box_append(GTK_BOX(runtime->sidebar_download_box),
                       runtime->sidebar_download_title);
        gtk_box_append(GTK_BOX(runtime->sidebar_download_box),
                       runtime->sidebar_download_progress);
        gtk_box_append(GTK_BOX(runtime->sidebar_download_box),
                       runtime->sidebar_download_detail);
        gtk_widget_set_visible(runtime->sidebar_download_box, FALSE);
        gtk_box_append(GTK_BOX(sidebar), spacer);
        gtk_box_append(GTK_BOX(sidebar), runtime->sidebar_download_box);
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
        return classicsetup_gui_source_selection_is_valid(runtime->session);
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
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
static void start_retail_browser(struct classicsetup_gui_runtime *runtime);
#endif

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
    const struct source_task_request *request = task_data;
    struct classicsetup_source_catalog *catalog = g_new0(
        struct classicsetup_source_catalog, 1);

    (void)source_object;
    (void)cancellable;
    if (request->backend ==
        CLASSICSETUP_SOURCE_MICROSOFT_UUP) {
        (void)classicsetup_uup_recommended_catalog(
            request->family, catalog);
    } else if (classicsetup_retail_recommended_catalog(
                   request->family, catalog) != 0) {
        g_task_return_pointer(task, catalog, g_free);
        return;
    }
    g_task_return_pointer(task, catalog, g_free);
}

static void clear_string_model(GtkStringList *model)
{
    gtk_string_list_splice(
        model, 0, g_list_model_get_n_items(G_LIST_MODEL(model)), NULL);
}

static const char *language_label(enum classicsetup_windows_language language)
{
    return language == CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN
               ? "Korean"
               : "English";
}

static bool has_release_choice(
    const struct classicsetup_gui_runtime *runtime,
    const char *name)
{
    size_t index;

    for (index = 0; index < runtime->release_choice_count; ++index) {
        if (strcmp(runtime->release_choices[index], name) == 0) {
            return true;
        }
    }
    return false;
}

static bool has_language_choice(
    const struct classicsetup_gui_runtime *runtime,
    enum classicsetup_windows_language language)
{
    size_t index;

    for (index = 0; index < runtime->language_choice_count; ++index) {
        if (runtime->language_choices[index] == language) {
            return true;
        }
    }
    return false;
}

static bool has_architecture_choice(
    const struct classicsetup_gui_runtime *runtime,
    enum classicsetup_windows_architecture architecture)
{
    size_t index;

    for (index = 0; index < runtime->architecture_choice_count; ++index) {
        if (runtime->architecture_choices[index] == architecture) {
            return true;
        }
    }
    return false;
}

static void update_source_controls(
    struct classicsetup_gui_runtime *runtime)
{
    const struct classicsetup_source_catalog *catalog =
        &runtime->session->source_catalog;
    enum classicsetup_gui_source_change_requirement requirement =
        classicsetup_gui_source_change_requirement(runtime->session);
    gboolean editable = requirement == CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED &&
                        !runtime->source_task_active &&
                        !runtime->download_task_active;
    guint selected_release = GTK_INVALID_LIST_POSITION;
    guint selected_language = GTK_INVALID_LIST_POSITION;
    guint selected_architecture = GTK_INVALID_LIST_POSITION;
    size_t index;

    if (runtime->source_status == NULL || runtime->release_model == NULL ||
        runtime->language_model == NULL ||
        runtime->architecture_model == NULL) {
        return;
    }
    runtime->updating_source_controls = true;
    runtime->release_choice_count = 0;
    runtime->language_choice_count = 0;
    runtime->architecture_choice_count = 0;
    clear_string_model(runtime->release_model);
    clear_string_model(runtime->language_model);
    clear_string_model(runtime->architecture_model);

    if (catalog->state == CLASSICSETUP_SOURCE_READY) {
        for (index = 0; index < catalog->release_count; ++index) {
            const struct classicsetup_windows_release *candidate =
                &catalog->releases[index];

            if (!has_release_choice(runtime, candidate->release_name)) {
                (void)snprintf(
                    runtime->release_choices[runtime->release_choice_count],
                    CLASSICSETUP_SOURCE_NAME_SIZE, "%s",
                    candidate->release_name);
                gtk_string_list_append(
                    runtime->release_model, candidate->release_name);
                ++runtime->release_choice_count;
            }
        }
        if (!runtime->session->has_selected_release_name && editable &&
            runtime->release_choice_count > 0) {
            (void)classicsetup_gui_select_release_name(
                runtime->session, runtime->release_choices[0]);
        }
        for (index = 0; index < runtime->release_choice_count; ++index) {
            if (runtime->session->has_selected_release_name &&
                strcmp(runtime->release_choices[index],
                       runtime->session->selected_release_name) == 0) {
                selected_release = (guint)index;
                break;
            }
        }
        if (runtime->session->has_selected_release_name) {
            for (index = 0; index < catalog->release_count; ++index) {
                const struct classicsetup_windows_release *candidate =
                    &catalog->releases[index];

                if (strcmp(candidate->release_name,
                           runtime->session->selected_release_name) == 0 &&
                    !has_language_choice(runtime, candidate->language)) {
                    runtime->language_choices[
                        runtime->language_choice_count++] =
                        candidate->language;
                    gtk_string_list_append(
                        runtime->language_model,
                        language_label(candidate->language));
                }
            }
        }
        if (!runtime->session->has_selected_language && editable &&
            runtime->language_choice_count > 0) {
            enum classicsetup_windows_language preferred =
                CLASSICSETUP_WINDOWS_LANGUAGE_ENGLISH;

            if (has_language_choice(
                    runtime, CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN)) {
                preferred = CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN;
            } else {
                preferred = runtime->language_choices[0];
            }
            (void)classicsetup_gui_select_language(
                runtime->session, preferred);
        }
        for (index = 0; index < runtime->language_choice_count; ++index) {
            if (runtime->session->has_selected_language &&
                runtime->language_choices[index] ==
                    runtime->session->selected_language) {
                selected_language = (guint)index;
                break;
            }
        }
        if (runtime->session->has_selected_release_name &&
            runtime->session->has_selected_language) {
            for (index = 0; index < catalog->release_count; ++index) {
                const struct classicsetup_windows_release *candidate =
                    &catalog->releases[index];

                if (strcmp(candidate->release_name,
                           runtime->session->selected_release_name) == 0 &&
                    candidate->language == runtime->session->selected_language &&
                    !has_architecture_choice(
                        runtime, candidate->architecture)) {
                    runtime->architecture_choices[
                        runtime->architecture_choice_count++] =
                        candidate->architecture;
                    gtk_string_list_append(
                        runtime->architecture_model,
                        classicsetup_windows_architecture_label(
                            candidate->architecture));
                }
            }
        }
        if (!runtime->session->has_selected_architecture && editable &&
            runtime->architecture_choice_count > 0) {
            enum classicsetup_windows_architecture preferred =
                runtime->architecture_choices[0];

            for (index = 0;
                 index < runtime->architecture_choice_count; ++index) {
                if (classicsetup_windows_architecture_is_native(
                        runtime->architecture_choices[index])) {
                    preferred = runtime->architecture_choices[index];
                    break;
                }
            }
            (void)classicsetup_gui_select_architecture(
                runtime->session, preferred);
        }
        for (index = 0; index < runtime->architecture_choice_count; ++index) {
            if (runtime->session->has_selected_architecture &&
                runtime->architecture_choices[index] ==
                    runtime->session->selected_architecture) {
                selected_architecture = (guint)index;
                break;
            }
        }
    }
    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(runtime->release_dropdown), selected_release);
    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(runtime->language_dropdown), selected_language);
    gtk_drop_down_set_selected(
        GTK_DROP_DOWN(runtime->architecture_dropdown), selected_architecture);
    runtime->updating_source_controls = false;

    if (catalog->state == CLASSICSETUP_SOURCE_DISCOVERING) {
        gtk_label_set_text(GTK_LABEL(runtime->source_status),
                           "Loading official Microsoft releases...");
    } else if (catalog->state == CLASSICSETUP_SOURCE_ERROR) {
        gtk_label_set_text(GTK_LABEL(runtime->source_status), catalog->error);
    } else if (catalog->state == CLASSICSETUP_SOURCE_READY) {
        gtk_label_set_text(
            GTK_LABEL(runtime->source_status),
            runtime->session->source_backend ==
                    CLASSICSETUP_SOURCE_MICROSOFT_UUP
                ? "Source: Microsoft Windows Update. The resulting image version will be shown after verification."
                : "Source: Microsoft official ISO. Available editions are read from the downloaded image.");
    } else {
        gtk_label_set_text(GTK_LABEL(runtime->source_status),
                           "Release discovery has not started.");
    }
    gtk_widget_set_sensitive(runtime->windows11_button, editable);
    gtk_widget_set_sensitive(runtime->windows10_button, editable);
    gtk_widget_set_sensitive(
        runtime->release_dropdown,
        editable && runtime->release_choice_count > 0);
    gtk_widget_set_sensitive(
        runtime->language_dropdown,
        editable && runtime->language_choice_count > 0);
    gtk_widget_set_sensitive(
        runtime->architecture_dropdown,
        editable && runtime->architecture_choice_count > 0);
    for (index = 0; index < 4; ++index) {
        if (runtime->retail_source_buttons[index] != NULL) {
            gtk_widget_set_sensitive(
                runtime->retail_source_buttons[index],
                editable && index <=
                    CLASSICSETUP_GUI_RETAIL_MICROSOFT_PAGE);
        }
    }
    gtk_widget_set_visible(
        runtime->change_source_button,
        requirement != CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED);
    if (runtime->edition_value != NULL) {
        const char *edition_name = "Windows 11 Pro";

        if (runtime->session->has_selected_release &&
            runtime->session->source_catalog.releases[
                runtime->session->selected_release_index]
                    .edition_name[0] != '\0') {
            edition_name = runtime->session->source_catalog.releases[
                runtime->session->selected_release_index].edition_name;
        }
        gtk_label_set_text(
            GTK_LABEL(runtime->edition_value), edition_name);
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
    struct source_task_request *request;

    if (runtime->source_task_active || runtime->download_task_active) {
        return;
    }
    runtime->session->has_selected_release = false;
    runtime->session->has_selected_release_name = false;
    runtime->session->has_selected_language = false;
    runtime->session->has_selected_architecture = false;
    runtime->session->selected_release_name[0] = '\0';
    classicsetup_source_resolve_diagnostics_reset(
        &runtime->session->source_diagnostics);
    classicsetup_source_catalog_reset(&runtime->session->source_catalog);
    runtime->session->source_catalog.state =
        CLASSICSETUP_SOURCE_DISCOVERING;
    runtime->source_task_active = true;
    update_source_controls(runtime);
    task = g_task_new(NULL, NULL, source_discovery_finished, runtime);
    request = g_new0(struct source_task_request, 1);
    request->family = selected_family(runtime->session);
    request->backend = runtime->session->source_backend;
    g_task_set_task_data(task, request, g_free);
    g_task_run_in_thread(task, source_discovery_worker);
    g_object_unref(task);
}

static gboolean progress_on_main(gpointer user_data)
{
    struct progress_event *event = user_data;

    event->runtime->session->download = event->status;
    if (event->runtime->session->retail_browser_status.stage >=
            CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING &&
        event->runtime->session->retail_browser_status.stage <
            CLASSICSETUP_RETAIL_BROWSER_COMPLETE) {
        if (event->status.state == CLASSICSETUP_DOWNLOAD_VERIFYING) {
            event->runtime->session->retail_browser_status.stage =
                strncmp(event->status.message, "Inspecting", 10) == 0
                    ? CLASSICSETUP_RETAIL_BROWSER_INSPECTING_IMAGE
                    : CLASSICSETUP_RETAIL_BROWSER_VERIFYING_ISO;
        } else if (event->status.state == CLASSICSETUP_DOWNLOAD_DOWNLOADING ||
                   event->status.state == CLASSICSETUP_DOWNLOAD_PREPARING) {
            event->runtime->session->retail_browser_status.stage =
                CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING;
        }
    }
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

static const char *uup_stage_message(enum classicsetup_uup_stage stage)
{
    switch (stage) {
    case CLASSICSETUP_UUP_CHECKING_TOOL:
        return "Checking Windows source tools...";
    case CLASSICSETUP_UUP_SEARCHING:
        return "Searching Microsoft Windows Update...";
    case CLASSICSETUP_UUP_RESOLVING:
        return "Resolving a stable Windows release...";
    case CLASSICSETUP_UUP_DOWNLOADING:
        return "Downloading Windows files...";
    case CLASSICSETUP_UUP_VERIFYING_PAYLOAD:
        return "Verifying downloaded Windows files...";
    case CLASSICSETUP_UUP_BUILDING_IMAGE:
        return "Preparing the Windows image...";
    case CLASSICSETUP_UUP_VERIFYING_IMAGE:
        return "Verifying the Windows image...";
    case CLASSICSETUP_UUP_COMPLETE:
        return "Windows is ready to install.";
    case CLASSICSETUP_UUP_CANCELLED:
        return "Windows download was cancelled.";
    case CLASSICSETUP_UUP_FAILED:
        return "Windows could not be prepared.";
    case CLASSICSETUP_UUP_IDLE:
        break;
    }
    return "Ready to download Windows.";
}

static gboolean uup_progress_on_main(gpointer user_data)
{
    struct uup_progress_event *event = user_data;
    struct classicsetup_download_status *download =
        &event->runtime->session->download;

    event->runtime->session->uup_status = event->status;
    download->bytes_received = event->status.bytes_received;
    download->total_bytes = event->status.total_bytes;
    download->progress_fraction =
        event->status.stage == CLASSICSETUP_UUP_COMPLETE ? 1.0 : 0.0;
    if (event->status.stage == CLASSICSETUP_UUP_CANCELLED) {
        download->state = CLASSICSETUP_DOWNLOAD_CANCELLED;
        download->error = CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED;
    } else if (event->status.stage == CLASSICSETUP_UUP_FAILED) {
        download->state = CLASSICSETUP_DOWNLOAD_FAILED;
        download->error = CLASSICSETUP_DOWNLOAD_ERROR_SOURCE;
    } else if (event->status.stage == CLASSICSETUP_UUP_COMPLETE) {
        download->state = CLASSICSETUP_DOWNLOAD_COMPLETE;
        download->error = CLASSICSETUP_DOWNLOAD_ERROR_NONE;
    } else if (event->status.stage == CLASSICSETUP_UUP_CHECKING_TOOL ||
               event->status.stage == CLASSICSETUP_UUP_SEARCHING ||
               event->status.stage == CLASSICSETUP_UUP_RESOLVING) {
        download->state = CLASSICSETUP_DOWNLOAD_PREPARING;
    } else if (event->status.stage ==
               CLASSICSETUP_UUP_VERIFYING_IMAGE) {
        download->state = CLASSICSETUP_DOWNLOAD_VERIFYING;
    } else {
        download->state = CLASSICSETUP_DOWNLOAD_DOWNLOADING;
    }
    (void)snprintf(download->message, sizeof(download->message), "%s",
                   uup_stage_message(event->status.stage));
    update_download_page(event->runtime);
    g_free(event);
    return G_SOURCE_REMOVE;
}

static void uup_progress_from_worker(
    const struct classicsetup_uup_status *status,
    void *user_data)
{
    struct uup_progress_event *event = g_new0(
        struct uup_progress_event, 1);

    event->runtime = user_data;
    event->status = *status;
    g_main_context_invoke(NULL, uup_progress_on_main, event);
}

static bool uup_cancel_requested(void *context)
{
    return atomic_load((atomic_bool *)context);
}

static gboolean retail_progress_on_main(gpointer user_data)
{
    struct retail_progress_event *event = user_data;
    struct classicsetup_download_status *download =
        &event->runtime->session->download;

    event->runtime->session->retail_status = event->status;
    if (event->status.stage == CLASSICSETUP_RETAIL_CANCELLED) {
        download->state = CLASSICSETUP_DOWNLOAD_CANCELLED;
        download->error = CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED;
    } else if (event->status.stage == CLASSICSETUP_RETAIL_FAILED) {
        download->state = CLASSICSETUP_DOWNLOAD_FAILED;
        download->error = CLASSICSETUP_DOWNLOAD_ERROR_SOURCE;
    } else {
        download->state = CLASSICSETUP_DOWNLOAD_PREPARING;
        download->error = CLASSICSETUP_DOWNLOAD_ERROR_NONE;
    }
    (void)snprintf(download->message, sizeof(download->message), "%s",
                   event->status.detail);
    update_download_page(event->runtime);
    g_free(event);
    return G_SOURCE_REMOVE;
}

static void retail_progress_from_worker(
    const struct classicsetup_retail_status *status, void *user_data)
{
    struct retail_progress_event *event = g_new0(
        struct retail_progress_event, 1);

    event->runtime = user_data;
    event->status = *status;
    g_main_context_invoke(NULL, retail_progress_on_main, event);
}

static void download_worker(
    GTask *task, gpointer source_object, gpointer task_data,
    GCancellable *cancellable)
{
    const struct download_task_request *request = task_data;
    struct classicsetup_gui_runtime *runtime = request->runtime;
    struct download_task_result *result = g_new0(
        struct download_task_result, 1);

    (void)source_object;
    (void)cancellable;
    result->release = request->release;
    classicsetup_download_status_reset(&result->status);
    result->status.state = CLASSICSETUP_DOWNLOAD_PREPARING;
    if (request->backend ==
        CLASSICSETUP_SOURCE_MICROSOFT_UUP) {
        struct classicsetup_uup_target target;
        struct classicsetup_workspace_diagnostics workspace_diagnostics;
        int workspace_result;

        workspace_result = classicsetup_workspace_create_for_reserve(
            &result->workspace,
            CLASSICSETUP_UUP_WORKSPACE_RESERVE_BYTES,
            &workspace_diagnostics);
#ifndef NDEBUG
        {
            char diagnostic_text[512];

            if (classicsetup_workspace_format_diagnostics(
                    &workspace_diagnostics, diagnostic_text,
                    sizeof(diagnostic_text)) == 0) {
                g_debug("ClassicSetup workspace: %s", diagnostic_text);
            }
        }
#endif
        if (workspace_result != CLASSICSETUP_WORKSPACE_CREATE_OK) {
            result->status.state = CLASSICSETUP_DOWNLOAD_FAILED;
            result->status.error =
                workspace_result == CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE
                    ? CLASSICSETUP_DOWNLOAD_ERROR_OUT_OF_SPACE
                    : CLASSICSETUP_DOWNLOAD_ERROR_WRITE;
            result->uup_status.stage = CLASSICSETUP_UUP_FAILED;
            result->uup_status.error =
                workspace_result == CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE
                    ? CLASSICSETUP_UUP_ERROR_OUT_OF_SPACE
                    : CLASSICSETUP_UUP_ERROR_WORKSPACE_FAILED;
            result->uup_status.workspace_available_bytes =
                workspace_diagnostics.available_bytes;
            result->uup_status.workspace_required_bytes =
                workspace_diagnostics.required_bytes;
            (void)snprintf(result->uup_status.workspace_root,
                           sizeof(result->uup_status.workspace_root), "%s",
                           workspace_diagnostics.root_path);
            (void)snprintf(result->status.message,
                           sizeof(result->status.message), "%s",
                           workspace_result ==
                                   CLASSICSETUP_WORKSPACE_CREATE_NO_SPACE
                               ? "There is not enough temporary storage."
                               : "The temporary workspace could not be created.");
        } else if (classicsetup_uup_recommended_target(&target) != 0 ||
                   classicsetup_uup_download_and_build_iso(
                       &target, &result->workspace,
                       uup_cancel_requested,
                       &runtime->download_cancel_requested,
                       uup_progress_from_worker, runtime,
                       &result->uup_status,
                       &result->verified_source) != 0) {
            result->status.state =
                result->uup_status.stage == CLASSICSETUP_UUP_CANCELLED
                    ? CLASSICSETUP_DOWNLOAD_CANCELLED
                    : CLASSICSETUP_DOWNLOAD_FAILED;
            result->status.error =
                result->uup_status.stage == CLASSICSETUP_UUP_CANCELLED
                    ? CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED
                    : CLASSICSETUP_DOWNLOAD_ERROR_SOURCE;
            (void)snprintf(result->status.message,
                           sizeof(result->status.message), "%s",
                           classicsetup_uup_error_message(
                               result->uup_status.error));
        } else {
            result->status.state = CLASSICSETUP_DOWNLOAD_COMPLETE;
            result->status.error = CLASSICSETUP_DOWNLOAD_ERROR_NONE;
            result->status.progress_fraction = 1.0;
            result->status.bytes_received =
                result->uup_status.bytes_received;
            result->status.total_bytes = result->uup_status.total_bytes;
            (void)snprintf(result->status.message,
                           sizeof(result->status.message), "%s",
                           "Windows is ready to install.");
        }
    } else if (!request->pre_resolved && classicsetup_retail_resolve(
            &result->release, &runtime->download_cancel_requested,
            retail_progress_from_worker, runtime,
            &result->retail_status) != 0) {
        result->status.state = CLASSICSETUP_DOWNLOAD_FAILED;
        result->status.error =
            result->retail_status.stage == CLASSICSETUP_RETAIL_CANCELLED
                ? CLASSICSETUP_DOWNLOAD_ERROR_CANCELLED
                : CLASSICSETUP_DOWNLOAD_ERROR_SOURCE;
        (void)snprintf(result->status.message,
                       sizeof(result->status.message), "%s",
                       classicsetup_retail_error_message(
                           result->retail_status.error));
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
            &result->status,
            &result->verified_source);
    }
    g_task_return_pointer(task, result, g_free);
}

static void free_download_task_request(gpointer user_data)
{
    struct download_task_request *request = user_data;

    if (request != NULL) {
        classicsetup_retail_browser_clear_uri(&request->release);
        g_free(request);
    }
}

static void download_finished(
    GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    struct download_task_result *task_result;
    GError *error = NULL;
    bool start_webview_fallback = false;

    (void)source_object;
    task_result = g_task_propagate_pointer(G_TASK(result), &error);
    runtime->download_task_active = false;
    if (task_result != NULL) {
        memset(task_result->release.download_uri, 0,
               sizeof(task_result->release.download_uri));
        task_result->release.resolved = false;
        runtime->session->source_catalog.releases[
            runtime->session->selected_release_index] = task_result->release;
        runtime->session->source_diagnostics =
            task_result->resolve_diagnostics;
        runtime->session->download = task_result->status;
        runtime->session->workspace = task_result->workspace;
        runtime->session->uup_status = task_result->uup_status;
        runtime->session->retail_status = task_result->retail_status;
        runtime->session->verified_source =
            task_result->verified_source;
        if (task_result->status.state == CLASSICSETUP_DOWNLOAD_FAILED &&
            task_result->status.error == CLASSICSETUP_DOWNLOAD_ERROR_SOURCE &&
            runtime->session->source_backend ==
                CLASSICSETUP_SOURCE_MICROSOFT_RETAIL &&
            runtime->session->retail_source_mode ==
                CLASSICSETUP_GUI_RETAIL_AUTOMATIC) {
            start_webview_fallback =
                classicsetup_gui_retail_start_webview_once(runtime->session);
        }
        if (runtime->session->retail_browser_status.stage >=
            CLASSICSETUP_RETAIL_BROWSER_DOWNLOADING) {
            if (task_result->status.state ==
                CLASSICSETUP_DOWNLOAD_COMPLETE) {
                runtime->session->retail_browser_status.stage =
                    CLASSICSETUP_RETAIL_BROWSER_COMPLETE;
            } else if (task_result->status.state ==
                       CLASSICSETUP_DOWNLOAD_CANCELLED) {
                runtime->session->retail_browser_status.stage =
                    CLASSICSETUP_RETAIL_BROWSER_CANCELLED;
            } else {
                runtime->session->retail_browser_status.stage =
                    CLASSICSETUP_RETAIL_BROWSER_FAILED;
            }
        }
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
    if (runtime->source_change_pending) {
        runtime->source_change_pending = false;
        classicsetup_gui_discard_downloaded_source(runtime->session);
        update_source_controls(runtime);
    }
    update_download_page(runtime);
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
    if (start_webview_fallback) {
        start_retail_browser(runtime);
    }
#else
    (void)start_webview_fallback;
#endif
    finish_pending_exit(runtime);
}

static void start_download_release(
    struct classicsetup_gui_runtime *runtime,
    const struct classicsetup_windows_release *release,
    bool pre_resolved)
{
    GTask *task;
    struct download_task_request *request;

    if (runtime->download_task_active || runtime->source_task_active ||
        release == NULL ||
        !classicsetup_gui_source_selection_is_valid(runtime->session) ||
        !classicsetup_network_can_continue(&runtime->session->network)) {
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
    request = g_new0(struct download_task_request, 1);
    request->runtime = runtime;
    request->release = *release;
    request->backend = runtime->session->source_backend;
    request->pre_resolved = pre_resolved;
    task = g_task_new(NULL, NULL, download_finished, runtime);
    g_task_set_task_data(task, request, free_download_task_request);
    g_task_run_in_thread(task, download_worker);
    g_object_unref(task);
}

static void start_download(struct classicsetup_gui_runtime *runtime)
{
    if (runtime == NULL || !runtime->session->has_selected_release) {
        return;
    }
    start_download_release(
        runtime,
        &runtime->session->source_catalog.releases[
            runtime->session->selected_release_index],
        false);
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
    if (!runtime->updating_source_controls &&
        selected < runtime->release_choice_count &&
        classicsetup_gui_select_release_name(
            runtime->session,
            runtime->release_choices[selected]) == 0) {
        update_source_controls(runtime);
    }
    update_navigation(runtime);
}

static void on_language_selected(
    GObject *object, GParamSpec *parameter, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));

    (void)parameter;
    if (!runtime->updating_source_controls &&
        selected < runtime->language_choice_count &&
        classicsetup_gui_select_language(
            runtime->session,
            runtime->language_choices[selected]) == 0) {
        update_source_controls(runtime);
    }
    update_navigation(runtime);
}

static void on_architecture_selected(
    GObject *object, GParamSpec *parameter, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    guint selected = gtk_drop_down_get_selected(GTK_DROP_DOWN(object));

    (void)parameter;
    if (!runtime->updating_source_controls &&
        selected < runtime->architecture_choice_count) {
        (void)classicsetup_gui_select_architecture(
            runtime->session,
            runtime->architecture_choices[selected]);
    }
    update_navigation(runtime);
}

static void on_retail_source_toggled(
    GtkCheckButton *button, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    int value;

    if (!gtk_check_button_get_active(button)) {
        return;
    }
    value = GPOINTER_TO_INT(g_object_get_data(
        G_OBJECT(button), "classicsetup-retail-source"));
    (void)classicsetup_gui_set_retail_source_mode(
        runtime->session,
        (enum classicsetup_gui_retail_source_mode)value);
    update_download_page(runtime);
}

static GtkWidget *build_source_option(
    struct classicsetup_gui_runtime *runtime,
    enum classicsetup_gui_retail_source_mode mode,
    const char *icon_text, const char *title, const char *description,
    gboolean enabled, GtkCheckButton *group)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *button = gtk_check_button_new();
    GtkWidget *icon = gtk_label_new(icon_text);
    GtkWidget *text = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);

    gtk_widget_add_css_class(row, "classic-source-option");
    gtk_widget_add_css_class(icon, "classic-source-icon");
    if (group != NULL) {
        gtk_check_button_set_group(GTK_CHECK_BUTTON(button), group);
    }
    g_object_set_data(G_OBJECT(button), "classicsetup-retail-source",
                      GINT_TO_POINTER(mode));
    g_signal_connect(button, "toggled",
                     G_CALLBACK(on_retail_source_toggled), runtime);
    gtk_widget_set_sensitive(row, enabled);
    gtk_widget_set_sensitive(button, enabled);
    add_classic_label(text, title, "classic-option-title", TRUE);
    add_classic_label(text, description, "classic-option-description", TRUE);
    gtk_widget_set_hexpand(text, TRUE);
    gtk_box_append(GTK_BOX(row), icon);
    gtk_box_append(GTK_BOX(row), button);
    gtk_box_append(GTK_BOX(row), text);
    runtime->retail_source_buttons[mode] = button;
    return row;
}

static void source_change_confirmed(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    GError *error = NULL;
    int choice = gtk_alert_dialog_choose_finish(
        GTK_ALERT_DIALOG(source_object), result, &error);

    if (error != NULL) {
        g_error_free(error);
        return;
    }
    if (choice != 0) {
        return;
    }
    if (runtime->download_task_active) {
        runtime->source_change_pending = true;
        atomic_store(&runtime->download_cancel_requested, true);
        gtk_label_set_text(
            GTK_LABEL(runtime->source_status),
            "Cancelling the current download before changing the source...");
    } else {
        classicsetup_gui_discard_downloaded_source(runtime->session);
        update_source_controls(runtime);
        update_download_page(runtime);
    }
}

static void on_change_source_clicked(GtkButton *button, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    enum classicsetup_gui_source_change_requirement requirement =
        classicsetup_gui_source_change_requirement(runtime->session);
    GtkAlertDialog *dialog;
    const char *cancel_buttons[] = {
        "Cancel Download and Change", "Keep Current Download", NULL
    };
    const char *discard_buttons[] = {
        "Discard and Change", "Keep Current Image", NULL
    };

    (void)button;
    if (requirement == CLASSICSETUP_GUI_SOURCE_CHANGE_ALLOWED) {
        return;
    }
    dialog = gtk_alert_dialog_new(
        "%s",
        requirement == CLASSICSETUP_GUI_SOURCE_CHANGE_CANCEL_DOWNLOAD
            ? "Changing the Windows source will cancel the current download."
            : "The selected Windows image has already been downloaded.");
    gtk_alert_dialog_set_detail(
        dialog,
        requirement == CLASSICSETUP_GUI_SOURCE_CHANGE_CANCEL_DOWNLOAD
            ? "The partial download will be removed after the worker stops."
            : "Changing the source will remove the verified image and require a new download.");
    gtk_alert_dialog_set_buttons(
        dialog,
        requirement == CLASSICSETUP_GUI_SOURCE_CHANGE_CANCEL_DOWNLOAD
            ? cancel_buttons
            : discard_buttons);
    gtk_alert_dialog_set_default_button(dialog, 1);
    gtk_alert_dialog_set_cancel_button(dialog, 1);
    gtk_alert_dialog_choose(
        dialog,
        GTK_WINDOW(runtime->window),
        NULL,
        source_change_confirmed,
        runtime);
    g_object_unref(dialog);
}

static GtkWidget *build_windows_version_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Choose a Windows source",
        "Select how ClassicSetup should obtain the Windows installation image.");
    GtkWidget *windows11 = gtk_check_button_new_with_label("Windows 11");
    GtkWidget *windows10 = gtk_check_button_new_with_label("Windows 10");
    GtkWidget *automatic;

    add_classic_label(box, "Windows installation files",
                      "classic-section-title", FALSE);
    automatic = build_source_option(
        runtime, CLASSICSETUP_GUI_RETAIL_AUTOMATIC, "↓",
        "Download automatically from Microsoft — Recommended",
        "ClassicSetup prepares an official Microsoft download automatically and opens the Microsoft page only if needed.",
        TRUE, NULL);
    gtk_box_append(GTK_BOX(box), automatic);
    gtk_box_append(GTK_BOX(box), build_source_option(
        runtime, CLASSICSETUP_GUI_RETAIL_MICROSOFT_PAGE, "W",
        "Use Microsoft download page",
        "Use Microsoft's official page and click the real 64-bit download control.",
        TRUE, GTK_CHECK_BUTTON(runtime->retail_source_buttons[
                  CLASSICSETUP_GUI_RETAIL_AUTOMATIC])));
    gtk_box_append(GTK_BOX(box), build_source_option(
        runtime, CLASSICSETUP_GUI_RETAIL_EXISTING_ISO, "▣",
        "Use existing ISO — Not available yet",
        "Choose a local Windows ISO file.", FALSE,
        GTK_CHECK_BUTTON(runtime->retail_source_buttons[
            CLASSICSETUP_GUI_RETAIL_AUTOMATIC])));
    gtk_box_append(GTK_BOX(box), build_source_option(
        runtime, CLASSICSETUP_GUI_RETAIL_CUSTOM, "…",
        "Custom download — Future option",
        "Use a custom source after this safe workflow is implemented.", FALSE,
        GTK_CHECK_BUTTON(runtime->retail_source_buttons[
            CLASSICSETUP_GUI_RETAIL_AUTOMATIC])));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(
        runtime->retail_source_buttons[runtime->session->retail_source_mode]),
        TRUE);

    runtime->windows11_button = windows11;
    runtime->windows10_button = windows10;

    add_classic_label(
        box, "Windows image", "classic-section-title", FALSE);
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
    add_classic_label(box, "Release", "classic-section-title", FALSE);
    runtime->release_model = gtk_string_list_new(NULL);
    runtime->release_dropdown = gtk_drop_down_new(
        G_LIST_MODEL(runtime->release_model), NULL);
    gtk_widget_set_sensitive(runtime->release_dropdown, FALSE);
    g_signal_connect(runtime->release_dropdown, "notify::selected",
                     G_CALLBACK(on_release_selected), runtime);
    gtk_box_append(GTK_BOX(box), runtime->release_dropdown);
    add_classic_label(
        box, "Installation language", "classic-section-title", FALSE);
    runtime->language_model = gtk_string_list_new(NULL);
    runtime->language_dropdown = gtk_drop_down_new(
        G_LIST_MODEL(runtime->language_model), NULL);
    gtk_widget_set_sensitive(runtime->language_dropdown, FALSE);
    g_signal_connect(runtime->language_dropdown, "notify::selected",
                     G_CALLBACK(on_language_selected), runtime);
    gtk_box_append(GTK_BOX(box), runtime->language_dropdown);
    add_classic_label(box, "Architecture", "classic-section-title", FALSE);
    runtime->architecture_model = gtk_string_list_new(NULL);
    runtime->architecture_dropdown = gtk_drop_down_new(
        G_LIST_MODEL(runtime->architecture_model), NULL);
    gtk_widget_set_sensitive(runtime->architecture_dropdown, FALSE);
    g_signal_connect(runtime->architecture_dropdown, "notify::selected",
                     G_CALLBACK(on_architecture_selected), runtime);
    gtk_box_append(GTK_BOX(box), runtime->architecture_dropdown);
    add_classic_label(box, "Edition", "classic-section-title", FALSE);
    runtime->edition_value = gtk_label_new("Windows 11 Pro");
    gtk_label_set_xalign(GTK_LABEL(runtime->edition_value), 0.0F);
    gtk_widget_add_css_class(runtime->edition_value,
                             "classic-source-summary");
    gtk_box_append(GTK_BOX(box), runtime->edition_value);
    runtime->source_status = gtk_label_new(
        "Release discovery has not started.");
    gtk_label_set_xalign(GTK_LABEL(runtime->source_status), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->source_status), TRUE);
    gtk_box_append(GTK_BOX(box), runtime->source_status);
    runtime->change_source_button = gtk_button_new_with_label(
        "Change Windows Source...");
    gtk_widget_set_visible(runtime->change_source_button, FALSE);
    g_signal_connect(runtime->change_source_button, "clicked",
                     G_CALLBACK(on_change_source_clicked), runtime);
    gtk_box_append(GTK_BOX(box), runtime->change_source_button);
    return box;
}

#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
/* These IDs are present in Microsoft's current Windows 11 public download
 * page. The script stops and exposes the full page if any expected element is
 * absent; it never guesses a SKU or interacts with a challenge. */
static const char retail_prepare_script_format[] =
    "(() => {"
    "if(document.querySelector('iframe[src*=\"captcha\" i],"
    "[id*=\"captcha\" i],[class*=\"captcha\" i]'))return 'manual';"
    "const links=document.querySelector('#SoftwareDownload_DownloadLinks');"
    "const dl=links&&links.querySelector("
    "'a[href^=\"https://software.download.prss.microsoft.com/\"]');"
    "if(dl){let hash='';const rows=[...links.querySelectorAll('tr')];"
    "const row=rows.find(r=>/(%s)/i.test(r.textContent||''));"
    "if(row){const match=(row.textContent||'').match(/\\b[0-9a-f]{64}\\b/i);"
    "if(match)hash=match[0].toUpperCase();}"
    "if(!document.getElementById('classicsetup-download-focus')){"
    "const s=document.createElement('style');"
    "s.id='classicsetup-download-focus';"
    "s.textContent='body *{visibility:hidden!important}' +"
    "'#SoftwareDownload_DownloadLinks,' +"
    "'#SoftwareDownload_DownloadLinks *{visibility:visible!important}' +"
    "'#SoftwareDownload_DownloadLinks{position:fixed!important;inset:0!important;' +"
    "'overflow:auto!important;background:white!important;padding:24px!important}';"
    "document.head.appendChild(s);}return 'ready|'+hash;}"
    "const edition=document.querySelector('#product-edition');"
    "if(edition&&edition.value==='null'){"
    "const option=[...edition.options].find(o=>o.value&&o.value!=='null'&&"
    "/x64/i.test(o.textContent||''));"
    "if(!option)return 'waiting';edition.value=option.value;"
    "edition.dispatchEvent(new Event('change',{bubbles:true}));"
    "const confirm=document.querySelector('#submit-product-edition');"
    "if(!confirm)return 'manual';confirm.click();return 'waiting';}"
    "const language=document.querySelector('#product-languages');"
    "if(language&&language.options.length>1&&language.value==='null'){"
    "const option=[...language.options].find(o=>{let value=o.value;"
    "try{const data=JSON.parse(value);value=[data.language,data.Language,"
    "data.locale,data.Locale].filter(Boolean).join(' ');}catch(e){}"
    "return /(%s)/i.test(value)||"
    "/(%s)/i.test(o.textContent||'');});"
    "if(!option)return 'manual';language.value=option.value;"
    "language.dispatchEvent(new Event('change',{bubbles:true}));"
    "const confirm=document.querySelector('#submit-sku');"
    "if(!confirm)return 'manual';confirm.click();return 'waiting';}"
    "return 'waiting';})()";

static void schedule_retail_automation(
    struct classicsetup_gui_runtime *runtime);

static bool valid_sha256_text(const char *text)
{
    size_t index;

    if (text == NULL || strlen(text) != 64) {
        return false;
    }
    for (index = 0; index < 64; ++index) {
        if (!isxdigit((unsigned char)text[index])) {
            return false;
        }
    }
    return true;
}

static void set_retail_browser_failure(
    struct classicsetup_gui_runtime *runtime,
    const char *message)
{
    classicsetup_retail_browser_fallback_to_full_page(
        &runtime->session->retail_browser_status);
    gtk_label_set_text(GTK_LABEL(runtime->download_status), message);
    gtk_widget_set_visible(runtime->retail_retry_button, TRUE);
    update_download_page(runtime);
}

static void start_captured_retail_download(
    struct classicsetup_gui_runtime *runtime,
    const char *uri,
    WebKitDownload *webkit_download)
{
    struct classicsetup_windows_release release;

    if (webkit_download != NULL) {
        webkit_download_cancel(webkit_download);
    }
    if (runtime->download_task_active ||
        !runtime->session->has_selected_release) {
        return;
    }
    release = runtime->session->source_catalog.releases[
        runtime->session->selected_release_index];
    if (valid_sha256_text(runtime->retail_expected_sha256)) {
        release.official_hash_available = true;
        (void)snprintf(release.expected_sha256,
                       sizeof(release.expected_sha256), "%s",
                       runtime->retail_expected_sha256);
    }
    if (classicsetup_retail_browser_capture_download(
            uri, &release,
            &runtime->session->retail_browser_status) != 0) {
        classicsetup_retail_browser_clear_uri(&release);
        set_retail_browser_failure(
            runtime,
            "ClassicSetup could not safely capture the Microsoft download. Retry the Microsoft page or use an existing ISO when that option becomes available.");
        return;
    }
    if (runtime->retail_automation_timer != 0) {
        g_source_remove(runtime->retail_automation_timer);
        runtime->retail_automation_timer = 0;
    }
    gtk_widget_set_visible(runtime->retail_browser_box, FALSE);
    gtk_widget_set_visible(runtime->retail_retry_button, FALSE);
    gtk_label_set_text(GTK_LABEL(runtime->download_status),
                       "Downloading Windows...");
    start_download_release(runtime, &release, true);
    classicsetup_retail_browser_clear_uri(&release);
}

static void on_retail_download_started(
    WebKitNetworkSession *session,
    WebKitDownload *download,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    WebKitURIRequest *request = webkit_download_get_request(download);
    const char *uri = request != NULL
                          ? webkit_uri_request_get_uri(request)
                          : NULL;

    (void)session;
    start_captured_retail_download(runtime, uri, download);
}

static gboolean on_retail_decide_policy(
    WebKitWebView *web_view,
    WebKitPolicyDecision *decision,
    WebKitPolicyDecisionType type,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    WebKitNavigationPolicyDecision *navigation;
    WebKitNavigationAction *action;
    WebKitURIRequest *request;
    const char *uri;

    (void)web_view;
    if (type != WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION &&
        type != WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION) {
        return FALSE;
    }
    navigation = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
    action = webkit_navigation_policy_decision_get_navigation_action(
        navigation);
    request = action != NULL
                  ? webkit_navigation_action_get_request(action)
                  : NULL;
    uri = request != NULL ? webkit_uri_request_get_uri(request) : NULL;
    if (classicsetup_retail_browser_delivery_uri_is_allowed(uri)) {
        webkit_policy_decision_ignore(decision);
        start_captured_retail_download(runtime, uri, NULL);
        return TRUE;
    }
    if (!classicsetup_retail_browser_navigation_is_allowed(uri)) {
        webkit_policy_decision_ignore(decision);
        return TRUE;
    }
    return FALSE;
}

static gboolean retail_automation_tick(gpointer user_data);

static void retail_automation_finished(
    GObject *source_object,
    GAsyncResult *result,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    GError *error = NULL;
    JSCValue *value;
    char *stage = NULL;

    value = webkit_web_view_evaluate_javascript_finish(
        WEBKIT_WEB_VIEW(source_object), result, &error);
    if (value != NULL) {
        stage = jsc_value_to_string(value);
        g_object_unref(value);
    }
    if (error != NULL || stage == NULL || strcmp(stage, "manual") == 0) {
        if (error != NULL) {
            g_error_free(error);
        }
        classicsetup_retail_browser_fallback_to_full_page(
            &runtime->session->retail_browser_status);
        gtk_label_set_text(
            GTK_LABEL(runtime->download_status),
            "ClassicSetup could not automatically prepare the Microsoft download page. You can continue using the Microsoft page below.");
        g_free(stage);
        update_download_page(runtime);
        return;
    }
    if (strncmp(stage, "ready|", 6) == 0) {
        const char *hash = stage + 6;

        if (valid_sha256_text(hash)) {
            (void)snprintf(runtime->retail_expected_sha256,
                           sizeof(runtime->retail_expected_sha256), "%s",
                           hash);
        }
        runtime->session->retail_browser_status.stage =
            CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_USER_DOWNLOAD_CLICK;
        gtk_label_set_text(
            GTK_LABEL(runtime->download_status),
            "Choose Microsoft's 64-bit download control below.");
        g_free(stage);
        update_download_page(runtime);
        return;
    }
    g_free(stage);
    if (++runtime->retail_automation_attempts >= 90) {
        classicsetup_retail_browser_fallback_to_full_page(
            &runtime->session->retail_browser_status);
        gtk_label_set_text(
            GTK_LABEL(runtime->download_status),
            "ClassicSetup could not automatically prepare the Microsoft download page. You can continue using the Microsoft page below.");
        update_download_page(runtime);
        return;
    }
    schedule_retail_automation(runtime);
}

static gboolean retail_automation_tick(gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;
    const bool korean = runtime->session->selected_language ==
                        CLASSICSETUP_WINDOWS_LANGUAGE_KOREAN;
    const char *pattern = korean ? "Korean|ko-KR|한국어"
                                 : "English|en-US";
    char *script;

    runtime->retail_automation_timer = 0;
    if (runtime->retail_web_view == NULL ||
        runtime->download_task_active ||
        runtime->session->retail_browser_status.full_page_fallback) {
        return G_SOURCE_REMOVE;
    }
    script = g_strdup_printf(retail_prepare_script_format,
                             pattern, pattern, pattern);
    webkit_web_view_evaluate_javascript(
        runtime->retail_web_view,
        script,
        -1,
        NULL,
        NULL,
        NULL,
        retail_automation_finished,
        runtime);
    g_free(script);
    return G_SOURCE_REMOVE;
}

static void schedule_retail_automation(
    struct classicsetup_gui_runtime *runtime)
{
    if (runtime->retail_automation_timer == 0) {
        runtime->retail_automation_timer =
            g_timeout_add(1000, retail_automation_tick, runtime);
    }
}

static void on_retail_load_changed(
    WebKitWebView *web_view,
    WebKitLoadEvent load_event,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)web_view;
    if (load_event != WEBKIT_LOAD_FINISHED) {
        return;
    }
    runtime->session->retail_browser_status.stage =
        CLASSICSETUP_RETAIL_BROWSER_WAITING_FOR_MICROSOFT;
    gtk_label_set_text(GTK_LABEL(runtime->download_status),
                       "Preparing Microsoft download...");
    schedule_retail_automation(runtime);
    update_download_page(runtime);
}

static gboolean on_retail_load_failed(
    WebKitWebView *web_view,
    WebKitLoadEvent load_event,
    const char *failing_uri,
    GError *error,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)web_view;
    (void)load_event;
    (void)failing_uri;
    (void)error;
    set_retail_browser_failure(
        runtime,
        "The Microsoft download page could not be loaded. Check the network connection and retry.");
    return TRUE;
}

static void start_retail_browser(
    struct classicsetup_gui_runtime *runtime)
{
    if (runtime->retail_web_view == NULL || runtime->download_task_active ||
        !classicsetup_gui_source_selection_is_valid(runtime->session) ||
        !classicsetup_network_can_continue(&runtime->session->network)) {
        return;
    }
    classicsetup_retail_browser_status_reset(
        &runtime->session->retail_browser_status);
    (void)classicsetup_retail_browser_transition(
        &runtime->session->retail_browser_status,
        CLASSICSETUP_RETAIL_BROWSER_PREPARING_MICROSOFT_PAGE);
    runtime->retail_automation_attempts = 0;
    memset(runtime->retail_expected_sha256, 0,
           sizeof(runtime->retail_expected_sha256));
    gtk_widget_set_visible(runtime->retail_browser_box, FALSE);
    gtk_widget_set_visible(runtime->retail_preparing_box, TRUE);
    gtk_widget_set_visible(runtime->retail_retry_button, FALSE);
    gtk_label_set_text(GTK_LABEL(runtime->download_status),
                       "Preparing Microsoft download...");
    webkit_web_view_load_uri(
        runtime->retail_web_view,
        classicsetup_retail_browser_page_uri(
            runtime->session->selected_language));
    update_download_page(runtime);
}

static void on_retail_retry_clicked(GtkButton *button, gpointer user_data)
{
    (void)button;
    start_retail_browser(user_data);
}
#endif

static void on_download_start_clicked(GtkButton *button, gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)button;
    if (runtime->session->source_backend ==
        CLASSICSETUP_SOURCE_MICROSOFT_RETAIL) {
        if (runtime->session->retail_source_mode ==
                CLASSICSETUP_GUI_RETAIL_AUTOMATIC &&
            classicsetup_gui_retail_try_fido_once(runtime->session)) {
            start_download(runtime);
            return;
        }
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
        if (classicsetup_gui_retail_start_webview_once(runtime->session) ||
            runtime->session->retail_webview_started) {
            start_retail_browser(runtime);
        }
#else
        gtk_label_set_text(
            GTK_LABEL(runtime->download_status),
            "Microsoft browser download support is not available in this build.");
        update_download_page(runtime);
#endif
        return;
    }
    start_download(runtime);
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
    char size_text[160];
    char rate_text[96];
    char eta_text[128];
    gboolean active = runtime->download_task_active;
    gboolean source_supported = TRUE;

#if !CLASSICSETUP_ENABLE_WEBKIT_RETAIL
    if (runtime->session->source_backend ==
            CLASSICSETUP_SOURCE_MICROSOFT_RETAIL &&
        runtime->session->retail_source_mode ==
            CLASSICSETUP_GUI_RETAIL_MICROSOFT_PAGE) {
        source_supported = FALSE;
    }
#endif

    if (runtime->download_status == NULL) {
        return;
    }
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
    if (runtime->retail_browser_box != NULL &&
        runtime->retail_preparing_box != NULL) {
        bool show_webview = classicsetup_retail_browser_should_show_webview(
            &runtime->session->retail_browser_status);
        bool browser_active = runtime->session->retail_browser_status.stage !=
                                  CLASSICSETUP_RETAIL_BROWSER_IDLE &&
                              runtime->session->retail_browser_status.stage !=
                                  CLASSICSETUP_RETAIL_BROWSER_COMPLETE;
        gtk_widget_set_visible(runtime->retail_browser_box, show_webview);
        gtk_widget_set_visible(runtime->retail_preparing_box,
                               browser_active && !show_webview && !active);
        gtk_widget_set_visible(runtime->retail_manual_notice,
                               runtime->session->retail_browser_status
                                   .full_page_fallback);
    }
#endif
    if (runtime->download_release != NULL &&
        runtime->session->has_selected_release) {
        const struct classicsetup_windows_release *release =
            &runtime->session->source_catalog.releases[
                runtime->session->selected_release_index];

        (void)snprintf(detail, sizeof(detail),
                       "%.70s\n%.55s  |  %s  |  %.55s\nSource: %s",
                       release->release_name, release->language_name,
                       classicsetup_windows_architecture_label(
                           release->architecture),
                       release->edition_name[0] != '\0'
                           ? release->edition_name
                           : "Edition selected after verification",
                       runtime->session->source_backend ==
                               CLASSICSETUP_SOURCE_MICROSOFT_UUP
                           ? "Microsoft Windows Update"
                           : "Microsoft official ISO");
        gtk_label_set_text(GTK_LABEL(runtime->download_release), detail);
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
        if (runtime->retail_preparing_selection != NULL) {
            (void)snprintf(detail, sizeof(detail),
                           "Windows 11\n%.80s\n64-bit",
                           release->language_name);
            gtk_label_set_text(
                GTK_LABEL(runtime->retail_preparing_selection), detail);
        }
#endif
    }
    gtk_label_set_text(
        GTK_LABEL(runtime->download_status),
        status->message[0] != '\0' ? status->message
                                   : "Ready to download the selected Windows image.");
    if (runtime->session->source_backend ==
            CLASSICSETUP_SOURCE_MICROSOFT_UUP &&
        runtime->session->uup_status.total_files != 0) {
        (void)snprintf(
            detail, sizeof(detail), "%zu files verified    %.2f GiB",
            runtime->session->uup_status.files_completed,
            (double)runtime->session->uup_status.total_bytes /
                1073741824.0);
    } else if (status->total_bytes != 0) {
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
    if (status->total_bytes != 0) {
        (void)snprintf(size_text, sizeof(size_text),
                       "Downloaded: %.2f GiB / %.2f GiB",
                       (double)status->bytes_received / 1073741824.0,
                       (double)status->total_bytes / 1073741824.0);
    } else {
        (void)snprintf(size_text, sizeof(size_text),
                       "Downloaded: %.2f GiB",
                       (double)status->bytes_received / 1073741824.0);
    }
    (void)snprintf(rate_text, sizeof(rate_text),
                   status->bytes_per_second > 0.0
                       ? "Transfer rate: %.1f MiB/s"
                       : "Transfer rate: Calculating...",
                   status->bytes_per_second / 1048576.0);
    if (status->total_bytes > status->bytes_received &&
        status->bytes_per_second > 0.0) {
        unsigned long long seconds = (unsigned long long)(
            (double)(status->total_bytes - status->bytes_received) /
            status->bytes_per_second);
        if (seconds >= 3600) {
            (void)snprintf(eta_text, sizeof(eta_text),
                           "Estimated time remaining: %llu hr %llu min",
                           seconds / 3600, (seconds % 3600) / 60);
        } else {
            (void)snprintf(eta_text, sizeof(eta_text),
                           "Estimated time remaining: %llu min %llu sec",
                           seconds / 60, seconds % 60);
        }
    } else {
        (void)snprintf(eta_text, sizeof(eta_text), "%s",
                       status->state == CLASSICSETUP_DOWNLOAD_COMPLETE
                           ? "Estimated time remaining: Complete"
                           : "Estimated time remaining: Calculating...");
    }
    gtk_label_set_text(GTK_LABEL(runtime->download_size), size_text);
    gtk_label_set_text(GTK_LABEL(runtime->download_rate), rate_text);
    gtk_label_set_text(GTK_LABEL(runtime->download_eta), eta_text);
    if (runtime->session->source_backend ==
            CLASSICSETUP_SOURCE_MICROSOFT_UUP &&
        active && status->state != CLASSICSETUP_DOWNLOAD_COMPLETE) {
        gtk_progress_bar_pulse(
            GTK_PROGRESS_BAR(runtime->download_progress));
        gtk_progress_bar_set_text(
            GTK_PROGRESS_BAR(runtime->download_progress), "In progress");
    } else {
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(runtime->download_progress),
            status->progress_fraction >= 0.0 &&
                    status->progress_fraction <= 1.0
                ? status->progress_fraction : 0.0);
        gtk_progress_bar_set_text(
            GTK_PROGRESS_BAR(runtime->download_progress),
            status->state == CLASSICSETUP_DOWNLOAD_COMPLETE
                ? "Complete"
                : NULL);
    }
    gtk_widget_set_sensitive(
        runtime->download_start_button,
        classicsetup_gui_source_selection_is_valid(runtime->session) &&
        classicsetup_network_can_continue(&runtime->session->network) &&
        source_supported && !active &&
        status->state != CLASSICSETUP_DOWNLOAD_COMPLETE);
    gtk_button_set_label(
        GTK_BUTTON(runtime->download_start_button),
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
        runtime->session->source_backend ==
                CLASSICSETUP_SOURCE_MICROSOFT_RETAIL
            ? (runtime->session->retail_source_mode ==
                       CLASSICSETUP_GUI_RETAIL_AUTOMATIC &&
                   !runtime->session->retail_fido_attempted
                   ? "Download from Microsoft"
                   : "Open Microsoft Download Page")
            :
#else
        runtime->session->source_backend ==
                CLASSICSETUP_SOURCE_MICROSOFT_RETAIL
            ? (runtime->session->retail_source_mode ==
                       CLASSICSETUP_GUI_RETAIL_AUTOMATIC &&
                   !runtime->session->retail_fido_attempted
                   ? "Download from Microsoft"
                   : "Microsoft Browser Unavailable")
            :
#endif
        status->state == CLASSICSETUP_DOWNLOAD_FAILED ||
                status->state == CLASSICSETUP_DOWNLOAD_CANCELLED
            ? "Retry"
            : "Download");
    gtk_widget_set_sensitive(runtime->download_cancel_button, active);
    if (runtime->sidebar_download_box != NULL) {
        bool show_sidebar = status->state != CLASSICSETUP_DOWNLOAD_NOT_STARTED ||
            runtime->session->retail_browser_status.stage !=
                CLASSICSETUP_RETAIL_BROWSER_IDLE;
        const char *sidebar_title = "Preparing Windows download";
        if (status->state == CLASSICSETUP_DOWNLOAD_DOWNLOADING) {
            sidebar_title = "Downloading Windows";
        } else if (status->state == CLASSICSETUP_DOWNLOAD_VERIFYING) {
            sidebar_title = "Verifying Windows image";
        } else if (status->state == CLASSICSETUP_DOWNLOAD_COMPLETE) {
            sidebar_title = "Windows image ready";
        } else if (status->state == CLASSICSETUP_DOWNLOAD_FAILED) {
            sidebar_title = "Windows download failed";
        } else if (status->state == CLASSICSETUP_DOWNLOAD_CANCELLED) {
            sidebar_title = "Windows download cancelled";
        }
        gtk_label_set_text(GTK_LABEL(runtime->sidebar_download_title),
                           sidebar_title);
        gtk_progress_bar_set_fraction(
            GTK_PROGRESS_BAR(runtime->sidebar_download_progress),
            status->progress_fraction >= 0.0 &&
                    status->progress_fraction <= 1.0
                ? status->progress_fraction : 0.0);
        (void)snprintf(detail, sizeof(detail), "%.2f / %.2f GiB\n%.1f MiB/s",
                       (double)status->bytes_received / 1073741824.0,
                       (double)status->total_bytes / 1073741824.0,
                       status->bytes_per_second / 1048576.0);
        gtk_label_set_text(GTK_LABEL(runtime->sidebar_download_detail), detail);
        gtk_widget_set_visible(runtime->sidebar_download_box, show_sidebar);
    }
    update_source_controls(runtime);
    update_navigation(runtime);
}

static GtkWidget *build_download_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Download Windows",
        "ClassicSetup is preparing a Windows installation image from Microsoft.");
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *download_details = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

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
    runtime->download_size = gtk_label_new("Downloaded: 0 bytes");
    runtime->download_rate = gtk_label_new("Transfer rate: Calculating...");
    runtime->download_eta = gtk_label_new(
        "Estimated time remaining: Calculating...");
    gtk_label_set_xalign(GTK_LABEL(runtime->download_size), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->download_rate), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->download_eta), 0.0F);
    gtk_widget_add_css_class(download_details, "classic-download-details");
    gtk_box_append(GTK_BOX(download_details), runtime->download_size);
    gtk_box_append(GTK_BOX(download_details), runtime->download_rate);
    gtk_box_append(GTK_BOX(download_details), runtime->download_eta);
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
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
    runtime->retail_preparing_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 7);
    gtk_widget_add_css_class(runtime->retail_preparing_box,
                             "classic-preparing-pane");
    add_classic_label(runtime->retail_preparing_box,
        "Preparing the Microsoft download page",
        "classic-section-title", TRUE);
    add_classic_label(runtime->retail_preparing_box,
        "ClassicSetup is automatically setting the selected Windows version, language, and 64-bit option. Please wait until Microsoft's real 64-bit download control is ready.",
        NULL, TRUE);
    runtime->retail_preparing_selection = gtk_label_new(
        "Windows 11\nKorean\n64-bit");
    gtk_label_set_xalign(GTK_LABEL(runtime->retail_preparing_selection), 0.0F);
    gtk_widget_add_css_class(runtime->retail_preparing_selection,
                             "classic-preparing-selection");
    gtk_box_append(GTK_BOX(runtime->retail_preparing_box),
                   runtime->retail_preparing_selection);
    gtk_widget_set_visible(runtime->retail_preparing_box, FALSE);
    gtk_box_append(GTK_BOX(box), runtime->retail_preparing_box);
    runtime->retail_manual_notice = gtk_label_new(
        "Automatic preparation could not be completed. Continue with the real Microsoft page below, or use an existing ISO when available.");
    gtk_label_set_xalign(GTK_LABEL(runtime->retail_manual_notice), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->retail_manual_notice), TRUE);
    gtk_widget_add_css_class(runtime->retail_manual_notice, "classic-warning");
    gtk_widget_set_visible(runtime->retail_manual_notice, FALSE);
    gtk_box_append(GTK_BOX(box), runtime->retail_manual_notice);
    runtime->retail_browser_box = gtk_box_new(
        GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_size_request(runtime->retail_browser_box, 600, 320);
    gtk_widget_set_hexpand(runtime->retail_browser_box, TRUE);
    gtk_widget_set_vexpand(runtime->retail_browser_box, TRUE);
    runtime->retail_network_session =
        webkit_network_session_new_ephemeral();
    webkit_network_session_set_persistent_credential_storage_enabled(
        runtime->retail_network_session, FALSE);
    runtime->retail_web_view = WEBKIT_WEB_VIEW(g_object_new(
        WEBKIT_TYPE_WEB_VIEW,
        "network-session", runtime->retail_network_session,
        NULL));
    gtk_widget_set_hexpand(GTK_WIDGET(runtime->retail_web_view), TRUE);
    gtk_widget_set_vexpand(GTK_WIDGET(runtime->retail_web_view), TRUE);
    g_signal_connect(
        runtime->retail_network_session, "download-started",
        G_CALLBACK(on_retail_download_started), runtime);
    g_signal_connect(
        runtime->retail_web_view, "decide-policy",
        G_CALLBACK(on_retail_decide_policy), runtime);
    g_signal_connect(
        runtime->retail_web_view, "load-changed",
        G_CALLBACK(on_retail_load_changed), runtime);
    g_signal_connect(
        runtime->retail_web_view, "load-failed",
        G_CALLBACK(on_retail_load_failed), runtime);
    gtk_box_append(GTK_BOX(runtime->retail_browser_box),
                   GTK_WIDGET(runtime->retail_web_view));
    gtk_widget_set_visible(runtime->retail_browser_box, FALSE);
    gtk_box_append(GTK_BOX(box), runtime->retail_browser_box);
    runtime->retail_retry_button =
        gtk_button_new_with_label("Retry Microsoft Page");
    g_signal_connect(runtime->retail_retry_button, "clicked",
                     G_CALLBACK(on_retail_retry_clicked), runtime);
    gtk_widget_set_visible(runtime->retail_retry_button, FALSE);
    gtk_box_append(GTK_BOX(actions), runtime->retail_retry_button);
    runtime->retail_existing_iso_button =
        gtk_button_new_with_label("Use Existing ISO (not yet available)");
    gtk_widget_set_sensitive(runtime->retail_existing_iso_button, FALSE);
    gtk_box_append(GTK_BOX(actions), runtime->retail_existing_iso_button);
#else
    runtime->retail_browser_box = NULL;
    runtime->retail_preparing_box = NULL;
    runtime->retail_preparing_selection = NULL;
    runtime->retail_manual_notice = NULL;
    runtime->retail_retry_button = NULL;
    runtime->retail_existing_iso_button = NULL;
#endif
    gtk_box_append(GTK_BOX(box), runtime->download_progress);
    gtk_box_append(GTK_BOX(box), download_details);
    gtk_widget_set_visible(runtime->download_detail, FALSE);
    gtk_box_append(GTK_BOX(box), actions);
    add_classic_label(
        box,
        "You can continue configuring installation settings while Windows downloads. ClassicSetup keeps the transfer running in the background.",
        "classic-download-information", TRUE);
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
    runtime->summary_release = gtk_label_new("Release: not selected");
    runtime->summary_language = gtk_label_new("Language: not selected");
    runtime->summary_architecture = gtk_label_new(
        "Architecture: not selected");
    runtime->summary_storage = gtk_label_new(
        "Storage plan: not yet applied");
    runtime->summary_source = gtk_label_new("Windows source: not ready");
    runtime->summary_verification = gtk_label_new(
        "Download verification: not complete");
    runtime->summary_network = gtk_label_new("Network: not ready");
    runtime->summary_options = gtk_label_new(
        "Installation options: recommended placeholder settings");
    runtime->summary_notice = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_disk), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_version), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_release), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_language), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_architecture), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_storage), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_source), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_verification), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_network), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_options), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_notice), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_disk), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_network), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_version), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_release), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_language), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_architecture), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_storage), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_source), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_verification), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_options), TRUE);
    gtk_label_set_wrap(GTK_LABEL(runtime->summary_notice), TRUE);
    gtk_widget_add_css_class(runtime->summary_disk, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_network, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_version, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_release, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_language, "classic-summary-row");
    gtk_widget_add_css_class(
        runtime->summary_architecture, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_storage, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_source, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_verification, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_options, "classic-summary-row");
    gtk_widget_add_css_class(runtime->summary_notice, "classic-status");
    gtk_widget_add_css_class(runtime->summary_notice, "classic-warning");
    gtk_widget_set_visible(runtime->summary_notice, FALSE);
    gtk_box_append(GTK_BOX(box), runtime->summary_disk);
    gtk_box_append(GTK_BOX(box), runtime->summary_network);
    gtk_box_append(GTK_BOX(box), runtime->summary_version);
    gtk_box_append(GTK_BOX(box), runtime->summary_release);
    gtk_box_append(GTK_BOX(box), runtime->summary_language);
    gtk_box_append(GTK_BOX(box), runtime->summary_architecture);
    gtk_box_append(GTK_BOX(box), runtime->summary_storage);
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
    if (runtime->session->verified_source.verified) {
        (void)snprintf(line, sizeof(line), "Windows: %.180s",
                       runtime->session->verified_source.edition[0] != '\0'
                           ? runtime->session->verified_source.edition
                           : runtime->session->verified_source.release_name);
        gtk_label_set_text(GTK_LABEL(runtime->summary_version), line);
    } else {
        gtk_label_set_text(
            GTK_LABEL(runtime->summary_version),
            runtime->session->windows_version == CLASSICSETUP_GUI_WINDOWS_10
                ? "Windows family: Windows 10"
                : "Windows family: Windows 11");
    }
    gtk_label_set_text(
        GTK_LABEL(runtime->summary_network),
        classicsetup_network_can_continue(&runtime->session->network)
            ? "Network: connected to the Internet"
            : "Network: Internet connection required");
    if (runtime->session->verified_source.verified) {
        const struct classicsetup_verified_windows_source *source =
            &runtime->session->verified_source;

        (void)snprintf(line, sizeof(line), "Image build: %.180s",
                       source->build);
        gtk_label_set_text(GTK_LABEL(runtime->summary_release), line);
        (void)snprintf(line, sizeof(line), "Language: %s",
                       language_label(source->language));
        gtk_label_set_text(GTK_LABEL(runtime->summary_language), line);
        (void)snprintf(
            line, sizeof(line), "Architecture: %s",
            classicsetup_windows_architecture_label(
                source->architecture));
        gtk_label_set_text(GTK_LABEL(runtime->summary_architecture), line);
        (void)snprintf(
            line, sizeof(line), "Source: %s",
            source->backend == CLASSICSETUP_SOURCE_MICROSOFT_UUP
                ? "Microsoft Windows Update"
                : "Microsoft official ISO");
    } else if (runtime->session->has_selected_release) {
        const struct classicsetup_windows_release *release =
            &runtime->session->source_catalog.releases[
                runtime->session->selected_release_index];

        (void)snprintf(line, sizeof(line), "Release: %.180s",
                       release->release_name);
        gtk_label_set_text(GTK_LABEL(runtime->summary_release), line);
        (void)snprintf(line, sizeof(line), "Language: %.180s",
                       release->language_name);
        gtk_label_set_text(GTK_LABEL(runtime->summary_language), line);
        (void)snprintf(
            line, sizeof(line), "Architecture: %s",
            classicsetup_windows_architecture_label(
                release->architecture));
        gtk_label_set_text(GTK_LABEL(runtime->summary_architecture), line);
        (void)snprintf(line, sizeof(line), "%s",
                       "Source: Official Microsoft download");
    } else {
        gtk_label_set_text(
            GTK_LABEL(runtime->summary_release), "Release: not selected");
        gtk_label_set_text(
            GTK_LABEL(runtime->summary_language), "Language: not selected");
        gtk_label_set_text(
            GTK_LABEL(runtime->summary_architecture),
            "Architecture: not selected");
        (void)snprintf(line, sizeof(line), "%s",
                       "Windows source: not selected");
    }
    gtk_label_set_text(GTK_LABEL(runtime->summary_source), line);
    gtk_label_set_text(
        GTK_LABEL(runtime->summary_verification),
        runtime->session->verified_source.verified
            ? (runtime->session->source_catalog.releases[
                   runtime->session->selected_release_index]
                       .official_hash_available
                   ? "Verified with official SHA-256"
                   : "Windows installation image verified")
            : (runtime->session->has_selected_release &&
            classicsetup_download_is_ready(
            &runtime->session->download, &runtime->session->workspace)
            ? (runtime->session->source_catalog.releases[
                   runtime->session->selected_release_index]
                       .official_hash_available
                   ? "Download: verified using official SHA-256"
                   : "Windows installation image verified")
            : "Download: not verified"));
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
        can_next = classicsetup_gui_source_selection_is_valid(
            runtime->session);
    } else if (runtime->session->page == CLASSICSETUP_GUI_PAGE_SUMMARY) {
        can_next = classicsetup_gui_summary_is_ready(runtime->session);
    }

    gtk_widget_set_sensitive(runtime->back_button, TRUE);
    gtk_widget_set_sensitive(runtime->next_button, can_next);
    gtk_button_set_label(
        GTK_BUTTON(runtime->next_button),
        summary ? "Install" :
        (runtime->session->page == CLASSICSETUP_GUI_PAGE_DOWNLOAD &&
         runtime->download_task_active)
            ? "Continue Setup >" : "Next >");
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
    } else if (page == CLASSICSETUP_GUI_PAGE_WINDOWS_VERSION) {
        update_source_controls(runtime);
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
#if CLASSICSETUP_ENABLE_WEBKIT_RETAIL
    if (runtime.retail_automation_timer != 0) {
        g_source_remove(runtime.retail_automation_timer);
    }
    if (runtime.retail_network_session != NULL) {
        g_object_unref(runtime.retail_network_session);
    }
#endif
    if (session->workspace.valid && !session->workspace.verified_iso) {
        classicsetup_workspace_cleanup_after_install(
            &session->workspace, false);
    } else if (session->workspace.valid) {
        classicsetup_workspace_cleanup_success(&session->workspace);
    }
    g_object_unref(application);
    return runtime.result;
}
