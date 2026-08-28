#include "classicsetup/network.h"

#include <gio/gio.h>
#include <stdio.h>
#include <string.h>

#define NM_SERVICE "org.freedesktop.NetworkManager"
#define NM_PATH "/org/freedesktop/NetworkManager"
#define NM_INTERFACE "org.freedesktop.NetworkManager"
#define DBUS_PROPERTIES "org.freedesktop.DBus.Properties"
#define NM_DEVICE_INTERFACE "org.freedesktop.NetworkManager.Device"
#define NM_WIFI_INTERFACE "org.freedesktop.NetworkManager.Device.Wireless"
#define NM_AP_INTERFACE "org.freedesktop.NetworkManager.AccessPoint"

enum {
    NM_DEVICE_TYPE_ETHERNET = 1,
    NM_DEVICE_TYPE_WIFI = 2,
    NM_DEVICE_STATE_ACTIVATED = 100,
    NM_CONNECTIVITY_PORTAL = 2,
    NM_CONNECTIVITY_LIMITED = 3,
    NM_CONNECTIVITY_FULL = 4,
    NM_AP_FLAGS_PRIVACY = 1,
    NM_AP_SEC_KEY_MGMT_PSK = 0x100,
    NM_AP_SEC_KEY_MGMT_802_1X = 0x200
};

struct nm_backend {
    gint references;
    gint destroyed;
    gint busy;
    GCancellable *cancellable;
};

enum nm_operation_kind {
    NM_OPERATION_REFRESH,
    NM_OPERATION_CONNECT
};

struct nm_operation {
    struct nm_backend *backend;
    enum nm_operation_kind kind;
    struct classicsetup_wifi_network network;
    char password[256];
    classicsetup_network_backend_callback callback;
    void *user_data;
};

static void secure_clear(void *memory, size_t size)
{
    volatile unsigned char *bytes = memory;

    while (size > 0) {
        *bytes++ = 0;
        --size;
    }
}

static struct nm_backend *backend_ref(struct nm_backend *backend)
{
    g_atomic_int_inc(&backend->references);
    return backend;
}

static void backend_unref(struct nm_backend *backend)
{
    if (g_atomic_int_dec_and_test(&backend->references)) {
        g_clear_object(&backend->cancellable);
        secure_clear(backend, sizeof(*backend));
        g_free(backend);
    }
}

static void operation_free(void *data)
{
    struct nm_operation *operation = data;

    if (operation == NULL) {
        return;
    }
    secure_clear(operation->password, sizeof(operation->password));
    backend_unref(operation->backend);
    secure_clear(operation, sizeof(*operation));
    g_free(operation);
}

static GVariant *get_all_properties(
    GDBusConnection *connection,
    const char *object_path,
    const char *interface_name,
    GCancellable *cancellable,
    GError **error)
{
    GVariant *reply;
    GVariant *properties;

    reply = g_dbus_connection_call_sync(
        connection,
        NM_SERVICE,
        object_path,
        DBUS_PROPERTIES,
        "GetAll",
        g_variant_new("(s)", interface_name),
        G_VARIANT_TYPE("(a{sv})"),
        G_DBUS_CALL_FLAGS_NONE,
        10000,
        cancellable,
        error);
    if (reply == NULL) {
        return NULL;
    }
    g_variant_get(reply, "(@a{sv})", &properties);
    g_variant_unref(reply);
    return properties;
}

static void set_connectivity(
    struct classicsetup_network_snapshot *snapshot,
    guint32 connectivity)
{
    if (connectivity == NM_CONNECTIVITY_FULL) {
        snapshot->connectivity =
            CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET;
        snapshot->state = CLASSICSETUP_NETWORK_CONNECTED;
        (void)snprintf(
            snapshot->status,
            sizeof(snapshot->status),
            "%s",
            "Connected to the Internet.");
    } else if (connectivity == NM_CONNECTIVITY_LIMITED ||
               connectivity == NM_CONNECTIVITY_PORTAL) {
        snapshot->connectivity =
            CLASSICSETUP_NETWORK_CONNECTIVITY_NETWORK;
        snapshot->state = CLASSICSETUP_NETWORK_DISCONNECTED;
        (void)snprintf(
            snapshot->status,
            sizeof(snapshot->status),
            "%s",
            "Connected to a local network, but the Internet is unavailable.");
    } else if (snapshot->ethernet_connected || snapshot->wifi_connected) {
        snapshot->connectivity =
            CLASSICSETUP_NETWORK_CONNECTIVITY_LINK;
        snapshot->state = CLASSICSETUP_NETWORK_DISCONNECTED;
        (void)snprintf(
            snapshot->status,
            sizeof(snapshot->status),
            "%s",
            "A network link is active, but Internet access was not verified.");
    } else {
        snapshot->connectivity =
            CLASSICSETUP_NETWORK_CONNECTIVITY_NONE;
        snapshot->state = CLASSICSETUP_NETWORK_DISCONNECTED;
        (void)snprintf(
            snapshot->status,
            sizeof(snapshot->status),
            "%s",
            "Connect to a network to continue.");
    }
}

static void merge_access_point(
    struct classicsetup_network_snapshot *snapshot,
    const struct classicsetup_wifi_network *candidate)
{
    size_t index;

    for (index = 0; index < snapshot->wifi_count; ++index) {
        if (strcmp(snapshot->wifi[index].ssid, candidate->ssid) == 0) {
            if (candidate->connected ||
                candidate->signal_strength >
                    snapshot->wifi[index].signal_strength) {
                snapshot->wifi[index] = *candidate;
            }
            return;
        }
    }
    if (snapshot->wifi_count < CLASSICSETUP_NETWORK_MAX_WIFI) {
        snapshot->wifi[snapshot->wifi_count++] = *candidate;
    }
}

static int read_access_point(
    GDBusConnection *connection,
    const char *device_path,
    const char *access_point_path,
    const char *active_path,
    GCancellable *cancellable,
    struct classicsetup_wifi_network *network)
{
    GError *error = NULL;
    GVariant *properties;
    GVariant *ssid_value = NULL;
    gconstpointer ssid_bytes;
    gsize ssid_size = 0;
    gchar *valid_ssid;
    guint8 strength = 0;
    guint32 flags = 0;
    guint32 wpa_flags = 0;
    guint32 rsn_flags = 0;

    properties = get_all_properties(
        connection,
        access_point_path,
        NM_AP_INTERFACE,
        cancellable,
        &error);
    if (properties == NULL) {
        g_clear_error(&error);
        return -1;
    }
    ssid_value = g_variant_lookup_value(
        properties,
        "Ssid",
        G_VARIANT_TYPE("ay"));
    if (ssid_value == NULL) {
        g_variant_unref(properties);
        return -1;
    }
    ssid_bytes = g_variant_get_fixed_array(
        ssid_value,
        &ssid_size,
        sizeof(guint8));
    if (ssid_bytes == NULL || ssid_size == 0) {
        g_variant_unref(ssid_value);
        g_variant_unref(properties);
        return -1;
    }
    valid_ssid = g_utf8_make_valid(ssid_bytes, (gssize)ssid_size);
    memset(network, 0, sizeof(*network));
    (void)snprintf(network->ssid, sizeof(network->ssid), "%s", valid_ssid);
    g_free(valid_ssid);
    (void)g_variant_lookup(properties, "Strength", "y", &strength);
    (void)g_variant_lookup(properties, "Flags", "u", &flags);
    (void)g_variant_lookup(properties, "WpaFlags", "u", &wpa_flags);
    (void)g_variant_lookup(properties, "RsnFlags", "u", &rsn_flags);
    network->signal_strength = (int)strength;
    network->secured = (flags & NM_AP_FLAGS_PRIVACY) != 0 ||
                       wpa_flags != 0 || rsn_flags != 0;
    network->enterprise =
        (((wpa_flags | rsn_flags) & NM_AP_SEC_KEY_MGMT_802_1X) != 0) &&
        (((wpa_flags | rsn_flags) & NM_AP_SEC_KEY_MGMT_PSK) == 0);
    network->connected = active_path != NULL &&
                         strcmp(access_point_path, active_path) == 0;
    (void)snprintf(
        network->device_id,
        sizeof(network->device_id),
        "%s",
        device_path);
    (void)snprintf(
        network->access_point_id,
        sizeof(network->access_point_id),
        "%s",
        access_point_path);
    g_variant_unref(ssid_value);
    g_variant_unref(properties);
    return 0;
}

static void scan_wifi_device(
    GDBusConnection *connection,
    const char *device_path,
    GCancellable *cancellable,
    struct classicsetup_network_snapshot *snapshot)
{
    GError *error = NULL;
    GVariant *properties;
    GVariant *reply;
    GVariantIter *paths = NULL;
    const char *path;
    const char *active_borrowed = NULL;
    char active_path[CLASSICSETUP_NETWORK_BACKEND_ID_SIZE] = "";
    GVariantBuilder scan_options;
    GVariant *scan_reply;

    g_variant_builder_init(&scan_options, G_VARIANT_TYPE("a{sv}"));
    scan_reply = g_dbus_connection_call_sync(
        connection,
        NM_SERVICE,
        device_path,
        NM_WIFI_INTERFACE,
        "RequestScan",
        g_variant_new("(@a{sv})", g_variant_builder_end(&scan_options)),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        10000,
        cancellable,
        &error);
    if (scan_reply != NULL) {
        g_variant_unref(scan_reply);
    }
    g_clear_error(&error);

    properties = get_all_properties(
        connection,
        device_path,
        NM_WIFI_INTERFACE,
        cancellable,
        &error);
    if (properties != NULL) {
        if (g_variant_lookup(
                properties,
                "ActiveAccessPoint",
                "&o",
                &active_borrowed) && active_borrowed != NULL) {
            (void)snprintf(active_path, sizeof(active_path), "%s", active_borrowed);
        }
        g_variant_unref(properties);
    }
    g_clear_error(&error);
    reply = g_dbus_connection_call_sync(
        connection,
        NM_SERVICE,
        device_path,
        NM_WIFI_INTERFACE,
        "GetAllAccessPoints",
        NULL,
        G_VARIANT_TYPE("(ao)"),
        G_DBUS_CALL_FLAGS_NONE,
        10000,
        cancellable,
        &error);
    if (reply == NULL) {
        g_clear_error(&error);
        return;
    }
    g_variant_get(reply, "(ao)", &paths);
    while (g_variant_iter_loop(paths, "&o", &path)) {
        struct classicsetup_wifi_network network;

        if (read_access_point(
                connection,
                device_path,
                path,
                active_path[0] != '\0' ? active_path : NULL,
                cancellable,
                &network) == 0) {
            if (network.connected) {
                snapshot->wifi_connected = true;
            }
            merge_access_point(snapshot, &network);
        }
    }
    g_variant_iter_free(paths);
    g_variant_unref(reply);
}

static guint32 check_connectivity(
    GDBusConnection *connection,
    GCancellable *cancellable)
{
    GError *error = NULL;
    GVariant *reply;
    guint32 connectivity = 0;

    reply = g_dbus_connection_call_sync(
        connection,
        NM_SERVICE,
        NM_PATH,
        NM_INTERFACE,
        "CheckConnectivity",
        NULL,
        G_VARIANT_TYPE("(u)"),
        G_DBUS_CALL_FLAGS_NONE,
        10000,
        cancellable,
        &error);
    if (reply != NULL) {
        g_variant_get(reply, "(u)", &connectivity);
        g_variant_unref(reply);
    }
    g_clear_error(&error);
    return connectivity;
}

static int collect_snapshot(
    GDBusConnection *connection,
    GCancellable *cancellable,
    struct classicsetup_network_snapshot *snapshot,
    GError **error)
{
    GVariant *reply;
    GVariantIter *paths = NULL;
    const char *path;

    classicsetup_network_snapshot_reset(snapshot);
    reply = g_dbus_connection_call_sync(
        connection,
        NM_SERVICE,
        NM_PATH,
        NM_INTERFACE,
        "GetDevices",
        NULL,
        G_VARIANT_TYPE("(ao)"),
        G_DBUS_CALL_FLAGS_NONE,
        10000,
        cancellable,
        error);
    if (reply == NULL) {
        return -1;
    }
    g_variant_get(reply, "(ao)", &paths);
    while (g_variant_iter_loop(paths, "&o", &path)) {
        GVariant *properties;
        GError *device_error = NULL;
        guint32 device_type = 0;
        guint32 state = 0;

        properties = get_all_properties(
            connection,
            path,
            NM_DEVICE_INTERFACE,
            cancellable,
            &device_error);
        if (properties == NULL) {
            g_clear_error(&device_error);
            continue;
        }
        (void)g_variant_lookup(properties, "DeviceType", "u", &device_type);
        (void)g_variant_lookup(properties, "State", "u", &state);
        if (device_type == NM_DEVICE_TYPE_ETHERNET) {
            snapshot->ethernet_available = true;
            if (state == NM_DEVICE_STATE_ACTIVATED) {
                snapshot->ethernet_connected = true;
            }
        } else if (device_type == NM_DEVICE_TYPE_WIFI) {
            snapshot->wifi_available = true;
            scan_wifi_device(connection, path, cancellable, snapshot);
        }
        g_variant_unref(properties);
    }
    g_variant_iter_free(paths);
    g_variant_unref(reply);
    set_connectivity(snapshot, check_connectivity(connection, cancellable));
    return 0;
}

static GVariant *build_connection_settings(
    const struct nm_operation *operation)
{
    GVariantBuilder settings;
    GVariantBuilder connection;
    GVariantBuilder wifi;
    GVariantBuilder security;
    GVariantBuilder ipv4;
    GVariantBuilder ipv6;
    GVariant *ssid;

    g_variant_builder_init(&settings, G_VARIANT_TYPE("a{sa{sv}}"));
    g_variant_builder_init(&connection, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&connection, "{sv}", "id",
                          g_variant_new_string("ClassicSetup Wi-Fi"));
    g_variant_builder_add(&connection, "{sv}", "type",
                          g_variant_new_string("802-11-wireless"));
    g_variant_builder_add(&connection, "{sv}", "autoconnect",
                          g_variant_new_boolean(FALSE));
    g_variant_builder_add(&settings, "{s@a{sv}}", "connection",
                          g_variant_builder_end(&connection));

    g_variant_builder_init(&wifi, G_VARIANT_TYPE("a{sv}"));
    ssid = g_variant_new_fixed_array(
        G_VARIANT_TYPE_BYTE,
        operation->network.ssid,
        strlen(operation->network.ssid),
        sizeof(guchar));
    g_variant_builder_add(&wifi, "{sv}", "ssid", ssid);
    if (operation->network.secured) {
        g_variant_builder_add(
            &wifi,
            "{sv}",
            "security",
            g_variant_new_string("802-11-wireless-security"));
    }
    g_variant_builder_add(&settings, "{s@a{sv}}", "802-11-wireless",
                          g_variant_builder_end(&wifi));

    if (operation->network.secured) {
        g_variant_builder_init(&security, G_VARIANT_TYPE("a{sv}"));
        g_variant_builder_add(&security, "{sv}", "key-mgmt",
                              g_variant_new_string("wpa-psk"));
        g_variant_builder_add(&security, "{sv}", "psk",
                              g_variant_new_string(operation->password));
        g_variant_builder_add(
            &settings,
            "{s@a{sv}}",
            "802-11-wireless-security",
            g_variant_builder_end(&security));
    }

    g_variant_builder_init(&ipv4, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&ipv4, "{sv}", "method",
                          g_variant_new_string("auto"));
    g_variant_builder_add(&settings, "{s@a{sv}}", "ipv4",
                          g_variant_builder_end(&ipv4));
    g_variant_builder_init(&ipv6, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&ipv6, "{sv}", "method",
                          g_variant_new_string("auto"));
    g_variant_builder_add(&settings, "{s@a{sv}}", "ipv6",
                          g_variant_builder_end(&ipv6));
    return g_variant_builder_end(&settings);
}

static int connect_wifi(
    GDBusConnection *connection,
    struct nm_operation *operation,
    GCancellable *cancellable,
    GError **error)
{
    GVariantBuilder options;
    GVariant *reply;

    g_variant_builder_init(&options, G_VARIANT_TYPE("a{sv}"));
    g_variant_builder_add(&options, "{sv}", "persist",
                          g_variant_new_string("memory"));
    reply = g_dbus_connection_call_sync(
        connection,
        NM_SERVICE,
        NM_PATH,
        NM_INTERFACE,
        "AddAndActivateConnection2",
        g_variant_new(
            "(@a{sa{sv}}oo@a{sv})",
            build_connection_settings(operation),
            operation->network.device_id,
            operation->network.access_point_id,
            g_variant_builder_end(&options)),
        G_VARIANT_TYPE("(ooa{sv})"),
        G_DBUS_CALL_FLAGS_NONE,
        30000,
        cancellable,
        error);
    if (reply == NULL) {
        return -1;
    }
    g_variant_unref(reply);
    return 0;
}

static int collect_after_connection(
    GDBusConnection *connection,
    GCancellable *cancellable,
    struct classicsetup_network_snapshot *snapshot,
    GError **error)
{
    unsigned int attempt;

    for (attempt = 0; attempt < 20; ++attempt) {
        GError *scan_error = NULL;

        if (collect_snapshot(
                connection,
                cancellable,
                snapshot,
                &scan_error) == 0) {
            if (snapshot->connectivity ==
                CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET) {
                return 0;
            }
        } else if (attempt == 19) {
            g_propagate_error(error, scan_error);
            return -1;
        }
        g_clear_error(&scan_error);
        if (g_cancellable_is_cancelled(cancellable)) {
            g_set_error_literal(
                error,
                G_IO_ERROR,
                G_IO_ERROR_CANCELLED,
                "Network connection check was cancelled");
            return -1;
        }
        g_usleep(500000);
    }
    return 0;
}

static void operation_thread(
    GTask *task,
    gpointer source_object,
    gpointer task_data,
    GCancellable *cancellable)
{
    struct nm_operation *operation = task_data;
    struct classicsetup_network_snapshot *snapshot;
    GDBusConnection *connection;
    GError *error = NULL;

    (void)source_object;
    connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, cancellable, &error);
    if (connection == NULL) {
        g_task_return_error(task, error);
        return;
    }
    if (operation->kind == NM_OPERATION_CONNECT &&
        connect_wifi(connection, operation, cancellable, &error) != 0) {
        g_object_unref(connection);
        g_task_return_error(task, error);
        return;
    }
    snapshot = g_new0(struct classicsetup_network_snapshot, 1);
    if ((operation->kind == NM_OPERATION_CONNECT &&
         collect_after_connection(
             connection,
             cancellable,
             snapshot,
             &error) != 0) ||
        (operation->kind == NM_OPERATION_REFRESH &&
         collect_snapshot(
             connection,
             cancellable,
             snapshot,
             &error) != 0)) {
        g_free(snapshot);
        g_object_unref(connection);
        g_task_return_error(task, error);
        return;
    }
    g_object_unref(connection);
    g_task_return_pointer(task, snapshot, g_free);
}

static void operation_finished(GObject *source, GAsyncResult *result, gpointer data)
{
    struct nm_operation *operation = data;
    struct classicsetup_network_snapshot snapshot;
    struct classicsetup_network_snapshot *completed;
    GError *error = NULL;

    (void)source;
    completed = g_task_propagate_pointer(G_TASK(result), &error);
    g_atomic_int_set(&operation->backend->busy, 0);
    if (g_atomic_int_get(&operation->backend->destroyed)) {
        g_free(completed);
        g_clear_error(&error);
        return;
    }
    if (completed != NULL) {
        operation->callback(completed, operation->user_data);
        g_free(completed);
        return;
    }
    classicsetup_network_snapshot_reset(&snapshot);
    snapshot.state =
        g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED) ||
        g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_SERVICE_UNKNOWN) ||
        g_error_matches(error, G_DBUS_ERROR, G_DBUS_ERROR_NAME_HAS_NO_OWNER)
            ? CLASSICSETUP_NETWORK_UNAVAILABLE
            : CLASSICSETUP_NETWORK_ERROR;
    (void)snprintf(
        snapshot.status,
        sizeof(snapshot.status),
        "%s",
        operation->kind == NM_OPERATION_CONNECT
            ? "Could not connect to this network. Check the password and try again."
            : "Network service is not available. Try again or go back.");
    g_clear_error(&error);
    operation->callback(&snapshot, operation->user_data);
}

static int start_operation(
    struct nm_backend *backend,
    enum nm_operation_kind kind,
    const struct classicsetup_wifi_network *network,
    const char *password,
    classicsetup_network_backend_callback callback,
    void *user_data)
{
    struct nm_operation *operation;
    GTask *task;

    if (backend == NULL || callback == NULL ||
        g_atomic_int_get(&backend->destroyed) ||
        !g_atomic_int_compare_and_exchange(&backend->busy, 0, 1)) {
        return -1;
    }
    operation = g_new0(struct nm_operation, 1);
    operation->backend = backend_ref(backend);
    operation->kind = kind;
    operation->callback = callback;
    operation->user_data = user_data;
    if (network != NULL) {
        operation->network = *network;
    }
    if (password != NULL) {
        (void)snprintf(
            operation->password,
            sizeof(operation->password),
            "%s",
            password);
    }
    task = g_task_new(NULL, backend->cancellable, operation_finished, operation);
    g_task_set_task_data(task, operation, operation_free);
    g_task_run_in_thread(task, operation_thread);
    g_object_unref(task);
    return 0;
}

static int refresh_async(
    void *context,
    classicsetup_network_backend_callback callback,
    void *user_data)
{
    return start_operation(
        context,
        NM_OPERATION_REFRESH,
        NULL,
        NULL,
        callback,
        user_data);
}

static int connect_wifi_async(
    void *context,
    const struct classicsetup_wifi_network *network,
    const char *password,
    classicsetup_network_backend_callback callback,
    void *user_data)
{
    return start_operation(
        context,
        NM_OPERATION_CONNECT,
        network,
        password,
        callback,
        user_data);
}

static void destroy_backend(void *context)
{
    struct nm_backend *backend = context;

    if (backend == NULL) {
        return;
    }
    g_atomic_int_set(&backend->destroyed, 1);
    g_cancellable_cancel(backend->cancellable);
    backend_unref(backend);
}

int classicsetup_network_manager_backend_create(
    struct classicsetup_network_backend *backend)
{
    static const struct classicsetup_network_backend_ops operations = {
        .refresh_async = refresh_async,
        .connect_wifi_async = connect_wifi_async,
        .destroy = destroy_backend
    };
    struct nm_backend *context;

    if (backend == NULL) {
        return -1;
    }
    context = g_new0(struct nm_backend, 1);
    context->references = 1;
    context->cancellable = g_cancellable_new();
    backend->ops = &operations;
    backend->context = context;
    return 0;
}
