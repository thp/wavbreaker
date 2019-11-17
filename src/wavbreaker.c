/* -*- c-basic-offset: 4 -*- */
/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2006 Timothy Robinson
 * Copyright (C) 2006-2007 Thomas Perl
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "wavbreaker.h"
#include "sample.h"
#include "about.h"
#include "appconfig.h"
#include "autosplit.h"
#include "saveas.h"
#include "popupmessage.h"
#include "overwritedialog.h"
#include "toc.h"
#include "cue.h"
#include "reallyquit.h"
#include "guimerge.h"
#include "moodbar.h"
#include "draw.h"

#include <locale.h>
#include "gettext.h"

#define APPNAME "wavbreaker"

#define SILENCE_MIN_LENGTH 4

static struct WaveformSurface *sample_surface;
static struct WaveformSurface *summary_surface;

static GtkWidget *main_window;
static GtkWidget *header_bar;
static GtkWidget *header_bar_save_button;
static GtkWidget *vpane1, *vpane2;
static GtkWidget *scrollbar;
static GtkAdjustment *adj;
static GtkWidget *draw;
static GtkWidget *draw_summary;
static GtkWidget *play_button;
static GtkWidget *jump_to_popover;
static GtkWidget *autosplit_popover;
static GtkWidget *menu_popover;

static GtkWidget *cursor_marker_spinner;
static GtkWidget *cursor_marker_min_spinner;
static GtkWidget *cursor_marker_sec_spinner;
static GtkWidget *cursor_marker_subsec_spinner;
static GtkWidget *button_seek_backward;
static GtkWidget *button_jump_to_time;
static GtkWidget *button_seek_forward;
static GtkWidget *button_auto_split;
static GtkWidget *button_add_break;
static GtkWidget *button_remove_break;

static GtkAdjustment *cursor_marker_spinner_adj;
static GtkAdjustment *cursor_marker_min_spinner_adj;
static GtkAdjustment *cursor_marker_sec_spinner_adj;
static GtkAdjustment *cursor_marker_subsec_spinner_adj;

static GraphData graphData;

static MoodbarData *moodbarData;

static gulong cursor_marker;
static gulong play_marker;
static int pixmap_offset;
char *sample_filename = NULL;
struct stat sample_stat;
static gboolean overwrite_track_names = FALSE;

// one-shot idle_add-style event sources
static guint open_file_source_id;
static guint redraw_source_id;

// timeout-based (periodic) progress UI update event sources
static guint file_open_progress_source_id;
static guint play_progress_source_id;
static guint file_write_progress_source_id;

static gdouble progress_pct;
static WriteInfo write_info;

typedef struct CursorData_ CursorData;

struct CursorData_ {
    gulong marker;
    gboolean is_equal;
};

enum {
    COLUMN_WRITE,
    COLUMN_FILENAME,
    COLUMN_TIME,
    COLUMN_DURATION,
    COLUMN_OFFSET,
    COLUMN_EDITABLE,
    NUM_COLUMNS
};

static GList *track_break_list = NULL;
static GtkListStore *store = NULL;
GtkWidget *treeview;

/*
 *-------------------------------------------------------------------------
 * Function Prototypes
 *-------------------------------------------------------------------------
 */

/* Track Break Functions */
static gboolean track_break_button_press(GtkWidget *widget,
    GdkEventButton *event, gpointer user_data);
void track_break_write_toggled(GtkWidget *widget, gchar *path_str,
    gpointer data);
void track_break_filename_edited(GtkCellRendererText *cell,
    const gchar *path_str, const gchar *new_text, gpointer user_data);
void track_break_start_time_edited(GtkCellRendererText *cell,
    const gchar *path_str, const gchar *new_text, gpointer user_data);
guint track_break_find_offset();
void track_break_delete_entry();
void track_break_setup_filename(gpointer data, gpointer user_data);
void track_break_rename( gboolean overwrite);
void track_break_add_to_model(gpointer data, gpointer user_data);
void track_break_add_entry();
void track_break_set_durations();
void track_break_set_duration(gpointer data, gpointer user_data);

int track_breaks_export_to_file( char* filename);
int track_breaks_load_from_file( gchar const *filename);
void track_break_write_text( gpointer data, gpointer user_data);
void track_break_write_cue( gpointer data, gpointer user_data);

/* File Functions */
void set_sample_filename(const char *f);
static void open_file();
static void set_title( char* title);

/* Sample and Summary Display Functions */
static void force_redraw();
static void redraw();
static gboolean redraw_later( gpointer data);

static void reset_sample_display(guint);

static gboolean
configure_event(GtkWidget *widget,
                GdkEventConfigure *event,
                gpointer data);

static gboolean
draw_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);

static gboolean
draw_summary_configure_event(GtkWidget *widget,
                             GdkEventConfigure *event,
                             gpointer user_data);

static gboolean
draw_summary_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data);

static gboolean
draw_summary_button_release(GtkWidget *widget,
                            GdkEventButton *event,
                            gpointer user_data);

/* Menu Functions */
static void
menu_open_file(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_menu(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_delete_track_break(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_import(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_save(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_save_as(GSimpleAction *action, GVariant *parameter, gpointer user_data);

#if defined(WANT_MOODBAR)
static void
menu_view_moodbar(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_moodbar(GSimpleAction *action, GVariant *parameter, gpointer user_data);
#endif

static void
menu_about(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_config(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_merge(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_export(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_autosplit(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_rename(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void
menu_play(GtkWidget *widget, gpointer user_data);

static void
menu_stop(GtkWidget *widget, gpointer user_data);

static void
menu_next_silence( GtkWidget* widget, gpointer user_data);

static void
menu_jump_to(GtkWidget *widget, gpointer user_data);

static void
menu_prev_silence( GtkWidget* widget, gpointer user_data);

void
menu_add_track_break(GSimpleAction *action, GVariant *parameter, gpointer user_data);

static void set_stop_icon();
static void set_play_icon();

static void save_window_sizes();
static void check_really_quit();

static void
offset_to_time(guint, gchar *, gboolean);

static guint
time_to_offset(gint min, gint sec, gint subsec);

static void
offset_to_duration(guint, guint, gchar *);

static void
update_status(gboolean);

/*
static char *status_message = NULL;

char *get_status_message()
{
    return status_message;
}

void set_status_message(const char *val)
{
    if (status_message != NULL) {
        g_free(status_message);
    }
    status_message = g_strdup(val);
}
*/

void parts_check_cb(GtkWidget *widget, gpointer data) {

    TrackBreak *track_break;
    guint list_pos;
    gpointer list_data;
    gint i;
    GtkTreeIter iter;

    i = 0;

    while (gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i++)) {

        list_pos = i - 1;
        list_data = g_list_nth_data(track_break_list, list_pos);
        track_break = (TrackBreak *)list_data;

        switch ((glong)data) {

            case CHECK_ALL:
                track_break->write = TRUE;
                break;
            case CHECK_NONE:
                track_break->write = FALSE;
                break;
            case CHECK_INVERT:
                track_break->write = !track_break->write;
                break;
        }

        gtk_list_store_set(GTK_LIST_STORE(store), &iter, COLUMN_WRITE,
                           track_break->write, -1);
    }

    force_redraw();
}

void jump_to_cursor_marker(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    reset_sample_display(cursor_marker);
    redraw();
}

void jump_to_track_break(GSimpleAction *action, GVariant *parameter, gpointer user_data) {
    guint n = 0;

    n = track_break_find_offset();
    if (n <= graphData.numSamples) {
        reset_sample_display(n);
    }

    redraw();
}

void wavbreaker_autosplit(long x) {
    long n = x;

    gulong orig_cursor_marker = cursor_marker;

    while (n <= graphData.numSamples) {
        cursor_marker = n;
        track_break_add_entry();
        n += x;
    }

    cursor_marker = orig_cursor_marker;
    force_redraw();
}

/*
 *-------------------------------------------------------------------------
 * Track Break
 *-------------------------------------------------------------------------
 */

/* TODO */
/*
static void
cell_data_func_gpa (GtkTreeViewColumn *col,
                    GtkCellRenderer   *cell,
                    GtkTreeModel      *model,
                    GtkTreeIter       *iter,
                    gpointer           data)
{
	gchar   buf[32];
	GValue  val = {0, };

	gtk_tree_model_get_value(model, iter, COLUMN_TIME, &val);

	g_snprintf(buf, sizeof(buf), "%s", g_value_get_string(&val));

    g_printf("text: %s\n", buf);
//	g_object_set(cell, "text", buf, NULL);
}
*/

static void
set_action_enabled(const char *action, gboolean enabled)
{
    g_object_set(G_OBJECT(g_action_map_lookup_action(G_ACTION_MAP(main_window), action)),
            "enabled", enabled,
            NULL);
}

static void
on_tree_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    gboolean can_remove = TRUE;

    GList *list = gtk_tree_selection_get_selected_rows(selection, NULL);
    GList *cur = list;
    while (cur) {
        GtkTreePath *path = cur->data;
        if (gtk_tree_path_get_indices(path)[0] == 0) {
            can_remove = FALSE;
            break;
        }
        cur = cur->next;
    }
    g_list_free(list);

    set_action_enabled("remove_break", can_remove);
}

GtkWidget *
track_break_create_list_gui()
{
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *sw;

/* create the scrolled window for the list */
    sw = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);

    /* create the data store */
    store = gtk_list_store_new(NUM_COLUMNS,
                               G_TYPE_BOOLEAN,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_STRING,
                               G_TYPE_UINT,
                               G_TYPE_BOOLEAN);

    /* create the treeview */
    treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_container_add(GTK_CONTAINER(sw), treeview);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    g_signal_connect(G_OBJECT(selection), "changed",
            G_CALLBACK(on_tree_selection_changed), NULL);

    /* connect/add the right-click signal */
    gtk_widget_add_events(draw_summary, GDK_BUTTON_RELEASE_MASK);
    gtk_widget_add_events(draw_summary, GDK_BUTTON_PRESS_MASK);
    g_signal_connect(G_OBJECT(treeview), "button_press_event",
                     G_CALLBACK(track_break_button_press), NULL);

    gtk_widget_show(treeview);

    /* create the columns */
    /* Write Toggle Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_toggle_new();
    g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(track_break_write_toggled), store);
    gtk_tree_view_column_set_title(column, _("Write"));
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "active", COLUMN_WRITE);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* File Name Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(track_break_filename_edited), store);
    gtk_tree_view_column_set_title(column, _("File Name"));
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_FILENAME);
    gtk_tree_view_column_add_attribute(column, renderer, "editable", COLUMN_EDITABLE);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    //gtk_tree_view_column_set_fixed_width(column, 200);
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* File Time Start Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    /* TODO
    renderer = gui_cell_renderer_spin_new(0.0, 5.0, 0.1, 1.0, 1.0, 0.1, 0);
    g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(track_break_start_time_edited), store);
    gtk_tree_view_column_set_cell_data_func(column, renderer, cell_data_func_gpa, NULL, NULL);
    */
    gtk_tree_view_column_set_title(column, _("Time"));
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_TIME);
    //gtk_tree_view_column_add_attribute(column, renderer, "editable", COLUMN_EDITABLE);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* File Duration Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title(column, _("Duration"));
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_DURATION);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* File Offset Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title(column, _("Offset"));
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_OFFSET);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    return sw;
}

gint
track_break_sort(gconstpointer a, gconstpointer b)
{
    TrackBreak *x = (TrackBreak *)a;
    TrackBreak *y = (TrackBreak *)b;

    if (x->offset < y->offset) {
        return -1;
    } else if (x->offset > y->offset) {
        return 1;
    } else {
        return 0;
    }
}

static gboolean
track_break_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
    if ((event->type == GDK_2BUTTON_PRESS) && (event->button == 1)) {

        cursor_marker = track_break_find_offset();
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (cursor_marker_spinner), cursor_marker);
        jump_to_cursor_marker(NULL, NULL, NULL);
        return FALSE;

    } else if (event->button != 3) {
        return FALSE;
    }

    GMenu *menu_model = g_menu_new();

    GMenu *check_model = g_menu_new();
    g_menu_append(check_model, _("Check all"), "win.check_all");
    g_menu_append(check_model, _("Check none"), "win.check_none");
    g_menu_append(check_model, _("Invert check"), "win.check_invert");
    g_menu_append_section(menu_model, NULL, G_MENU_MODEL(check_model));

    GMenu *all_model = g_menu_new();
    g_menu_append(all_model, _("Auto-rename track breaks"), "win.auto_rename");
    g_menu_append_section(menu_model, NULL, G_MENU_MODEL(all_model));

    GMenu *break_model = g_menu_new();
    g_menu_append(break_model, _("Remove track break"), "win.remove_break");
    g_menu_append(break_model, _("Jump to track break"), "win.jump_break");
    g_menu_append_section(menu_model, NULL, G_MENU_MODEL(break_model));

    GtkMenu *menu = GTK_MENU(gtk_menu_new_from_model(G_MENU_MODEL(menu_model)));
    gtk_menu_attach_to_widget(menu, main_window, NULL);
    gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);

    return FALSE;
}

/* DEBUG FUNCTION START */
void track_break_print_element(gpointer data, gpointer user_data)
{
    TrackBreak *breakup;

    breakup = (TrackBreak *)data;

    printf("filename: %s", breakup->filename);
    printf("\ttime: %s", breakup->time);
    printf("\tduration: %s", breakup->duration);
    printf("\toffset: %lu\n", breakup->offset);
}
/* DEBUG FUNCTION END */

void
track_break_free_element(gpointer data, gpointer user_data)
{
    TrackBreak *track_break;

    track_break = (TrackBreak *)data;

    g_free(track_break->filename);
    g_free(track_break);
}

void track_break_compare_cursor_marker(gpointer data, gpointer user_data)
{
    TrackBreak *track_break = (TrackBreak *) data;
    CursorData *cd = (CursorData *) user_data;

    if (cd->marker == track_break->offset) {
        cd->is_equal = TRUE;
    }
}

void track_break_set_duration(gpointer data, gpointer user_data)
{
    TrackBreak *track_break = (TrackBreak *) data;
    TrackBreak *next_track_break;
    guint index;

    index = g_list_index(track_break_list, track_break);
    index++;
    /*
    printf("index: %d\n", index);
    printf("cursor_marker: %d\n", cursor_marker);
    printf("numSamples: %d\n", graphData.numSamples);
    */
    next_track_break = (TrackBreak *) g_list_nth_data(track_break_list, index);

    if (next_track_break != NULL) {
        // Take the offset of the next track as the end of the duration.
        offset_to_duration(track_break->offset, next_track_break->offset,
            track_break->duration);
    } else {
        // There is no next track.
        // Take the end of the sample as the end of the duration.
        offset_to_duration(track_break->offset, graphData.numSamples,
            track_break->duration);
    }
    //printf("\n");
}

void track_break_clear_list()
{
    gtk_list_store_clear(store);
    g_list_foreach(track_break_list, track_break_free_element, NULL);
    g_list_free(track_break_list);
    track_break_list = NULL;
}

void track_break_selection(gpointer data, gpointer user_data)
{
    GtkTreePath *path = (GtkTreePath*)data;
    TrackBreak *track_break;
    guint list_pos;
    gpointer list_data;
    GtkTreeModel *model;
    GtkTreeIter iter;

    list_pos = gtk_tree_path_get_indices(path)[0];
    if (list_pos == 0) {
        // Do not allow first break to be deleted
        return;
    }

    list_data = g_list_nth_data(track_break_list, list_pos);
    track_break = (TrackBreak *)list_data;
    track_break_list = g_list_remove(track_break_list, track_break);
    track_break_free_element(track_break, NULL);

    model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
    gtk_tree_model_get_iter(model, &iter, path);
    gtk_list_store_remove(GTK_LIST_STORE(model), &iter);

/* DEBUG CODE START */
/*
    g_list_foreach(track_break_list, track_break_print_element, NULL);
    g_print("\n");
*/
/* DEBUG CODE END */

    gtk_tree_path_free(path);
}

void
track_break_delete_entry()
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GList *list;

    if (sample_filename == NULL) {
        return;
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    list = gtk_tree_selection_get_selected_rows(selection, &model);
    g_list_foreach(list, track_break_selection, NULL);
    g_list_free(list);

    track_break_set_durations();
    track_break_rename( FALSE);

    force_redraw();
}

void
track_break_setup_filename(gpointer data, gpointer user_data)
{
    TrackBreak *track_break = (TrackBreak *)data;
    gchar *orig_filename = (gchar *)user_data;

    gchar fn[128];
    gchar buf[128];
    int index;
    int disc_num;
    static int prev_disc_num;
    static int track_num = 1;

    /* when not overwriting track names, only update not-yet-set names */
    if( overwrite_track_names == FALSE) {

        // try and determine if the user has modified the filename

        if (track_break->filename != NULL) {
            size_t cut = 0;
            gboolean remove_from_start = FALSE;
            if (appconfig_get_use_etree_filename_suffix()) {
                // remove the dXtXX from the end
                cut = 5;
            } else {
                // remove the XX- from the beginning or -XX from the end
                cut = 3;
                remove_from_start = appconfig_get_prepend_file_number();
            }

            int length = strlen(track_break->filename);
            int orig_length = strlen(orig_filename);

            if (length != orig_length + cut) {
                return;
            }

            if (memcmp(orig_filename, track_break->filename + (remove_from_start ? cut : 0), orig_length) != 0) {
                return;
            }
        }
    }

    index = g_list_index(track_break_list, track_break);
    index++;
    if (appconfig_get_use_etree_filename_suffix()) {
        int cd_length = atoi(appconfig_get_etree_cd_length());
        disc_num = (track_break->offset / CD_BLOCKS_PER_SEC / 60) / cd_length;
        disc_num++;
        if (index == 1) {
            prev_disc_num = 0;
        }
        if (prev_disc_num != disc_num) {
            track_num = 1;
        } else {
            track_num++;
        }
        prev_disc_num = disc_num;
        sprintf(buf, "d%dt%02d", disc_num, track_num);
    } else {
        if (appconfig_get_prepend_file_number()) {
            sprintf(buf, "%02d%s", index, appconfig_get_etree_filename_suffix());
        } else {
            sprintf(buf, "%s%02d", appconfig_get_etree_filename_suffix(), index);
        }
    }

    fn[0] = '\0';

    if (!appconfig_get_use_etree_filename_suffix() && appconfig_get_prepend_file_number()) {
        strcat(fn, buf);
        strcat(fn, orig_filename);
    } else {
        strcat(fn, orig_filename);
        strcat(fn, buf);
    }

    g_free(track_break->filename);
    track_break->filename = g_strdup(fn);
}

void
track_break_add_to_model(gpointer data, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreePath *path;
    gchar path_str[8];
    TrackBreak *track_break = (TrackBreak *)data;
    int index = g_list_index(track_break_list, track_break);

    sprintf(path_str, "%d", index);
    path = gtk_tree_path_new_from_string(path_str);

/* DEBUG CODE START */
/*
    g_print("gtktreepath: %s\n", path_str);
    printf("list contents:\n");
    g_list_foreach(track_break_list, print_element, NULL);
*/
/* DEBUG CODE END */

    gtk_list_store_append(store, &iter);
/*
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &sibling, path)) {
        gtk_list_store_insert_before(store, &iter, &sibling);
    } else {
        gtk_list_store_append(store, &iter);
    }
*/
    gtk_list_store_set(store, &iter, COLUMN_WRITE, track_break->write,
                                     COLUMN_FILENAME, track_break->filename,
                                     COLUMN_TIME, track_break->time,
                                     COLUMN_DURATION, track_break->duration,
                                     COLUMN_OFFSET, track_break->offset,
                                     COLUMN_EDITABLE, track_break->editable,
                                     -1);

    gtk_tree_path_free(path);
}

guint track_break_find_offset()
{
    GtkTreeSelection *selection;
    GtkTreeIter iter;
    GtkTreeModel *model;
    guint offset = 0;
    gchar *time;
    gchar *duration;
    gchar *filename;

    if (sample_filename == NULL) {
        return 0;
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
        gtk_tree_model_get(model, &iter,
            COLUMN_FILENAME, &filename,
            COLUMN_TIME, &time,
            COLUMN_DURATION, &duration,
            COLUMN_OFFSET, &offset, -1);
        g_free(time);
        g_free(duration);
        g_free(filename);
    }

    return offset;
/*
    *tb = (TrackBreak *) gtk_tree_selection_get_user_data(selection);

    cursor_data->is_equal = FALSE;
    cursor_data->marker = cursor_marker;

    printf("tb->offset: %s\n", tb->offset);
*/
}

static void
select_and_show_track_break(int index)
{
    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    gtk_tree_selection_unselect_all(selection);
    GtkTreePath *path = gtk_tree_path_new_from_indices(index, -1);
    gtk_tree_selection_select_path(selection, path);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.f, 0.f);
    gtk_tree_path_free(path);
}

void track_break_add_entry()
{
    TrackBreak *track_break = NULL;
    CursorData cursor_data;

    if (sample_filename == NULL) {
        return;
    }

    // check for duplicate track breaks
    cursor_data.is_equal = FALSE;
    cursor_data.marker = cursor_marker;
    g_list_foreach(track_break_list, track_break_compare_cursor_marker,
                   &cursor_data);
    if (cursor_data.is_equal == TRUE) {
        return;
    }

    if (! (track_break = (TrackBreak *)g_malloc(sizeof(TrackBreak)))) {
        printf("couldn't malloc enough memory for track_break\n");
        exit(1);
    }

    track_break->write = 1;
    track_break->offset = cursor_marker;
    track_break->editable = TRUE;
    offset_to_time(cursor_marker, track_break->time, TRUE);
    track_break->filename = NULL;

    track_break_list = g_list_insert_sorted(track_break_list, track_break,
                                            track_break_sort);

    track_break_set_durations();
    track_break_rename( FALSE);

    select_and_show_track_break(g_list_index(track_break_list, track_break));

    force_redraw();
}

void track_break_add_offset( char* filename, gulong offset)
{
    TrackBreak *track_break = NULL;

    if (sample_filename == NULL) {
        return;
    }

    if( offset > graphData.numSamples) {
        if( filename != NULL) {
            printf( "Offset for %s is too big, skipping.\n", filename);
        }
        return;
    }

    if( !(track_break = (TrackBreak *)g_malloc(sizeof(TrackBreak)))) {
        printf("couldn't malloc enough memory for track_break\n");
        return;
    }

    track_break->editable = TRUE;
    track_break->offset = offset;
    offset_to_time( track_break->offset, track_break->time, FALSE);

    if( filename == NULL) {
        track_break->write = 0;
        track_break->filename = NULL;
    }
    else {
	if( filename == (void *)-1 ) {
	    track_break->write = 1;
	    track_break->filename = NULL;
	}
	else {
	    track_break->write = 1;
	    track_break->filename = g_strdup( filename);
	}
    }

    track_break_list = g_list_insert_sorted( track_break_list, track_break,
                                             track_break_sort);

    track_break_set_durations();
    track_break_rename( FALSE);
}

void track_break_set_durations() {
    g_list_foreach(track_break_list, track_break_set_duration, NULL);
    gtk_list_store_clear(store);
    g_list_foreach(track_break_list, track_break_add_to_model, NULL);
}

void track_break_rename( gboolean overwrite) {
    if (sample_filename == NULL) {
        return;
    }

    /* do we have to overwrite already-set names or just NULL names? */
    overwrite_track_names = overwrite;

    /* setup the filename */
    gchar *str_ptr = g_path_get_basename(sample_filename);
    gchar *end = strrchr(str_ptr, '.');
    if (end) {
        *end = '\0';
    }

    g_list_foreach(track_break_list, track_break_setup_filename, str_ptr);
    gtk_list_store_clear(store);
    g_list_foreach(track_break_list, track_break_add_to_model, NULL);
    g_free(str_ptr);

    redraw();

/* DEBUG CODE START */
/*
    g_list_foreach(track_break_list, track_break_print_element, NULL);
    g_print("\n");
*/
/* DEBUG CODE END */
}

void track_break_write_toggled(GtkWidget *widget,
                               gchar *path_str,
                               gpointer user_data)
{
    GtkTreeModel *model = (GtkTreeModel *)user_data;
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    TrackBreak *track_break;
    guint list_pos;
    gpointer list_data;

    list_pos = atoi(path_str);
    list_data = g_list_nth_data(track_break_list, list_pos);
    track_break = (TrackBreak *)list_data;
    track_break->write = !track_break->write;

/* DEBUG CODE START */
/*
    g_print("gtktreepath: %s\n", path_str);
    g_print("list_pos: %d\n", list_pos);
    g_print("track_break->offset: %d\n", track_break->offset);
    g_print("track_break->write: %d\n\n", track_break->write);
*/
/* DEBUG CODE END */

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_WRITE,
                       track_break->write, -1);

    gtk_tree_path_free(path);
    force_redraw();
}

void track_break_filename_edited(GtkCellRendererText *cell,
                                 const gchar *path_str,
                                 const gchar *new_text,
                                 gpointer user_data)
{
    GtkTreeModel *model = GTK_TREE_MODEL(user_data);
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    TrackBreak *track_break;
    guint list_pos;
    gpointer list_data;

    list_pos = atoi(path_str);
    list_data = g_list_nth_data(track_break_list, list_pos);
    track_break = (TrackBreak *)list_data;
    track_break->filename = g_strdup(new_text);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_FILENAME,
                       track_break->filename, -1);

    gtk_tree_path_free(path);

    force_redraw();
}

void track_break_start_time_edited(GtkCellRendererText *cell,
                                   const gchar *path_str,
                                   const gchar *new_text,
                                   gpointer user_data)
{
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    TrackBreak *track_break;
    guint list_pos;
    gpointer list_data;

    list_pos = atoi(path_str);
    list_data = g_list_nth_data(track_break_list, list_pos);
    track_break = (TrackBreak *)list_data;
    printf("new time: %s\n", new_text);
    printf("old time: %s\n", track_break->time);
    /*
    track_break->filename = g_strdup(new_text);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_FILENAME,
                       track_break->filename, -1);
                       */

    gtk_tree_path_free(path);

    force_redraw();
}

/*
 *-------------------------------------------------------------------------
 * File Save Dialog Stuff
 *-------------------------------------------------------------------------
 */
gboolean
file_write_progress_idle_func(gpointer data) {
    static GtkWidget *window;
    static GtkWidget *pbar;
    static GtkWidget *vbox;
    static GtkWidget *label;
    static GtkWidget *status_label;
    char *str_ptr;
    static int cur_file_displayed = 0;
    static double fraction;

    if (write_info.check_file_exists) {
        if (window != NULL) {
            gtk_widget_destroy(window);
            window = NULL;
        }
        write_info.sync_check_file_overwrite_to_write_progress = 1;
        write_info.check_file_exists = 0;
        overwritedialog_show( wavbreaker_get_main_window(), &write_info);

        return TRUE;
    }

    if (write_info.sync_check_file_overwrite_to_write_progress) {
        return TRUE;
    }

    if (window == NULL) {
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_widget_realize(window);
        gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
        gtk_window_set_modal(GTK_WINDOW(window), TRUE);
        gtk_window_set_transient_for(GTK_WINDOW(window),
                GTK_WINDOW(main_window));
        gtk_window_set_type_hint(GTK_WINDOW(window),
                GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_position(GTK_WINDOW(window),
                GTK_WIN_POS_CENTER_ON_PARENT);
        gdk_window_set_functions(GDK_WINDOW(gtk_widget_get_window(window)), GDK_FUNC_MOVE);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(window), vbox);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

        gtk_window_set_title( GTK_WINDOW(window), _("Splitting wave file"));

        gchar *tmp_str = g_markup_printf_escaped("<span size=\"larger\" weight=\"bold\">%s</span>",
                gtk_window_get_title(GTK_WINDOW(window)));

        label = gtk_label_new( NULL);
        gtk_label_set_markup( GTK_LABEL(label), tmp_str);
        g_free(tmp_str);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 5);

        label = gtk_label_new( _("The selected track breaks are now written to disk. This can take some time."));
        gtk_label_set_line_wrap( GTK_LABEL(label), TRUE);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 5);

        pbar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), pbar, FALSE, TRUE, 5);

        status_label = gtk_label_new( NULL);
        g_object_set(G_OBJECT(status_label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_label_set_ellipsize( GTK_LABEL(status_label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, TRUE, 5);

        gtk_widget_show_all(GTK_WIDGET(window));
        cur_file_displayed = -1;
    }

    if (write_info.sync) {
        write_info.sync = 0;
        gtk_widget_destroy(window);
        window = NULL;

        gchar *tmp_str;
        if( write_info.num_files > 1) {
            tmp_str = g_strdup_printf(_("The file %s has been split into %d parts."), basename( sample_filename), write_info.num_files);
        } else {
            tmp_str = g_strdup_printf(_("The file %s has been split into one part."), basename( sample_filename));
        }
        popupmessage_show( NULL, _("Operation successful"), tmp_str);
        g_free(tmp_str);

        file_write_progress_source_id = 0;
        return FALSE;
    }

    if (cur_file_displayed != write_info.cur_file) {
        str_ptr = basename( write_info.cur_filename);

        if( str_ptr == NULL) {
            str_ptr = write_info.cur_filename;
        }

        gchar *fn = g_markup_escape_text(str_ptr, -1);
        gchar *tmp = g_strdup_printf(_("Writing %s"), fn);
        g_free(fn);
        gchar *msg = g_markup_printf_escaped("<i>%s</i>", tmp);
        g_free(tmp);

        gtk_label_set_markup(GTK_LABEL(status_label), msg);
        g_free(msg);

        cur_file_displayed = write_info.cur_file;
    }

    fraction = 1.00*(write_info.cur_file-1+write_info.pct_done)/write_info.num_files;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), fraction);

    gchar *tmp_str;
    // FIXME: i18n plural forms
    if( write_info.num_files > 1) {
        tmp_str = g_strdup_printf(_("%d of %d parts written"), write_info.cur_file-1, write_info.num_files);
    } else {
        tmp_str = g_strdup_printf(_("%d of 1 part written"), write_info.cur_file-1);
    }
    gtk_progress_bar_set_text( GTK_PROGRESS_BAR(pbar), tmp_str);
    g_free(tmp_str);

    return TRUE;
}

gboolean
file_play_progress_idle_func(gpointer data) {
    GtkAllocation allocation;
    gtk_widget_get_allocation(draw, &allocation);
    gint half_width = allocation.width / 2;
    gint offset = allocation.width * (1.0/PLAY_MARKER_SCROLL);

    gint x = play_marker - half_width;
    gint y = play_marker - pixmap_offset;
    gint z = allocation.width * (1.0 - 1.0/PLAY_MARKER_SCROLL);

    if (y > z && x > 0) {
        reset_sample_display(play_marker - offset + half_width);
    } else if (pixmap_offset > play_marker) {
        reset_sample_display(play_marker);
    }

    gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), pixmap_offset);
    gtk_widget_queue_draw(scrollbar);

    redraw();
    update_status(FALSE);

    if (sample_is_playing()) {
        return TRUE;
    } else {
        set_play_icon();
        play_progress_source_id = 0;
        return FALSE;
    }
}

/*
 *-------------------------------------------------------------------------
 * File Open Dialog Stuff
 *-------------------------------------------------------------------------
 */

gboolean
file_open_progress_idle_func(gpointer data) {
    static GtkWidget *window;
    static GtkWidget *pbar;
    static GtkWidget *vbox;
    static GtkWidget *label;
    static char tmp_str[6144];
    static char tmp_str2[6144];
    static int current, size;

    if (window == NULL) {
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_widget_realize(window);
        gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
        gtk_window_set_modal(GTK_WINDOW(window), TRUE);
        gtk_window_set_transient_for(GTK_WINDOW(window),
                GTK_WINDOW(main_window));
        gtk_window_set_type_hint(GTK_WINDOW(window),
                GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_position(GTK_WINDOW(window),
                GTK_WIN_POS_CENTER_ON_PARENT);
        gdk_window_set_functions(GDK_WINDOW(gtk_widget_get_window(window)), GDK_FUNC_MOVE);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(window), vbox);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

        gtk_window_set_title( GTK_WINDOW(window), _("Analyzing waveform"));

        tmp_str[0] = '\0';
        strcat( tmp_str, "<span size=\"larger\" weight=\"bold\">");
        strcat( tmp_str, gtk_window_get_title( GTK_WINDOW( window)));
        strcat( tmp_str, "</span>");

        label = gtk_label_new( NULL);
        gtk_label_set_markup( GTK_LABEL(label), tmp_str);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 5);

        label = gtk_label_new( _("The waveform data of the selected file is being analyzed and processed. This can take some time."));
        gtk_label_set_line_wrap( GTK_LABEL(label), TRUE);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 5);

        pbar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), pbar, FALSE, TRUE, 5);

        sprintf( tmp_str, _("Analyzing %s"), basename( sample_filename));
        gchar *str = g_markup_escape_text(tmp_str, -1);
        sprintf( tmp_str2, "<i>%s</i>", str);
        g_free(str);

        label = gtk_label_new( NULL);
        gtk_label_set_markup( GTK_LABEL(label), tmp_str2);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_label_set_ellipsize( GTK_LABEL(label), PANGO_ELLIPSIZE_END);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 5);

        gtk_widget_show_all(GTK_WIDGET(window));
    }

    if (progress_pct >= 1.0) {
        progress_pct = 0;

        gtk_widget_destroy(window);
        window = NULL;

        /* --------------------------------------------------- */
        /* Reset things because we have a new file             */
        /* --------------------------------------------------- */

        cursor_marker = 0;
        track_break_clear_list();
        track_break_add_entry();

        gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), 0);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(cursor_marker_spinner_adj), 0);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(cursor_marker_min_spinner_adj), 0);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(cursor_marker_sec_spinner_adj), 0);
        gtk_adjustment_set_value(GTK_ADJUSTMENT(cursor_marker_subsec_spinner_adj), 0);

        gtk_widget_queue_draw(scrollbar);

        /* TODO: Remove FIX !!!!!!!!!!! */
        configure_event(draw, NULL, NULL);

#if defined(WANT_MOODBAR)
        if (moodbarData) {
            moodbar_free(moodbarData);
        }
        moodbarData = moodbar_open(sample_filename);
        set_action_enabled("display_moodbar", moodbarData != NULL);
        set_action_enabled("generate_moodbar", moodbarData == NULL);
#endif

        redraw();

        /* --------------------------------------------------- */

        file_open_progress_source_id = 0;
        return FALSE;

    } else {
        size = sample_stat.st_size/(1024*1024);
        current = size*progress_pct;
        sprintf( tmp_str, _("%d of %d MB analyzed"), current, size);
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), progress_pct);
        gtk_progress_bar_set_text( GTK_PROGRESS_BAR(pbar), tmp_str);

        return TRUE;
    }
}

static void open_file() {
    if (sample_open_file(sample_filename, &graphData, &progress_pct) != 0) {
        char *message = sample_get_error_message();
        popupmessage_show(main_window, _("Error opening file"), message);
        sample_filename = NULL;
        g_free(message);
        return;
    }

    set_action_enabled("add_break", TRUE);
    set_action_enabled("jump_cursor", TRUE);

    set_action_enabled("check_all", TRUE);
    set_action_enabled("check_none", TRUE);
    set_action_enabled("check_invert", TRUE);

    set_action_enabled("auto_rename", TRUE);
    set_action_enabled("remove_break", TRUE);
    set_action_enabled("jump_break", TRUE);

    set_action_enabled("export", TRUE);
    set_action_enabled("import", TRUE);

#if defined(WANT_MOODBAR)
    set_action_enabled("display_moodbar", moodbarData != NULL);
    set_action_enabled("generate_moodbar", moodbarData == NULL);
#endif
    gtk_widget_set_sensitive( play_button, TRUE);
    gtk_widget_set_sensitive( header_bar_save_button, TRUE);

    gtk_widget_set_sensitive( cursor_marker_spinner, TRUE);
    gtk_widget_set_sensitive( cursor_marker_min_spinner, TRUE);
    gtk_widget_set_sensitive( cursor_marker_sec_spinner, TRUE);
    gtk_widget_set_sensitive( cursor_marker_subsec_spinner, TRUE);

    gtk_widget_set_sensitive(button_seek_backward, TRUE);
    gtk_widget_set_sensitive(button_jump_to_time, TRUE);
    gtk_widget_set_sensitive(button_seek_forward, TRUE);
    gtk_widget_set_sensitive(button_auto_split, TRUE);
    gtk_widget_set_sensitive(button_add_break, TRUE);
    gtk_widget_set_sensitive(button_remove_break, TRUE);

    menu_stop(NULL, NULL);

    if (file_open_progress_source_id) {
        g_source_remove(file_open_progress_source_id);
    }
    file_open_progress_source_id = g_timeout_add(100, file_open_progress_idle_func, NULL);
    set_title( basename( sample_filename));
}

static gboolean
open_file_arg(gpointer data)
{
    if (data) {
        set_sample_filename((char *)data);
        g_free(data);
        open_file();
    }

    /* do not call this function again = return FALSE */
    open_file_source_id = 0;
    return FALSE;
}

static void set_title( char* title)
{
  char buf[1024];

  if( title == NULL) {
    gtk_window_set_title( (GtkWindow*)main_window, APPNAME);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), APPNAME);
    return;
  }

  sprintf( buf, "%s (%s)", APPNAME, title);
  gtk_window_set_title( (GtkWindow*)main_window, buf);
  gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), buf);
}

static void open_select_file() {
    GtkWidget *dialog;

    GtkFileFilter *filter_all;
    GtkFileFilter *filter_supported;

    filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_all, _("All files"));
    gtk_file_filter_add_pattern( filter_all, "*");

    filter_supported = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_supported, _("Supported files"));
    gtk_file_filter_add_pattern( filter_supported, "*.wav");
#if defined(HAVE_MPG123)
    gtk_file_filter_add_pattern( filter_supported, "*.mp3");
#endif
    gtk_file_filter_add_pattern( filter_supported, "*.dat");
    gtk_file_filter_add_pattern( filter_supported, "*.raw");

    dialog = gtk_file_chooser_dialog_new(_("Open File"), GTK_WINDOW(main_window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Open"), GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_all);
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_supported);
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER(dialog), filter_supported);

    if (sample_filename != NULL) {
        char* filename = g_strdup(sample_filename);
        char* dir = dirname(filename);

        if (dir != NULL && dir[0] != '.') {
            gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), dir);
        }

        g_free(filename);
    }

    if (gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;
        char *dirname;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        set_sample_filename(filename);
        g_free(filename);
        dirname = gtk_file_chooser_get_current_folder(GTK_FILE_CHOOSER(dialog));
        saveas_set_dirname(dirname);
        g_free(dirname);
        gtk_widget_destroy(dialog);
        open_file();
    } else {
        gtk_widget_destroy(dialog);
    }
}

void set_sample_filename(const char *f) {
    if (sample_filename != NULL) {
        g_free(sample_filename);
        sample_close_file();
    }
    sample_filename = g_strdup(f);
    stat( sample_filename, &sample_stat);
}

/*
 *-------------------------------------------------------------------------
 * Sample Drawing Area Stuff
 *-------------------------------------------------------------------------
 */

static inline void blit_cairo_surface(cairo_t *cr, cairo_surface_t *surface, int width, int height)
{
    if (!surface) {
        return;
    }

    cairo_set_source_surface(cr, surface, 0.f, 0.f);
    cairo_rectangle(cr, 0.f, 0.f, (float)width, (float)height);
    cairo_fill(cr);
}

static void force_redraw()
{
    waveform_surface_invalidate(sample_surface);
    waveform_surface_invalidate(summary_surface);

    redraw();
}

static void redraw()
{
    static int redraw_done = 1;

    if( redraw_done) {
        /* Only redraw if the last operation finished already. */
        redraw_done = 0;
        if (redraw_source_id) {
            g_source_remove(redraw_source_id);
        }
        redraw_source_id = g_idle_add(redraw_later, &redraw_done);
    }
}

static gboolean redraw_later( gpointer data)
{
    int *redraw_done = (int*)data;

    struct WaveformSurfaceDrawContext ctx = {
        .widget = draw,
        .pixmap_offset = pixmap_offset,
        .track_break_list = track_break_list,
        .graphData = &graphData,
        .moodbarData = appconfig_get_show_moodbar() ? moodbarData : NULL,
    };
    waveform_surface_draw(sample_surface, &ctx);
    gtk_widget_queue_draw(draw);

    ctx.widget = draw_summary;
    waveform_surface_draw(summary_surface, &ctx);
    gtk_widget_queue_draw(draw_summary);

    *redraw_done = 1;

    redraw_source_id = 0;
    return FALSE;
}

static gboolean configure_event(GtkWidget *widget,
    GdkEventConfigure *event, gpointer data)
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    int width = allocation.width;

    if (graphData.numSamples == 0) {
        pixmap_offset = 0;
        gtk_adjustment_set_page_size(adj, 1);
        gtk_adjustment_set_upper(adj, 1);
        gtk_adjustment_set_page_increment(adj, 1);
    } else if (width > graphData.numSamples) {
        pixmap_offset = 0;
        gtk_adjustment_set_page_size(adj, graphData.numSamples);
        gtk_adjustment_set_upper(adj, graphData.numSamples);
        gtk_adjustment_set_page_increment(adj, width / 2);
    } else {
        if (pixmap_offset + width > graphData.numSamples) {
            pixmap_offset = graphData.numSamples - width;
        }
        gtk_adjustment_set_page_size(adj, width);
        gtk_adjustment_set_upper(adj, graphData.numSamples);
        gtk_adjustment_set_page_increment(adj, width / 2);
    }

    gtk_adjustment_set_step_increment(adj, 10);
    gtk_adjustment_set_value(adj, pixmap_offset);
    gtk_adjustment_set_upper(cursor_marker_spinner_adj, graphData.numSamples - 1);

    struct WaveformSurfaceDrawContext ctx = {
        .widget = widget,
        .pixmap_offset = pixmap_offset,
        .track_break_list = track_break_list,
        .graphData = &graphData,
        .moodbarData = appconfig_get_show_moodbar() ? moodbarData : NULL,
    };
    waveform_surface_draw(sample_surface, &ctx);

    return TRUE;
}

static gboolean draw_draw_event(GtkWidget *widget, cairo_t *cr, gpointer data)
{
    GList *tbl;
    TrackBreak *tb_cur = NULL, *tb_first = NULL;
    TrackBreak **tbs;
    const int border = 3;
    int tbc = 0, i, border_left, border_right, text_height = 0, ellipsis_width = 0;
    gboolean label_truncated = FALSE;
    char tmp[1024];

    cairo_text_extents_t te;

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);

    guint width = allocation.width,
          height = allocation.height;

    blit_cairo_surface(cr, sample_surface->surface, width, height);

    cairo_set_line_width( cr, 1);
    if( cursor_marker >= pixmap_offset && cursor_marker <= pixmap_offset + width) {
        /**
         * Draw RED cursor marker
         **/
        float x = cursor_marker - pixmap_offset + 0.5f;

        cairo_set_source_rgba(cr, 1.f, 0.f, 0.f, 0.9f);
        cairo_move_to(cr, x, 0.f);
        cairo_line_to(cr, x, height);
        cairo_stroke(cr);

        static const float TRIANGLE_SIDE = 3.f;

        cairo_move_to(cr, x-TRIANGLE_SIDE, 0.f);
        cairo_line_to(cr, x+TRIANGLE_SIDE, 0.f);
        cairo_line_to(cr, x, 2.f*TRIANGLE_SIDE);

        cairo_move_to(cr, x-TRIANGLE_SIDE, height);
        cairo_line_to(cr, x+TRIANGLE_SIDE, height);
        cairo_line_to(cr, x, height-2.f*TRIANGLE_SIDE);

        cairo_fill(cr);
    }

    if (sample_is_playing()) {
        /**
         * Draw GREEN play marker
         **/
        float x = play_marker - pixmap_offset + 0.5f;

        cairo_set_source_rgba(cr, 0.f, 0.7f, 0.f, 0.9f);
        cairo_move_to(cr, x, 0.f);
        cairo_line_to(cr, x, height);
        cairo_stroke(cr);
    }

    /**
     * Prepare text output (filename labels)
     **/

    cairo_select_font_face( cr, "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size( cr, 10);

    cairo_text_extents( cr, "...", &te);
    ellipsis_width = te.width;

    /**
     * Find track breaks for which we need to draw labels
     **/

    tb_cur = NULL;
    tbs = NULL;;
    for( tbl = track_break_list; tbl != NULL; tbl = g_list_next( tbl)) {
        tb_cur = g_list_nth_data( track_break_list, g_list_position( track_break_list, tbl));
        if (tb_cur != NULL) {
            cairo_text_extents( cr, tb_cur->filename, &te);

            if( pixmap_offset <= tb_cur->offset) {
                if( tbs == NULL && tb_first != NULL) {
                    tbs = (TrackBreak**)malloc( sizeof( TrackBreak*));
                    tbs[tbc] = tb_first;
                    tbc++;
                }
                tbs = (TrackBreak**)realloc( tbs, (tbc+1)*sizeof( TrackBreak*));
                tbs[tbc] = tb_cur;
                tbc++;
                if( te.height > text_height) {
                    text_height = te.height;
                }
            }

            tb_first = tb_cur;
        }
    }

    if( tbs == NULL && tb_first != NULL) {
        tbs = (TrackBreak**)malloc( sizeof( TrackBreak*));
        tbs[tbc] = tb_first;
        tbc++;
        text_height = te.height;
    }

    /**
     * Calculate label size (and if we need to draw it) and
     * finally draw the label with the right size and position
     **/

    for( i=0; i<tbc; i++) {
        if( !(tbs[i]->write)) {
            continue;
        }
        border_left = (tbs[i]->offset > pixmap_offset)?(tbs[i]->offset - pixmap_offset):(0);
        border_right = (i+1 == tbc)?(width+100):(tbs[i+1]->offset - pixmap_offset);
        strcpy( tmp, tbs[i]->filename);
        cairo_text_extents( cr, tmp, &te);
        label_truncated = FALSE;
        while( border_left + te.width + ellipsis_width + border*2 > border_right - border*2 && strlen( tmp) > 1) {
            tmp[strlen( tmp)-1] = '\0';
            cairo_text_extents( cr, tmp, &te);
            label_truncated = TRUE;
        }
        if( label_truncated) {
            strcat( tmp, "...");
            cairo_text_extents( cr, tmp, &te);
            if( border_left + te.width + border*2 > border_right - border*2) {
                border_left -= (border_left + te.width + border*2) - (border_right - border*2);
            }
        }

        cairo_set_source_rgba(cr, 1.f, 1.f, 1.f, 0.8f);
        cairo_rectangle(cr, border_left, height - text_height - border*2, te.width + border*2, text_height + border*2);
        cairo_fill(cr);

        cairo_set_source_rgb(cr, 0.f, 0.f, 0.f);
        cairo_move_to(cr, border_left + border, height - (text_height+1)/2);
        cairo_show_text(cr, tmp);
    }

    if( tbs != NULL) {
        free( tbs);
    }

    return FALSE;
}

static gboolean
draw_summary_configure_event(GtkWidget *widget,
                             GdkEventConfigure *event,
                             gpointer user_data)
{
    struct WaveformSurfaceDrawContext ctx = {
        .widget = widget,
        .pixmap_offset = pixmap_offset,
        .track_break_list = track_break_list,
        .graphData = &graphData,
        .moodbarData = appconfig_get_show_moodbar() ? moodbarData : NULL,
    };
    waveform_surface_draw(summary_surface, &ctx);

    return TRUE;
}

static gboolean
draw_summary_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    guint width = allocation.width,
          height = allocation.height;

    gfloat summary_scale;

    summary_scale = (float)(graphData.numSamples) / (float)(width);

    /**
     * Draw shadow in summary pixmap to show current view
     **/

    blit_cairo_surface(cr, summary_surface->surface, width, height);

    cairo_set_source_rgba( cr, 0, 0, 0, 0.3);
    cairo_rectangle( cr, 0, 0, pixmap_offset / summary_scale, height);
    cairo_fill( cr);
    cairo_rectangle( cr, (pixmap_offset+width) / summary_scale, 0, width - (pixmap_offset+width) / summary_scale, height);
    cairo_fill( cr);

    cairo_set_source_rgba( cr, 1, 1, 1, 0.6);
    cairo_set_line_width( cr, 1);
    cairo_move_to( cr, (int)(pixmap_offset / summary_scale) + 0.5, 0);
    cairo_line_to( cr, (int)(pixmap_offset / summary_scale) + 0.5, height);
    cairo_move_to( cr, (int)((pixmap_offset+width) / summary_scale) + 0.5, 0);
    cairo_line_to( cr, (int)((pixmap_offset+width) / summary_scale) + 0.5, height);
    cairo_stroke( cr);

    return FALSE;
}

static gboolean draw_summary_button_release(GtkWidget *widget,
    GdkEventButton *event, gpointer user_data)
{
    guint midpoint, width;
    int x_scale, x_scale_leftover, x_scale_mod;
    int leftover_count;

    if (sample_is_playing()) {
        return TRUE;
    }

    if (graphData.numSamples == 0) {
        return TRUE;
    }

    GtkAllocation allocation;
    gtk_widget_get_allocation(widget, &allocation);
    width = allocation.width;
    x_scale = graphData.numSamples / width;
    x_scale_leftover = graphData.numSamples % width;
    if (x_scale_leftover > 0) {
        x_scale_mod =  width / x_scale_leftover;
        leftover_count = event->x / x_scale_mod;
    } else {
        x_scale_mod =  0;
        leftover_count = 0;
    }

    if (event->x < 0) {
        return TRUE;
    }

    midpoint = event->x * x_scale + leftover_count;
    reset_sample_display(midpoint);
    redraw();

    return TRUE;
}

void reset_sample_display(guint midpoint)
{
    GtkAllocation allocation;
    gtk_widget_get_allocation(draw, &allocation);
    int width = allocation.width;
    int start = midpoint - width / 2;

    if (graphData.numSamples == 0) {
        pixmap_offset = 0;
    } else if (width > graphData.numSamples) {
        pixmap_offset = 0;
    } else if (start + width > graphData.numSamples) {
        pixmap_offset = graphData.numSamples - width;
    } else {
        pixmap_offset = start;
    }

    if (pixmap_offset < 0) {
        pixmap_offset = 0;
    }

    gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), pixmap_offset);
    gtk_widget_queue_draw(scrollbar);
}

/*
 *-------------------------------------------------------------------------
 * Scrollbar and Buttons
 *-------------------------------------------------------------------------
 */

static gboolean adj_value_changed(GtkAdjustment *adj, gpointer data)
{
    if (sample_is_playing()) {
        return FALSE;
    }

    pixmap_offset = gtk_adjustment_get_value(adj);

    redraw();

    return TRUE;
}

static void cursor_marker_time_spinners_changed(GtkAdjustment *adj, gpointer data)
{
    gint min, sec, subsec;

    if (sample_is_playing()) {
        return;
    }

    min = gtk_adjustment_get_value(cursor_marker_min_spinner_adj);
    sec = gtk_adjustment_get_value(cursor_marker_sec_spinner_adj);
    subsec = gtk_adjustment_get_value(cursor_marker_subsec_spinner_adj);

    cursor_marker = time_to_offset (min, sec, subsec);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON (cursor_marker_spinner), cursor_marker);

    if (gtk_widget_get_visible(jump_to_popover)) {
        reset_sample_display(cursor_marker);
    }

    redraw();
    update_status(FALSE);
}

static void cursor_marker_spinner_changed(GtkAdjustment *adj, gpointer data)
{
    if (sample_is_playing()) {
        return;
    }
    cursor_marker = gtk_adjustment_get_value(adj);
    /*
    printf("adj->value: %lu\n", adj->value);
    printf("cursor_marker: %lu\n", cursor_marker);
    printf("pixmap_offset: %lu\n", pixmap_offset);
    */

    update_status(TRUE);
    redraw();
}

static gboolean scroll_event( GtkWidget *widget, GdkEventScroll *event, gpointer data)
{
    long step, upper, size;

    step = gtk_adjustment_get_page_increment(adj);
    upper = gtk_adjustment_get_upper(adj);
    size = gtk_adjustment_get_page_size(adj);

    if( widget == draw) {
        /* Scroll in more detail on the zoomed view */
        step /= 2;
    }

    if( event->direction == GDK_SCROLL_UP || event->direction == GDK_SCROLL_LEFT) {
        if( pixmap_offset >= step) {
            pixmap_offset -= step;
        } else {
            pixmap_offset = 0;
        }
    }
    if( event->direction == GDK_SCROLL_DOWN || event->direction == GDK_SCROLL_RIGHT) {
        if( pixmap_offset <= upper-step-size) {
            pixmap_offset += step;
        } else {
            pixmap_offset = upper-size;
        }
    }

    gtk_adjustment_set_value( GTK_ADJUSTMENT(adj), pixmap_offset);
    gtk_widget_queue_draw( scrollbar);

    redraw();
    update_status(FALSE);

    return TRUE;
}

static gboolean button_release(GtkWidget *widget, GdkEventButton *event,
    gpointer data)
{
    gtk_widget_grab_focus(play_button);

    if (event->x + pixmap_offset > graphData.numSamples) {
        return TRUE;
    }

    if (sample_is_playing()) {
        return TRUE;
    }

    int w = gtk_widget_get_allocated_width(widget);

    int center = pixmap_offset + w/2;

    static const int MINIMUM_SCROLL_STEP = 10;
    static const int MAXIMUM_SCROLL_STEP = 50;

    if (event->x < 0) {
        // scroll left
        int offset = event->x;
        if (offset > -MINIMUM_SCROLL_STEP) {
            offset = -MINIMUM_SCROLL_STEP;
        } else if (offset < -MAXIMUM_SCROLL_STEP) {
            offset = -MAXIMUM_SCROLL_STEP;
        }

        reset_sample_display(center + offset);

        cursor_marker = pixmap_offset;
    } else if (event->x > w-1) {
        // scroll right
        int offset = event->x - (w-1);
        if (offset < MINIMUM_SCROLL_STEP) {
            offset = MINIMUM_SCROLL_STEP;
        } else if (offset > MAXIMUM_SCROLL_STEP) {
            offset = MAXIMUM_SCROLL_STEP;
        }

        reset_sample_display(center + offset);

        cursor_marker = pixmap_offset + w-1;
    } else {
        cursor_marker = pixmap_offset + event->x;
    }

    if (event->type == GDK_BUTTON_RELEASE && event->button == 3) {
        GMenu *menu_model = g_menu_new();

        g_menu_append(menu_model, _("Add track break"), "win.add_break");
        g_menu_append(menu_model, _("Remove track break"), "win.remove_break");
        g_menu_append(menu_model, _("Jump to cursor marker"), "win.jump_cursor");

        GtkMenu *menu = GTK_MENU(gtk_menu_new_from_model(G_MENU_MODEL(menu_model)));
        gtk_menu_attach_to_widget(menu, main_window, NULL);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), NULL);

        redraw();
        return TRUE;
    }

    TrackBreak *nearest_track_break = NULL;
    int nearest_track_break_index = 0;
    int containing_track_break_index = 0;
    {
        GList *cur = track_break_list;
        int idx = 0;

        nearest_track_break = cur ? cur->data : NULL;
        while (cur) {
            TrackBreak *tb = cur->data;
            if (ABS((long)tb->offset - (long)cursor_marker) < ABS((long)nearest_track_break->offset - (long)cursor_marker)) {
                nearest_track_break = tb;
                nearest_track_break_index = idx;
            }

            if (tb->offset < cursor_marker) {
                containing_track_break_index = idx;
            }

            cur = cur->next;
            ++idx;
        }
    }

    static const long SNAP_DISTANCE_FRAMES = 20;
    if (nearest_track_break && ABS((long)cursor_marker - (long)nearest_track_break->offset) < SNAP_DISTANCE_FRAMES) {
        // snap cursor to track break
        cursor_marker = nearest_track_break->offset;
        containing_track_break_index = nearest_track_break_index;
    }

    select_and_show_track_break(containing_track_break_index);

    gtk_adjustment_set_value(cursor_marker_spinner_adj, cursor_marker);

    /* DEBUG CODE START */
    /*
    printf("cursor_marker: %lu\n", cursor_marker);
    */
    /* DEBUG CODE END */

    update_status(FALSE);
    redraw();

    return TRUE;
}

static void offset_to_duration(guint start_time, guint end_time, gchar *str) {
    guint duration = end_time - start_time;
/*
printf("start time: %d\n", start_time);
printf("end time: %d\n", end_time);
*/
    offset_to_time(duration, str, FALSE);
}

static guint time_to_offset(gint min, gint sec, gint subsec) {
    guint offset;

    offset = (min * CD_BLOCKS_PER_SEC * 60) + sec * CD_BLOCKS_PER_SEC + subsec;

    return offset;
}

static void offset_to_time(guint time, gchar *str, gboolean time_offset_update) {

    int min, sec, subsec;

    if (time > 0) {
        min = time / (CD_BLOCKS_PER_SEC * 60);
        sec = time % (CD_BLOCKS_PER_SEC * 60);
        subsec = sec % CD_BLOCKS_PER_SEC;
        sec = sec / CD_BLOCKS_PER_SEC;
    } else {
        min = sec = subsec = 0;
    }

    if (time_offset_update) {
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (cursor_marker_min_spinner), min);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (cursor_marker_sec_spinner), sec);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (cursor_marker_subsec_spinner), subsec);
    }

    sprintf(str, "%d:%02d.%02d", min, sec, subsec);
}

static void update_status(gboolean update_time_offset) {
    char str[1024];
    char strbuf[1024];

    sprintf( str, _("Cursor"));
    strcat( str, ": ");
    offset_to_time(cursor_marker, strbuf, update_time_offset);
    strcat(str, strbuf);

    if (sample_is_playing()) {
        strcat( str, "\t");
        strcat( str, _("Playing"));
        strcat( str, ": ");
        offset_to_time(play_marker, strbuf, update_time_offset);
        strcat(str, strbuf);
    }

    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header_bar), str);
}

static void menu_play(GtkWidget *widget, gpointer user_data)
{
    if (sample_is_playing()) {
        menu_stop( NULL, NULL);
        update_status(FALSE);
        set_play_icon();
        return;
    }

    play_marker = cursor_marker;
    switch (play_sample(cursor_marker, &play_marker)) {
        case 0:
            if (play_progress_source_id) {
                g_source_remove(play_progress_source_id);
            }
            play_progress_source_id = g_timeout_add(10, file_play_progress_idle_func, NULL);
            set_stop_icon();
            break;
        case 1:
            printf("error in play_sample\n");
            break;
        case 2:
            menu_stop( NULL, NULL);
            update_status(FALSE);
            set_play_icon();
//            printf("already playing\n");
//            menu_stop(NULL, NULL);
//            play_sample(cursor_marker, &play_marker);
            break;
        case 3:
//            printf("must open sample file to play\n");
            break;
    }
}

static void set_stop_icon() {
    gtk_button_set_image(GTK_BUTTON(play_button),
            gtk_image_new_from_icon_name("media-playback-stop-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR));
}

static void set_play_icon() {
    gtk_button_set_image(GTK_BUTTON(play_button),
            gtk_image_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR));
}

static void menu_stop(GtkWidget *widget, gpointer user_data)
{
    stop_sample();
}

static void
menu_jump_to(GtkWidget *widget, gpointer user_data)
{
    gtk_popover_popup(GTK_POPOVER(jump_to_popover));
}

static void menu_next_silence( GtkWidget* widget, gpointer user_data)
{
    int i, c = SILENCE_MIN_LENGTH+1, v;
    int amp = graphData.minSampleAmp + (graphData.maxSampleAmp-graphData.minSampleAmp)*appconfig_get_silence_percentage()/100;

    for( i=cursor_marker+1; i<graphData.numSamples; i++) {
        v = graphData.data[i].max - graphData.data[i].min;
        if( v < amp) {
            c++;
        } else {
            c = 0;
        }

        if( c==SILENCE_MIN_LENGTH) {
            cursor_marker = i;
            jump_to_cursor_marker(NULL, NULL, NULL);
            update_status(FALSE);
            return;
        }
    }
}

static void menu_prev_silence( GtkWidget* widget, gpointer user_data)
{
    int i, c = SILENCE_MIN_LENGTH+1, v;
    int amp = graphData.minSampleAmp + (graphData.maxSampleAmp-graphData.minSampleAmp)*appconfig_get_silence_percentage()/100;

    for( i=cursor_marker-1; i>0; i--) {
        v = graphData.data[i].max - graphData.data[i].min;
        if( v < amp) {
            c++;
        } else {
            c = 0;
        }

        if( c==SILENCE_MIN_LENGTH) {
            cursor_marker = i;
            jump_to_cursor_marker(NULL, NULL, NULL);
            update_status(FALSE);
            return;
        }
    }
}

static void menu_save(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    if( sample_filename == NULL) {
        return;
    }

    if( appconfig_get_use_outputdir()) {
        wavbreaker_write_files( appconfig_get_outputdir());
    } else {
        wavbreaker_write_files( ".");
    }
}

static void menu_save_as(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    if( sample_filename == NULL) {
        return;
    }

    saveas_show(main_window);
}

void wavbreaker_write_files(char *dirname) {
    if (!sample_is_writing()) {
        sample_write_files(track_break_list, &write_info, dirname);

        file_write_progress_source_id = g_timeout_add(50, file_write_progress_idle_func, NULL);
    }
}

static void filter_changed (GtkFileChooser* chooser, gpointer user_data)
{
    gchar *old_filename;
    gchar *base;
    gchar *dot;
    gchar *new_filename;
    GtkFileFilter *filter;

    filter = gtk_file_chooser_get_filter (chooser);
    if (!filter) {
	return;
    }

    old_filename = gtk_file_chooser_get_uri( chooser);

    base = basename( old_filename);
    new_filename = malloc( strlen(base) + 4);
    strcpy( new_filename, base);
    dot = g_strrstr( new_filename, ".");
    if ( !dot) {
	dot = new_filename + strlen(new_filename);
    } else {
	*dot = '\0';
    }

    if( strcmp( gtk_file_filter_get_name( filter), _("Text files")) == 0) {
	strcat( new_filename, ".txt");
    } else if( strcmp( gtk_file_filter_get_name( filter), _("TOC files")) == 0) {
	strcat( new_filename, ".toc");
    } else if( strcmp( gtk_file_filter_get_name( filter), _("CUE files")) == 0) {
	strcat( new_filename, ".cue");
    }

    gtk_file_chooser_set_current_name( chooser, new_filename);

    g_free( old_filename);
    free( new_filename);
}

static void menu_export(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *dialog;
    GtkFileFilter *filter_text;
    GtkFileFilter *filter_toc;
    GtkFileFilter *filter_cue;
    gchar* filename = NULL;

    filename = g_strdup( sample_filename);
    strcpy( filename + strlen( filename) - 3, "txt");

    filter_text = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_text, _("Text files"));
    gtk_file_filter_add_pattern( filter_text, "*.txt");

    filter_toc = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_toc, _("TOC files"));
    gtk_file_filter_add_pattern( filter_toc, "*.toc");

    filter_cue = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_cue, _("CUE files"));
    gtk_file_filter_add_pattern( filter_cue, "*.cue");

    dialog = gtk_file_chooser_dialog_new(_("Export track breaks to file"), GTK_WINDOW(main_window),
        GTK_FILE_CHOOSER_ACTION_SAVE,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Save"), GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_text);
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_toc);
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_cue);
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER(dialog), filter_text);

    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dialog), basename( filename));
    gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), dirname( filename));

    g_signal_connect ( GTK_FILE_CHOOSER(dialog), "notify::filter", G_CALLBACK( filter_changed), NULL);

    if (gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        track_breaks_export_to_file( gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog)));
    }

    g_free( filename);

    gtk_widget_destroy(dialog);
}

void menu_delete_track_break(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    track_break_delete_entry();
}

void menu_import(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GtkWidget *dialog;
    GtkFileFilter *filter_all;
    GtkFileFilter *filter_supported;
    gchar* filename = NULL;
    int rc;

    filename = g_strdup( sample_filename);
    strcpy( filename + strlen( filename) - 3, "txt");

    filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_all, _("All files"));
    gtk_file_filter_add_pattern( filter_all, "*");

    filter_supported = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_supported, _("Supported files"));
    gtk_file_filter_add_pattern( filter_supported, "*.txt");
    gtk_file_filter_add_pattern( filter_supported, "*.toc");
    gtk_file_filter_add_pattern( filter_supported, "*.cue");

    dialog = gtk_file_chooser_dialog_new(_("Import track breaks from file"), GTK_WINDOW(main_window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Open"), GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_all);
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_supported);
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER(dialog), filter_supported);

    gtk_file_chooser_set_filename( GTK_FILE_CHOOSER(dialog), filename);

    if (gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
	gchar const *selected = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog));
	if (g_str_has_suffix( selected, ".toc") || g_str_has_suffix( selected, ".TOC")) {
	    rc = toc_read_file( selected, track_break_list);
	    if( rc ) {
		popupmessage_show( main_window, _("Import failed"), _("There has been an error importing track breaks from the TOC file."));
	    }
	} else if (g_str_has_suffix( selected, ".cue") || g_str_has_suffix( selected, ".CUE")) {
	    rc = cue_read_file( selected, track_break_list);
	    if( rc ) {
		popupmessage_show( main_window, _("Import failed"), _("There has been an error importing track breaks from the CUE file."));
	    }
	} else {
	    track_breaks_load_from_file( selected);
	}
    }

    g_free( filename);

    gtk_widget_destroy(dialog);

    force_redraw();
}

void menu_add_track_break(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    track_break_add_entry();
}

static void
menu_open_file(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    open_select_file();
}

static void
menu_menu(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    gtk_popover_popup(GTK_POPOVER(menu_popover));
}

#if defined(WANT_MOODBAR)
static void
menu_view_moodbar(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    GVariant *state = g_action_get_state(G_ACTION(action));
    gboolean new_value = !g_variant_get_boolean(state);
    g_variant_unref(state);

    g_action_change_state(G_ACTION(action), g_variant_new("b", new_value));
    appconfig_set_show_moodbar(new_value);

    if (moodbarData) {
        moodbar_free(moodbarData);
    }
    moodbarData = moodbar_open(sample_filename);
    set_action_enabled("display_moodbar", moodbarData != NULL);
    set_action_enabled("generate_moodbar", moodbarData == NULL);

    redraw();
}

static void
menu_moodbar(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    (void)moodbar_generate(main_window, sample_filename);

    if (moodbarData) {
        moodbar_free(moodbarData);
    }
    moodbarData = moodbar_open(sample_filename);
    set_action_enabled("display_moodbar", moodbarData != NULL);
    set_action_enabled("generate_moodbar", moodbarData == NULL);

    redraw();
}
#endif

static void
menu_about(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    about_show(main_window);
}

static void
menu_config(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    appconfig_show(main_window);
}

static void
menu_merge(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    guimerge_show(main_window);
}

static void
menu_autosplit(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    gtk_popover_popup(GTK_POPOVER(autosplit_popover));
}

static void
menu_rename(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    /* rename (AND overwrite) track breaks */
    track_break_rename( TRUE);
}

static void save_window_sizes()
{
    gint x, y, w, h;

    gdk_window_get_root_origin(GDK_WINDOW(gtk_widget_get_window(main_window)), &x, &y);
    gtk_window_get_size(GTK_WINDOW(main_window), &w, &h);
    /*
    g_print("w: %d\n", w);
    g_print("h: %d\n", h);
    */
    appconfig_set_main_window_xpos(x);
    appconfig_set_main_window_ypos(y);
    appconfig_set_main_window_width(w);
    appconfig_set_main_window_height(h);
    appconfig_set_vpane1_position(gtk_paned_get_position(GTK_PANED(vpane1)));
    appconfig_set_vpane2_position(gtk_paned_get_position(GTK_PANED(vpane2)));
}

void wavbreaker_quit() {
    stop_sample();

    if (file_write_progress_source_id) {
        g_source_remove(file_write_progress_source_id);
        file_write_progress_source_id = 0;
    }

    if (open_file_source_id) {
        g_source_remove(open_file_source_id);
        open_file_source_id = 0;
    }

    if (file_open_progress_source_id) {
        g_source_remove(file_open_progress_source_id);
        file_open_progress_source_id = 0;
    }

    if (redraw_source_id) {
        g_source_remove(redraw_source_id);
        redraw_source_id = 0;
    }

    if (play_progress_source_id) {
        g_source_remove(play_progress_source_id);
        play_progress_source_id = 0;
    }

    save_window_sizes();
    gtk_widget_destroy(main_window);
}

static void check_really_quit()
{
    if( sample_filename != NULL && g_list_length(track_break_list) != 1) {
        reallyquit_show(main_window);
    } else {
        wavbreaker_quit();
    }
}

GtkWidget *wavbreaker_get_main_window()
{
    return main_window;
}


/*
 *-------------------------------------------------------------------------
 * Main Window Events
 *-------------------------------------------------------------------------
 */

static gboolean
delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
    //g_print("delete_event event occurred\n");
    check_really_quit();
    return TRUE;
}

static GtkWidget *
make_time_offset_widget()
{
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    gtk_box_pack_start( GTK_BOX( hbox), gtk_label_new( _("Time offset:")), FALSE, FALSE, 0);

    cursor_marker_min_spinner_adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 1000.0, 1.0, 74.0, 0);
    cursor_marker_min_spinner = gtk_spin_button_new(cursor_marker_min_spinner_adj, 1.0, 0);
    gtk_widget_set_sensitive( cursor_marker_min_spinner, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), cursor_marker_min_spinner, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(cursor_marker_min_spinner_adj), "value-changed",
             G_CALLBACK(cursor_marker_time_spinners_changed), NULL);

    gtk_box_pack_start( GTK_BOX( hbox), gtk_label_new(":"), FALSE, FALSE, 0);

    cursor_marker_sec_spinner_adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 59.0, 1.0, 74.0, 0);
    cursor_marker_sec_spinner = gtk_spin_button_new(cursor_marker_sec_spinner_adj, 1.0, 0);
    gtk_widget_set_sensitive( cursor_marker_sec_spinner, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), cursor_marker_sec_spinner, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(cursor_marker_sec_spinner_adj), "value-changed",
             G_CALLBACK(cursor_marker_time_spinners_changed), NULL);

    gtk_box_pack_start( GTK_BOX( hbox), gtk_label_new("."), FALSE, FALSE, 0);

    cursor_marker_subsec_spinner_adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, CD_BLOCKS_PER_SEC-1, 1.0, 74.0, 0);
    cursor_marker_subsec_spinner = gtk_spin_button_new(cursor_marker_subsec_spinner_adj, 1.0, 0);
    gtk_widget_set_sensitive( cursor_marker_subsec_spinner, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), cursor_marker_subsec_spinner, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(cursor_marker_subsec_spinner_adj), "value-changed",
             G_CALLBACK(cursor_marker_time_spinners_changed), NULL);

    gtk_widget_show_all(hbox);

    return hbox;
}

static void
do_startup(GApplication *application, gpointer user_data)
{
    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);

    appconfig_init();
}

static void
menu_check_all(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    parts_check_cb(NULL, GINT_TO_POINTER(CHECK_ALL));
}

static void
menu_check_none(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    parts_check_cb(NULL, GINT_TO_POINTER(CHECK_NONE));
}

static void
menu_check_invert(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    parts_check_cb(NULL, GINT_TO_POINTER(CHECK_INVERT));
}

static void
do_activate(GApplication *app, gpointer user_data)
{
    GtkWidget *vbox;
    GtkWidget *tbl_widget;

    GtkWidget *vpane_vbox;
    GtkWidget *list_vbox;
    GtkWidget *frame;

    GtkWidget *hbox;
    GtkWidget *button;

    sample_surface = waveform_surface_create_sample();
    summary_surface = waveform_surface_create_summary();

    main_window = gtk_application_window_new(GTK_APPLICATION(app));

    GActionEntry entries[] = {
        { "open", menu_open_file, NULL, NULL, NULL, },
        { "menu", menu_menu, NULL, NULL, NULL, },

        // TODO: "save" is currently unused
        { "save", menu_save, NULL, NULL, NULL, },
        { "save_to_folder", menu_save_as, NULL, NULL, NULL, },
        { "export", menu_export, NULL, NULL, NULL, },
        { "import", menu_import, NULL, NULL, NULL, },

        { "add_break", menu_add_track_break, NULL, NULL, NULL, },
        { "jump_cursor", jump_to_cursor_marker, NULL, NULL, NULL, },

#if defined(WANT_MOODBAR)
        { "display_moodbar", menu_view_moodbar, NULL, appconfig_get_show_moodbar()?"true":"false", NULL, },
        { "generate_moodbar", menu_moodbar, NULL, NULL, NULL, },
#endif

        { "check_all", menu_check_all, NULL, NULL, NULL, },
        { "check_none", menu_check_none, NULL, NULL, NULL, },
        { "check_invert", menu_check_invert, NULL, NULL, NULL, },

        { "auto_rename", menu_rename, NULL, NULL, NULL, },
        { "remove_break", menu_delete_track_break, NULL, NULL, NULL, },
        { "jump_break", jump_to_track_break, NULL, NULL, NULL, },
    };

    g_action_map_add_action_entries(G_ACTION_MAP(main_window),
            entries, G_N_ELEMENTS(entries),
            main_window);

    set_action_enabled("add_break", FALSE);
    set_action_enabled("jump_cursor", FALSE);

    set_action_enabled("check_all", FALSE);
    set_action_enabled("check_none", FALSE);
    set_action_enabled("check_invert", FALSE);

    set_action_enabled("auto_rename", FALSE);
    set_action_enabled("remove_break", FALSE);
    set_action_enabled("jump_break", FALSE);

    set_action_enabled("export", FALSE);
    set_action_enabled("import", FALSE);

#if defined(WANT_MOODBAR)
    set_action_enabled("display_moodbar", FALSE);
    set_action_enabled("generate_moodbar", FALSE);
#endif

    gtk_window_set_default_icon_name( PACKAGE);

    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), APPNAME);
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);

    GtkWidget *open_button = gtk_button_new_from_icon_name("document-open-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(open_button, _("Open file"));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(open_button), "win.open");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), open_button);

    GtkWidget *menu_button = gtk_button_new_from_icon_name("open-menu-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(menu_button, _("Open menu"));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(menu_button), "win.menu");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), menu_button);

    GMenu *top_menu = g_menu_new();

#if defined(WANT_MOODBAR)
    GMenu *display_menu = g_menu_new();
    g_menu_append(display_menu, _("Display moodbar"), "win.display_moodbar");
    g_menu_append(display_menu, _("Generate moodbar"), "win.generate_moodbar");
    g_menu_append_section(top_menu, NULL, G_MENU_MODEL(display_menu));
#endif

    GMenu *toc_menu = g_menu_new();
    g_menu_append(toc_menu, _("Import track breaks"), "win.import");
    g_menu_append(toc_menu, _("Export track breaks"), "win.export");
    g_menu_append_section(top_menu, NULL, G_MENU_MODEL(toc_menu));

    GMenu *tools_menu = g_menu_new();
    g_menu_append(tools_menu, _("Merge wave files"), "app.guimerge");
    g_menu_append_section(top_menu, NULL, G_MENU_MODEL(tools_menu));

    GMenu *prefs_menu = g_menu_new();
    g_menu_append(prefs_menu, _("Preferences"), "app.preferences");
    g_menu_append_section(top_menu, NULL, G_MENU_MODEL(prefs_menu));

    GMenu *about_menu = g_menu_new();
    g_menu_append(about_menu, _("About"), "app.about");
    g_menu_append_section(top_menu, NULL, G_MENU_MODEL(about_menu));

    menu_popover = gtk_popover_new_from_model(menu_button, G_MENU_MODEL(top_menu));
    gtk_popover_set_position(GTK_POPOVER(menu_popover), GTK_POS_BOTTOM);

    header_bar_save_button = GTK_WIDGET(gtk_button_new_from_icon_name("document-save-as-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(header_bar_save_button), "win.save_to_folder");
    gtk_widget_set_sensitive(header_bar_save_button, FALSE);
    gtk_widget_set_tooltip_text(header_bar_save_button, _("Save file parts"));
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), header_bar_save_button);

    gtk_window_set_titlebar(GTK_WINDOW(main_window), header_bar);

    set_title( NULL);

    g_signal_connect(G_OBJECT(main_window), "delete_event",
             G_CALLBACK(delete_event), NULL);

    gtk_container_set_border_width(GTK_CONTAINER(main_window), 0);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);

    /* paned view */
    vpane1 = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(vbox), vpane1, TRUE, TRUE, 0);

    /* vbox for the vpane */
    vpane_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_paned_pack1(GTK_PANED(vpane1), vpane_vbox, TRUE, TRUE);

    /* paned view */
    vpane2 = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(vpane_vbox), vpane2, TRUE, TRUE, 0);

    /* The summary_surface drawing area */
    draw_summary = gtk_drawing_area_new();

    g_signal_connect(G_OBJECT(draw_summary), "draw",
             G_CALLBACK(draw_summary_draw_event), NULL);
    g_signal_connect(G_OBJECT(draw_summary), "configure_event",
             G_CALLBACK(draw_summary_configure_event), NULL);
    g_signal_connect(G_OBJECT(draw_summary), "button_release_event",
             G_CALLBACK(draw_summary_button_release), NULL);
    g_signal_connect(G_OBJECT(draw_summary), "motion_notify_event",
             G_CALLBACK(draw_summary_button_release), NULL);
    g_signal_connect(G_OBJECT(draw_summary), "scroll-event",
             G_CALLBACK(scroll_event), NULL);

    gtk_widget_add_events(draw_summary, GDK_BUTTON_RELEASE_MASK);
    gtk_widget_add_events(draw_summary, GDK_BUTTON_PRESS_MASK);
    gtk_widget_add_events(draw_summary, GDK_BUTTON_MOTION_MASK);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    gtk_container_add(GTK_CONTAINER(frame), draw_summary);
    gtk_paned_add1(GTK_PANED(vpane2), frame);

    /* The sample_surface drawing area */
    draw = gtk_drawing_area_new();

    g_signal_connect(G_OBJECT(draw), "draw",
             G_CALLBACK(draw_draw_event), NULL);
    g_signal_connect(G_OBJECT(draw), "configure_event",
             G_CALLBACK(configure_event), NULL);
    g_signal_connect(G_OBJECT(draw), "button_release_event",
             G_CALLBACK(button_release), NULL);
    g_signal_connect(G_OBJECT(draw), "motion_notify_event",
             G_CALLBACK(button_release), NULL);
    g_signal_connect(G_OBJECT(draw), "scroll-event",
             G_CALLBACK(scroll_event), NULL);

    gtk_widget_add_events(draw, GDK_BUTTON_RELEASE_MASK);
    gtk_widget_add_events(draw, GDK_BUTTON_PRESS_MASK);
    gtk_widget_add_events(draw, GDK_BUTTON_MOTION_MASK);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);

    gtk_container_add(GTK_CONTAINER(frame), draw);
    gtk_paned_add2(GTK_PANED(vpane2), frame);
//    gtk_box_pack_start(GTK_BOX(vpane_vbox), draw, TRUE, TRUE, 5);

/* Add scrollbar */
    adj = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0, 100, 1, 10, 100));
    g_signal_connect(G_OBJECT(adj), "value_changed",
             G_CALLBACK(adj_value_changed), NULL);

    scrollbar = gtk_scrollbar_new(GTK_ORIENTATION_HORIZONTAL, GTK_ADJUSTMENT(adj));
    gtk_box_pack_start(GTK_BOX(vpane_vbox), scrollbar, FALSE, TRUE, 0);


/* vbox for the list */
    list_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(list_vbox), 5);
    gtk_paned_pack2(GTK_PANED(vpane1), list_vbox, FALSE, TRUE);

/* Add cursor marker spinner and track break add and delete buttons */
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(list_vbox), hbox, FALSE, FALSE, 0);

    play_button = gtk_button_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_sensitive(play_button, FALSE);
    gtk_widget_set_tooltip_text(play_button, _("Toggle playback"));
    gtk_box_pack_start(GTK_BOX(hbox), play_button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(play_button), "clicked", G_CALLBACK(menu_play), NULL);


    gtk_box_pack_start( GTK_BOX( hbox), gtk_label_new( _("Cursor position:")), FALSE, FALSE, 0);

    cursor_marker_spinner_adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 1000.0, 1.0, 74.0, 0);
    cursor_marker_spinner = gtk_spin_button_new(cursor_marker_spinner_adj, 1.0, 0);
    gtk_widget_set_sensitive( cursor_marker_spinner, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), cursor_marker_spinner, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(cursor_marker_spinner_adj), "value-changed",
             G_CALLBACK(cursor_marker_spinner_changed), NULL);

    //gtk_box_pack_start(GTK_BOX(hbox), make_time_offset_widget(), FALSE, FALSE, 0);

    // Jump Around
    GtkWidget *bbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(hbox), bbox, FALSE, FALSE, 0);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(bbox), GTK_BUTTONBOX_EXPAND);

    button = gtk_button_new_from_icon_name("media-seek-backward-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(button, _("Seek to previous silence"));
    gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
             G_CALLBACK(menu_prev_silence), NULL);
    button_seek_backward = button;

    button = gtk_button_new_from_icon_name("preferences-system-time-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(button, _("Jump to time"));
    gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
             G_CALLBACK(menu_jump_to), NULL);

    jump_to_popover = gtk_popover_new(button);
    gtk_popover_set_position(GTK_POPOVER(jump_to_popover), GTK_POS_BOTTOM);
    gtk_container_add(GTK_CONTAINER(jump_to_popover), make_time_offset_widget());

    button_jump_to_time = button;

    button = gtk_button_new_from_icon_name("media-seek-forward-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(button, _("Seek to next silence"));
    gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
             G_CALLBACK(menu_next_silence), NULL);
    button_seek_forward = button;

    // Spacer
    gtk_box_pack_start(GTK_BOX(hbox), gtk_label_new(""), TRUE, TRUE, 0);

    button = gtk_button_new_from_icon_name("edit-cut-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_tooltip_text(button, _("Auto-split by interval"));
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
             G_CALLBACK(menu_autosplit), NULL);
    autosplit_popover = gtk_popover_new(button);
    gtk_popover_set_position(GTK_POPOVER(autosplit_popover), GTK_POS_BOTTOM);
    gtk_container_add(GTK_CONTAINER(autosplit_popover), autosplit_create(GTK_POPOVER(autosplit_popover)));
    button_auto_split = button;

    gtk_box_pack_start(GTK_BOX(hbox), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

    /* add track break button */
    button = gtk_button_new_from_icon_name("list-add-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "win.add_break");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    button_add_break = button;

    /* delete track break button */
    button = gtk_button_new_from_icon_name("list-remove-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_actionable_set_action_name(GTK_ACTIONABLE(button), "win.remove_break");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    button_remove_break = button;

    /* Set buttons to be disabled initially */
    gtk_widget_set_sensitive(button_seek_forward, FALSE);
    gtk_widget_set_sensitive(button_jump_to_time, FALSE);
    gtk_widget_set_sensitive(button_seek_backward, FALSE);
    gtk_widget_set_sensitive(button_auto_split, FALSE);
    gtk_widget_set_sensitive(button_add_break, FALSE);
    gtk_widget_set_sensitive(button_remove_break, FALSE);

/* Track Break List */
    tbl_widget = track_break_create_list_gui();
    gtk_box_pack_start(GTK_BOX(list_vbox), tbl_widget, TRUE, TRUE, 0);

/* Finish up */

    write_info.cur_filename = NULL;

    sample_init();

    if (appconfig_get_main_window_xpos() > 0) {
        gtk_window_move (GTK_WINDOW (main_window),
                    appconfig_get_main_window_xpos(),
                    appconfig_get_main_window_ypos());
    }

    if( appconfig_get_main_window_width() > 0) {
        gtk_window_resize(GTK_WINDOW(main_window),
                appconfig_get_main_window_width(),
                appconfig_get_main_window_height());
        gtk_paned_set_position(GTK_PANED(vpane1), appconfig_get_vpane1_position());
        gtk_paned_set_position(GTK_PANED(vpane2), appconfig_get_vpane2_position());
    }

    gtk_widget_show_all( GTK_WIDGET(main_window));

    if (user_data) {
        open_file_source_id = g_idle_add(open_file_arg, user_data);
    }
}

static void
do_open(GApplication *application, gpointer files, gint n_files, gchar *hint, gpointer user_data)
{
    GFile **gfiles = (GFile **)files;
    for (int i=0; i<n_files; ++i) {
        const char *path = g_file_get_path(gfiles[i]);
        do_activate(application, strdup(path));
    }
}

static void
do_shutdown(GApplication *application, gpointer user_data)
{
    waveform_surface_free(sample_surface);
    waveform_surface_free(summary_surface);

    appconfig_write_file();
}

int
main(int argc, char *argv[])
{
    GtkApplication *app;
    int status;

    app = gtk_application_new("net.sourceforge.wavbreaker", G_APPLICATION_HANDLES_OPEN);
    g_signal_connect(app, "startup", G_CALLBACK (do_startup), NULL);
    g_signal_connect(app, "activate", G_CALLBACK (do_activate), NULL);
    g_signal_connect(app, "open", G_CALLBACK (do_open), NULL);
    g_signal_connect(app, "shutdown", G_CALLBACK (do_shutdown), NULL);

    static const GActionEntry entries[] = {
        { "guimerge", menu_merge, NULL, NULL, NULL, },
        { "preferences", menu_config, NULL, NULL, NULL },
        { "about", menu_about, NULL, NULL, NULL },
    };
    g_action_map_add_action_entries(G_ACTION_MAP(app), entries, G_N_ELEMENTS(entries), app);

    status = g_application_run(G_APPLICATION (app), argc, argv);
    g_object_unref(app);

    return status;
}

struct WriteStatus {
    FILE* fp;
    int index;
};

int track_breaks_export_to_file( char* filename) {
    FILE *fp = NULL;
    int write_err = -1;
    char *data_filename = NULL;

    data_filename = basename(sample_filename);

    if( g_str_has_suffix (filename, ".txt")) {

	fp = fopen( filename, "w");
	if( !fp) {
	    fprintf( stderr, "Error opening %s.\n", filename);
	    return -1;
	}

	fprintf( fp, "\n; Created by " PACKAGE " " VERSION "\n; http://thpinfo.com/2006/wavbreaker/tb-file-format.txt\n\n");

	g_list_foreach( track_break_list, track_break_write_text, fp);

	fprintf( fp, "\n; Total breaks: %d\n; Original file: %s\n\n", g_list_length( track_break_list), sample_filename);

	fclose( fp);

    } else if( g_str_has_suffix (filename, ".toc")) {

        write_err = toc_write_file( filename, data_filename, track_break_list);

        if( write_err) {
            popupmessage_show( main_window, _("Export failed"), _("There has been an error exporting track breaks to the TOC file."));
	    return -1;
        }

    } else if( g_str_has_suffix (filename, ".cue")) {

	fp = fopen( filename, "w");
	if( !fp) {
	    fprintf( stderr, "Error opening %s.\n", filename);
	    return -1;
	}

	fprintf( fp, "FILE \"%s\" WAVE\n", data_filename);

	struct WriteStatus ws;
	ws.fp = fp;
	ws.index = 1;

	g_list_foreach( track_break_list, track_break_write_cue, &ws);
	fclose( fp);

    } else {
	popupmessage_show( main_window, _("Export failed"), _("Unrecognised export type"));
	return -1;
    }

    return 0;
}

void track_break_write_text( gpointer data, gpointer user_data) {
    FILE* fp = (FILE*)user_data;
    TrackBreak* track_break = (TrackBreak*)data;

    if( track_break->write) {
        fprintf(fp, "%lu=%s\n", track_break->offset, track_break->filename);
    } else {
        fprintf(fp, "%lu\n", track_break->offset);
    }
}

void track_break_write_cue( gpointer data, gpointer user_data) {
    struct WriteStatus* ws = (struct WriteStatus*)user_data;
    TrackBreak* track_break = (TrackBreak*)data;
    char* time;
    char* p;

    time = strdup( track_break->time);
    p = time;
    while (*p != '\0') {
	if (*p == '.') {
	    *p = ':';
	}
	++p;
    }

    fprintf( ws->fp, "TRACK %02d AUDIO\n", ws->index);
    fprintf( ws->fp, "INDEX 01 %s\n", time);
    ws->index++;
    free( time);
}

int track_breaks_load_from_file( gchar const *filename) {
    FILE* fp;
    char tmp[1024];
    char* ptr;
    char* fname;
    int c;

    fp = fopen( filename, "r");
    if( !fp) {
        fprintf( stderr, "Error opening %s.\n", filename);
        return 1;
    }

    track_break_clear_list();

    ptr = tmp;
    while( !feof( fp)) {
        c = fgetc( fp);
        if( c == EOF)
            break;

        if( c == '\n') {
            *ptr = '\0';
            if( ptr != tmp && tmp[0] != ';') {
                fname = strchr( tmp, '=');
                if( fname == NULL) {
                    //DEBUG: printf( "Empty cut at %d\n", atoi( tmp));
                    track_break_add_offset(NULL, atol(tmp));
                } else {
                    *(fname++) = '\0';
                    while( *fname == ' ')
                        fname++;
                    //DEBUG: printf( "Cut at %d for %s\n", atoi( tmp), fname);
                    track_break_add_offset(fname, atol(tmp));
                }
            }
            ptr = tmp;
        } else {
            *ptr = c;
            ptr++;
            if( ptr > tmp+1024) {
                fprintf( stderr, "Error parsing file.\n");
                fclose( fp);
                return 1;
            }
        }
    }

    fclose( fp);
    force_redraw();
    return 0;
}

/** @param str Time in MM:SS:FF format (where there are CD_BLOCKS_PER_SEC frames per second).
 *  @return offset in frames.
 */
guint
msf_time_to_offset( gchar *str )
{
    guint   offset;
    int    mm = 0, ss = 0, ff = 0;
    int    consumed;

    consumed = sscanf(str, "%d:%d:%d", &mm, &ss, &ff);
    if (consumed != 3) {
	return 0;
    }

    offset  = mm * CD_BLOCKS_PER_SEC * 60;
    offset += ss * CD_BLOCKS_PER_SEC;
    offset += ff;

    return offset;
}
