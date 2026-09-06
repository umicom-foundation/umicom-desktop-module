/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: src/console/main.c
 *
 * PURPOSE:
 *   Provide deterministic headless validation of the thin Umicom Desk module.
 *
 * AUTHOR AND ORGANISATION:
 * Sammy Hegab
 * Umicom Foundation
 *
 * LICENCE:
 * MIT
 *---------------------------------------------------------------------------*/
#include "umicom/desktop_module/desktop_module.h"
#include "umicom/application/runtime/readiness.h"

#include <stdio.h>

/*
 * Start this command or application, report setup failures, and return a process exit code
 * to the operating system.
 */
int main(void)
{
    UmiDesktopModule *module = NULL;
    UmiDesktopModuleSnapshot snapshot;
    UmiApplicationLaunchReadinessSummary portfolio;
    UmiStatus status = umi_desktop_module_create(NULL, &module);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_start(module);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_snapshot(module, &snapshot);
    }
    /* Ask Framework for one portfolio-wide readiness view used by the launcher dashboard. */
    if (status == UMI_STATUS_OK) {
        status = umi_application_launch_readiness_summary(&portfolio);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        (void)printf(
            "Umicom Desk: %zu applications, %zu launchable, %zu blocked, "
            "%zu running, %zu layouts\n",
            snapshot.desk.strip.item_count,
            snapshot.desk.launch_selection.eligible_count,
            snapshot.desk.launch_selection.readiness_blocked_count,
            snapshot.desk.strip.running_count,
            snapshot.desk.has_shell
                ? snapshot.desk.shell.tab_count
                : 0U);
        (void)printf(
            "Portfolio readiness: %zu/%zu launchable, %zu blocked, %u%% average\n",
            portfolio.ready_count,
            portfolio.application_count,
            portfolio.blocked_count,
            portfolio.average_feature_readiness_percent);
    } /* Use this fallback path when the earlier condition does not apply. */ else {
        (void)fprintf(stderr, "Umicom Desk failed: %s\n",
                      umi_status_text(status));
    }
    umi_desktop_module_destroy(module);
    return status == UMI_STATUS_OK ? 0 : 1;
}
