/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: tests/test_desktop_module.c
 *
 * PURPOSE:
 *   Verify thin product composition and Framework-owned runtime projection.
 *
 * AUTHOR AND ORGANISATION:
 * Sammy Hegab
 * Umicom Foundation
 *
 * LICENCE:
 * MIT
 *---------------------------------------------------------------------------*/
#include "umicom/desktop_module/desktop_module.h"

#include <stdio.h>
#include <string.h>

#ifndef UMICOM_DESKTOP_STUDIO_EXECUTABLE
#define UMICOM_DESKTOP_STUDIO_EXECUTABLE "umicom-studio-ide"
#endif

#define REQUIRE(condition)                                                     \
    do {                                                                       \
        if (!(condition)) {                                                    \
            (void)fprintf(stderr, "[FAIL] %s:%d: %s\n",                       \
                          __FILE__, __LINE__, #condition);                      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

int main(void)
{
    UmiDesktopModule *module = NULL;
    UmiDesktopModuleSnapshot snapshot;
    UmiApplicationRuntimeRecord record;
    UmiApplicationLaunchSelectionSnapshot launch_selection;

    REQUIRE(umi_desktop_module_create(NULL, &module) == UMI_STATUS_OK);
    REQUIRE(umi_desktop_module_start(module) == UMI_STATUS_OK);
    REQUIRE(umi_desktop_module_snapshot(module, &snapshot) ==
            UMI_STATUS_OK);
    REQUIRE(snapshot.started);
    REQUIRE(snapshot.desk.strip.item_count >= 3U);
    REQUIRE(strcmp(snapshot.desk.strip.active_application_id,
                   "org.umicom.desktop") == 0);
    REQUIRE(umi_application_runtime_catalogue_find(
                umi_desk_runtime_applications(
                    umi_desktop_module_desk_runtime(module)),
                "org.umicom.studio",
                &record) == UMI_STATUS_OK);
    REQUIRE(record.installed);
    REQUIRE(strcmp(record.executable_name,
                   UMICOM_DESKTOP_STUDIO_EXECUTABLE) == 0);
    REQUIRE(umi_application_launch_selection_snapshot(
                umi_desk_runtime_launch_selection(
                    umi_desktop_module_desk_runtime(module)),
                &launch_selection) == UMI_STATUS_OK);
    REQUIRE(launch_selection.eligible_count >= 1U);
    REQUIRE(umi_desktop_module_stop(module) == UMI_STATUS_OK);
    umi_desktop_module_destroy(module);
    return 0;
}
