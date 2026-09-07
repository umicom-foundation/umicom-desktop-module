/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: src/gtk/main.c
 *
 * PURPOSE:
 *   Start Umicom Desk with Framework-owned launcher services and the reusable
 *   cross-application context-link strip.
 *
 * AUTHOR AND ORGANISATION:
 * Sammy Hegab
 * Umicom Foundation
 *
 * LICENCE:
 * MIT
 *---------------------------------------------------------------------------*/
#include <gtk/gtk.h>

#include "umicom/desktop_module/context_link_centre.h"
#include "umicom/desktop_module/desktop_module.h"
#include "umicom/ui/appearance_catalogue.h"
#include "umicom/platform/filesystem.h"
#include "umicom/ui/gtk4/desk.h"
#include "umicom/ui/gtk4/workstation/shell_header.h"
#include "umicom/workbench_context_host/gtk4.h"
#include "desktop_window.h"

/*
 * Provide the attach context strip operation used by this module and its client
 * applications.
 */
static UmiStatus attach_context_strip(UmiDesktopGtkRun *run)
{
    GtkWindow *window;
    GtkWidget *existing;
    GtkWidget *root;
    UmiGtk4WorkstationShellHeaderConfig identity_config;
    UmiUiAppearanceProfile appearance;
    UmiStatus status;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run == NULL || run->desk == NULL || run->context_links == NULL) {
        return UMI_STATUS_INVALID_ARGUMENT;
    }

    window = GTK_WINDOW(umi_gtk4_desk_native_window(run->desk));
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (window == NULL) return UMI_STATUS_INVALID_STATE;

    existing = gtk_window_get_child(window);
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (existing != NULL) g_object_ref(existing);

    root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (root == NULL) {
        /*
         * Protect caller-owned memory by checking that required state is available before it is
         * used.
         */
        if (existing != NULL) g_object_unref(existing);
        return UMI_STATUS_OUT_OF_MEMORY;
    }
    /* Keep context callbacks reachable even after an external native close
     * removes this body from its window before product disposal. */
    g_object_ref_sink(root);

    /* Desk uses the same Framework identity component as product workstations.
     * The executable directory contains the SVG files staged by packaging. */
    identity_config = umi_gtk4_ws_shell_header_config_default(
        "org.umicom.desktop", "Umicom Desk");
    identity_config.subtitle = "Applications and workspaces";
    identity_config.compact = true;
    identity_config.resource_root = run->executable_root;
    status = umi_gtk4_ws_shell_header_create_managed(
        &identity_config, &run->identity);
    /* The topmost picker uses the same Framework runtime as Desktop Home.
     * It must not start a separate PATH-resolved process behind that owner. */
    if (status == UMI_STATUS_OK)
        status = umi_gtk4_desk_bind_shell_header(run->desk, run->identity);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) {
        /*
         * Protect caller-owned memory by checking that required state is available before it is
         * used.
         */
        if (existing != NULL) g_object_unref(existing);
        umi_gtk4_ws_shell_header_destroy(run->identity);
        run->identity = NULL;
        g_object_unref(root);
        return status;
    }
    /* Apply this branch only when its contract condition is satisfied. */
    if (umi_ui_appearance_catalogue_find(
            "umicom-dark", &appearance) == UMI_STATUS_OK) {
        (void)umi_gtk4_ws_shell_header_apply_appearance(
            run->identity, &appearance);
    }

    run->context_strip = umi_workbench_context_host_gtk4_strip_new(
        umi_desktop_context_link_centre_host(run->context_links));
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run->context_strip == NULL) {
        /*
         * Protect caller-owned memory by checking that required state is available before it is
         * used.
         */
        if (existing != NULL) g_object_unref(existing);
        umi_gtk4_ws_shell_header_destroy(run->identity);
        run->identity = NULL;
        g_object_unref(root);
        return UMI_STATUS_OUT_OF_MEMORY;
    }

    gtk_widget_add_css_class(root, "umicom-desk-context-root");
    gtk_box_append(GTK_BOX(root), run->context_strip);

    /* Transfer the existing identity and its application controls into the
     * real titlebar. Its appearance/selection state is preserved and no second
     * identity remains in the workbench content. Ownership moves on success. */
    status = umi_gtk4_ws_window_titlebar_create_from_header(
        window, run->identity, "Umicom Desk", &run->titlebar);
    if (status != UMI_STATUS_OK) {
        umi_workbench_context_host_gtk4_invalidate(root,
            umi_desktop_context_link_centre_host(run->context_links));
        run->context_strip = NULL;
        if (existing != NULL) g_object_unref(existing);
        umi_gtk4_ws_shell_header_destroy(run->identity);
        run->identity = NULL;
        g_object_unref(root);
        return status;
    }
    run->identity = NULL;
    (void)umi_gtk4_desk_set_content_identity_visible(run->desk, false);

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (existing != NULL) {
        gtk_window_set_child(window, NULL);
        gtk_widget_set_hexpand(existing, TRUE);
        gtk_widget_set_vexpand(existing, TRUE);
        gtk_box_append(GTK_BOX(root), existing);
        g_object_unref(existing);
    }

    gtk_window_set_child(window, root);
    run->context_root = root;
    return UMI_STATUS_OK;
}

/* Provide the poll processes operation used by this module and its client applications. */
gboolean umi_desktop_gtk_window_poll(gpointer user_data)
{
    UmiDesktopGtkRun *run = (UmiDesktopGtkRun *)user_data;
    UmiApplicationNativeDiscoveryReport discovery = {0};
    UmiStatus status;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run == NULL) return G_SOURCE_REMOVE;
    if (run->module == NULL) {
        run->poll_source_id = 0U;
        return G_SOURCE_REMOVE;
    }

    status = umi_desktop_module_poll(run->module);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) {
        g_printerr(
            "Umicom Desk process reconciliation failed: %s\n",
            umi_status_text(status));
    }

    /* The Framework owns timing, probing and admission. This native callback
     * supplies its clock only; it never scans directories or starts products. */
    status = umi_desk_runtime_poll_native_discovery(
        umi_desktop_module_desk_runtime(run->module),
        (uint64_t)(g_get_monotonic_time() / 1000), false, &discovery);
    if (status != UMI_STATUS_OK && !discovery.skipped) {
        g_printerr("Umicom Desk installation refresh failed: %s\n",
            umi_status_text(status));
    }

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run->context_links != NULL) {
        status = umi_desktop_context_link_centre_refresh(
            run->context_links,
            run->module,
            (uint64_t)(g_get_monotonic_time() / 1000));
        /* Preserve the original failure result so the caller can respond to the correct cause. */
        if (status != UMI_STATUS_OK) {
            g_printerr(
                "Umicom Desk context refresh failed: %s\n",
                umi_status_text(status));
        }
    }

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run->desk != NULL) {
        (void)umi_gtk4_desk_refresh(run->desk);
    }
    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run->context_strip != NULL && run->context_links != NULL) {
        (void)umi_workbench_context_host_gtk4_strip_refresh(
            run->context_strip,
            umi_desktop_context_link_centre_host(run->context_links));
    }

    return G_SOURCE_CONTINUE;
}

/* Build the actual product graph while keeping first presentation with the
 * entry point. Tests use the same path without presentation or a polling timer. */
UmiStatus umi_desktop_gtk_window_prepare(GtkApplication *application, UmiDesktopGtkRun *run)
{
    UmiDesktopModuleConfig config = umi_desktop_module_config_default();
    UmiStatus status;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (application == NULL || !GTK_IS_APPLICATION(application) || run == NULL)
        return UMI_STATUS_INVALID_ARGUMENT;
    if (run->module != NULL) return UMI_STATUS_INVALID_STATE;
    config.executable_root = run->executable_root;
    config.working_directory = run->executable_root;
    status = umi_desktop_module_create(&config, &run->module);
    /* Opt in once on this fresh native composition. Headless callers and
     * unpresented fixtures without a root retain their explicit test policy.
     * Framework supplies every product identity and all discovery behaviour. */
    if (status == UMI_STATUS_OK && run->executable_root != NULL &&
        run->executable_root[0] != '\0') {
        UmiDeskRuntime *runtime = umi_desktop_module_desk_runtime(run->module);
        UmiApplicationNativeDiscoveryConfig discovery =
            run->discovery_config != NULL ? *run->discovery_config :
                umi_application_native_discovery_config_default();
        UmiApplicationNativeDiscoveryReport report;
        if (run->discovery_config == NULL)
            discovery.executable_root = run->executable_root;
        status = umi_desk_runtime_admit_native_portfolio(runtime);
        if (status == UMI_STATUS_OK)
            status = umi_desk_runtime_configure_native_discovery(runtime, &discovery);
        if (status == UMI_STATUS_OK)
            status = umi_desk_runtime_poll_native_discovery(runtime,
                (uint64_t)(g_get_monotonic_time() / 1000), true, &report);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_start(run->module);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_gtk4_desk_create(
            application,
            umi_desktop_module_desk_runtime(run->module),
            &run->desk);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_context_link_centre_create(
            &run->context_links);
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_context_link_centre_refresh(
            run->context_links,
            run->module,
            (uint64_t)(g_get_monotonic_time() / 1000));
    }
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status == UMI_STATUS_OK) {
        status = attach_context_strip(run);
    }

    return status;
}

/* Cancel asynchronous work before native widgets or their borrowed services
 * are released. This also supports a partially prepared startup graph. */
void umi_desktop_gtk_window_dispose(UmiDesktopGtkRun *run)
{
    if (run == NULL) return;
    if (run->poll_source_id != 0U) {
        (void)g_source_remove(run->poll_source_id);
        run->poll_source_id = 0U;
    }
    if (run->context_links != NULL)
        umi_workbench_context_host_gtk4_invalidate(run->context_root,
            umi_desktop_context_link_centre_host(run->context_links));
    /* Release native identity callbacks before the Desk-owned window. An
     * identity left by a failed transfer is still owned directly by this run. */
    umi_gtk4_ws_window_titlebar_destroy(run->titlebar);
    run->titlebar = NULL;
    umi_gtk4_ws_shell_header_destroy(run->identity);
    run->identity = NULL;
    umi_gtk4_desk_destroy(run->desk);
    run->desk = NULL;
    if (run->context_root != NULL) g_object_unref(run->context_root);
    run->context_root = NULL;
    run->context_strip = NULL;
    umi_desktop_context_link_centre_destroy(run->context_links);
    run->context_links = NULL;
    if (run->module != NULL) (void)umi_desktop_module_stop(run->module);
    umi_desktop_module_destroy(run->module);
    run->module = NULL;
}

#ifndef UMICOM_DESKTOP_GTK_NO_ENTRYPOINT
/* Native activation presents only after identity and context composition are
 * complete, avoiding a flash of the old in-content header during startup. */
static void on_activate(GtkApplication *application, gpointer user_data)
{
    UmiDesktopGtkRun *run = user_data;
    UmiStatus status;
    if (run == NULL) return;
    if (run->module != NULL) {
        if (run->desk != NULL) (void)umi_gtk4_desk_present(run->desk);
        return;
    }
    status = umi_desktop_gtk_window_prepare(application, run);
    if (status == UMI_STATUS_OK) status = umi_gtk4_desk_present(run->desk);
    if (status != UMI_STATUS_OK) {
        g_printerr(
            "Umicom Desk startup failed: %s\n",
            umi_status_text(status));
        g_application_quit(G_APPLICATION(application));
        return;
    }
    run->poll_source_id = g_timeout_add(250U, umi_desktop_gtk_window_poll, run);
}

/*
 * Start this command or application, report setup failures, and return a process exit code
 * to the operating system.
 */
int main(int argc, char **argv)
{
    GtkApplication *application;
    UmiDesktopGtkRun run = {0};
    char program_path[UMI_PATH_CAPACITY];
    char installation_root[UMI_PATH_CAPACITY];
    UmiStatus path_status;
    int result;

    /* The packaged desktop entry and executable use this stable program name
     * to select the shared Umicom icon rather than the toolkit fallback. */
    g_set_prgname("umicom-desk");

    /* Derive installation evidence from the actual process image through
     * Framework. A shortcut or PATH launch must not turn the working directory
     * into an implicit application search root. */
    path_status = umi_fs_executable_path(program_path, sizeof(program_path));
    if (path_status == UMI_STATUS_OK)
        path_status = umi_fs_parent(installation_root, sizeof(installation_root), program_path);
    if (path_status != UMI_STATUS_OK) {
        g_printerr("Cannot locate the Umicom Desk installation: %s\n",
            umi_status_text(path_status));
        return 1;
    }
    run.executable_root = g_strdup(installation_root);
    if (run.executable_root == NULL) return 1;

    application = gtk_application_new(
        "org.umicom.desktop",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(
        application,
        "activate",
        G_CALLBACK(on_activate),
        &run);
    result = g_application_run(
        G_APPLICATION(application),
        argc,
        argv);

    /*
     * Destroy the GTK window and its signal closures before releasing the
     * toolkit-neutral host borrowed by the context-strip callbacks.
     */
    umi_desktop_gtk_window_dispose(&run);
    g_object_unref(application);
    g_free(run.executable_root);
    return result;
}

#ifdef _WIN32
#include <windows.h>

/* Provide the win main operation used by this module and its client applications. */
int WINAPI WinMain(
    HINSTANCE instance,
    HINSTANCE previous_instance,
    LPSTR command_line,
    int show_command)
{
    (void)instance;
    (void)previous_instance;
    (void)command_line;
    (void)show_command;
    return main(__argc, __argv);
}
#endif
#endif /* UMICOM_DESKTOP_GTK_NO_ENTRYPOINT */
