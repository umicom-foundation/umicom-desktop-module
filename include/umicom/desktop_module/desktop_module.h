/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: include/umicom/desktop_module/desktop_module.h
 *
 * PURPOSE:
 *   Define the thin Umicom Desk product composition. Reusable launcher,
 *   taskbar, layout, process and GTK4 behaviour remains in Umicom Framework.
 *
 * AUTHOR AND ORGANISATION:
 * Sammy Hegab
 * Umicom Foundation
 *
 * LICENCE:
 * MIT
 *---------------------------------------------------------------------------*/
#ifndef UMICOM_DESKTOP_MODULE_DESKTOP_MODULE_H
#define UMICOM_DESKTOP_MODULE_DESKTOP_MODULE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "umicom/application/productisation/launch_guidance.h"
#include "umicom/desktop/desk_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Represent the desktop module config data shared with callers of this public contract.
 */
typedef struct UmiDesktopModuleConfig {
    uint32_t structure_size;
    const char *executable_root;
    const char *working_directory;
    bool compose_studio;
    bool compose_trader;
    bool compose_bank;
    bool compose_tms;
    bool compose_os_control_centre;
} UmiDesktopModuleConfig;

/**
 * Represent the desktop module snapshot data shared with callers of this public contract.
 */
typedef struct UmiDesktopModuleSnapshot {
    UmiDeskRuntimeSnapshot desk;
    bool started;
    size_t supervised_process_count;
    size_t completed_process_count;
    uint64_t revision;
} UmiDesktopModuleSnapshot;

/**
 * Represent the desktop module data shared with callers of this public contract.
 */
typedef struct UmiDesktopModule UmiDesktopModule;

/**
 * Provide the desktop module config default operation used by this module and its client
 * applications.
 */
UmiDesktopModuleConfig umi_desktop_module_config_default(void);

/**
 * Initialise desktop module from caller-provided values so later operations receive a
 * known state.
 */
UmiStatus umi_desktop_module_create(
    const UmiDesktopModuleConfig *config,
    UmiDesktopModule **out_module);
/**
 * Release or reset state held by desktop module so the same storage can be reused safely.
 */
void umi_desktop_module_destroy(UmiDesktopModule *module);

/**
 * Provide the desktop module start operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_start(UmiDesktopModule *module);
/**
 * Provide the desktop module stop operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_stop(UmiDesktopModule *module);
/**
 * Provide the desktop module poll operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_poll(UmiDesktopModule *module);
/**
 * Provide the desktop module snapshot operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_snapshot(
    const UmiDesktopModule *module,
    UmiDesktopModuleSnapshot *out_snapshot);

/* Join this Desk selection to suite guidance without starting any process. */
UmiStatus umi_desktop_module_guided_launch_plan(
    UmiDesktopModule *module,
    const UmiProductWorkspaceGuidePortfolio *portfolio,
    UmiProductGuidedLaunchPlan *out_plan);

/**
 * Provide the desktop module desk runtime operation used by this module and its client
 * applications.
 */
UmiDeskRuntime *umi_desktop_module_desk_runtime(
    UmiDesktopModule *module);
/**
 * Provide the desktop module shell model operation used by this module and its client
 * applications.
 */
UmiDesktopShellModel *umi_desktop_module_shell_model(
    UmiDesktopModule *module);

#ifdef __cplusplus
}
#endif

#endif
