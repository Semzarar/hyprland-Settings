#ifndef KEYBINDS_H
#define KEYBINDS_H

#include <gtk/gtk.h>
#include <glib.h>

#define MAX_LINE_LENGTH 512

typedef struct Section {
    char header[128];
    GPtrArray *buttons;
} Section;

typedef struct {
    int section_index;
    int button_index;
    Section *sections;
    int section_count;
    char filepath[512];
    GtkWidget *parent_win;
} ButtonContext;

extern Section *g_sections;
extern int g_section_count;
extern char g_filepath[512];
extern GPtrArray *preamble_lines;
extern GtkWidget *g_main_box;
extern GtkWidget *g_app_window;

Section *parse_keybinds(const char *filepath, int *out_section_count);
void rewrite_config(const char *filepath, Section *sections, int section_count);
void rebuild_ui(void);
void open_add_dialog(void);
void on_existing_button_clicked(GtkButton *button, gpointer user_data);

#endif // KEYBINDS_H
