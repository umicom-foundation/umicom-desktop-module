/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: src/gtk/desktop_window.h
 * Purpose: Expose the thin native composition to its unpresented C regression.
 * Author: Sammy Hegab, Umicom Foundation
 * Licence: MIT
 *---------------------------------------------------------------------------*/
#ifndef UMICOM_DESKTOP_PRIVATE_GTK_WINDOW_H
#define UMICOM_DESKTOP_PRIVATE_GTK_WINDOW_H

#include <gtk/gtk.h>
#include "umicom/desktop_module/context_link_centre.h"
#include "umicom/desktop_module/desktop_module.h"
#include "umicom/ui/gtk4/desk.h"
#include "umicom/ui/gtk4/workstation/shell_header.h"

/* Product composition owns its controllers; executable_root is borrowed from
 * the entry point. context_root remains strongly held until callback cleanup. */
typedef struct UmiDesktopGtkRun {
    UmiDesktopModule *module;
    UmiGtk4Desk *desk;
    UmiDesktopContextLinkCentre *context_links;
    UmiGtk4WorkstationShellHeader *identity;
    UmiGtk4WorkstationWindowTitlebar *titlebar;
    GtkWidget *context_strip;
    GtkWidget *context_root;
    char *executable_root;
    /* Optional borrowed Framework configuration for native host composition.
     * NULL uses the real file probe. Tests inject evidence without disk writes;
     * any supplied root/suffix must match the Framework launcher location. */
    const UmiApplicationNativeDiscoveryConfig *discovery_config;
    guint poll_source_id;
} UmiDesktopGtkRun;

/* Prepare the actual product without presenting, scheduling a poll timer or
 * launching a child process. Dispose safely after success or partial failure. */
UmiStatus umi_desktop_gtk_window_prepare(GtkApplication *application, UmiDesktopGtkRun *run);
/* Reconcile the existing services once; production schedules this callback. */
gboolean umi_desktop_gtk_window_poll(gpointer user_data);
/* Cancel polling and disconnect native callbacks before releasing services. */
void umi_desktop_gtk_window_dispose(UmiDesktopGtkRun *run);

#endif
