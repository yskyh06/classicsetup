#include "classicsetup/gui.h"

#include <gtk/gtk.h>
#include <stdio.h>

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
    GtkWidget *disk_status;
    GtkWidget *summary_disk;
    GtkWidget *summary_version;
    GtkWidget *network_status;
    GtkWidget *ethernet_status;
    GtkWidget *wifi_list;
    GtkWidget *password_label;
    GtkWidget *password_entry;
    GtkWidget *connect_button;
    GtkWidget *refresh_button;
    GtkWidget *network_spinner;
    struct classicsetup_network_controller network_controller;
    bool network_controller_ready;
    bool has_selected_wifi;
    size_t selected_wifi_index;
    int result;
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
    if (css_class != NULL) {
        gtk_widget_add_css_class(label, css_class);
    }
    gtk_box_append(GTK_BOX(box), label);
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
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *list = gtk_list_box_new();
    size_t index;

    gtk_widget_add_css_class(box, "classic-content");
    add_classic_label(
        box,
        "Select a disk for Windows",
        "classic-title",
        FALSE);
    add_classic_label(
        box,
        "Recommended installation only uses disks that the core safety policy marks as available.",
        "classic-muted",
        TRUE);
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
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);

    gtk_widget_add_css_class(box, "classic-content");
    add_classic_label(box, title, "classic-title", FALSE);
    add_classic_label(box, description, "classic-muted", TRUE);
    return box;
}

static void clear_wifi_rows(GtkWidget *list)
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
    char line[256];
    size_t index;
    gboolean busy = snapshot->state == CLASSICSETUP_NETWORK_SCANNING ||
                    snapshot->state == CLASSICSETUP_NETWORK_CONNECTING;

    if (runtime->network_status == NULL) {
        return;
    }
    gtk_label_set_text(GTK_LABEL(runtime->network_status), snapshot->status);
    if (!snapshot->ethernet_available) {
        gtk_label_set_text(
            GTK_LABEL(runtime->ethernet_status),
            "Wired connection: not detected");
    } else if (snapshot->ethernet_connected) {
        gtk_label_set_text(
            GTK_LABEL(runtime->ethernet_status),
            "Wired connection: connected");
    } else {
        gtk_label_set_text(
            GTK_LABEL(runtime->ethernet_status),
            "Wired connection: cable not connected");
    }
    clear_wifi_rows(runtime->wifi_list);
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
        gtk_widget_add_css_class(row, "classic-network-row");
        g_object_set_data(
            G_OBJECT(row),
            "classicsetup-wifi-index",
            GSIZE_TO_POINTER(index));
        gtk_widget_set_sensitive(row, !network->enterprise && !busy);
        gtk_list_box_append(GTK_LIST_BOX(runtime->wifi_list), row);
    }
    if (!snapshot->wifi_available && !busy) {
        GtkWidget *row = gtk_list_box_row_new();
        GtkWidget *label = gtk_label_new("No Wi-Fi device was detected.");

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
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);

    gtk_widget_add_css_class(box, "classic-content");
    add_classic_label(box, "Connect to the Internet", "classic-title", FALSE);
    add_classic_label(
        box,
        "An Internet connection is required for the Recommended online-download workflow.",
        "classic-muted",
        TRUE);
    runtime->ethernet_status = gtk_label_new("Wired connection: checking...");
    gtk_label_set_xalign(GTK_LABEL(runtime->ethernet_status), 0.0F);
    gtk_box_append(GTK_BOX(box), runtime->ethernet_status);
    add_classic_label(box, "Wi-Fi", NULL, FALSE);
    runtime->wifi_list = gtk_list_box_new();
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
    gtk_label_set_xalign(GTK_LABEL(runtime->network_status), 0.0F);
    gtk_label_set_wrap(GTK_LABEL(runtime->network_status), TRUE);
    gtk_box_append(GTK_BOX(status_box), runtime->network_spinner);
    gtk_box_append(GTK_BOX(status_box), runtime->network_status);
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
    (void)classicsetup_gui_set_windows_version(
        runtime->session,
        (enum classicsetup_gui_windows_version)value);
}

static GtkWidget *build_windows_version_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Select Windows Version",
        "Source and edition validation will be added in a later milestone.");
    GtkWidget *windows11 = gtk_check_button_new_with_label("Windows 11 (placeholder)");
    GtkWidget *windows10 = gtk_check_button_new_with_label("Windows 10 (placeholder)");

    gtk_check_button_set_group(GTK_CHECK_BUTTON(windows10),
                               GTK_CHECK_BUTTON(windows11));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(windows11), TRUE);
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

    gtk_check_button_set_active(GTK_CHECK_BUTTON(check), TRUE);
    runtime->session->options_placeholder = true;
    gtk_box_append(GTK_BOX(box), check);
    return box;
}

static GtkWidget *build_summary_page(
    struct classicsetup_gui_runtime *runtime)
{
    GtkWidget *box = build_placeholder_page(
        "Ready to Continue",
        "The installation engine connection will be enabled in a later milestone.");

    runtime->summary_disk = gtk_label_new("Target disk: not selected");
    runtime->summary_version = gtk_label_new("Windows version: Windows 11 (placeholder)");
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_disk), 0.0F);
    gtk_label_set_xalign(GTK_LABEL(runtime->summary_version), 0.0F);
    gtk_box_append(GTK_BOX(box), runtime->summary_disk);
    gtk_box_append(GTK_BOX(box), runtime->summary_version);
    add_classic_label(
        box,
        "No partition, format, download, or install executor is called from this GUI foundation.",
        "classic-muted",
        TRUE);
    return box;
}

static void update_summary(struct classicsetup_gui_runtime *runtime)
{
    char line[256];

    if (runtime->session->has_selected_disk) {
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
            ? "Windows version: Windows 10 (placeholder)"
            : "Windows version: Windows 11 (placeholder)");
}

static void update_navigation(struct classicsetup_gui_runtime *runtime)
{
    gboolean can_next = TRUE;
    gboolean summary = runtime->session->page == CLASSICSETUP_GUI_PAGE_SUMMARY;

    if (runtime->session->page == CLASSICSETUP_GUI_PAGE_DISK) {
        can_next = runtime->session->has_selected_disk;
    } else if (runtime->session->page == CLASSICSETUP_GUI_PAGE_NETWORK) {
        can_next = classicsetup_network_can_continue(
            &runtime->session->network);
    }

    gtk_widget_set_sensitive(runtime->back_button, TRUE);
    gtk_widget_set_sensitive(runtime->next_button, can_next);
    gtk_button_set_label(
        GTK_BUTTON(runtime->next_button),
        summary ? "Install (later)" : "Next");
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
    update_navigation(runtime);
}

static void on_back_clicked(
    GtkButton *button,
    gpointer user_data)
{
    struct classicsetup_gui_runtime *runtime = user_data;

    (void)button;
    if (runtime->session->page == CLASSICSETUP_GUI_PAGE_DISK) {
        runtime->result = CLASSICSETUP_GUI_BACK;
        g_application_quit(G_APPLICATION(runtime->application));
        return;
    }
    set_page(
        runtime,
        classicsetup_gui_page_back(runtime->session->page));
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
        runtime->result = CLASSICSETUP_GUI_FINISHED;
        g_application_quit(G_APPLICATION(runtime->application));
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
    runtime->result = CLASSICSETUP_GUI_BACK;
    g_application_quit(G_APPLICATION(runtime->application));
    return FALSE;
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
    GtkWidget *footer;
    GtkWidget *page;

    load_css();
    runtime->application = application;
    runtime->window = gtk_application_window_new(application);
    gtk_window_set_title(
        GTK_WINDOW(runtime->window),
        "ClassicSetup - Recommended installation");
    gtk_window_set_default_size(GTK_WINDOW(runtime->window), 820, 540);
    g_signal_connect(
        runtime->window,
        "close-request",
        G_CALLBACK(on_window_close),
        runtime);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(header, "classic-header");
    add_classic_label(header, "ClassicSetup", NULL, FALSE);
    gtk_box_append(GTK_BOX(root), header);

    body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_add_css_class(sidebar, "classic-sidebar");
    add_classic_label(sidebar, "Recommended", "classic-title", FALSE);
    add_classic_label(
        sidebar,
        "Safe automatic settings for a supported empty disk.",
        "classic-muted",
        TRUE);
    gtk_box_append(GTK_BOX(body), sidebar);

    runtime->stack = gtk_stack_new();
    gtk_stack_set_transition_type(
        GTK_STACK(runtime->stack),
        GTK_STACK_TRANSITION_TYPE_NONE);
    page = build_disk_page(runtime);
    gtk_stack_add_named(GTK_STACK(runtime->stack), page, "disk");
    page = build_network_page(runtime);
    gtk_stack_add_named(GTK_STACK(runtime->stack), page, "network");
    page = build_windows_version_page(runtime);
    gtk_stack_add_named(GTK_STACK(runtime->stack), page, "version");
    page = build_placeholder_page(
        "Download Windows",
        "Windows download will begin here in a future milestone. Installation options can be configured while it downloads.");
    gtk_stack_add_named(GTK_STACK(runtime->stack), page, "download");
    page = build_options_page(runtime);
    gtk_stack_add_named(GTK_STACK(runtime->stack), page, "options");
    page = build_summary_page(runtime);
    gtk_stack_add_named(GTK_STACK(runtime->stack), page, "summary");
    gtk_widget_set_hexpand(runtime->stack, TRUE);
    gtk_widget_set_vexpand(runtime->stack, TRUE);
    gtk_box_append(GTK_BOX(body), runtime->stack);
    gtk_box_append(GTK_BOX(root), body);

    footer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(footer, "classic-sidebar");
    runtime->back_button = gtk_button_new_with_label("Back");
    runtime->next_button = gtk_button_new_with_label("Next");
    gtk_widget_set_hexpand(runtime->back_button, TRUE);
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
    gtk_box_append(GTK_BOX(footer), runtime->next_button);
    gtk_box_append(GTK_BOX(root), footer);
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
    set_page(runtime, CLASSICSETUP_GUI_PAGE_DISK);
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
    application = gtk_application_new(
        "org.classicsetup.Recommended",
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
    g_object_unref(application);
    return runtime.result;
}
