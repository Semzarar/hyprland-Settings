#include "keybinds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------- globals ------------------------ */
Section *g_sections = NULL;
int g_section_count = 0;
char g_filepath[512];
GPtrArray *preamble_lines = NULL;
GtkWidget *g_main_box = NULL;
GtkWidget *g_app_window = NULL;

/* ------------------------- utilities ------------------------ */
static char *trim(char *str) {
    while (*str == ' ' || *str == '\t') str++;
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = 0;
        end--;
    }
    return str;
}

static gboolean is_blank(const char *s) {
    if (!s) return TRUE;
    while (*s) {
        if (*s != ' ' && *s != '\t' && *s != '\r' && *s != '\n') return FALSE;
        s++;
    }
    return TRUE;
}

/* ------------------------- parse & rewrite ------------------------ */
Section *parse_keybinds(const char *filepath, int *out_section_count) {
    FILE *f = fopen(filepath, "r");
    if (!f) return NULL;

    if (preamble_lines) g_ptr_array_free(preamble_lines, TRUE);
    preamble_lines = g_ptr_array_new_with_free_func(g_free);

    int capacity = 8;
    Section *sections = malloc(capacity * sizeof(Section));
    *out_section_count = 0;

    char line[MAX_LINE_LENGTH];
    gboolean seen_first_section = FALSE;

    while (fgets(line, sizeof(line), f)) {
        if (!seen_first_section) {
            if (strncmp(line, "##", 2) == 0) {
                seen_first_section = TRUE;
                char *header = trim(line + 2);
                strncpy(sections[*out_section_count].header, header, sizeof(sections[*out_section_count].header)-1);
                sections[*out_section_count].header[sizeof(sections[*out_section_count].header)-1] = '\0';
                sections[*out_section_count].buttons = g_ptr_array_new_with_free_func(g_free);
                (*out_section_count)++;
            } else {
                g_ptr_array_add(preamble_lines, g_strdup(line));
            }
            continue;
        }

        char *t = trim(line);
        if (strlen(t) == 0) continue;

        if (strncmp(t, "##", 2) == 0) {
            if (*out_section_count == capacity) {
                capacity *= 2;
                sections = realloc(sections, capacity * sizeof(Section));
            }
            char *header = trim(t + 2);
            strncpy(sections[*out_section_count].header, header, sizeof(sections[*out_section_count].header)-1);
            sections[*out_section_count].header[sizeof(sections[*out_section_count].header)-1] = '\0';
            sections[*out_section_count].buttons = g_ptr_array_new_with_free_func(g_free);
            (*out_section_count)++;
        } else if (strncmp(t, "bind", 4) == 0 || strncmp(t, "bindm", 5) == 0
                   || strncmp(t, "bindel", 6) == 0 || strncmp(t, "bindl", 5) == 0) {
            if (!is_blank(t))
                g_ptr_array_add(sections[*out_section_count-1].buttons, g_strdup(t));
        } else {
            if (!is_blank(t))
                g_ptr_array_add(sections[*out_section_count-1].buttons, g_strdup(t));
        }
    }

    fclose(f);
    return sections;
}

void rewrite_config(const char *filepath, Section *sections, int section_count) {
    FILE *f = fopen(filepath, "w");
    if (!f) return;

    if (preamble_lines) {
        for (size_t i = 0; i < preamble_lines->len; i++) {
            fprintf(f, "%s", g_ptr_array_index(preamble_lines, i));
        }
    }

    for (int i = 0; i < section_count; i++) {
        fprintf(f, "## %s\n", sections[i].header);
        GPtrArray *btns = sections[i].buttons;
        for (size_t j = 0; j < btns->len; j++) {
            char *line = g_ptr_array_index(btns, j);
            if (!line) continue;
            if (is_blank(line)) continue;
            fprintf(f, "%s\n", line);
        }
    }

    fclose(f);
}

/* ------------------------- UI helpers ------------------------ */
static void clear_box_children(GtkWidget *box_widget) {
    GtkBox *box = GTK_BOX(box_widget);
    GtkWidget *child = gtk_widget_get_first_child(box_widget);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(box, child);
        child = next;
    }
}

static void on_delete_button_clicked(GtkButton *button, gpointer user_data) {
    ButtonContext *ctx = g_object_get_data(G_OBJECT(button), "button-ctx");
    if (!ctx) return;
    GPtrArray *arr = ctx->sections[ctx->section_index].buttons;
    if (!arr) return;
    if ((size_t)ctx->button_index >= arr->len) return;
    g_ptr_array_remove_index(arr, ctx->button_index);
    rewrite_config(ctx->filepath, ctx->sections, ctx->section_count);
    rebuild_ui();
}

/* ----- edit existing bind dialog ----- */
typedef struct {
    ButtonContext *bctx;
    GtkWidget *dialog;
    GtkWidget *entry;
} EditDialogData;

static void on_edit_ok(GtkWidget *w, gpointer user_data) {
    EditDialogData *d = user_data;
    ButtonContext *ctx = d->bctx;

    gchar *new_text = NULL;
    g_object_get(d->entry, "text", &new_text, NULL);
    if (!new_text) new_text = g_strdup("");

    char *trimmed = trim(new_text);
    GPtrArray *arr = ctx->sections[ctx->section_index].buttons;
    if ((size_t)ctx->button_index >= arr->len) {
        g_free(new_text);
        gtk_window_destroy(GTK_WINDOW(d->dialog));
        g_free(d);
        return;
    }

    char *old = g_ptr_array_index(arr, ctx->button_index);
    if (strlen(trimmed) == 0) {
        g_ptr_array_remove_index(arr, ctx->button_index);
    } else {
        if (old) g_free(old);
        g_ptr_array_index(arr, ctx->button_index) = g_strdup(trimmed);
    }

    rewrite_config(ctx->filepath, ctx->sections, ctx->section_count);
    rebuild_ui();

    g_free(new_text);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void on_edit_cancel(GtkWidget *w, gpointer user_data) {
    EditDialogData *d = user_data;
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void open_edit_dialog_for(ButtonContext *ctx) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Edit Keybind");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(g_app_window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 480, 120);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    const char *cur = NULL;
    GPtrArray *arr = ctx->sections[ctx->section_index].buttons;
    if ((size_t)ctx->button_index < arr->len)
        cur = g_ptr_array_index(arr, ctx->button_index);

    GtkWidget *entry = gtk_entry_new();
    g_object_set(entry, "text", cur ? cur : "", NULL);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_append(GTK_BOX(vbox), entry);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);
    GtkWidget *spacer = gtk_label_new(NULL);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(hbox), spacer);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    GtkWidget *btn_ok = gtk_button_new_with_label("OK");
    gtk_box_append(GTK_BOX(hbox), btn_cancel);
    gtk_box_append(GTK_BOX(hbox), btn_ok);

    EditDialogData *d = g_new0(EditDialogData, 1);
    d->bctx = ctx;
    d->dialog = dialog;
    d->entry = entry;

    g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_edit_ok), d);
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_edit_cancel), d);

    gtk_window_present(GTK_WINDOW(dialog));
}

/* ----- existing button click handler ----- */
void on_existing_button_clicked(GtkButton *button, gpointer user_data) {
    ButtonContext *ctx = g_object_get_data(G_OBJECT(button), "button-ctx");
    if (!ctx) return;
    open_edit_dialog_for(ctx);
}

/* ----- add new bind dialog ----- */
typedef struct {
    GtkWidget *dialog;
    GtkWidget *entry;
    GtkWidget *section_combo;
} AddDialogData;

static void on_add_ok(GtkWidget *w, gpointer user_data) {
    AddDialogData *d = user_data;

    gchar *bind_text = NULL;
    g_object_get(d->entry, "text", &bind_text, NULL);
    if (!bind_text) bind_text = g_strdup("");

    char *trimmed = trim(bind_text);
    if (strlen(trimmed) == 0) {
        g_free(bind_text);
        gtk_window_destroy(GTK_WINDOW(d->dialog));
        g_free(d);
        return;
    }

    gint active = gtk_combo_box_get_active(GTK_COMBO_BOX(d->section_combo));
    if (active < 0) active = 0;

    GPtrArray *arr = g_sections[active].buttons;
    g_ptr_array_add(arr, g_strdup(trimmed));

    rewrite_config(g_filepath, g_sections, g_section_count);
    rebuild_ui();

    g_free(bind_text);
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

static void on_add_cancel(GtkWidget *w, gpointer user_data) {
    AddDialogData *d = user_data;
    gtk_window_destroy(GTK_WINDOW(d->dialog));
    g_free(d);
}

void open_add_dialog(void) {
    GtkWidget *dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Add Keybind");
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(g_app_window));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 560, 280);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_top(vbox, 12);
    gtk_widget_set_margin_bottom(vbox, 12);
    gtk_widget_set_margin_start(vbox, 12);
    gtk_widget_set_margin_end(vbox, 12);
    gtk_window_set_child(GTK_WINDOW(dialog), vbox);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Enter new bind line e.g. bind = SUPER+ALT, exec, your-command");
    gtk_box_append(GTK_BOX(vbox), entry);

    GtkWidget *combo = gtk_combo_box_text_new();
    for (int i = 0; i < g_section_count; ++i)
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo), g_sections[i].header);
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    gtk_box_append(GTK_BOX(vbox), combo);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(vbox), hbox);
    GtkWidget *spacer = gtk_label_new(NULL);
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(hbox), spacer);

    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    GtkWidget *btn_ok = gtk_button_new_with_label("Add");
    gtk_box_append(GTK_BOX(hbox), btn_cancel);
    gtk_box_append(GTK_BOX(hbox), btn_ok);

    AddDialogData *d = g_new0(AddDialogData, 1);
    d->dialog = dialog;
    d->entry = entry;
    d->section_combo = combo;

    g_signal_connect(btn_ok, "clicked", G_CALLBACK(on_add_ok), d);
    g_signal_connect(btn_cancel, "clicked", G_CALLBACK(on_add_cancel), d);

    gtk_window_present(GTK_WINDOW(dialog));
}

/* ----- rebuild UI ----- */
void rebuild_ui(void) {
    if (!g_main_box) return;
    clear_box_children(g_main_box);

    GtkWidget *add_btn = gtk_button_new_with_label("Add keybind");
    g_signal_connect(add_btn, "clicked", G_CALLBACK((GCallback)(void(*)(GtkButton*,gpointer))((void(*)(void))open_add_dialog)), NULL);
    gtk_box_append(GTK_BOX(g_main_box), add_btn);

    for (int i = 0; i < g_section_count; ++i) {
        GtkWidget *header_label = gtk_label_new(NULL);
        char *markup = g_markup_printf_escaped("<b>%s</b>", g_sections[i].header);
        gtk_label_set_markup(GTK_LABEL(header_label), markup);
        g_free(markup);
        gtk_widget_set_halign(header_label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(g_main_box), header_label);

        GPtrArray *btns = g_sections[i].buttons;
        for (size_t j = 0; j < btns->len; ++j) {
            char *btn_label = g_ptr_array_index(btns, j);

            GtkWidget *hrow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
            gtk_widget_set_hexpand(hrow, TRUE);

            GtkWidget *button = gtk_button_new_with_label(btn_label ? btn_label : "");
            GtkWidget *label = gtk_label_new(NULL);
            gtk_label_set_selectable(GTK_LABEL(label), TRUE);

            ButtonContext *ctx = g_new0(ButtonContext, 1);
            ctx->section_index = i;
            ctx->button_index = j;
            ctx->sections = g_sections;
            ctx->section_count = g_section_count;
            strncpy(ctx->filepath, g_filepath, sizeof(ctx->filepath)-1);
            ctx->parent_win = g_app_window;

            g_object_set_data_full(G_OBJECT(button), "button-ctx", ctx, g_free);

            g_signal_connect(button, "clicked", G_CALLBACK(on_existing_button_clicked), NULL);

            gtk_box_append(GTK_BOX(hrow), button);

            GtkWidget *del_btn = gtk_button_new_with_label("Delete");
            g_signal_connect(del_btn, "clicked", G_CALLBACK(on_delete_button_clicked), NULL);
            gtk_box_append(GTK_BOX(hrow), del_btn);

            gtk_box_append(GTK_BOX(g_main_box), hrow);
        }
    }
}
