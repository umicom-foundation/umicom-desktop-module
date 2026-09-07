/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: src/desktop_module.c
 *
 * PURPOSE:
 *   Compose Framework application, desktop, layout and process services into
 *   the Umicom Desk product profile.
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
#include <stdlib.h>
#include <string.h>

#include "umicom/application/context_hub.h"
#include "umicom/application/portfolio.h"
#include "umicom/application/presentation.h"
#include "umicom/desktop/runtime.h"
#include "umicom/platform/process_supervisor.h"

#define UMI_DESKTOP_MODULE_MAX_PROCESSES 64U

#ifndef UMICOM_DESKTOP_COMPOSE_STUDIO
#define UMICOM_DESKTOP_COMPOSE_STUDIO 1
#endif
#ifndef UMICOM_DESKTOP_COMPOSE_TRADER
#define UMICOM_DESKTOP_COMPOSE_TRADER 0
#endif
#ifndef UMICOM_DESKTOP_COMPOSE_BANK
#define UMICOM_DESKTOP_COMPOSE_BANK 0
#endif
#ifndef UMICOM_DESKTOP_COMPOSE_TMS
#define UMICOM_DESKTOP_COMPOSE_TMS 0
#endif
#ifndef UMICOM_DESKTOP_COMPOSE_OS
#define UMICOM_DESKTOP_COMPOSE_OS 1
#endif
#ifndef UMICOM_DESKTOP_STUDIO_EXECUTABLE
#define UMICOM_DESKTOP_STUDIO_EXECUTABLE "umicom-studio-ide"
#endif
#ifndef UMICOM_DESKTOP_TRADER_EXECUTABLE
#define UMICOM_DESKTOP_TRADER_EXECUTABLE "umicom-trader"
#endif
#ifndef UMICOM_DESKTOP_BANK_EXECUTABLE
#define UMICOM_DESKTOP_BANK_EXECUTABLE "umicom-bank"
#endif
#ifndef UMICOM_DESKTOP_TMS_EXECUTABLE
#define UMICOM_DESKTOP_TMS_EXECUTABLE "umicom-tms"
#endif

typedef struct UmiDesktopModuleProcess {
    char application_id[UMI_APPLICATION_RUNTIME_ID_CAPACITY];
    UmiProcessJobId job_id;
    bool reconciled;
} UmiDesktopModuleProcess;

struct UmiDesktopModule {
    UmiApplicationContextHub *context_hub;
    UmiDesktopRuntime *desktop_runtime;
    UmiDesktopShellModel *shell_model;
    UmiDeskRuntime *desk_runtime;
    UmiProcessSupervisor *processes;
    UmiDesktopModuleProcess process_map[UMI_DESKTOP_MODULE_MAX_PROCESSES];
    size_t process_count;
    size_t completed_process_count;
    bool started;
    uint64_t revision;
};

/* Provide the copy text operation used by this module and its client applications. */
static UmiStatus copy_text(char *destination,
                           size_t capacity,
                           const char *source)
{
    size_t length;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (destination == NULL || capacity == 0U ||
        source == NULL || source[0] == '\0') {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    length = strlen(source);
    /* Keep the operation inside its valid bounds before reading, writing or adding data. */
    if (length >= capacity) return UMI_STATUS_CAPACITY_EXCEEDED;
    (void)memcpy(destination, source, length + 1U);
    return UMI_STATUS_OK;
}

/* Provide the process start operation used by this module and its client applications. */
static UmiStatus process_start(
    void *context,
    const UmiApplicationLaunchPlan *plan,
    uint64_t *out_process_token)
{
    UmiDesktopModule *module = (UmiDesktopModule *)context;
    const char *arguments[UMI_APPLICATION_LAUNCH_MAX_ARGUMENTS];
    UmiProcessRequest request;
    UmiProcessJobId job_id;
    UmiStatus status;
    size_t index;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL || plan == NULL || out_process_token == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    /* Keep the operation inside its valid bounds before reading, writing or adding data. */
    if (module->process_count >= UMI_DESKTOP_MODULE_MAX_PROCESSES) {
        return UMI_STATUS_CAPACITY_EXCEEDED;
    }
    /* Visit each bounded item once so every record receives the same rule. */
    for (index = 0U; index < plan->argument_count; ++index) {
        arguments[index] = plan->arguments[index];
    }

    (void)memset(&request, 0, sizeof(request));
    request.program = plan->executable_path;
    request.arguments = plan->argument_count > 0U
        ? arguments
        : NULL;
    request.argument_count = plan->argument_count;
    request.working_directory =
        plan->working_directory[0] != '\0'
            ? plan->working_directory
            : NULL;
    request.capture_stdout = 0;
    request.capture_stderr = 0;
    request.timeout_ms = 0U;
    request.poll_interval_ms = 50U;
    request.window_mode = UMI_PROCESS_WINDOW_VISIBLE;

    status = umi_process_supervisor_submit(
        module->processes,
        plan->application_id,
        &request,
        &job_id);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) return status;

    status = copy_text(
        module->process_map[module->process_count].application_id,
        sizeof(module->process_map[module->process_count].application_id),
        plan->application_id);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) {
        (void)umi_process_supervisor_cancel(module->processes, job_id);
        return status;
    }
    module->process_map[module->process_count].job_id = job_id;
    module->process_map[module->process_count].reconciled = false;
    module->process_count += 1U;
    module->revision += 1U;
    *out_process_token = job_id;
    return UMI_STATUS_OK;
}

/* Provide the process activate operation used by this module and its client applications. */
static UmiStatus process_activate(
    void *context,
    const char *application_id,
    uint64_t process_token)
{
    UmiDesktopModule *module = (UmiDesktopModule *)context;
    (void)application_id;
    (void)process_token;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;

    /*
     * Cross-process window activation will later use Framework IPC and a
     * platform window-service adapter. Until that adapter is connected, report
     * the missing capability rather than claiming the window was activated.
     * The Framework launcher keeps the tracked process; it must not start a
     * duplicate as a fallback for this unsupported operation.
     */
    return UMI_STATUS_NOT_IMPLEMENTED;
}

/* Provide the process stop operation used by this module and its client applications. */
static UmiStatus process_stop(
    void *context,
    const char *application_id,
    uint64_t process_token,
    uint32_t graceful_timeout_ms)
{
    UmiDesktopModule *module = (UmiDesktopModule *)context;
    (void)application_id;
    (void)graceful_timeout_ms;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL || process_token == 0U) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    return umi_process_supervisor_cancel(
        module->processes, (UmiProcessJobId)process_token);
}

/* Provide the make registration operation used by this module and its client applications. */
static UmiStatus make_registration(
    const char *application_id,
    const char *executable_name,
    bool installed,
    UmiApplicationRuntimeRegistration *out_registration)
{
    const UmiApplicationDefinition *definition;
    const UmiApplicationPresentation *presentation;
    UmiApplicationRuntimeRegistration registration;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (application_id == NULL || executable_name == NULL ||
        out_registration == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    definition = umi_application_portfolio_find(application_id);
    presentation = umi_application_presentation_find(application_id);
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (definition == NULL || presentation == NULL) {
        return UMI_STATUS_NOT_FOUND;
    }

    /* Product identity stays in the Framework portfolio. Desk contributes
     * only the executable that the current suite build actually produced. */
    (void)memset(&registration, 0, sizeof(registration));
    registration.structure_size = sizeof(registration);
    registration.application_id = definition->application_id;
    registration.display_name = definition->display_name;
    registration.executable_name = executable_name;
    registration.working_directory = "";
    registration.icon_resource_id = presentation->icon_resource_id;
    registration.default_layout_id = presentation->default_layout_id;
    registration.taskbar_group = presentation->taskbar_group;
    registration.family = definition->family;
    registration.maturity = definition->maturity;
    registration.entry_kind = presentation->entry_kind;
    registration.installed = installed;
    registration.compatible = installed;
    registration.enabled = installed;
    registration.pinned = installed && presentation->pinned_by_default;
    registration.visible_when_unavailable =
        presentation->visible_when_unavailable;
    *out_registration = registration;
    return UMI_STATUS_OK;
}

/*
 * Provide the desktop module config default operation used by this module and its client
 * applications.
 */
UmiDesktopModuleConfig umi_desktop_module_config_default(void)
{
    UmiDesktopModuleConfig config;
    (void)memset(&config, 0, sizeof(config));
    config.structure_size = sizeof(config);
    config.executable_root = "";
    config.working_directory = "";
    config.compose_studio = UMICOM_DESKTOP_COMPOSE_STUDIO != 0;
    config.compose_trader = UMICOM_DESKTOP_COMPOSE_TRADER != 0;
    config.compose_bank = UMICOM_DESKTOP_COMPOSE_BANK != 0;
    config.compose_tms = UMICOM_DESKTOP_COMPOSE_TMS != 0;
    config.compose_os_control_centre = UMICOM_DESKTOP_COMPOSE_OS != 0;
    return config;
}

/*
 * Initialise desktop module from caller-provided values so later operations receive a
 * known state.
 */
UmiStatus umi_desktop_module_create(
    const UmiDesktopModuleConfig *config,
    UmiDesktopModule **out_module)
{
    UmiDesktopModuleConfig effective;
    UmiDesktopModule *module;
    UmiProcessSupervisorConfig process_config;
    UmiApplicationLauncherAdapter launch_adapter;
    UmiDeskRuntimeConfig desk_config;
    UmiApplicationRuntimeRegistration registration;
    UmiStatus status;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (out_module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    *out_module = NULL;
    effective = config != NULL
        ? *config
        : umi_desktop_module_config_default();
    /* Apply this branch only when its contract condition is satisfied. */
    if (effective.structure_size < sizeof(UmiDesktopModuleConfig)) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }

    module = (UmiDesktopModule *)calloc(1U, sizeof(*module));
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL) return UMI_STATUS_OUT_OF_MEMORY;
    module->revision = 1U;

    process_config = umi_process_supervisor_config_default();
    /* Match the Framework catalogue so every selected product can be
     * supervised without an unrelated lower process limit. */
    process_config.capacity = UMI_DESKTOP_MODULE_MAX_PROCESSES;
    status = umi_application_context_hub_create(&module->context_hub);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_runtime_create(
            module->context_hub, &module->desktop_runtime);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_runtime_seed(module->desktop_runtime);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_shell_model_create(
            module->desktop_runtime, &module->shell_model);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_process_supervisor_create(
            &process_config, &module->processes);
    }

    (void)memset(&launch_adapter, 0, sizeof(launch_adapter));
    launch_adapter.structure_size = sizeof(launch_adapter);
    launch_adapter.adapter_context = module;
    launch_adapter.start = process_start;
    launch_adapter.activate = process_activate;
    launch_adapter.stop = process_stop;

    desk_config = umi_desk_runtime_config_default();
    desk_config.seed_framework_portfolio = true;
    desk_config.launcher.executable_root =
        effective.executable_root != NULL
            ? effective.executable_root : "";
    desk_config.launcher.default_working_directory =
        effective.working_directory != NULL
            ? effective.working_directory : "";
#ifdef _WIN32
    desk_config.launcher.executable_suffix = ".exe";
#else
    desk_config.launcher.executable_suffix = "";
#endif
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_create(
            module->shell_model,
            &desk_config,
            &launch_adapter,
            &module->desk_runtime);
    }

    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = make_registration(
            "org.umicom.desktop", "umicom-desk", true, &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = make_registration(
            "org.umicom.studio",
            UMICOM_DESKTOP_STUDIO_EXECUTABLE,
            effective.compose_studio,
            &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = make_registration(
            "org.umicom.trader",
            UMICOM_DESKTOP_TRADER_EXECUTABLE,
            effective.compose_trader,
            &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = make_registration(
            "org.umicom.bank",
            UMICOM_DESKTOP_BANK_EXECUTABLE,
            effective.compose_bank,
            &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = make_registration(
            "org.umicom.tms",
            UMICOM_DESKTOP_TMS_EXECUTABLE,
            effective.compose_tms,
            &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = make_registration(
            "org.umicom.os",
            "umicom-os-control-centre",
            effective.compose_os_control_centre,
            &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        UmiApplicationRuntimeCatalogue *catalogue =
            umi_desk_runtime_applications(module->desk_runtime);
        status = umi_application_runtime_catalogue_set_state(
            catalogue, "org.umicom.desktop",
            UMI_APPLICATION_RUNTIME_RUNNING, "");
        /* Preserve the original failure result so the caller can respond to the correct cause. */
        if (status == UMI_STATUS_OK) {
            status = umi_application_runtime_catalogue_activate(
                catalogue, "org.umicom.desktop");
        }
        /* Preserve the original failure result so the caller can respond to the correct cause. */
        if (status == UMI_STATUS_OK) {
            status = umi_desk_runtime_refresh(module->desk_runtime);
        }
    }

    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) {
        umi_desktop_module_destroy(module);
        return status;
    }
    *out_module = module;
    return UMI_STATUS_OK;
}

/* Release or reset state held by desktop module so the same storage can be reused safely. */
void umi_desktop_module_destroy(UmiDesktopModule *module)
{
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL) return;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module->processes != NULL) {
        umi_process_supervisor_destroy(module->processes);
    }
    umi_desk_runtime_destroy(module->desk_runtime);
    umi_desktop_shell_model_destroy(module->shell_model);
    umi_desktop_runtime_destroy(module->desktop_runtime);
    umi_application_context_hub_destroy(module->context_hub);
    free(module);
}

/*
 * Provide the desktop module start operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_start(UmiDesktopModule *module)
{
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (module->started) return UMI_STATUS_OK;
    module->started = true;
    module->revision += 1U;
    return UMI_STATUS_OK;
}

/*
 * Provide the desktop module stop operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_stop(UmiDesktopModule *module)
{
    UmiStatus status;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (!module->started) return UMI_STATUS_OK;
    status = umi_process_supervisor_shutdown(module->processes);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) return status;
    module->started = false;
    module->revision += 1U;
    return UMI_STATUS_OK;
}

/*
 * Provide the desktop module poll operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_poll(UmiDesktopModule *module)
{
    size_t index;
    UmiStatus status = UMI_STATUS_OK;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    /* Visit each bounded item once so every record receives the same rule. */
    for (index = 0U; index < module->process_count; ++index) {
        UmiDesktopModuleProcess *mapping = &module->process_map[index];
        UmiProcessJobSnapshot process;
        int exit_code;
        const char *message;
        /* Apply this branch only when its contract condition is satisfied. */
        if (mapping->reconciled) continue;
        status = umi_process_supervisor_snapshot(
            module->processes, mapping->job_id, &process);
        /* Preserve the original failure result so the caller can respond to the correct cause. */
        if (status != UMI_STATUS_OK) return status;
        /* Apply this branch only when its contract condition is satisfied. */
        if (process.state == UMI_PROCESS_JOB_CREATED ||
            process.state == UMI_PROCESS_JOB_RUNNING) {
            continue;
        }
        exit_code = process.state == UMI_PROCESS_JOB_SUCCEEDED
            ? 0
            : (process.exit_code != 0 ? process.exit_code : 1);
        message = process.output[0] != '\0'
            ? process.output
            : umi_process_job_state_text(process.state);
        status = umi_desk_runtime_reconcile_application_exit(
            module->desk_runtime,
            mapping->application_id,
            exit_code,
            message);
        /* Preserve the original failure result so the caller can respond to the correct cause. */
        if (status != UMI_STATUS_OK) return status;
        mapping->reconciled = true;
        module->completed_process_count += 1U;
        module->revision += 1U;
    }
    return UMI_STATUS_OK;
}

/*
 * Provide the desktop module snapshot operation used by this module and its client
 * applications.
 */
UmiStatus umi_desktop_module_snapshot(
    const UmiDesktopModule *module,
    UmiDesktopModuleSnapshot *out_snapshot)
{
    UmiStatus status;
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (module == NULL || out_snapshot == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));
    status = umi_desk_runtime_snapshot(
        module->desk_runtime, &out_snapshot->desk);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) return status;
    out_snapshot->started = module->started;
    out_snapshot->supervised_process_count = module->process_count;
    out_snapshot->completed_process_count =
        module->completed_process_count;
    out_snapshot->revision = module->revision;
    return UMI_STATUS_OK;
}

/* Join the current Desk choices to Framework guidance without executing them. */
UmiStatus umi_desktop_module_guided_launch_plan(
    UmiDesktopModule *module,
    const UmiProductWorkspaceGuidePortfolio *portfolio,
    UmiProductGuidedLaunchPlan *out_plan)
{
    UmiApplicationLaunchSelection *selection;

    /* Desk, suite guidance and destination storage are all required inputs. */
    if (module == NULL || portfolio == NULL || out_plan == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }

    selection = umi_desk_runtime_launch_selection(module->desk_runtime);
    /* A missing selection indicates an incomplete or damaged Desk runtime. */
    if (selection == NULL) {
        return UMI_STATUS_INVALID_STATE;
    }

    /* Framework owns the join, validation and beginner-readable explanations. */
    return umi_product_guided_launch_plan_build(
        selection, portfolio, out_plan);
}

/*
 * Provide the desktop module desk runtime operation used by this module and its client
 * applications.
 */
UmiDeskRuntime *umi_desktop_module_desk_runtime(
    UmiDesktopModule *module)
{
    return module != NULL ? module->desk_runtime : NULL;
}

/*
 * Provide the desktop module shell model operation used by this module and its client
 * applications.
 */
UmiDesktopShellModel *umi_desktop_module_shell_model(
    UmiDesktopModule *module)
{
    return module != NULL ? module->shell_model : NULL;
}
