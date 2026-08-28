#ifndef CLASSICSETUP_NETWORK_H
#define CLASSICSETUP_NETWORK_H

#include <stdbool.h>
#include <stddef.h>

enum {
    CLASSICSETUP_NETWORK_MAX_WIFI = 32,
    CLASSICSETUP_NETWORK_SSID_SIZE = 128,
    CLASSICSETUP_NETWORK_BACKEND_ID_SIZE = 256,
    CLASSICSETUP_NETWORK_STATUS_SIZE = 192
};

enum classicsetup_network_state {
    CLASSICSETUP_NETWORK_UNAVAILABLE,
    CLASSICSETUP_NETWORK_DISCONNECTED,
    CLASSICSETUP_NETWORK_SCANNING,
    CLASSICSETUP_NETWORK_CONNECTING,
    CLASSICSETUP_NETWORK_CONNECTED,
    CLASSICSETUP_NETWORK_ERROR
};

enum classicsetup_network_connectivity {
    CLASSICSETUP_NETWORK_CONNECTIVITY_NONE,
    CLASSICSETUP_NETWORK_CONNECTIVITY_LINK,
    CLASSICSETUP_NETWORK_CONNECTIVITY_NETWORK,
    CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET
};

struct classicsetup_wifi_network {
    char ssid[CLASSICSETUP_NETWORK_SSID_SIZE];
    int signal_strength;
    bool secured;
    bool enterprise;
    bool connected;
    char device_id[CLASSICSETUP_NETWORK_BACKEND_ID_SIZE];
    char access_point_id[CLASSICSETUP_NETWORK_BACKEND_ID_SIZE];
};

struct classicsetup_network_snapshot {
    enum classicsetup_network_state state;
    enum classicsetup_network_connectivity connectivity;
    bool ethernet_available;
    bool ethernet_connected;
    bool wifi_available;
    bool wifi_connected;
    struct classicsetup_wifi_network wifi[CLASSICSETUP_NETWORK_MAX_WIFI];
    size_t wifi_count;
    char status[CLASSICSETUP_NETWORK_STATUS_SIZE];
};

typedef void (*classicsetup_network_backend_callback)(
    const struct classicsetup_network_snapshot *snapshot,
    void *user_data);

struct classicsetup_network_backend_ops {
    int (*refresh_async)(
        void *context,
        classicsetup_network_backend_callback callback,
        void *user_data);
    int (*connect_wifi_async)(
        void *context,
        const struct classicsetup_wifi_network *network,
        const char *password,
        classicsetup_network_backend_callback callback,
        void *user_data);
    void (*destroy)(void *context);
};

struct classicsetup_network_backend {
    const struct classicsetup_network_backend_ops *ops;
    void *context;
};

typedef void (*classicsetup_network_observer)(
    const struct classicsetup_network_snapshot *snapshot,
    void *user_data);

struct classicsetup_network_controller {
    struct classicsetup_network_backend backend;
    struct classicsetup_network_snapshot snapshot;
    classicsetup_network_observer observer;
    void *observer_data;
    bool busy;
};

void classicsetup_network_snapshot_reset(
    struct classicsetup_network_snapshot *snapshot);

bool classicsetup_network_can_continue(
    const struct classicsetup_network_snapshot *snapshot);

bool classicsetup_network_has_connection(
    const struct classicsetup_network_snapshot *snapshot);

void classicsetup_network_controller_init(
    struct classicsetup_network_controller *controller,
    struct classicsetup_network_backend backend,
    classicsetup_network_observer observer,
    void *observer_data);

void classicsetup_network_controller_destroy(
    struct classicsetup_network_controller *controller);

int classicsetup_network_controller_refresh(
    struct classicsetup_network_controller *controller);

int classicsetup_network_controller_connect_wifi(
    struct classicsetup_network_controller *controller,
    const struct classicsetup_wifi_network *network,
    const char *password);

int classicsetup_network_manager_backend_create(
    struct classicsetup_network_backend *backend);

#endif
