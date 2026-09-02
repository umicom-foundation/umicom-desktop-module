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

#include <stdio.h>

/*
 * Start this command or application, report setup failures, and return a process exit code
 * to the operating system.
 */
int main(void)
{
    UmiDesktopModule *module = NULL;
    UmiDesktopModuleSnapshot snapshot;
    UmiStatus status = umi_desktop_module_create(NULL, &module);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_start(module);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_snapshot(module, &snapshot);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        (void)printf(
            "Umicom Desk: %zu applications, %zu running, %zu layouts\n",
            snapshot.desk.strip.item_count,
            snapshot.desk.strip.running_count,
            snapshot.desk.has_shell
                ? snapshot.desk.shell.tab_count
                : 0U);
    } /* Use this fallback path when the earlier condition does not apply. */ else {
        (void)fprintf(stderr, "Umicom Desk failed: %s\n",
                      umi_status_text(status));
    }
    umi_desktop_module_destroy(module);
    return status == UMI_STATUS_OK ? 0 : 1;
}
