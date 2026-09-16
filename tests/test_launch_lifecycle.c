/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: tests/test_launch_lifecycle.c
 * PURPOSE: Launch a harmless test child repeatedly through the real Desk module.
 * AUTHOR AND ORGANISATION: Sammy Hegab, Umicom Foundation
 * LICENCE: MIT
 *---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "umicom/desktop_module/desktop_module.h"
#include "umicom/platform/filesystem.h"
#include "umicom/platform/threading.h"
#define CHECK(test) do { if (!(test)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #test); return EXIT_FAILURE; } } while (0)
int main(int argc, char **argv)
{
    UmiDesktopModule *module = NULL;
    UmiDesktopModuleConfig config = umi_desktop_module_config_default();
    UmiApplicationRuntimeRegistration registration = {0};
    UmiApplicationRuntimeRecord record;
    UmiDesktopModuleSnapshot snapshot;
    char bin[UMI_PATH_CAPACITY], name[UMI_PATH_CAPACITY];
    CHECK(argc == 2);
    CHECK(umi_fs_is_file(argv[1]));
    CHECK(umi_path_parent(argv[1], bin, sizeof(bin)) == UMI_STATUS_OK);
    CHECK(umi_path_basename(argv[1], name, sizeof(name)) == UMI_STATUS_OK);
    config.executable_root = bin;
    config.working_directory = bin;
    config.compose_studio = false; config.compose_trader = false; config.compose_bank = false;
    config.compose_tms = false; config.compose_os_control_centre = false;
    CHECK(umi_desktop_module_create(&config, &module) == UMI_STATUS_OK);
    CHECK(umi_desktop_module_start(module) == UMI_STATUS_OK);
    UmiDeskRuntime *desk = umi_desktop_module_desk_runtime(module);
    registration.structure_size = sizeof(registration);
    registration.application_id = "org.umicom.lifecycle-test";
    registration.display_name = "Umicom lifecycle test child";
    registration.executable_name = name; registration.working_directory = bin;
    registration.icon_resource_id = "umicom.icon.application.studio";
    registration.default_layout_id = "develop"; registration.taskbar_group = "development";
    registration.family = UMI_APPLICATION_FAMILY_DEVELOPMENT;
    registration.maturity = UMI_APPLICATION_AVAILABLE; registration.entry_kind = UMI_APPLICATION_ENTRY_WORKBENCH;
    registration.installed = true; registration.compatible = true; registration.enabled = true;
    CHECK(umi_desk_runtime_upsert_application(desk, &registration) == UMI_STATUS_OK);
    for (unsigned i = 0U; i < 72U; ++i) {
        UmiStatus status = umi_desk_runtime_request_application(desk, registration.application_id,
            UMI_DESKTOP_APPLICATION_STRIP_LAUNCH_OR_ACTIVATE);
        if (status != UMI_STATUS_OK) fprintf(stderr, "Launch %u failed: %s\n", i + 1U, umi_status_text(status));
        CHECK(status == UMI_STATUS_OK);
        for (unsigned wait = 0U; wait < 2000U; ++wait) {
            CHECK(umi_desktop_module_poll(module) == UMI_STATUS_OK);
            CHECK(umi_application_runtime_catalogue_find(umi_desk_runtime_applications(desk),
                registration.application_id, &record) == UMI_STATUS_OK);
            if (!record.running) break;
            umi_thread_sleep_ms(2U);
        }
        CHECK(!record.running && record.last_exit_code == 0);
        CHECK(umi_desktop_module_snapshot(module, &snapshot) == UMI_STATUS_OK);
        CHECK(snapshot.supervised_process_count == (size_t)i + 1U);
        CHECK(snapshot.completed_process_count == (size_t)i + 1U);
    }
    CHECK(umi_desktop_module_stop(module) == UMI_STATUS_OK);
    umi_desktop_module_destroy(module);
    return EXIT_SUCCESS;
}
