#include "classicsetup/windows_source.h"

#include <stdio.h>

int classicsetup_microsoft_source_discover(
    enum classicsetup_windows_family family,
    struct classicsetup_source_catalog *catalog)
{
    (void)family;
    if (catalog != NULL) {
        classicsetup_source_catalog_reset(catalog);
        catalog->state = CLASSICSETUP_SOURCE_ERROR;
        (void)snprintf(catalog->error, sizeof(catalog->error), "%s",
                       "Windows source download support is unavailable.");
    }
    return -1;
}

int classicsetup_microsoft_source_resolve(
    struct classicsetup_windows_release *release)
{
    (void)release;
    return -1;
}
