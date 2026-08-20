/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: src/gtk/main.c
 *
 * PURPOSE:
 *   Start the GTK4 Umicom Desk shell using Framework-owned desktop and launcher
 *   services. This file contributes product startup only.
 *
 * Created by: Sammy Hegab
 * Organisation: Umicom Foundation
 * Licence: MIT
 *---------------------------------------------------------------------------*/
#include <gtk/gtk.h>

#include "umicom/desktop_module/desktop_module.h"
#include "umicom/ui/gtk4/desk.h"

typedef struct UmiDesktopGtkRun {
    UmiDesktopModule *module;
    UmiGtk4Desk *desk;
    char *executable_root;
} UmiDesktopGtkRun;

static gboolean poll_processes(gpointer user_data)
{
    UmiDesktopGtkRun *run = (UmiDesktopGtkRun *)user_data;
    if (run == NULL || run->module == NULL) return G_SOURCE_REMOVE;
    (void)umi_desktop_module_poll(run->module);
    if (run->desk != NULL) (void)umi_gtk4_desk_refresh(run->desk);
    return G_SOURCE_CONTINUE;
}

static void on_activate(GtkApplication *application, gpointer user_data)
{
    UmiDesktopGtkRun *run = (UmiDesktopGtkRun *)user_data;
    UmiDesktopModuleConfig config = umi_desktop_module_config_default();
    UmiStatus status;
    if (run == NULL || run->module != NULL) return;
    config.executable_root = run->executable_root;
    config.working_directory = run->executable_root;
    status = umi_desktop_module_create(&config, &run->module);
    if (status == UMI_STATUS_OK) {
        status = umi_desktop_module_start(run->module);
    }
    if (status == UMI_STATUS_OK) {
        status = umi_gtk4_desk_create(
            application,
            umi_desktop_module_desk_runtime(run->module),
            &run->desk);
    }
    if (status == UMI_STATUS_OK) {
        status = umi_gtk4_desk_present(run->desk);
    }
    if (status != UMI_STATUS_OK) {
        g_printerr("Umicom Desk startup failed: %s\n",
                   umi_status_text(status));
        g_application_quit(G_APPLICATION(application));
        return;
    }
    (void)g_timeout_add(250U, poll_processes, run);
}

int main(int argc, char **argv)
{
    GtkApplication *application;
    UmiDesktopGtkRun run = {0};
    char *absolute_program;
    int result;

    absolute_program = g_canonicalize_filename(
        argc > 0 ? argv[0] : ".", NULL);
    run.executable_root = absolute_program != NULL
        ? g_path_get_dirname(absolute_program)
        : g_strdup(".");

    application = gtk_application_new(
        "org.umicom.desktop",
        G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(
        application, "activate", G_CALLBACK(on_activate), &run);
    result = g_application_run(
        G_APPLICATION(application), argc, argv);

    umi_gtk4_desk_destroy(run.desk);
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

int WINAPI WinMain(HINSTANCE instance,
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
