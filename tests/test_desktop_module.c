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
#ifndef UMICOM_DESKTOP_BANK_EXECUTABLE
#define UMICOM_DESKTOP_BANK_EXECUTABLE "umicom-bank"
#endif
#ifndef UMICOM_DESKTOP_TMS_EXECUTABLE
#define UMICOM_DESKTOP_TMS_EXECUTABLE "umicom-tms"
#endif

#define REQUIRE(condition)                                                     \
    do {                                                                       \
        if (!(condition)) {                                                    \
            (void)fprintf(stderr, "[FAIL] %s:%d: %s\n",                       \
                          __FILE__, __LINE__, #condition);                      \
            return 1;                                                          \
        }                                                                      \
    } while (0)

/*
 * Start this command or application, report setup failures, and return a process exit code
 * to the operating system.
 */
int main(void)
{
    UmiDesktopModule *module = NULL;
    UmiDesktopModuleSnapshot snapshot;
    UmiApplicationRuntimeRecord record;
    UmiApplicationLaunchSelectionSnapshot launch_selection;
    UmiApplicationLaunchSelection *selection;
    UmiProductWorkspaceGuidePortfolio empty_portfolio = {0};
    UmiProductGuidedLaunchPlan guided_plan;
    const UmiProductGuidedLaunchEntry *guided_entry;

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
    /* Desk must launch the native product workstations, while their console
     * executables remain separate verification tools. */
    REQUIRE(umi_application_runtime_catalogue_find(
                umi_desk_runtime_applications(
                    umi_desktop_module_desk_runtime(module)),
                "org.umicom.bank",
                &record) == UMI_STATUS_OK);
    REQUIRE(strcmp(record.executable_name,
                   UMICOM_DESKTOP_BANK_EXECUTABLE) == 0);
    REQUIRE(umi_application_runtime_catalogue_find(
                umi_desk_runtime_applications(
                    umi_desktop_module_desk_runtime(module)),
                "org.umicom.tms",
                &record) == UMI_STATUS_OK);
    REQUIRE(strcmp(record.executable_name,
                   UMICOM_DESKTOP_TMS_EXECUTABLE) == 0);
    selection = umi_desk_runtime_launch_selection(
        umi_desktop_module_desk_runtime(module));
    REQUIRE(umi_application_launch_selection_snapshot(
                selection, &launch_selection) == UMI_STATUS_OK);
    REQUIRE(launch_selection.eligible_count >= 1U);
    /* An empty suite portfolio demonstrates the explicit missing-guide warning. */
    empty_portfolio.structure_size =
        (uint32_t)sizeof(empty_portfolio);
    REQUIRE(umi_application_launch_selection_set_selected(
                selection, "org.umicom.studio", true) == UMI_STATUS_OK);
    REQUIRE(umi_desktop_module_guided_launch_plan(
                module, &empty_portfolio, &guided_plan) == UMI_STATUS_OK);
    REQUIRE(guided_plan.executable);
    REQUIRE(guided_plan.selected_count == 1U);
    REQUIRE(guided_plan.guidance_warning_count == 1U);
    guided_entry = umi_product_guided_launch_plan_find(
        &guided_plan, "org.umicom.studio");
    REQUIRE(guided_entry != NULL);
    REQUIRE(guided_entry->guidance_state ==
            UMI_PRODUCT_LAUNCH_GUIDANCE_MISSING_GUIDE);
    REQUIRE(guided_entry->ready_to_execute);
    REQUIRE(umi_desktop_module_stop(module) == UMI_STATUS_OK);
    umi_desktop_module_destroy(module);
    return 0;
}
