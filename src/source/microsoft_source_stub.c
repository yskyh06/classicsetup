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
    struct classicsetup_source_resolve_diagnostics diagnostics;

    return classicsetup_microsoft_source_resolve_with_diagnostics(
        release, &diagnostics);
}

int classicsetup_microsoft_source_resolve_with_diagnostics(
    struct classicsetup_windows_release *release,
    struct classicsetup_source_resolve_diagnostics *diagnostics)
{
    (void)release;
    if (diagnostics != NULL) {
        classicsetup_source_resolve_diagnostics_reset(diagnostics);
        diagnostics->error =
            CLASSICSETUP_SOURCE_RESOLVE_NETWORK_ERROR;
    }
    return -1;
}
