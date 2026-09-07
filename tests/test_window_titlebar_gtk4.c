/*-----------------------------------------------------------------------------
 * Umicom Desktop Module
 * File: tests/test_window_titlebar_gtk4.c
 * Purpose: Verify the actual unpresented Desk composition and safe teardown.
 * Author: Sammy Hegab, Umicom Foundation
 * Licence: MIT
 *---------------------------------------------------------------------------*/
#include "desktop_window.h"
#include "umicom/ui/appearance_catalogue.h"

#include <stdio.h>
#include <string.h>

/* Keep checks active in release builds and release partial native fixtures. */
#define REQUIRE(expression) do { \
    if (!(expression)) { \
        (void)fprintf(stderr, "Line %d: %s\n", __LINE__, #expression); \
        failed = 1; goto cleanup; \
    } \
} while (0)

/* Match stable tags or CSS classes already supplied by the production widgets. */
static GtkWidget *find_widget(GtkWidget *widget, const char *tag, const char *css_class)
{
    GtkWidget *child;
    const char *actual;
    if (widget == NULL) return NULL;
    actual = g_object_get_data(G_OBJECT(widget), "umicom-automation-id");
    if ((tag != NULL && actual != NULL && strcmp(actual, tag) == 0) ||
        (css_class != NULL && gtk_widget_has_css_class(widget, css_class))) return widget;
    for (child = gtk_widget_get_first_child(widget); child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_widget(child, tag, css_class);
        if (found != NULL) return found;
    }
    return NULL;
}

/* Launch selection is identified by its canonical application ID, not row order. */
static GtkWidget *find_choice(GtkWidget *widget, const char *id)
{
    GtkWidget *child;
    const char *actual;
    if (widget == NULL) return NULL;
    actual = g_object_get_data(G_OBJECT(widget), "umicom-application-id");
    if (GTK_IS_CHECK_BUTTON(widget) && actual != NULL && strcmp(actual, id) == 0) return widget;
    for (child = gtk_widget_get_first_child(widget); child != NULL;
         child = gtk_widget_get_next_sibling(child)) {
        GtkWidget *found = find_choice(child, id);
        if (found != NULL) return found;
    }
    return NULL;
}

/* Count visible workbench branding, while allowing application names within
 * the actual launcher catalogue and taskbar where they remain useful content. */
static unsigned visible_inner_identity_count(GtkWidget *widget)
{
    GtkWidget *child;
    unsigned count = 0U;
    if (widget == NULL) return 0U;
    if (gtk_widget_get_visible(widget) &&
        (gtk_widget_has_css_class(widget, "umicom-workstation-header") ||
         gtk_widget_has_css_class(widget, "umicom-desk-content-identity"))) ++count;
    for (child = gtk_widget_get_first_child(widget); child != NULL;
         child = gtk_widget_get_next_sibling(child)) count += visible_inner_identity_count(child);
    return count;
}

/* Installation evidence is injected; no binaries are created, discovered or
 * launched. The real Desk poll still owns the catalogue-to-widget journey. */
typedef struct InstallationFixture {
    bool studio_present;
    size_t probe_count;
} InstallationFixture;

static UmiStatus probe_installation(const char *path, bool *present, void *context)
{
    InstallationFixture *fixture = context;
    if (path == NULL || present == NULL || fixture == NULL)
        return UMI_STATUS_INVALID_ARGUMENT;
    ++fixture->probe_count;
    *present = fixture->studio_present && strstr(path, "umicom-studio-ide") != NULL;
    return UMI_STATUS_OK;
}

/* Compile and call the same prepare/dispose functions as the product. No
 * presentation, launch button, GApplication run loop or external tool is used. */
int main(void)
{
    UmiDesktopGtkRun run = {0};
    GtkApplication *application = NULL;
    GtkWindow *window = NULL;
    GtkWidget *body;
    GtkWidget *bar;
    GtkWidget *icon;
    GtkWidget *title;
    GtkWidget *choice = NULL;
    GtkWidget *new_window = NULL;
    GtkWidget *navigation = NULL;
    GtkWidget *home_search = NULL;
    GtkWidget *control;
    GtkWindow *second_window = NULL;
    GFile *expected_file = NULL;
    GFile *actual_file = NULL;
    GdkPaintable *paintable;
    UmiUiAppearanceProfile appearance;
    UmiGtk4WorkstationWindowTitlebarSnapshot identity;
    UmiApplicationLaunchSelectionSnapshot selection;
    UmiDesktopModuleSnapshot *module_snapshot = NULL;
    UmiDeskRuntime *runtime;
    UmiDesktopShellTab navigation_layout;
    UmiDesktopShellSnapshot shell_snapshot;
    UmiGtk4Desk *desk_controller;
    UmiApplicationNativeDiscoveryConfig discovery =
        umi_application_native_discovery_config_default();
    UmiApplicationRuntimeRecord record;
    InstallationFixture installation = {0};
    GError *error = NULL;
    guint clicked_signal;
    guint poll_source;
    int failed = 0;

    (void)g_setenv("GTK_A11Y", "test", TRUE);
    (void)g_setenv("GSETTINGS_BACKEND", "memory", TRUE);
    if (!gtk_init_check()) return 77;
    /* Exercise the real native discovery composition from its first scan,
     * with the same explicit location used by its launcher and a fake probe. */
#ifdef _WIN32
    run.executable_root = "C:/umicom-native-test";
#else
    run.executable_root = "/umicom-native-test";
#endif
    discovery.executable_root = run.executable_root;
    discovery.probe = probe_installation;
    discovery.probe_context = &installation;
    installation.studio_present = true;
    run.discovery_config = &discovery;
    application = gtk_application_new("org.umicom.desktop.native-test", G_APPLICATION_NON_UNIQUE);
    REQUIRE(g_application_register(G_APPLICATION(application), NULL, &error));
    REQUIRE(umi_desktop_gtk_window_prepare(application, &run) == UMI_STATUS_OK);
    REQUIRE(run.titlebar != NULL && run.identity == NULL && run.poll_source_id == 0U);
    window = g_object_ref(GTK_WINDOW(umi_gtk4_desk_native_window(run.desk)));
    REQUIRE(!gtk_widget_get_visible(GTK_WIDGET(window)));
    REQUIRE(!gtk_widget_get_realized(GTK_WIDGET(window)));
    REQUIRE(!gtk_widget_get_mapped(GTK_WIDGET(window)));
    body = gtk_window_get_child(window);
    REQUIRE(body == run.context_root && run.context_strip != NULL);
    REQUIRE(find_widget(body, NULL, "umicom-desk-application-chooser") != NULL);
    REQUIRE(find_widget(body, NULL, "umicom-desk-global-bar") != NULL);
    REQUIRE(strcmp(umi_gtk4_desk_visible_page(run.desk), "home") == 0);
    control = find_widget(body, "desk.home.search", NULL);
    REQUIRE(GTK_IS_SEARCH_ENTRY(control));
    home_search = g_object_ref(control);
    REQUIRE(visible_inner_identity_count(body) == 0U);
    bar = gtk_window_get_titlebar(window);
    REQUIRE(GTK_IS_HEADER_BAR(bar));
    REQUIRE(bar == umi_gtk4_ws_window_titlebar_widget(run.titlebar));
    title = find_widget(bar, "workstation.identity.title", NULL);
    icon = find_widget(bar, "workstation.identity.icon", NULL);
    REQUIRE(GTK_IS_LABEL(title) && GTK_IS_PICTURE(icon));
    REQUIRE(strcmp(gtk_label_get_text(GTK_LABEL(title)), "Umicom Desk") == 0);
    REQUIRE(find_widget(body, "workstation.identity.title", NULL) == NULL);
    REQUIRE(find_widget(bar, "workstation.application.catalogue", NULL) != NULL);
    control = find_widget(bar, "workstation.application.new-window", NULL);
    REQUIRE(GTK_IS_BUTTON(control));
    new_window = g_object_ref(control);
    clicked_signal = g_signal_lookup("clicked", GTK_TYPE_BUTTON);
    REQUIRE(g_signal_has_handler_pending(new_window, clicked_signal, 0U, TRUE));

    REQUIRE(umi_ui_appearance_catalogue_find("umicom-dark", &appearance) == UMI_STATUS_OK);
    REQUIRE(g_strlcpy(appearance.icon_resource, UMICOM_TEST_BRAND_ICON_PATH,
        sizeof(appearance.icon_resource)) < sizeof(appearance.icon_resource));
    REQUIRE(umi_gtk4_ws_window_titlebar_apply_appearance(run.titlebar, &appearance) == UMI_STATUS_OK);
    identity = umi_gtk4_ws_window_titlebar_snapshot(run.titlebar);
    REQUIRE(identity.installed && identity.icon_visible);
    paintable = gtk_picture_get_paintable(GTK_PICTURE(icon));
    REQUIRE(GTK_IS_ICON_PAINTABLE(paintable));
    REQUIRE(gdk_paintable_get_intrinsic_width(paintable) > 0 &&
        gdk_paintable_get_intrinsic_width(paintable) <= 18);
    expected_file = g_file_new_for_path(UMICOM_TEST_BRAND_ICON_PATH);
    actual_file = gtk_icon_paintable_get_file(GTK_ICON_PAINTABLE(paintable));
    REQUIRE(actual_file != NULL && g_file_equal(expected_file, actual_file));

    /* Explicit presence is a fixture value, not a real executable discovery or
     * launch. Selection and refresh still pass through the real Desk runtime. */
    runtime = umi_desktop_module_desk_runtime(run.module);
    REQUIRE(umi_desk_runtime_set_application_presence(runtime,
        "org.umicom.studio", true, true, true) == UMI_STATUS_OK);
    REQUIRE(umi_desk_runtime_clear_application_selection(runtime) == UMI_STATUS_OK);
    REQUIRE(umi_gtk4_desk_refresh(run.desk) == UMI_STATUS_OK);
    control = find_choice(body, "org.umicom.studio");
    REQUIRE(GTK_IS_CHECK_BUTTON(control));
    choice = g_object_ref(control);
    desk_controller = run.desk;
    gtk_check_button_set_active(GTK_CHECK_BUTTON(choice), TRUE);
    REQUIRE(umi_application_launch_selection_snapshot(
        umi_desk_runtime_launch_selection(runtime), &selection) == UMI_STATUS_OK);
    REQUIRE(selection.selected_count == 1U);
    REQUIRE(umi_desktop_gtk_window_poll(&run) == G_SOURCE_CONTINUE);
    REQUIRE(gtk_window_get_titlebar(window) == bar && gtk_window_get_child(window) == body);
    REQUIRE(visible_inner_identity_count(body) == 0U);
    REQUIRE(g_signal_handler_find(choice, G_SIGNAL_MATCH_DATA,
        0U, 0U, NULL, NULL, desk_controller) == 0U);
    gtk_check_button_set_active(GTK_CHECK_BUTTON(choice), FALSE);
    REQUIRE(umi_application_launch_selection_snapshot(
        umi_desk_runtime_launch_selection(runtime), &selection) == UMI_STATUS_OK);
    REQUIRE(selection.selected_count == 1U);
    gtk_window_set_title(window, "Umicom Desk — Workspace Alpha");
    identity = umi_gtk4_ws_window_titlebar_snapshot(run.titlebar);
    REQUIRE(strcmp(identity.title, "Umicom Desk") == 0);
    REQUIRE(strcmp(identity.context, "Workspace Alpha") == 0);
    module_snapshot = g_new0(UmiDesktopModuleSnapshot, 1U);
    REQUIRE(umi_desktop_module_snapshot(run.module, module_snapshot) == UMI_STATUS_OK);
    REQUIRE(module_snapshot->supervised_process_count == 0U);

    /* Desk search exposes real chooser/layout operations, never an implicit
     * launch. Refresh retains the current query and the same search widget. */
    control = find_widget(body, "umicom.command.search", NULL);
    REQUIRE(GTK_IS_SEARCH_ENTRY(control));
    navigation = g_object_ref(control);
    gtk_editable_set_text(GTK_EDITABLE(navigation), "+Applications");
    g_signal_emit_by_name(navigation, "search-changed");
    g_signal_emit_by_name(navigation, "activate");
    control = find_widget(body, NULL, "umicom-desk-application-chooser");
    REQUIRE(control != NULL && GTK_IS_STACK(gtk_widget_get_parent(control)));
    REQUIRE(gtk_stack_get_visible_child(GTK_STACK(gtk_widget_get_parent(control))) == control);
    REQUIRE(umi_desktop_shell_model_tab_at(umi_desk_runtime_shell(runtime), 0U,
        &navigation_layout) == UMI_STATUS_OK);
    gtk_editable_set_text(GTK_EDITABLE(navigation), navigation_layout.layout_id);
    g_signal_emit_by_name(navigation, "search-changed");
    g_signal_emit_by_name(navigation, "activate");
    REQUIRE(umi_desktop_shell_model_snapshot(umi_desk_runtime_shell(runtime),
        &shell_snapshot) == UMI_STATUS_OK);
    REQUIRE(strcmp(shell_snapshot.active_layout_id, navigation_layout.layout_id) == 0);
    REQUIRE(umi_desktop_gtk_window_poll(&run) == G_SOURCE_CONTINUE);
    REQUIRE(find_widget(body, "umicom.command.search", NULL) == navigation);
    REQUIRE(strcmp(gtk_editable_get_text(GTK_EDITABLE(navigation)), navigation_layout.layout_id) == 0);
    REQUIRE(umi_desktop_module_snapshot(run.module, module_snapshot) == UMI_STATUS_OK);
    REQUIRE(module_snapshot->supervised_process_count == 0U);

    /* Reconfigure the already-enabled Framework monitor to reset its deadline
     * without sleeping or changing the launcher's bound installation root. */
    REQUIRE(umi_gtk4_desk_show_home(run.desk) == UMI_STATUS_OK);
    gtk_editable_set_text(GTK_EDITABLE(home_search), "Studio");
    g_signal_emit_by_name(home_search, "search-changed");
    installation.studio_present = true;
    REQUIRE(umi_desk_runtime_configure_native_discovery(runtime, &discovery) == UMI_STATUS_OK);
    REQUIRE(umi_desktop_gtk_window_poll(&run) == G_SOURCE_CONTINUE);
    REQUIRE(installation.probe_count > 0U);
    REQUIRE(umi_application_runtime_catalogue_find(umi_desk_runtime_applications(runtime),
        "org.umicom.studio", &record) == UMI_STATUS_OK);
    REQUIRE(record.installed);
    REQUIRE(find_widget(body, "desk.home.search", NULL) == home_search);
    REQUIRE(strcmp(gtk_editable_get_text(GTK_EDITABLE(home_search)), "Studio") == 0);
    REQUIRE(strcmp(umi_gtk4_desk_visible_page(run.desk), "home") == 0);

    /* Removing installation evidence must not erase a still-running process.
     * This token is model evidence only, not an operating-system process. */
    REQUIRE(umi_application_runtime_catalogue_set_process(umi_desk_runtime_applications(runtime),
        "org.umicom.studio", 1234U) == UMI_STATUS_OK);
    installation.studio_present = false;
    REQUIRE(umi_desk_runtime_configure_native_discovery(runtime, &discovery) == UMI_STATUS_OK);
    REQUIRE(umi_desktop_gtk_window_poll(&run) == G_SOURCE_CONTINUE);
    REQUIRE(umi_application_runtime_catalogue_find(umi_desk_runtime_applications(runtime),
        "org.umicom.studio", &record) == UMI_STATUS_OK);
    REQUIRE(!record.installed && record.running && record.process_token == 1234U);
    /* The current product adapter cannot bring another native window forward.
     * It must report that limit without fabricating success or spawning again. */
    REQUIRE(umi_desk_runtime_request_application(runtime, "org.umicom.studio",
        UMI_DESKTOP_APPLICATION_STRIP_LAUNCH_OR_ACTIVATE) == UMI_STATUS_NOT_IMPLEMENTED);
    REQUIRE(umi_application_runtime_catalogue_find(umi_desk_runtime_applications(runtime),
        "org.umicom.studio", &record) == UMI_STATUS_OK);
    REQUIRE(record.running && record.process_token == 1234U);
    REQUIRE(umi_desk_runtime_reconcile_application_exit(runtime,
        "org.umicom.studio", 0, "Fixture exit") == UMI_STATUS_OK);
    REQUIRE(umi_desktop_module_snapshot(run.module, module_snapshot) == UMI_STATUS_OK);
    REQUIRE(module_snapshot->supervised_process_count == 0U);
    REQUIRE(find_widget(body, "desk.home.search", NULL) == home_search);
    REQUIRE(strcmp(gtk_editable_get_text(GTK_EDITABLE(home_search)), "Studio") == 0);

    /* A native close can precede product cleanup. Strong original-body/window
     * references keep callbacks reachable until they are explicitly disabled. */
    run.poll_source_id = g_timeout_add_seconds(3600U, umi_desktop_gtk_window_poll, &run);
    poll_source = run.poll_source_id;
    gtk_window_destroy(window);
    umi_desktop_gtk_window_dispose(&run);
    REQUIRE(run.poll_source_id == 0U && run.module == NULL && run.desk == NULL);
    REQUIRE(g_main_context_find_source_by_id(NULL, poll_source) == NULL);
    REQUIRE(!g_signal_has_handler_pending(new_window, clicked_signal, 0U, TRUE));
    gtk_check_button_set_active(GTK_CHECK_BUTTON(choice), TRUE);
    g_signal_emit_by_name(navigation, "activate");
    g_signal_emit_by_name(navigation, "search-changed");

    /* The ordinary controller-first order also destroys its window, rather
     * than leaving a GtkApplication-owned surface with borrowed Desk callbacks. */
    run.executable_root = NULL;
    run.discovery_config = NULL;
    REQUIRE(umi_desktop_gtk_window_prepare(application, &run) == UMI_STATUS_OK);
    second_window = g_object_ref(GTK_WINDOW(umi_gtk4_desk_native_window(run.desk)));
    REQUIRE(!gtk_widget_get_realized(GTK_WIDGET(second_window)));
    umi_desktop_gtk_window_dispose(&run);
    REQUIRE(gtk_window_get_child(second_window) == NULL);
    REQUIRE(!gtk_widget_get_visible(GTK_WIDGET(second_window)));

cleanup:
    umi_desktop_gtk_window_dispose(&run);
    if (new_window != NULL) g_object_unref(new_window);
    if (choice != NULL) g_object_unref(choice);
    if (navigation != NULL) g_object_unref(navigation);
    if (home_search != NULL) g_object_unref(home_search);
    if (window != NULL) g_object_unref(window);
    if (second_window != NULL) g_object_unref(second_window);
    if (application != NULL) g_object_unref(application);
    g_clear_object(&expected_file);
    g_clear_object(&actual_file);
    g_clear_error(&error);
    g_free(module_snapshot);
    return failed;
}
