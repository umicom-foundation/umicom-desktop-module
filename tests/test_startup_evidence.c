/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: tests/test_startup_evidence.c
 *
 * PURPOSE:
 *   Verify queued Desk launches reconcile bounded native failure evidence instead of remaining running.
 *
 * AUTHOR AND ORGANISATION:
 * Sammy Hegab
 * Umicom Foundation
 *
 * LICENCE:
 * MIT
 *---------------------------------------------------------------------------*/


#include "umicom/desktop_module/desktop_module.h"
#include "umicom/platform/filesystem.h"
#include "umicom/platform/threading.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(int argc, char **argv)
{
    CHECK(argc == 3);
    UmiDesktopModule *module = NULL;
    UmiDesktopModuleConfig config = umi_desktop_module_config_default();
    UmiApplicationRuntimeRegistration entry = {0};
    UmiApplicationRuntimeRecord record;
    char root[UMI_PATH_CAPACITY], name[UMI_PATH_CAPACITY];
    CHECK(umi_path_parent(argv[2], root, sizeof(root)) == UMI_STATUS_OK);
    CHECK(umi_path_basename(argv[2], name, sizeof(name)) == UMI_STATUS_OK);
    config.executable_root = root; config.working_directory = root;
    config.compose_studio = false; config.compose_trader = false;
    config.compose_bank = false; config.compose_tms = false; config.compose_os_control_centre = false;
    CHECK(umi_desktop_module_create(&config, &module) == UMI_STATUS_OK);
    CHECK(umi_desktop_module_start(module) == UMI_STATUS_OK);
    UmiDeskRuntime *desk = umi_desktop_module_desk_runtime(module);
    entry.structure_size = sizeof(entry); entry.application_id = "org.umicom.startup-fixture";
    entry.display_name = "Inert startup fixture";
    entry.executable_name = strcmp(argv[1], "missing") == 0 ? "missing-startup-fixture-never-created" : name;
    entry.working_directory = root; entry.icon_resource_id = "umicom.icon.application.studio";
    entry.default_layout_id = "develop"; entry.taskbar_group = "development";
    entry.family = UMI_APPLICATION_FAMILY_DEVELOPMENT; entry.maturity = UMI_APPLICATION_AVAILABLE;
    entry.entry_kind = UMI_APPLICATION_ENTRY_WORKBENCH;
    entry.installed = true; entry.enabled = true; entry.compatible = true;
    CHECK(umi_desk_runtime_upsert_application(desk, &entry) == UMI_STATUS_OK);
    CHECK(umi_desk_runtime_request_application(desk, entry.application_id,
        UMI_DESKTOP_APPLICATION_STRIP_LAUNCH_OR_ACTIVATE) == UMI_STATUS_OK);
    CHECK(umi_application_runtime_catalogue_find(umi_desk_runtime_applications(desk),
        entry.application_id, &record) == UMI_STATUS_OK);
    CHECK(record.state == UMI_APPLICATION_RUNTIME_STARTING && !record.active);
    for (unsigned attempt = 0U; attempt < 3000U; ++attempt) {
        CHECK(umi_desktop_module_poll(module) == UMI_STATUS_OK);
        CHECK(umi_application_runtime_catalogue_find(umi_desk_runtime_applications(desk),
            entry.application_id, &record) == UMI_STATUS_OK);
        if (!record.running) break;
        umi_thread_sleep_ms(2U);
    }
    CHECK(!record.running && record.state == UMI_APPLICATION_RUNTIME_FAILED);
    CHECK(record.process_token == 0U && !record.active);
    if (strcmp(argv[1], "missing") == 0) {
#ifdef _WIN32
        CHECK(strstr(record.last_error, "preparation failed") != NULL);
#endif
    } else {
        CHECK(record.last_exit_code == 7);
        CHECK(strstr(record.last_error, "FINAL-STARTUP-DETAIL") != NULL);
        CHECK(strstr(record.last_error, "truncated") != NULL);
        CHECK(strstr(record.last_error, "0x00000007") != NULL);
    }
    CHECK(umi_desktop_module_poll(module) == UMI_STATUS_OK);
    UmiDesktopModuleSnapshot snapshot;
    CHECK(umi_desktop_module_snapshot(module, &snapshot) == UMI_STATUS_OK);
    CHECK(snapshot.completed_process_count == 1U && snapshot.supervised_process_count == 1U);
    CHECK(umi_desktop_module_stop(module) == UMI_STATUS_OK);
    umi_desktop_module_destroy(module);
    return 0;
}
