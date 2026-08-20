/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: src/desktop_module.c
 *
 * PURPOSE:
 *   Compose Framework application, desktop, layout and process services into
 *   the Umicom Desk product profile.
 *
 * Created by: Sammy Hegab
 * Organisation: Umicom Foundation
 * Licence: MIT
 *---------------------------------------------------------------------------*/
#include "umicom/desktop_module/desktop_module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "umicom/application/context_hub.h"
#include "umicom/desktop/runtime.h"
#include "umicom/platform/process_supervisor.h"

#define UMI_DESKTOP_MODULE_MAX_PROCESSES 64U

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

static UmiStatus copy_text(char *destination,
                           size_t capacity,
                           const char *source)
{
    size_t length;
    if (destination == NULL || capacity == 0U ||
        source == NULL || source[0] == '\0') {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    length = strlen(source);
    if (length >= capacity) return UMI_STATUS_CAPACITY_EXCEEDED;
    (void)memcpy(destination, source, length + 1U);
    return UMI_STATUS_OK;
}

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

    if (module == NULL || plan == NULL || out_process_token == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    if (module->process_count >= UMI_DESKTOP_MODULE_MAX_PROCESSES) {
        return UMI_STATUS_CAPACITY_EXCEEDED;
    }
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
    if (status != UMI_STATUS_OK) return status;

    status = copy_text(
        module->process_map[module->process_count].application_id,
        sizeof(module->process_map[module->process_count].application_id),
        plan->application_id);
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

static UmiStatus process_activate(
    void *context,
    const char *application_id,
    uint64_t process_token)
{
    UmiDesktopModule *module = (UmiDesktopModule *)context;
    (void)application_id;
    (void)process_token;
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;

    /*
     * Cross-process window activation will later use Framework IPC and a
     * platform window-service adapter. Treating the already-running process as
     * active is deterministic and avoids GTK- or Win32-specific policy here.
     */
    module->revision += 1U;
    return UMI_STATUS_OK;
}

static UmiStatus process_stop(
    void *context,
    const char *application_id,
    uint64_t process_token,
    uint32_t graceful_timeout_ms)
{
    UmiDesktopModule *module = (UmiDesktopModule *)context;
    (void)application_id;
    (void)graceful_timeout_ms;
    if (module == NULL || process_token == 0U) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    return umi_process_supervisor_cancel(
        module->processes, (UmiProcessJobId)process_token);
}

static UmiApplicationRuntimeRegistration make_registration(
    const char *application_id,
    const char *display_name,
    const char *executable_name,
    const char *icon_resource_id,
    const char *default_layout_id,
    const char *taskbar_group,
    UmiApplicationFamily family,
    UmiApplicationEntryKind entry_kind,
    bool installed,
    bool pinned)
{
    UmiApplicationRuntimeRegistration registration;
    (void)memset(&registration, 0, sizeof(registration));
    registration.structure_size = sizeof(registration);
    registration.application_id = application_id;
    registration.display_name = display_name;
    registration.executable_name = executable_name;
    registration.working_directory = "";
    registration.icon_resource_id = icon_resource_id;
    registration.default_layout_id = default_layout_id;
    registration.taskbar_group = taskbar_group;
    registration.family = family;
    registration.maturity = installed
        ? UMI_APPLICATION_AVAILABLE
        : UMI_APPLICATION_FOUNDATION;
    registration.entry_kind = entry_kind;
    registration.installed = installed;
    registration.compatible = installed;
    registration.enabled = installed;
    registration.pinned = pinned;
    registration.visible_when_unavailable = false;
    return registration;
}

UmiDesktopModuleConfig umi_desktop_module_config_default(void)
{
    UmiDesktopModuleConfig config;
    (void)memset(&config, 0, sizeof(config));
    config.structure_size = sizeof(config);
    config.executable_root = "";
    config.working_directory = "";
    config.compose_studio = true;
    config.compose_os_control_centre = true;
    return config;
}

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

    if (out_module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    *out_module = NULL;
    effective = config != NULL
        ? *config
        : umi_desktop_module_config_default();
    if (effective.structure_size < sizeof(UmiDesktopModuleConfig)) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }

    module = (UmiDesktopModule *)calloc(1U, sizeof(*module));
    if (module == NULL) return UMI_STATUS_OUT_OF_MEMORY;
    module->revision = 1U;

    process_config = umi_process_supervisor_config_default();
    process_config.capacity = 16U;
    status = umi_application_context_hub_create(&module->context_hub);
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_runtime_create(
            module->context_hub, &module->desktop_runtime);
    }
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_runtime_seed(module->desktop_runtime);
    }
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_shell_model_create(
            module->desktop_runtime, &module->shell_model);
    }
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
    if (status == UMI_STATUS_OK) {
        status = umi_desk_runtime_create(
            module->shell_model,
            &desk_config,
            &launch_adapter,
            &module->desk_runtime);
    }

    if (status == UMI_STATUS_OK) {
        registration = make_registration(
            "org.umicom.desktop",
            "Umicom Desk",
            "umicom-desk",
            "umicom.icon.application.desktop",
            "mosaic",
            "system",
            UMI_APPLICATION_FAMILY_PLATFORM,
            UMI_APPLICATION_ENTRY_SYSTEM,
            true,
            true);
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    if (status == UMI_STATUS_OK) {
        registration = make_registration(
            "org.umicom.studio",
            "Umicom Studio IDE",
            "umicom-studio-ide",
            "umicom.icon.application.studio",
            "develop",
            "development",
            UMI_APPLICATION_FAMILY_DEVELOPMENT,
            UMI_APPLICATION_ENTRY_WORKBENCH,
            effective.compose_studio,
            effective.compose_studio);
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    if (status == UMI_STATUS_OK) {
        registration = make_registration(
            "org.umicom.os",
            "Umicom OS Control Centre",
            "umicom-os-control-centre",
            "umicom.icon.application.os",
            "system",
            "system",
            UMI_APPLICATION_FAMILY_OPERATING_SYSTEM,
            UMI_APPLICATION_ENTRY_SYSTEM,
            effective.compose_os_control_centre,
            effective.compose_os_control_centre);
        status = umi_desk_runtime_upsert_application(
            module->desk_runtime, &registration);
    }
    if (status == UMI_STATUS_OK) {
        UmiApplicationRuntimeCatalogue *catalogue =
            umi_desk_runtime_applications(module->desk_runtime);
        status = umi_application_runtime_catalogue_set_state(
            catalogue, "org.umicom.desktop",
            UMI_APPLICATION_RUNTIME_RUNNING, "");
        if (status == UMI_STATUS_OK) {
            status = umi_application_runtime_catalogue_activate(
                catalogue, "org.umicom.desktop");
        }
        if (status == UMI_STATUS_OK) {
            status = umi_desk_runtime_refresh(module->desk_runtime);
        }
    }

    if (status != UMI_STATUS_OK) {
        umi_desktop_module_destroy(module);
        return status;
    }
    *out_module = module;
    return UMI_STATUS_OK;
}

void umi_desktop_module_destroy(UmiDesktopModule *module)
{
    if (module == NULL) return;
    if (module->processes != NULL) {
        umi_process_supervisor_destroy(module->processes);
    }
    umi_desk_runtime_destroy(module->desk_runtime);
    umi_desktop_shell_model_destroy(module->shell_model);
    umi_desktop_runtime_destroy(module->desktop_runtime);
    umi_application_context_hub_destroy(module->context_hub);
    free(module);
}

UmiStatus umi_desktop_module_start(UmiDesktopModule *module)
{
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    if (module->started) return UMI_STATUS_OK;
    module->started = true;
    module->revision += 1U;
    return UMI_STATUS_OK;
}

UmiStatus umi_desktop_module_stop(UmiDesktopModule *module)
{
    UmiStatus status;
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    if (!module->started) return UMI_STATUS_OK;
    status = umi_process_supervisor_shutdown(module->processes);
    if (status != UMI_STATUS_OK) return status;
    module->started = false;
    module->revision += 1U;
    return UMI_STATUS_OK;
}

UmiStatus umi_desktop_module_poll(UmiDesktopModule *module)
{
    size_t index;
    UmiStatus status = UMI_STATUS_OK;
    if (module == NULL) return UMI_STATUS_INVALID_ARGUMENT;
    for (index = 0U; index < module->process_count; ++index) {
        UmiDesktopModuleProcess *mapping = &module->process_map[index];
        UmiProcessJobSnapshot process;
        int exit_code;
        const char *message;
        if (mapping->reconciled) continue;
        status = umi_process_supervisor_snapshot(
            module->processes, mapping->job_id, &process);
        if (status != UMI_STATUS_OK) return status;
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
        if (status != UMI_STATUS_OK) return status;
        mapping->reconciled = true;
        module->completed_process_count += 1U;
        module->revision += 1U;
    }
    return UMI_STATUS_OK;
}

UmiStatus umi_desktop_module_snapshot(
    const UmiDesktopModule *module,
    UmiDesktopModuleSnapshot *out_snapshot)
{
    UmiStatus status;
    if (module == NULL || out_snapshot == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }
    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));
    status = umi_desk_runtime_snapshot(
        module->desk_runtime, &out_snapshot->desk);
    if (status != UMI_STATUS_OK) return status;
    out_snapshot->started = module->started;
    out_snapshot->supervised_process_count = module->process_count;
    out_snapshot->completed_process_count =
        module->completed_process_count;
    out_snapshot->revision = module->revision;
    return UMI_STATUS_OK;
}

UmiDeskRuntime *umi_desktop_module_desk_runtime(
    UmiDesktopModule *module)
{
    return module != NULL ? module->desk_runtime : NULL;
}

UmiDesktopShellModel *umi_desktop_module_shell_model(
    UmiDesktopModule *module)
{
    return module != NULL ? module->shell_model : NULL;
}
