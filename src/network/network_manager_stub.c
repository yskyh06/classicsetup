#include "classicsetup/network.h"

int classicsetup_network_manager_backend_create(
    struct classicsetup_network_backend *backend)
{
    if (backend != NULL) {
        backend->ops = NULL;
        backend->context = NULL;
    }
    return -1;
}
