#include <assert.h>
#include <string.h>

#include "classicsetup/network.h"

struct mock_backend {
    classicsetup_network_backend_callback callback;
    void *user_data;
    int refresh_calls;
    int connect_calls;
    int destroy_calls;
    int password_was_copied;
};

struct observer_state {
    struct classicsetup_network_snapshot snapshot;
    int calls;
};

static void observe(
    const struct classicsetup_network_snapshot *snapshot,
    void *user_data)
{
    struct observer_state *state = user_data;

    state->snapshot = *snapshot;
    ++state->calls;
}

static int mock_refresh(
    void *context,
    classicsetup_network_backend_callback callback,
    void *user_data)
{
    struct mock_backend *mock = context;

    ++mock->refresh_calls;
    mock->callback = callback;
    mock->user_data = user_data;
    return 0;
}

static int mock_connect(
    void *context,
    const struct classicsetup_wifi_network *network,
    const char *password,
    classicsetup_network_backend_callback callback,
    void *user_data)
{
    struct mock_backend *mock = context;

    assert(network != NULL);
    assert(password != NULL);
    ++mock->connect_calls;
    mock->password_was_copied = 0;
    mock->callback = callback;
    mock->user_data = user_data;
    return 0;
}

static void mock_destroy(void *context)
{
    struct mock_backend *mock = context;

    ++mock->destroy_calls;
}

static struct classicsetup_network_backend make_backend(
    struct mock_backend *mock)
{
    static const struct classicsetup_network_backend_ops operations = {
        .refresh_async = mock_refresh,
        .connect_wifi_async = mock_connect,
        .destroy = mock_destroy
    };
    struct classicsetup_network_backend backend = {
        .ops = &operations,
        .context = mock
    };

    return backend;
}

static void complete(
    struct mock_backend *mock,
    const struct classicsetup_network_snapshot *snapshot)
{
    classicsetup_network_backend_callback callback = mock->callback;
    void *user_data = mock->user_data;

    mock->callback = NULL;
    mock->user_data = NULL;
    callback(snapshot, user_data);
}

static void test_unavailable_and_ethernet(void)
{
    struct classicsetup_network_snapshot snapshot;

    classicsetup_network_snapshot_reset(&snapshot);
    assert(snapshot.state == CLASSICSETUP_NETWORK_UNAVAILABLE);
    assert(!classicsetup_network_can_continue(&snapshot));
    snapshot.state = CLASSICSETUP_NETWORK_DISCONNECTED;
    snapshot.ethernet_available = true;
    assert(!classicsetup_network_can_continue(&snapshot));
    snapshot.ethernet_connected = true;
    snapshot.state = CLASSICSETUP_NETWORK_CONNECTED;
    snapshot.connectivity = CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET;
    assert(classicsetup_network_can_continue(&snapshot));
    snapshot.connectivity = CLASSICSETUP_NETWORK_CONNECTIVITY_NETWORK;
    assert(!classicsetup_network_can_continue(&snapshot));
}

static void test_async_state_transitions(void)
{
    struct mock_backend mock = {0};
    struct observer_state observer = {0};
    struct classicsetup_network_controller controller;
    struct classicsetup_network_snapshot snapshot;
    struct classicsetup_wifi_network wifi = {
        .ssid = "Test Wi-Fi",
        .signal_strength = 82,
        .secured = true
    };

    classicsetup_network_controller_init(
        &controller,
        make_backend(&mock),
        observe,
        &observer);
    assert(classicsetup_network_controller_refresh(&controller) == 0);
    assert(controller.busy);
    assert(observer.snapshot.state == CLASSICSETUP_NETWORK_SCANNING);
    classicsetup_network_snapshot_reset(&snapshot);
    snapshot.state = CLASSICSETUP_NETWORK_DISCONNECTED;
    snapshot.wifi_available = true;
    snapshot.wifi[0] = wifi;
    snapshot.wifi[1] = wifi;
    (void)strcpy(snapshot.wifi[1].ssid, "Open Wi-Fi");
    snapshot.wifi[1].secured = false;
    snapshot.wifi_count = 2;
    complete(&mock, &snapshot);
    assert(!controller.busy);
    assert(controller.snapshot.wifi_count == 2);
    assert(controller.snapshot.wifi[0].secured);
    assert(!controller.snapshot.wifi[1].secured);

    assert(classicsetup_network_controller_connect_wifi(
               &controller,
               &wifi,
               "temporary-secret") == 0);
    assert(controller.snapshot.state == CLASSICSETUP_NETWORK_CONNECTING);
    assert(mock.password_was_copied == 0);
    assert(strstr(controller.snapshot.status, "temporary-secret") == NULL);
    snapshot.state = CLASSICSETUP_NETWORK_CONNECTED;
    snapshot.connectivity = CLASSICSETUP_NETWORK_CONNECTIVITY_INTERNET;
    complete(&mock, &snapshot);
    assert(classicsetup_network_can_continue(&controller.snapshot));
    classicsetup_network_controller_destroy(&controller);
    assert(mock.destroy_calls == 1);
}

static void test_failure_and_enterprise_rejection(void)
{
    struct mock_backend mock = {0};
    struct observer_state observer = {0};
    struct classicsetup_network_controller controller;
    struct classicsetup_network_snapshot snapshot;
    struct classicsetup_wifi_network wifi = {
        .ssid = "Enterprise Wi-Fi",
        .secured = true,
        .enterprise = true
    };

    classicsetup_network_controller_init(
        &controller,
        make_backend(&mock),
        observe,
        &observer);
    assert(classicsetup_network_controller_connect_wifi(
               &controller,
               &wifi,
               "not-stored") != 0);
    assert(mock.connect_calls == 0);
    assert(classicsetup_network_controller_refresh(&controller) == 0);
    classicsetup_network_snapshot_reset(&snapshot);
    snapshot.state = CLASSICSETUP_NETWORK_ERROR;
    (void)strcpy(snapshot.status, "Could not refresh networks.");
    complete(&mock, &snapshot);
    assert(controller.snapshot.state == CLASSICSETUP_NETWORK_ERROR);
    assert(!classicsetup_network_can_continue(&controller.snapshot));
    classicsetup_network_controller_destroy(&controller);
}

int main(void)
{
    test_unavailable_and_ethernet();
    test_async_state_transitions();
    test_failure_and_enterprise_rejection();
    return 0;
}
