#include "waybar_presets.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

/* Struct for passing widgets safely to callbacks */
typedef struct {
    GtkWidget *dialog;
    GtkWidget *entry;
} SaveWidgets;

static GtkWidget *presets_box = NULL;

/* Forward declarations */
static void refresh_presets_list(void);

/* Copy directory recursively using GFile (GTK4-safe) */
static gboolean copy_directory(const char *src, const char *dest) {
    if (!g_file_test(src, G_FILE_TEST_EXISTS)) return FALSE;
    g_mkdir_with_parents(dest, 0755);

    GDir *dir = g_dir_open(src, 0, NULL);
    if (!dir) return FALSE;

    const char *filename;
    while ((filename = g_dir_read_name(dir))) {
        char src_path[1024], dest_path[1024];
        snprintf(src_path, sizeof(src_path), "%s/%s", src, filename);
        snprintf(dest_path, sizeof(dest_path), "%s/%s", dest, filename);

        if (g_file_test(src_path, G_FILE_TEST_IS_DIR)) {
            copy_directory(src_path, dest_path);
        } else {
            GFile *src_file = g_file_new_for_path(src_path);
            GFile *dest_file = g_file_new_for_path(dest_path);
            GError *error = NULL;

            g_file_copy(src_file, dest_file, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error);
            if (error) {
                g_printerr("Failed to copy %s: %s\n", src_path, error->message);
                g_clear_error(&error);
            }

            g_object_unref(src_file);
            g_object_unref(dest_file);
        }
    }
    g_dir_close(dir);
    return TRUE;
}

/* Recursively delete a directory */
static gboolean delete_directory(const char *path) {
    if (!g_file_test(path, G_FILE_TEST_EXISTS)) return TRUE;

    GDir *dir = g_dir_open(path, 0, NULL);
    if (!dir) return g_remove(path) == 0;

    const char *filename;
    while ((filename = g_dir_read_name(dir))) {
        char child_path[1024];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, filename);
        if (g_file_test(child_path, G_FILE_TEST_IS_DIR)) {
            delete_directory(child_path);
        } else {
            g_remove(child_path);
        }
    }
    g_dir_close(dir);

    return g_rmdir(path) == 0;
}

/* Empty a directory but keep the folder itself */
static void empty_directory(const char *dir_path) {
    GDir *dir = g_dir_open(dir_path, 0, NULL);
    if (!dir) return;

    const char *filename;
    while ((filename = g_dir_read_name(dir))) {
        char child_path[1024];
        snprintf(child_path, sizeof(child_path), "%s/%s", dir_path, filename);
        if (g_file_test(child_path, G_FILE_TEST_IS_DIR)) {
            delete_directory(child_path);
        } else {
            g_remove(child_path);
        }
    }
    g_dir_close(dir);
}

/* Callback when a preset button is clicked */
static void on_preset_clicked(GtkButton *button, gpointer user_data) {
    char *preset_name = (char *)user_data;

    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char preset_dir[1024], dest_dir[1024];
    snprintf(preset_dir, sizeof(preset_dir),
             "%s/.config/settings-app/waybar-presets/%s", home, preset_name);
    snprintf(dest_dir, sizeof(dest_dir),
             "%s/.config/waybar", home);

    /* Clear existing Waybar config */
    empty_directory(dest_dir);

    /* Copy preset into now-empty config folder */
    copy_directory(preset_dir, dest_dir);

    g_print("Applied Waybar preset: %s\n", preset_name);

    /* Run Waybar reload script */
    char script_path[1024];
    snprintf(script_path, sizeof(script_path),
             "%s/Dots/Scripts/Waybar/waybar.sh", home);

    GError *error = NULL;
    if (!g_spawn_command_line_async(script_path, &error)) {
        g_printerr("Failed to run script: %s\n", error->message);
        g_clear_error(&error);
    }
}

/* Callback when delete button is clicked */
static void on_delete_preset(GtkButton *button, gpointer user_data) {
    char *preset_name = (char *)user_data;

    const char *home = getenv("HOME");
    if (!home) home = "/root";

    char preset_dir[1024];
    snprintf(preset_dir, sizeof(preset_dir),
             "%s/.config/settings-app/waybar-presets/%s", home, preset_name);

    if (delete_directory(preset_dir)) {
        g_print("Deleted Waybar preset: %s\n", preset_name);
    } else {
        g_printerr("Failed to delete preset: %s\n", preset_name);
    }

    refresh_presets_list();
}

/* Refresh the list of existing presets */
static void refresh_presets_list(void) {
    if (!presets_box) return;

    /* Clear old children safely in GTK4 */
    GtkWidget *child;
    while ((child = gtk_widget_get_first_child(presets_box)) != NULL) {
        gtk_box_remove(GTK_BOX(presets_box), child);
    }

    const char *home = getenv("HOME");
    if (!home) home = "/root";
    char presets_dir[1024];
    snprintf(presets_dir, sizeof(presets_dir),
             "%s/.config/settings-app/waybar-presets", home);
    g_mkdir_with_parents(presets_dir, 0755);

    GDir *dir = g_dir_open(presets_dir, 0, NULL);
    if (!dir) return;

    const char *name;
    while ((name = g_dir_read_name(dir))) {
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

        GtkWidget *btn = gtk_button_new_with_label(name);
        gtk_widget_set_halign(btn, GTK_ALIGN_START);
        g_signal_connect(btn, "clicked", G_CALLBACK(on_preset_clicked),
                         g_strdup(name));

        GtkWidget *del_btn = gtk_button_new_with_label("Delete");
        g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_preset),
                         g_strdup(name));

        gtk_box_append(GTK_BOX(hbox), btn);
        gtk_box_append(GTK_BOX(hbox), del_btn);
        gtk_box_append(GTK_BOX(presets_box), hbox);
    }
    g_dir_close(dir);
}

/* Callback to destroy the dialog */
static void on_dialog_cancel(GtkButton *button, gpointer user_data) {
    GtkWidget *dialog = GTK_WIDGET(user_data);
    gtk_window_destroy(GTK_WINDOW(dialog));
}

/* Callback for saving preset */
static void on_save_ok(GtkButton *button, gpointer user_data) {
    SaveWidgets *data = (SaveWidgets *)user_data;
    GtkWidget *dialog = data->dialog;
    GtkWidget *entry  = data->entry;

    const char *name = gtk_editable_get_text(GTK_EDITABLE(entry));
    if (strlen(name) == 0) {
        g_free(data);
        return;
    }

    const char *home = getenv("HOME");
    if (!home) home = "/root";
    char src_dir[1024], dest_dir[1024];
    snprintf(src_dir, sizeof(src_dir), "%s/.config/waybar", home);
    snprintf(dest_dir, sizeof(dest_dir), "%s/.config/settings-app/waybar-presets/%s", home, name);

    copy_directory(src_dir, dest_dir);
    refresh_presets_list();
    gtk_window_destroy(GTK_WINDOW(dialog));

    g_free(data);
}

/* Callback when save button is clicked */
static void on_save_clicked(GtkButton *button, gpointer user_data) {
    GtkWindow *parent_window = GTK_WINDOW(user_data);

    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent_window);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_title(GTK_WINDOW(dialog), "Save Waybar Preset");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 120);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Preset name");
    gtk_box_append(GTK_BOX(vbox), entry);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    GtkWidget *btn_ok     = gtk_button_new_with_label("Save");
    gtk_box_append(GTK_BOX(hbox), btn_cancel);
    gtk_box_append(GTK_BOX(hbox), btn_ok);

    SaveWidgets *data = g_new(SaveWidgets, 1);
    data->dialog = dialog;
    data->entry  = entry;

    g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_save_ok), data);
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_dialog_cancel), dialog);

    gtk_window_present(GTK_WINDOW(dialog));
}

/* Create the Waybar Presets tab */
GtkWidget *create_waybar_presets_tab(GtkWindow *main_window) {
    GtkWidget *scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scroll), vbox);

    GtkWidget *save_btn = gtk_button_new_with_label("Save Current Waybar Config");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_save_clicked), main_window);
    gtk_box_append(GTK_BOX(vbox), save_btn);

    presets_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_append(GTK_BOX(vbox), presets_box);

    refresh_presets_list();

    return scroll;
}
