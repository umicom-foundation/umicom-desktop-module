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

int main(void)
{
    UmiDesktopModule *module = NULL;
    UmiDesktopModuleSnapshot snapshot;
    UmiStatus status = umi_desktop_module_create(NULL, &module);
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_start(module);
    }
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_snapshot(module, &snapshot);
    }
    if (status == UMI_STATUS_OK) {
        (void)printf(
            "Umicom Desk: %zu applications, %zu running, %zu layouts\n",
            snapshot.desk.strip.item_count,
            snapshot.desk.strip.running_count,
            snapshot.desk.has_shell
                ? snapshot.desk.shell.tab_count
                : 0U);
    } else {
        (void)fprintf(stderr, "Umicom Desk failed: %s\n",
                      umi_status_text(status));
    }
    umi_desktop_module_destroy(module);
    return status == UMI_STATUS_OK ? 0 : 1;
}
