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
#include "umicom/ui/gtk4/desk.h"
#include "umicom/ui/gtk4/workstation/shell_header.h"
#include "umicom/workbench_context_host/gtk4.h"

typedef struct UmiDesktopGtkRun {
    UmiDesktopModule *module;
    UmiGtk4Desk *desk;
    UmiDesktopContextLinkCentre *context_links;
    UmiGtk4WorkstationShellHeader *identity;
    GtkWidget *context_strip;
    GtkWidget *context_root;
    char *executable_root;
} UmiDesktopGtkRun;

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

    /* Desk uses the same Framework identity component as product workstations.
     * The executable directory contains the SVG files staged by packaging. */
    identity_config = umi_gtk4_ws_shell_header_config_default(
        "org.umicom.desktop", "Umicom Desk");
    identity_config.subtitle = "Applications and workspaces";
    identity_config.resource_root = run->executable_root;
    status = umi_gtk4_ws_shell_header_create_managed(
        &identity_config, &run->identity);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) {
        /*
         * Protect caller-owned memory by checking that required state is available before it is
         * used.
         */
        if (existing != NULL) g_object_unref(existing);
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
    gtk_box_append(
        GTK_BOX(root),
        umi_gtk4_ws_shell_header_widget(run->identity));
    gtk_box_append(GTK_BOX(root), run->context_strip);

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
static gboolean poll_processes(gpointer user_data)
{
    UmiDesktopGtkRun *run = (UmiDesktopGtkRun *)user_data;
    UmiStatus status;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run == NULL || run->module == NULL) return G_SOURCE_REMOVE;

    status = umi_desktop_module_poll(run->module);
    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) {
        g_printerr(
            "Umicom Desk process reconciliation failed: %s\n",
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

/* Provide the on activate operation used by this module and its client applications. */
static void on_activate(GtkApplication *application, gpointer user_data)
{
    UmiDesktopGtkRun *run = (UmiDesktopGtkRun *)user_data;
    UmiDesktopModuleConfig config = umi_desktop_module_config_default();
    UmiStatus status;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run == NULL || run->module != NULL) return;
    config.executable_root = run->executable_root;
    config.working_directory = run->executable_root;
    status = umi_desktop_module_create(&config, &run->module);
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
        status = umi_gtk4_desk_present(run->desk);
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

    /* Preserve the original failure result so the caller can respond to the correct cause. */
    if (status != UMI_STATUS_OK) {
        g_printerr(
            "Umicom Desk startup failed: %s\n",
            umi_status_text(status));
        g_application_quit(G_APPLICATION(application));
        return;
    }
    (void)g_timeout_add(250U, poll_processes, run);
}

/*
 * Start this command or application, report setup failures, and return a process exit code
 * to the operating system.
 */
int main(int argc, char **argv)
{
    GtkApplication *application;
    UmiDesktopGtkRun run = {0};
    char *absolute_program;
    int result;

    /* The packaged desktop entry and executable use this stable program name
     * to select the shared Umicom icon rather than the toolkit fallback. */
    g_set_prgname("umicom-desk");

    absolute_program = g_canonicalize_filename(
        argc > 0 ? argv[0] : ".", NULL);
    run.executable_root = absolute_program != NULL
        ? g_path_get_dirname(absolute_program)
        : g_strdup(".");

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
    /* Release the lightweight identity controller before its Desk-owned GTK
     * widget tree is destroyed. */
    umi_gtk4_ws_shell_header_destroy(run.identity);
    run.identity = NULL;
    umi_gtk4_desk_destroy(run.desk);
    run.desk = NULL;
    run.context_strip = NULL;
    run.context_root = NULL;

    umi_desktop_context_link_centre_destroy(run.context_links);
    run.context_links = NULL;

    /*
     * Protect caller-owned memory by checking that required state is available before it is
     * used.
     */
    if (run.module != NULL) {
        (void)umi_desktop_module_stop(run.module);
    }
    umi_desktop_module_destroy(run.module);
    g_object_unref(application);
    g_free(run.executable_root);
    g_free(absolute_program);
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
