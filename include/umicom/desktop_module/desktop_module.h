/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: include/umicom/desktop_module/desktop_module.h
 *
 * PURPOSE:
 *   Define the thin Umicom Desk product composition. Reusable launcher,
 *   taskbar, layout, process and GTK4 behaviour remains in Umicom Framework.
 *
 * Created by: Sammy Hegab
 * Organisation: Umicom Foundation
 * Licence: MIT
 *---------------------------------------------------------------------------*/
#ifndef UMICOM_DESKTOP_MODULE_DESKTOP_MODULE_H
#define UMICOM_DESKTOP_MODULE_DESKTOP_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "umicom/desktop/desk_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct UmiDesktopModuleConfig {
    uint32_t structure_size;
    const char *executable_root;
    const char *working_directory;
    bool compose_studio;
    bool compose_os_control_centre;
} UmiDesktopModuleConfig;

typedef struct UmiDesktopModuleSnapshot {
    UmiDeskRuntimeSnapshot desk;
    bool started;
    size_t supervised_process_count;
    size_t completed_process_count;
    uint64_t revision;
} UmiDesktopModuleSnapshot;

typedef struct UmiDesktopModule UmiDesktopModule;

UmiDesktopModuleConfig umi_desktop_module_config_default(void);

UmiStatus umi_desktop_module_create(
    const UmiDesktopModuleConfig *config,
    UmiDesktopModule **out_module);
void umi_desktop_module_destroy(UmiDesktopModule *module);

UmiStatus umi_desktop_module_start(UmiDesktopModule *module);
UmiStatus umi_desktop_module_stop(UmiDesktopModule *module);
UmiStatus umi_desktop_module_poll(UmiDesktopModule *module);
UmiStatus umi_desktop_module_snapshot(
    const UmiDesktopModule *module,
    UmiDesktopModuleSnapshot *out_snapshot);

UmiDeskRuntime *umi_desktop_module_desk_runtime(
    UmiDesktopModule *module);
UmiDesktopShellModel *umi_desktop_module_shell_model(
    UmiDesktopModule *module);

#ifdef __cplusplus
}
#endif

#endif
