#include "classicsetup/network.h"

#include <stdio.h>
#include <string.h>

static void notify_observer(
    struct classicsetup_network_controller *controller)
{
    if (controller->observer != NULL) {
        controller->observer(
            &controller->snapshot,
            controller->observer_data);
    }
}

static void backend_completed(
    const struct classicsetup_network_snapshot *snapshot,
    void *user_data)
{
    struct classicsetup_network_controller *controller = user_data;

    if (controller == NULL || snapshot == NULL) {
        return;
    }
    controller->snapshot = *snapshot;
    controller->busy = false;
    notify_observer(controller);
}

static void set_start_error(
    struct classicsetup_network_controller *controller,
    const char *message)
{
    controller->busy = false;
    controller->snapshot.state = CLASSICSETUP_NETWORK_ERROR;
    controller->snapshot.connectivity =
        CLASSICSETUP_NETWORK_CONNECTIVITY_NONE;
    (void)snprintf(
        controller->snapshot.status,
        sizeof(controller->snapshot.status),
        "%s",
        message);
    notify_observer(controller);
}

void classicsetup_network_snapshot_reset(
    struct classicsetup_network_snapshot *snapshot)
{
    if (snapshot == NULL) {
        return;
    }
    memset(snapshot, 0, sizeof(*snapshot));
    snapshot->state = CLASSICSETUP_NETWORK_UNAVAILABLE;
    (void)snprintf(
        snapshot->status,
        sizeof(snapshot->status),
        "%s",
        "Network service is not available.");
}

bool classicsetup_network_can_continue(
    const struct classicsetup_network_snapshot *snapshot)
{
    return snapshot != NULL &&
           snapshot->state == CLASSICSETUP_NETWORK_CONNECTED &&
           snapshot->connectivity ==
               CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET;
}

bool classicsetup_network_has_connection(
    const struct classicsetup_network_snapshot *snapshot)
{
    return snapshot != NULL &&
           snapshot->state == CLASSICSETUP_NETWORK_CONNECTED &&
           (snapshot->ethernet_connected || snapshot->wifi_connected);
}

void classicsetup_network_controller_init(
    struct classicsetup_network_controller *controller,
    struct classicsetup_network_backend backend,
    classicsetup_network_observer observer,
    void *observer_data)
{
    if (controller == NULL) {
        return;
    }
    memset(controller, 0, sizeof(*controller));
    controller->backend = backend;
    controller->observer = observer;
    controller->observer_data = observer_data;
    classicsetup_network_snapshot_reset(&controller->snapshot);
}

void classicsetup_network_controller_destroy(
    struct classicsetup_network_controller *controller)
{
    if (controller == NULL) {
        return;
    }
    if (controller->backend.ops != NULL &&
        controller->backend.ops->destroy != NULL) {
        controller->backend.ops->destroy(controller->backend.context);
    }
    memset(controller, 0, sizeof(*controller));
}

int classicsetup_network_controller_refresh(
    struct classicsetup_network_controller *controller)
{
    int result;

    if (controller == NULL || controller->busy ||
        controller->backend.ops == NULL ||
        controller->backend.ops->refresh_async == NULL) {
        return -1;
    }
    controller->busy = true;
    controller->snapshot.state = CLASSICSETUP_NETWORK_SCANNING;
    (void)snprintf(
        controller->snapshot.status,
        sizeof(controller->snapshot.status),
        "%s",
        "Checking network connections...");
    notify_observer(controller);
    result = controller->backend.ops->refresh_async(
        controller->backend.context,
        backend_completed,
        controller);
    if (result != 0) {
        set_start_error(controller, "Network scan could not be started.");
    }
    return result;
}

int classicsetup_network_controller_connect_wifi(
    struct classicsetup_network_controller *controller,
    const struct classicsetup_wifi_network *network,
    const char *password)
{
    int result;

    if (controller == NULL || network == NULL || controller->busy ||
        controller->backend.ops == NULL ||
        controller->backend.ops->connect_wifi_async == NULL ||
        network->ssid[0] == '\0' || network->enterprise ||
        (network->secured && (password == NULL || password[0] == '\0'))) {
        return -1;
    }
    controller->busy = true;
    controller->snapshot.state = CLASSICSETUP_NETWORK_CONNECTING;
    controller->snapshot.connectivity =
        CLASSICSETUP_NETWORK_CONNECTIVITY_NONE;
    (void)snprintf(
        controller->snapshot.status,
        sizeof(controller->snapshot.status),
        "Connecting to %s...",
        network->ssid);
    notify_observer(controller);
    result = controller->backend.ops->connect_wifi_async(
        controller->backend.context,
        network,
        password != NULL ? password : "",
        backend_completed,
        controller);
    if (result != 0) {
        set_start_error(controller, "Wi-Fi connection could not be started.");
    }
    return result;
}
