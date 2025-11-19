#include "keybinds.h"
#include "waybar_presets.h"
#include <stdlib.h>
#include <stdio.h>

/* GTK4 activate callback */
static void activate(GtkApplication *app, gpointer user_data) {
    const char *home = getenv("HOME");
    if (!home) home = "/root";
    snprintf(g_filepath, sizeof(g_filepath), "%s/.config/hypr/config/software/keybinds.conf", home);

    g_sections = parse_keybinds(g_filepath, &g_section_count);
    if (!g_sections || g_section_count == 0) {
        g_printerr("No keybinds loaded or failed to parse file.\n");
        return;
    }

    GtkWidget *window = gtk_application_window_new(app);
    g_app_window = window;
    gtk_window_set_title(GTK_WINDOW(window), "Keybind Settings");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 600);

    GtkWidget *notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(notebook), GTK_POS_LEFT);
    gtk_window_set_child(GTK_WINDOW(window), notebook);

    /* --- Keybinds tab --- */
    GtkWidget *keybinds_tab = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(keybinds_tab),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    g_main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(g_main_box, 20);
    gtk_widget_set_margin_end(g_main_box, 20);
    gtk_widget_set_margin_top(g_main_box, 20);
    gtk_widget_set_margin_bottom(g_main_box, 20);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(keybinds_tab), g_main_box);

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), keybinds_tab, gtk_label_new("Keybinds"));
    rebuild_ui();

    /* --- Waybar Presets tab --- */
    GtkWidget *waybar_tab = create_waybar_presets_tab(GTK_WINDOW(window));
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), waybar_tab, gtk_label_new("Waybar Presets"));

    gtk_window_present(GTK_WINDOW(window));
}

/* --- main() function --- */
int main(int argc, char *argv[]) {
    GtkApplication *app = gtk_application_new("com.example.settingsapp",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
