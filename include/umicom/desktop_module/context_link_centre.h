/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: include/umicom/desktop_module/context_link_centre.h
 *
 * PURPOSE:
 *   Compose Umicom Desk application-selection context links over the reusable Framework context-host runtime.
 *
 * AUTHOR AND ORGANISATION:
 * Sammy Hegab
 * Umicom Foundation
 *
 * LICENCE:
 * MIT
 *---------------------------------------------------------------------------*/

#ifndef UMICOM_DESKTOP_MODULE_CONTEXT_LINK_CENTRE_H
#define UMICOM_DESKTOP_MODULE_CONTEXT_LINK_CENTRE_H

#include <stdint.h>

#include "umicom/desktop_module/desktop_module.h"
#include "umicom/workbench_context_host/workbench_context_host.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Represent the desktop context link centre data shared with callers of this public
 * contract.
 */
typedef struct UmiDesktopContextLinkCentre UmiDesktopContextLinkCentre;

/**
 * Initialise desktop context link centre from caller-provided values so later operations
 * receive a known state.
 */
UmiStatus umi_desktop_context_link_centre_create(
    UmiDesktopContextLinkCentre **out_centre);
/**
 * Release or reset state held by desktop context link centre so the same storage can be
 * reused safely.
 */
void umi_desktop_context_link_centre_destroy(
    UmiDesktopContextLinkCentre *centre);
/**
 * Provide the desktop context link centre refresh operation used by this module and its
 * client applications.
 */
UmiStatus umi_desktop_context_link_centre_refresh(
    UmiDesktopContextLinkCentre *centre,
    UmiDesktopModule *module,
    uint64_t now_ms);
/**
 * Provide the desktop context link centre host operation used by this module and its
 * client applications.
 */
UmiWorkbenchContextHost *umi_desktop_context_link_centre_host(
    UmiDesktopContextLinkCentre *centre);

#ifdef __cplusplus
}
#endif

#endif
