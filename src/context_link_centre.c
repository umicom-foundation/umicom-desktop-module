/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: src/context_link_centre.c
 *
 * PURPOSE:
 *   Create Desk context groups and publish active application selections without moving routing logic into the product module.
 *
 * Created by: Sammy Hegab
 * Organisation: Umicom Foundation
 * Licence: MIT
 *---------------------------------------------------------------------------*/

#include "umicom/desktop_module/context_link_centre.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "umicom/application/runtime_catalogue.h"
#include "umicom/workbench_context_host/application_publisher.h"

#define UMI_DESKTOP_CONTEXT_GROUP_SYSTEM "desktop.context.system"
#define UMI_DESKTOP_CONTEXT_GROUP_DEVELOPMENT "desktop.context.development"
#define UMI_DESKTOP_CONTEXT_GROUP_TRADING "desktop.context.trading"
#define UMI_DESKTOP_CONTEXT_GROUP_OPERATIONS "desktop.context.operations"
#define UMI_DESKTOP_CONTEXT_GROUP_AI "desktop.context.ai"
#define UMI_DESKTOP_CONTEXT_GROUP_DATA "desktop.context.data"

struct UmiDesktopContextLinkCentre {
    UmiWorkbenchContextLinkSlaveController link_controller;
    UmiWorkbenchContextHost *host;
    UmiWorkbenchContextHostSlaveController host_controller;
    UmiWorkbenchContextHostProfile *profile;
    char last_application_id[UMI_APPLICATION_RUNTIME_ID_CAPACITY];
    uint64_t publication_sequence;
    uint64_t revision;
};

static uint64_t kind_mask(UmiContextKind kind)
{
    return umi_workbench_context_host_kind_mask(kind);
}

static UmiStatus add_group(
    UmiWorkbenchContextHostProfile *profile,
    const char *group_id,
    const char *title,
    UmiContextChannelColour colour,
    uint64_t allowed,
    bool active)
{
    UmiWorkbenchContextHostGroupDefinition group;
    UmiStatus status;
    umi_workbench_context_host_group_definition_init(&group, group_id);
    status = umi_workbench_context_host_copy_text(
        group.title, sizeof(group.title), title);
    if (status != UMI_STATUS_OK) return status;
    group.colour = colour;
    group.allowed_kinds_mask = allowed;
    group.default_mode = UMI_WORKBENCH_CONTEXT_LINK_MODE_BIDIRECTIONAL;
    group.default_active = active;
    return umi_workbench_context_host_profile_add_group(profile, &group);
}

static UmiStatus add_endpoint(
    UmiWorkbenchContextHostProfile *profile,
    const char *endpoint_id,
    const char *panel_id,
    const char *application_id,
    const char *display_name,
    const char *group_id,
    UmiWorkbenchContextHostPanelRole role,
    UmiWorkbenchContextLinkMode mode,
    uint64_t accepted,
    uint64_t published)
{
    UmiWorkbenchContextHostEndpoint endpoint;
    UmiStatus status;
    umi_workbench_context_host_endpoint_init(&endpoint, endpoint_id);
    status = umi_workbench_context_host_endpoint_set_identity(
        &endpoint, panel_id, application_id, display_name);
    if (status != UMI_STATUS_OK) return status;
    status = umi_workbench_context_host_endpoint_set_group(
        &endpoint, group_id, mode);
    if (status != UMI_STATUS_OK) return status;
    endpoint.role = role;
    endpoint.accepted_kinds_mask = accepted;
    endpoint.published_kinds_mask = published;
    endpoint.state = UMI_WORKBENCH_CONTEXT_HOST_ENDPOINT_ACTIVE;
    return umi_workbench_context_host_profile_add_endpoint(profile, &endpoint);
}

static UmiStatus add_launcher_binding(
    UmiWorkbenchContextHostProfile *profile,
    const char *endpoint_id,
    const char *group_id,
    uint64_t published)
{
    return add_endpoint(
        profile,
        endpoint_id,
        "desktop.launcher",
        "org.umicom.desktop",
        "Application Launcher",
        group_id,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_APPLICATION_LAUNCHER,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_PUBLISH,
        published,
        published);
}

static UmiStatus build_profile(UmiWorkbenchContextHostProfile *profile)
{
    const uint64_t selection = kind_mask(UMI_CONTEXT_KIND_SELECTION);
    const uint64_t project = kind_mask(UMI_CONTEXT_KIND_PROJECT);
    const uint64_t source = kind_mask(UMI_CONTEXT_KIND_SOURCE_LOCATION);
    const uint64_t workspace = kind_mask(UMI_CONTEXT_KIND_WORKSPACE);
    const uint64_t instrument = kind_mask(UMI_CONTEXT_KIND_INSTRUMENT);
    const uint64_t account = kind_mask(UMI_CONTEXT_KIND_ACCOUNT);
    const uint64_t trade = kind_mask(UMI_CONTEXT_KIND_TRADE);
    UmiStatus status;

    umi_workbench_context_host_profile_init(
        profile, "desktop.context.profile", "org.umicom.desktop");
    status = umi_workbench_context_host_profile_set_title(
        profile, "Umicom Desk Linked Applications");
    if (status != UMI_STATUS_OK) return status;

    status = add_group(
        profile,
        UMI_DESKTOP_CONTEXT_GROUP_SYSTEM,
        "System",
        UMI_CONTEXT_COLOUR_YELLOW,
        selection | workspace,
        true);
    if (status != UMI_STATUS_OK) return status;
    status = add_group(
        profile,
        UMI_DESKTOP_CONTEXT_GROUP_DEVELOPMENT,
        "Development",
        UMI_CONTEXT_COLOUR_BLUE,
        selection | project | source | workspace,
        false);
    if (status != UMI_STATUS_OK) return status;
    status = add_group(
        profile,
        UMI_DESKTOP_CONTEXT_GROUP_TRADING,
        "Trading",
        UMI_CONTEXT_COLOUR_RED,
        selection | instrument | account | trade,
        false);
    if (status != UMI_STATUS_OK) return status;
    status = add_group(
        profile,
        UMI_DESKTOP_CONTEXT_GROUP_OPERATIONS,
        "Operations",
        UMI_CONTEXT_COLOUR_GREEN,
        selection | account | trade | workspace,
        false);
    if (status != UMI_STATUS_OK) return status;
    status = add_group(
        profile,
        UMI_DESKTOP_CONTEXT_GROUP_AI,
        "AI",
        UMI_CONTEXT_COLOUR_PURPLE,
        selection | project | source | workspace,
        false);
    if (status != UMI_STATUS_OK) return status;
    status = add_group(
        profile,
        UMI_DESKTOP_CONTEXT_GROUP_DATA,
        "Data",
        UMI_CONTEXT_COLOUR_CYAN,
        selection | project | workspace,
        false);
    if (status != UMI_STATUS_OK) return status;

    /*
     * The launcher participates in every group using one stable panel ID and
     * unique endpoint identities. The generic Framework router therefore
     * authorises application selections for whichever group the selected
     * product belongs to without any special-case routing code.
     */
    status = add_launcher_binding(
        profile,
        "desktop.context.launcher.system",
        UMI_DESKTOP_CONTEXT_GROUP_SYSTEM,
        selection);
    if (status != UMI_STATUS_OK) return status;
    status = add_launcher_binding(
        profile,
        "desktop.context.launcher.development",
        UMI_DESKTOP_CONTEXT_GROUP_DEVELOPMENT,
        selection);
    if (status != UMI_STATUS_OK) return status;
    status = add_launcher_binding(
        profile,
        "desktop.context.launcher.trading",
        UMI_DESKTOP_CONTEXT_GROUP_TRADING,
        selection);
    if (status != UMI_STATUS_OK) return status;
    status = add_launcher_binding(
        profile,
        "desktop.context.launcher.operations",
        UMI_DESKTOP_CONTEXT_GROUP_OPERATIONS,
        selection);
    if (status != UMI_STATUS_OK) return status;
    status = add_launcher_binding(
        profile,
        "desktop.context.launcher.ai",
        UMI_DESKTOP_CONTEXT_GROUP_AI,
        selection);
    if (status != UMI_STATUS_OK) return status;
    status = add_launcher_binding(
        profile,
        "desktop.context.launcher.data",
        UMI_DESKTOP_CONTEXT_GROUP_DATA,
        selection);
    if (status != UMI_STATUS_OK) return status;

    status = add_endpoint(
        profile,
        "desktop.context.application.studio",
        "desktop.application.studio",
        "org.umicom.studio",
        "Umicom Studio IDE",
        UMI_DESKTOP_CONTEXT_GROUP_DEVELOPMENT,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_GENERIC,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_FOLLOW,
        selection | project | source | workspace,
        0U);
    if (status != UMI_STATUS_OK) return status;

    status = add_endpoint(
        profile,
        "desktop.context.application.os",
        "desktop.application.os",
        "org.umicom.os",
        "Umicom OS",
        UMI_DESKTOP_CONTEXT_GROUP_SYSTEM,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_GENERIC,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_FOLLOW,
        selection | workspace,
        0U);
    if (status != UMI_STATUS_OK) return status;

    status = add_endpoint(
        profile,
        "desktop.context.application.trader",
        "desktop.application.trader",
        "org.umicom.trader",
        "Umicom Trader",
        UMI_DESKTOP_CONTEXT_GROUP_TRADING,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_GENERIC,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_FOLLOW,
        selection | instrument | account | trade,
        0U);
    if (status != UMI_STATUS_OK) return status;

    status = add_endpoint(
        profile,
        "desktop.context.application.tms",
        "desktop.application.tms",
        "org.umicom.tms",
        "Umicom TMS",
        UMI_DESKTOP_CONTEXT_GROUP_TRADING,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_GENERIC,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_FOLLOW,
        selection | account | trade,
        0U);
    if (status != UMI_STATUS_OK) return status;

    status = add_endpoint(
        profile,
        "desktop.context.application.llm",
        "desktop.application.llm",
        "org.umicom.llm",
        "Umicom LLM",
        UMI_DESKTOP_CONTEXT_GROUP_AI,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_AI,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_FOLLOW,
        selection | project | source | workspace,
        0U);
    if (status != UMI_STATUS_OK) return status;

    status = add_endpoint(
        profile,
        "desktop.context.application.bank",
        "desktop.application.bank",
        "org.umicom.bank",
        "Umicom Bank",
        UMI_DESKTOP_CONTEXT_GROUP_OPERATIONS,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_GENERIC,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_FOLLOW,
        selection | account | trade | workspace,
        0U);
    if (status != UMI_STATUS_OK) return status;

    return add_endpoint(
        profile,
        "desktop.context.application.exchange",
        "desktop.application.exchange",
        "org.umicom.exchange",
        "Umicom Exchange",
        UMI_DESKTOP_CONTEXT_GROUP_TRADING,
        UMI_WORKBENCH_CONTEXT_HOST_PANEL_GENERIC,
        UMI_WORKBENCH_CONTEXT_LINK_MODE_FOLLOW,
        selection | instrument | account | trade,
        0U);
}

static const char *group_for_application(const char *application_id)
{
    if (application_id == NULL) return UMI_DESKTOP_CONTEXT_GROUP_SYSTEM;
    if (strcmp(application_id, "org.umicom.studio") == 0) {
        return UMI_DESKTOP_CONTEXT_GROUP_DEVELOPMENT;
    }
    if (strcmp(application_id, "org.umicom.trader") == 0 ||
        strcmp(application_id, "org.umicom.tms") == 0 ||
        strcmp(application_id, "org.umicom.exchange") == 0) {
        return UMI_DESKTOP_CONTEXT_GROUP_TRADING;
    }
    if (strcmp(application_id, "org.umicom.bank") == 0) {
        return UMI_DESKTOP_CONTEXT_GROUP_OPERATIONS;
    }
    if (strcmp(application_id, "org.umicom.llm") == 0) {
        return UMI_DESKTOP_CONTEXT_GROUP_AI;
    }
    return UMI_DESKTOP_CONTEXT_GROUP_SYSTEM;
}

UmiStatus umi_desktop_context_link_centre_create(
    UmiDesktopContextLinkCentre **out_centre)
{
    UmiDesktopContextLinkCentre *centre;
    UmiWorkbenchContextHostConfig config;
    UmiStatus status;

    if (out_centre == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    *out_centre = NULL;

    centre = (UmiDesktopContextLinkCentre *)calloc(1U, sizeof(*centre));
    if (centre == NULL) return UMI_STATUS_OUT_OF_MEMORY;
    centre->profile = (UmiWorkbenchContextHostProfile *)calloc(
        1U, sizeof(*centre->profile));
    if (centre->profile == NULL) {
        free(centre);
        return UMI_STATUS_OUT_OF_MEMORY;
    }
    centre->publication_sequence = 1U;
    centre->revision = 1U;

    umi_workbench_context_link_slave_controller_init(
        &centre->link_controller);
    status = umi_workbench_context_link_slave_controller_start(
        &centre->link_controller);
    if (status != UMI_STATUS_OK) {
        umi_desktop_context_link_centre_destroy(centre);
        return status;
    }

    config = umi_workbench_context_host_config_default();
    config.host_id = "desktop.workbench";
    config.application_id = "org.umicom.desktop";
    config.observer_panel_id = "desktop.launcher";

    status = umi_workbench_context_host_create(
        &config,
        umi_workbench_context_link_slave_controller_service(
            &centre->link_controller),
        &centre->host);
    if (status == UMI_STATUS_OK) {
        status = build_profile(centre->profile);
    }
    if (status == UMI_STATUS_OK) {
        status = umi_workbench_context_host_apply_profile(
            centre->host, centre->profile);
    }
    if (status != UMI_STATUS_OK) {
        umi_desktop_context_link_centre_destroy(centre);
        return status;
    }

    umi_workbench_context_host_slave_controller_init(
        &centre->host_controller, centre->host);
    status = umi_workbench_context_host_slave_controller_start(
        &centre->host_controller);
    if (status != UMI_STATUS_OK) {
        umi_desktop_context_link_centre_destroy(centre);
        return status;
    }

    *out_centre = centre;
    return UMI_STATUS_OK;
}

void umi_desktop_context_link_centre_destroy(
    UmiDesktopContextLinkCentre *centre)
{
    if (centre == NULL) return;
    if (centre->host != NULL) {
        (void)umi_workbench_context_host_slave_controller_stop(
            &centre->host_controller);
    }
    umi_workbench_context_host_destroy(centre->host);
    centre->host = NULL;

    (void)umi_workbench_context_link_slave_controller_stop(
        &centre->link_controller);
    umi_workbench_context_link_slave_controller_destroy(
        &centre->link_controller);

    free(centre->profile);
    free(centre);
}

UmiStatus umi_desktop_context_link_centre_refresh(
    UmiDesktopContextLinkCentre *centre,
    UmiDesktopModule *module,
    uint64_t now_ms)
{
    UmiDesktopModuleSnapshot snapshot;
    UmiApplicationRuntimeRecord record;
    UmiApplicationRuntimeCatalogue *catalogue;
    const char *active_application;
    const char *group_id;
    const char *state_text = "unknown";
    const char *taskbar_group = "";
    const char *layout_id = "";
    char context_id[UMI_WORKBENCH_CONTEXT_HOST_ID_CAPACITY];
    int written;
    UmiStatus status;

    if (centre == NULL || module == NULL || centre->host == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }

    status = umi_desktop_module_snapshot(module, &snapshot);
    if (status != UMI_STATUS_OK) return status;
    active_application = snapshot.desk.applications.active_application_id;
    if (active_application[0] == '\0') return UMI_STATUS_OK;
    if (strcmp(centre->last_application_id, active_application) == 0) {
        return UMI_STATUS_OK;
    }

    catalogue = umi_desk_runtime_applications(
        umi_desktop_module_desk_runtime(module));
    if (catalogue != NULL &&
        umi_application_runtime_catalogue_find(
            catalogue, active_application, &record) == UMI_STATUS_OK) {
        state_text = umi_application_runtime_state_text(record.state);
        taskbar_group = record.taskbar_group;
        layout_id = record.default_layout_id;
    }

    group_id = group_for_application(active_application);
    status = umi_workbench_context_host_set_active_group(
        centre->host, group_id);
    if (status != UMI_STATUS_OK) return status;

    written = snprintf(
        context_id,
        sizeof(context_id),
        "desktop-application-%llu",
        (unsigned long long)centre->publication_sequence++);
    if (written < 0 || (size_t)written >= sizeof(context_id)) {
        return UMI_STATUS_CAPACITY_EXCEEDED;
    }

    status = umi_workbench_context_host_publish_application(
        centre->host,
        group_id,
        "desktop.launcher",
        context_id,
        active_application,
        state_text,
        taskbar_group,
        layout_id,
        now_ms);
    if (status != UMI_STATUS_OK) return status;

    status = umi_workbench_context_host_copy_text(
        centre->last_application_id,
        sizeof(centre->last_application_id),
        active_application);
    if (status == UMI_STATUS_OK) ++centre->revision;
    return status;
}

UmiWorkbenchContextHost *umi_desktop_context_link_centre_host(
    UmiDesktopContextLinkCentre *centre)
{
    return centre != NULL ? centre->host : NULL;
}
