/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2006 Timothy Robinson
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

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libgen.h>

#include "wavbreaker.h"
#include "sample.h"
#include "about.h"
#include "appconfig.h"
#include "autosplit.h"
#include "saveas.h"
#include "popupmessage.h"
//#include "cellrendererspin.h"

#define APPNAME "wavbreaker"

static GdkPixmap *sample_pixmap;
static GdkPixmap *summary_pixmap;
static GdkPixmap *cursor_pixmap;
static GdkPixmap *play_pixmap;

static GtkWidget *main_window;
static GtkWidget *vpane1, *vpane2;
static GtkWidget *scrollbar;
static GtkObject *adj;
static GtkWidget *draw;
static GtkWidget *draw_summary;
static GtkWidget *statusbar;

static GtkAdjustment *cursor_marker_spinner_adj;

static GraphData graphData;

GdkColor sample_colors[10];
GdkColor bg_color;
GdkColor axis_color;
GdkColor cursor_marker_color;
GdkColor play_marker_color;

static gulong cursor_marker;
static gulong play_marker;
static int pixmap_offset;
static char *sample_filename = NULL;
static gboolean overwrite_track_names = FALSE;

static guint idle_func_num;
static gdouble progress_pct;
static WriteInfo write_info;

typedef struct CursorData_ CursorData;

struct CursorData_ {
    gulong marker;
    gboolean is_equal;
    guint index;
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

/* File Functions */
void filesel_ok_clicked(GtkWidget *widget, gpointer data);
void filesel_cancel_clicked(GtkWidget *widget, gpointer data);
void set_sample_filename(const char *f);
static void open_file();
static gboolean open_file_arg( gpointer data);
static void handle_arguments( int argc, char** argv);
static void set_title( char* title);

/* Sample and Summary Display Functions */
static void redraw();
static void draw_sample(GtkWidget *widget);
static void draw_cursor_marker();
static void draw_play_marker();
static void draw_summary_pixmap(GtkWidget *widget);
static void reset_sample_display(guint);

static gboolean
configure_event(GtkWidget *widget,
                GdkEventConfigure *event,
                gpointer data);

static gboolean
expose_event(GtkWidget *widget,
             GdkEventExpose *event,
             gpointer data);


static gboolean
draw_summary_configure_event(GtkWidget *widget,
                             GdkEventConfigure *event,
                             gpointer user_data);

static gboolean
draw_summary_expose_event(GtkWidget *widget,
                          GdkEventExpose *event,
                          gpointer user_data);

static gboolean
draw_summary_button_release(GtkWidget *widget,
                            GdkEventButton *event,
                            gpointer user_data);

/* Menu Functions */
static void
menu_open_file(gpointer callback_data, guint callback_action,
               GtkWidget *widget);
static void
menu_delete_track_break(GtkWidget *widget, gpointer user_data);

static void
menu_save(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_save_as(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_about(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_config(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_export(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_autosplit(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_rename(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_play(GtkWidget *widget, gpointer user_data);

static void
menu_stop(GtkWidget *widget, gpointer user_data);

void
menu_add_track_break(GtkWidget *widget, gpointer user_data);

static void save_window_sizes();
static void check_really_quit();

static void
offset_to_time(guint, gchar *);

static void
offset_to_duration(guint, guint, gchar *);

static void
update_status();

static GtkItemFactoryEntry menu_items[] = {
  { "/_File",      NULL,         0,              0, "<Branch>"},
  { "/File/_Open...", "<control>O", menu_open_file, 0, "<StockItem>", GTK_STOCK_OPEN},
  { "/File/sep1",  NULL,         NULL,           0, "<Separator>"},
  { "/File/_Save", "<control>S", menu_save,      0, "<StockItem>", GTK_STOCK_SAVE},
  { "/File/Save _as...", "<shift><control>S", menu_save_as,      0, "<StockItem>", GTK_STOCK_SAVE_AS},
  { "/File/sep2",  NULL,         NULL,           0, "<Separator>"},
  { "/File/_Quit", "<control>Q", menu_quit,      0, "<StockItem>", GTK_STOCK_QUIT},

  { "/_Edit",             NULL, 0,           0, "<Branch>" },
  { "/Edit/_Add break", "<control>B", menu_add_track_break, 0, "<StockItem>", GTK_STOCK_ADD },
  { "/Edit/_Remove break", "<control>X", menu_delete_track_break, 0, "<StockItem>", GTK_STOCK_REMOVE },
  { "/Edit/_Rename all breaks", "<control>R", menu_rename, 0, "<StockItem>", GTK_STOCK_EDIT },
  { "/Edit/sep3",  NULL,         NULL,           0, "<Separator>"},
  { "/Edit/_AutoSplit", "<control>A", menu_autosplit, 0, "<StockItem>", GTK_STOCK_CUT },
  { "/Edit/_Export TOC", "<control>E", menu_export, 0, "<StockItem>", GTK_STOCK_CDROM },
  { "/Edit/sep4",  NULL,         NULL,           0, "<Separator>"},
  { "/Edit/_Preferences", "<control>P", menu_config, 0, "<StockItem>", GTK_STOCK_PREFERENCES },

  { "/_Help",       NULL, 0,          0, "<Branch>" },
  { "/Help/_About", NULL, menu_about, 0, "<StockItem>", GTK_STOCK_ABOUT },
};

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

void jump_to_cursor_marker(GtkWidget *widget, gpointer data) {
    reset_sample_display(cursor_marker);
    redraw();
}

void jump_to_track_break(GtkWidget *widget, gpointer data) {
    guint n = 0;

    n = track_break_find_offset();
    if (n <= graphData.numSamples) {
        reset_sample_display(n);
    }

    redraw();
}

void wavbreaker_autosplit(long x) {
    long n = x;

    while (n <= graphData.numSamples) {
        cursor_marker = n;
        track_break_add_entry();
        n += x;
    }
//    redraw();
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
    gtk_widget_set_size_request(sw, 350, 100);

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
    gtk_tree_view_column_set_title(column, "Write");
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "active", COLUMN_WRITE);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_FIXED);
    gtk_tree_view_column_set_fixed_width(column, 50);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* File Name Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    g_signal_connect(G_OBJECT(renderer), "edited", G_CALLBACK(track_break_filename_edited), store);
    gtk_tree_view_column_set_title(column, "File Name                               ");
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_FILENAME);
    gtk_tree_view_column_add_attribute(column, renderer, "editable", COLUMN_EDITABLE);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    //gtk_tree_view_column_set_fixed_width(column, 200);
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
    gtk_tree_view_column_set_title(column, "Time");
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_TIME);
    //gtk_tree_view_column_add_attribute(column, renderer, "editable", COLUMN_EDITABLE);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* File Duration Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title(column, "Duration");
    gtk_tree_view_column_pack_start(column, renderer, FALSE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_DURATION);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    /* File Offset Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title(column, "Offset");
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
    GtkWidget *menu;
    GtkWidget *delete_item;
    GtkWidget *jump_item;

    if (event->button != 3) {
        return FALSE;
    }

    menu = gtk_menu_new();

    delete_item = gtk_menu_item_new_with_label("Remove Selected Track Break");
    g_signal_connect(G_OBJECT(delete_item), "activate", G_CALLBACK(menu_delete_track_break), NULL);
    gtk_widget_show(delete_item);

    jump_item = gtk_menu_item_new_with_label("Jump to Selected Track Break");
    g_signal_connect(G_OBJECT(jump_item), "activate", G_CALLBACK(jump_to_track_break), NULL);
    gtk_widget_show(jump_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu), delete_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), jump_item);

    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, 
              event->button, event->time);

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
    printf("\toffset: %d\n", breakup->offset);
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

void track_break_find(gpointer data, gpointer user_data)
{
    TrackBreak *tb = (TrackBreak *) data;
    CursorData *cd = (CursorData *) user_data;

    if (cd->marker < tb->offset) {
        cd->index = g_list_index(track_break_list, data);
    }
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
    gchar *path_str;
    GtkTreeModel *model;
    GtkTreeIter iter;

    path_str = gtk_tree_path_to_string(path);
    list_pos = atoi(path_str);
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

    g_free(path_str);
    gtk_tree_path_free(path);
}

void
track_break_delete_entry()
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GList *list;
    gchar str_tmp[1024];
    gchar *str_ptr;

    if (sample_filename == NULL) {
        return;
    }

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));

    list = gtk_tree_selection_get_selected_rows(selection, &model);
    g_list_foreach(list, track_break_selection, NULL);
    g_list_free(list);

    track_break_set_durations();
    track_break_rename( FALSE);
}

void
track_break_setup_filename(gpointer data, gpointer user_data)
{
    TrackBreak *track_break = (TrackBreak *)data;
    gchar fn[128];
    gchar buf[128];
    int index;
    int disc_num;
    static int prev_disc_num;
    static int track_num = 1;

    /* when not overwriting track names, only update not-yet-set names */
    if( overwrite_track_names == FALSE && track_break->filename != NULL) {
        return;
    }

    index = g_list_index(track_break_list, track_break);
    index++;
    if (get_use_etree_filename_suffix()) {
        int cd_length = atoi(get_etree_cd_length());
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
        if (get_prepend_file_number()) {
            sprintf(buf, "%02d%s", index, get_etree_filename_suffix());
        } else {
            sprintf(buf, "%s%02d", get_etree_filename_suffix(), index);
        }
    }

    fn[0] = '\0';

    if (!get_use_etree_filename_suffix() && get_prepend_file_number()) {
        strcat(fn, buf);
        strcat(fn, (gchar *)user_data);
    } else {
        strcat(fn, (gchar *)user_data);
        strcat(fn, buf);
    }

    if (track_break->filename != NULL) {
        g_free(track_break->filename);
    }
    track_break->filename = g_strdup(fn);
}

void
track_break_add_to_model(gpointer data, gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeIter sibling;
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
    guint offset;
    gchar *time;
    gchar *duration;
    gchar *filename;

    if (sample_filename == NULL) {
        return;
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

void track_break_add_entry()
{
    gint list_pos = 0;
    TrackBreak *track_break = NULL;
    CursorData cursor_data;
    gchar str_tmp[1024];
    gchar *str_ptr;

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
    offset_to_time(cursor_marker, track_break->time);
    track_break->filename = NULL;

    track_break_list = g_list_insert_sorted(track_break_list, track_break,
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
    gchar str_tmp[1024];
    gchar *str_ptr;

    if (sample_filename == NULL) {
        return;
    }

    /* do we have to overwrite already-set names or just NULL names? */
    overwrite_track_names = overwrite;

    /* setup the filename */
    strncpy(str_tmp, sample_filename, 1024);
    str_ptr = basename(str_tmp);
    strncpy(str_tmp, str_ptr, 1024);
    str_ptr = strrchr(str_tmp, '.');
    if (str_ptr != NULL) {
        *str_ptr = '\0';
    }
    g_list_foreach(track_break_list, track_break_setup_filename, str_tmp);
    gtk_list_store_clear(store);
    g_list_foreach(track_break_list, track_break_add_to_model, NULL);

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
}

void track_break_start_time_edited(GtkCellRendererText *cell,
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
    printf("new time: %s\n", new_text);
    printf("old time: %s\n", track_break->time);
    /*
    track_break->filename = g_strdup(new_text);

    gtk_tree_model_get_iter(model, &iter, path);
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_FILENAME,
                       track_break->filename, -1);
                       */

    gtk_tree_path_free(path);
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
    static GtkWidget *filename_label;
    static char tmp_str[1024];
    static char str[1024];
    char *str_ptr;
    static int cur_file_displayed = 0;

    if (write_info.check_file_exists) {
        gdk_threads_enter();

        if (window != NULL) {
            gtk_widget_destroy(window);
            window = NULL;
        }
        write_info.sync_check_file_overwrite_to_write_progress = 1;
        write_info.check_file_exists = 0;
        overwritedialog_show(wavbreaker_get_main_window(), &write_info);

        gdk_threads_leave();

        return TRUE;
    }

    if (write_info.sync_check_file_overwrite_to_write_progress) {
        return TRUE;
    }

    if (window == NULL) {
        gdk_threads_enter();
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
        gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(window), vbox);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
        gtk_widget_show(vbox);

        filename_label = gtk_label_new(NULL);
        gtk_box_pack_start(GTK_BOX(vbox), filename_label, FALSE, TRUE, 5);
        gtk_widget_show(filename_label);
        str[0] = '\0';
        tmp_str[0] = '\0';
        strcat(str, "Writing: ");

        str_ptr = basename(write_info.cur_filename);
        if (str_ptr != NULL) {
            strcat(str, str_ptr);
        } else {
            strcat(str, write_info.cur_filename);
        }
        sprintf(tmp_str, " (%d of %d)", write_info.cur_file,
                write_info.num_files);
        strcat(str, tmp_str);
        gtk_label_set_text(GTK_LABEL(filename_label), str);

        pbar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), pbar, FALSE, TRUE, 5);
        gtk_widget_show(pbar);

        gtk_widget_show(window);
        gdk_threads_leave();
    }

    if (write_info.sync) {
        gdk_threads_enter();

        write_info.sync = 0;
        gtk_widget_destroy(window);
        window = NULL;

        gdk_threads_leave();
        return FALSE;
    }

    if (cur_file_displayed != write_info.cur_file) {
        gdk_threads_enter();

        str[0] = '\0';
        tmp_str[0] = '\0';
        strcat(str, "Writing: ");
        str_ptr = basename(write_info.cur_filename);
        if (str_ptr != NULL) {
            strcat(str, str_ptr);
        } else {
            strcat(str, write_info.cur_filename);
        }
        sprintf(tmp_str, " (%d of %d)", write_info.cur_file,
                write_info.num_files);
        strcat(str, tmp_str);
        gtk_label_set_text(GTK_LABEL(filename_label), str);

        gdk_threads_leave();
    }

    gdk_threads_enter();
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar),
            write_info.pct_done);
    gdk_threads_leave();

    return TRUE;
}

gboolean
file_play_progress_idle_func(gpointer data) {
    gint n = play_marker + draw->allocation.width / 2;
    gint m = graphData.numSamples;
    gint x = play_marker - draw->allocation.width / 2;
    gint y = play_marker - pixmap_offset;
    gint z = draw->allocation.width / 2;

    if (y > z && x > 0 && n <= m) {
        pixmap_offset = play_marker - draw->allocation.width / 2;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), pixmap_offset);
        gtk_widget_queue_draw(scrollbar);
    } else if (pixmap_offset > play_marker) {
        pixmap_offset = 0;
        gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), pixmap_offset);
        gtk_widget_queue_draw(scrollbar);
    }

    redraw();
    update_status();

    if (sample_get_playing()) {
        return TRUE;
    } else {
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
    static GtkWidget *filename_label;
    static char tmp_str[1024];

    if (window == NULL) {
        gdk_threads_enter();
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
        gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

        vbox = gtk_vbox_new(FALSE, 0);
        gtk_container_add(GTK_CONTAINER(window), vbox);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
        gtk_widget_show(vbox);

        filename_label = gtk_label_new(NULL);
        gtk_box_pack_start(GTK_BOX(vbox), filename_label, FALSE, TRUE, 5);
        gtk_widget_show(filename_label);

        tmp_str[0] = '\0';
        strcat(tmp_str, "Opening: ");
        strcat(tmp_str, basename(sample_filename));

        gtk_label_set_text(GTK_LABEL(filename_label), tmp_str);

        pbar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), pbar, FALSE, TRUE, 5);
        gtk_widget_show(pbar);

        gtk_widget_show(window);
        gdk_threads_leave();
    }

    if (progress_pct >= 1.0) {
        progress_pct = 0;

        gdk_threads_enter();
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
    gtk_widget_queue_draw(scrollbar);

//    gdk_window_process_all_updates();

/* TODO: Remove FIX !!!!!!!!!!! */
    configure_event(draw, NULL, NULL);

    redraw();

/* --------------------------------------------------- */

        gdk_threads_leave();

        return FALSE;

    } else {
        gdk_threads_enter();
        gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), progress_pct);
        gdk_threads_leave();
        return TRUE;
    }
}

static void open_file() {
    if (sample_open_file(sample_filename, &graphData, &progress_pct) != 0) {
        char *message = sample_get_error_message();
        popupmessage_show(main_window, message);
        g_free(message);
        return;
    }

    menu_stop(NULL, NULL);

    idle_func_num = gtk_idle_add(file_open_progress_idle_func, NULL);
    set_title( basename( sample_filename));
}

static gboolean open_file_arg( gpointer data) {
    if( data != NULL) {
      set_sample_filename( (char *)data);
      open_file();
      set_title( basename( sample_filename));
    }

    /* do not call this function again = return FALSE */
    return FALSE;
}

static void handle_arguments( int argc, char** argv) {
  struct stat s;
  char* filename = NULL;

  if( argc < 2) {
    return; /* no arguments */
  }

  filename = argv[1];

  if( stat( filename, &s) != 0) {
    fprintf( stderr, "Cannot stat file: %s\n", filename);
    return;
  }

  if( S_ISREG( s.st_mode) || S_ISLNK( s.st_mode)) {
    g_idle_add( open_file_arg, (gpointer)filename);
  } else {
    fprintf( stderr, "Not a file: %s\n", filename);
  }
}

static void set_title( char* title)
{
  char buf[1024];

  if( title == NULL) {
    gtk_window_set_title( (GtkWindow*)main_window, APPNAME);
    return;
  }

  sprintf( buf, "%s (%s)", APPNAME, title);
  gtk_window_set_title( (GtkWindow*)main_window, buf);
}

void filesel_ok_clicked(GtkWidget *widget, gpointer user_data) {
    GtkWidget *filesel;

    filesel = GTK_WIDGET(user_data);

    set_sample_filename((char *)gtk_file_selection_get_filename(
        GTK_FILE_SELECTION(filesel)));

    gtk_widget_destroy(user_data);
    open_file();
}

void filesel_cancel_clicked(GtkWidget *widget, gpointer user_data) {
    gtk_widget_destroy(user_data);
}

static void open_select_file() {
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new("Open File", GTK_WINDOW(main_window),
        GTK_FILE_CHOOSER_ACTION_OPEN, GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT, NULL);
    if (sample_filename != NULL) {
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog),
            dirname(sample_filename));
    }

    if (gtk_dialog_run(GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        set_sample_filename(filename);
        g_free(filename);
        open_file();
    }

    gtk_widget_destroy(dialog);
}

void set_sample_filename(const char *f) {
    if (sample_filename != NULL) {
        g_free(sample_filename);
        sample_close_file();
    }
    sample_filename = g_strdup(f);
}

/*
 *-------------------------------------------------------------------------
 * Sample Drawing Area Stuff
 *-------------------------------------------------------------------------
 */

static void redraw()
{
    draw_sample(draw);
    gtk_widget_queue_draw(draw);
    draw_summary_pixmap(draw_summary);
    gtk_widget_queue_draw(draw_summary);
}

static void draw_sample(GtkWidget *widget)
{
    int xaxis;
    int width, height;
    int y_min, y_max;
    int scale;
    int i;
    GdkGC *gc;

    int count;
    int index;
    GList *tbl;
    TrackBreak *tb_cur;

    width = widget->allocation.width;
    height = widget->allocation.height;

    if (sample_pixmap) {
        gdk_pixmap_unref(sample_pixmap);
    }

    sample_pixmap = gdk_pixmap_new(widget->window, width, height, -1);

    if (!sample_pixmap) {
        printf("sample_pixmap is NULL\n");
        return;
    }

    gc = gdk_gc_new(sample_pixmap);

    /* clear sample_pixmap before drawing */

    gdk_gc_set_foreground(gc, &bg_color);
    gdk_draw_rectangle(sample_pixmap, gc, TRUE, 0, 0, width, height);

    xaxis = height / 2;
    if (xaxis != 0) {
        scale = graphData.maxSampleValue / xaxis;
        if (scale == 0) {
            scale = 1;
        }
    } else {
        scale = 1;
    }

    if (graphData.data == NULL) {
        /* draw axis */

        gdk_gc_set_foreground(gc, &axis_color);
        gdk_draw_line(sample_pixmap, gc, 0, xaxis, width, xaxis);
        
        //cleanup
        g_object_unref(gc);

        return;
    }

    /* draw sample graph */

    for (i = 0; i < width && i < graphData.numSamples; i++) {
        y_min = graphData.data[i + pixmap_offset].min;
        y_max = graphData.data[i + pixmap_offset].max;

        /*
        y_max = y_min;
        y_min = 100 * sin(i * .1);
        */

        y_min = xaxis + fabs(y_min) / scale;
        y_max = xaxis - y_max / scale;

        /* DEBUG CODE START */
        /*
        printf("i: %d\t", i);
        printf("y_min: %d\t", y_min);
        printf("y_max: %d\n", y_max);
        */
        /* DEBUG CODE END */

        /* find the track break we are drawing now */

        count = 0;
        tb_cur = NULL;
        for (tbl = track_break_list; tbl != NULL; tbl = g_list_next(tbl)) {
            index = g_list_position(track_break_list, tbl);
            tb_cur = g_list_nth_data(track_break_list, index);
            if (tb_cur != NULL) {
                if (i + pixmap_offset > tb_cur->offset) {
                    count++;
                }
            }
        }

        gdk_gc_set_foreground(gc, &sample_colors[(count - 1) % 10]);
        gdk_draw_line(sample_pixmap, gc, i, y_min, i, y_max);
    }

    /* draw axis */

    gdk_gc_set_foreground(gc, &axis_color);

    if (width > graphData.numSamples) {
        gdk_draw_line(sample_pixmap, gc, 0, xaxis, graphData.numSamples, xaxis);
    } else {
        gdk_draw_line(sample_pixmap, gc, 0, xaxis, width, xaxis);
    }

    //cleanup
    g_object_unref(gc);
}

static void draw_cursor_marker()
{
    int height;
    GdkGC *gc;

    height = draw->allocation.height;

    cursor_pixmap = gdk_pixmap_new(draw->window, 1, height, -1);

    if (!cursor_pixmap) {
        printf("cursor_pixmap is NULL\n");
        return;
    }

    gc = gdk_gc_new(cursor_pixmap);
    gdk_gc_set_foreground(gc, &cursor_marker_color);
    gdk_draw_line(cursor_pixmap, gc, 0, 0, 0, height);

    //cleanup
    g_object_unref(gc);
}

static void draw_play_marker()
{
    int height;
    GdkGC *gc;

    height = draw->allocation.height;

    play_pixmap = gdk_pixmap_new(draw->window, 1, height, -1);

    if (!play_pixmap) {
        printf("play_pixmap is NULL\n");
        return;
    }

    gc = gdk_gc_new(play_pixmap);
    gdk_gc_set_foreground(gc, &play_marker_color);
    gdk_draw_line(play_pixmap, gc, 0, 0, 0, height);

    //cleanup
    g_object_unref(gc);
}

static gboolean configure_event(GtkWidget *widget,
    GdkEventConfigure *event, gpointer data)
{
    int width;
    width = widget->allocation.width;

    if (graphData.numSamples == 0) {
        pixmap_offset = 0;
        GTK_ADJUSTMENT(adj)->page_size = 1;
        GTK_ADJUSTMENT(adj)->upper = 1;
        GTK_ADJUSTMENT(adj)->page_increment = 1;
    } else if (width > graphData.numSamples) {
        pixmap_offset = 0;
        GTK_ADJUSTMENT(adj)->page_size = graphData.numSamples;
        GTK_ADJUSTMENT(adj)->upper = graphData.numSamples;
        GTK_ADJUSTMENT(adj)->page_increment = width / 2;
    } else {
        if (pixmap_offset + width > graphData.numSamples) {
            pixmap_offset = graphData.numSamples - width;
        }
        GTK_ADJUSTMENT(adj)->upper = graphData.numSamples;
        GTK_ADJUSTMENT(adj)->page_size = width;
        GTK_ADJUSTMENT(adj)->page_increment = width / 2;
    }

    GTK_ADJUSTMENT(adj)->step_increment = 10;
    gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), pixmap_offset);

    GTK_ADJUSTMENT(cursor_marker_spinner_adj)->upper = graphData.numSamples - 1;

    draw_sample(widget);
    draw_cursor_marker();
    draw_play_marker();

    return TRUE;
}

static gboolean expose_event(GtkWidget *widget,
    GdkEventExpose *event, gpointer data)
{
    GdkGC *gc;

    gc = gdk_gc_new(sample_pixmap);

    gdk_draw_drawable(widget->window, gc, sample_pixmap, 0, 0, 0, 0, -1, -1);

    if (cursor_marker >= pixmap_offset &&
        cursor_marker <= pixmap_offset + widget->allocation.width) {

        gdk_draw_drawable(widget->window, gc, cursor_pixmap, 0, 0,
                  cursor_marker - pixmap_offset, 0, -1, -1);
    }

    if (sample_get_playing()) {
        gdk_draw_drawable(widget->window, gc, play_pixmap, 0, 0,
                  play_marker - pixmap_offset, 0, -1, -1);
    }

    //cleanup
    g_object_unref(gc);

    return FALSE;
}

static void draw_summary_pixmap(GtkWidget *widget)
{
    int xaxis;
    int width, height;
    int y_min, y_max;
    int min, max, x_scale, x_scale_leftover, x_scale_mod;
    int scale;
    int i, k;
    int leftover_count, loop_end, array_offset;

    GdkGC *gc;
    int count;
    int index;
    GList *tbl;
    TrackBreak *tb_cur;

    width = widget->allocation.width;
    height = widget->allocation.height; 
    if (summary_pixmap) {
        gdk_pixmap_unref(summary_pixmap);
    }

    summary_pixmap = gdk_pixmap_new(widget->window, width, height, -1);

    if (!summary_pixmap) {
        printf("summary_pixmap is NULL\n");
        //cleanup
        g_object_unref(gc);
        return;
    }

    gc = gdk_gc_new(summary_pixmap);

    /* clear summary_pixmap before drawing */

    gdk_gc_set_foreground(gc, &bg_color);
    gdk_draw_rectangle(summary_pixmap, gc, TRUE, 0, 0, width, height);

    xaxis = height / 2;
    if (xaxis != 0) {
        scale = graphData.maxSampleValue / xaxis;
        if (scale == 0) {
            scale = 1;
        }
    } else {
        scale = 1;
    }

    if (graphData.data == NULL) {
        /* draw axis */

        gdk_gc_set_foreground(gc, &axis_color);

        //cleanup
        g_object_unref(gc);
        return;
    }

    /* draw sample graph */

    x_scale = graphData.numSamples / width;
    if (x_scale == 0) {
        x_scale = 1;
    }

/*
printf("x_scale: %d\n", x_scale);
printf("width: %d\n", width);
printf("graphData.numSamples: %lu\n", graphData.numSamples);
*/

    x_scale_leftover = graphData.numSamples % width;
//printf("x_scale_leftover: %d\n", x_scale_leftover);
    if (x_scale_leftover > 0) {
        x_scale_mod =  width / x_scale_leftover;
    } else {
        x_scale_mod = 1;
    }

    if (! (x_scale_mod > 0)) {
        x_scale_mod = 1;
    }
/*
printf("x_scale_leftover: %d\n", x_scale_leftover);
printf("x_scale_mod: %d\n", x_scale_mod);
*/

leftover_count = 0;

    for (i = 0; i < width && i < graphData.numSamples; i++) {
        min = max = 0;
        array_offset = i * x_scale + leftover_count;

        if (x_scale != 1) {
            loop_end = x_scale;

            if (i % x_scale_mod == 0 &&
                leftover_count < x_scale_leftover) {

                loop_end++;
                leftover_count++;
            }

            for (k = 0; k < loop_end; k++) {
                if (graphData.data[array_offset + k].max > max) {
                    max = graphData.data[array_offset + k].max;
                } else if (graphData.data[array_offset + k].min < min) {
                    min = graphData.data[array_offset + k].min;
                }
            }
        } else {
            min = graphData.data[i].min;
            max = graphData.data[i].max;
        }

        y_min = min;
        y_max = max;

        y_min = xaxis + fabs(y_min) / scale;
        y_max = xaxis - y_max / scale;

        /* DEBUG CODE START */
        /*
        printf("i: %d\t", i);
        printf("y_min: %d\t", y_min);
        printf("y_max: %d\n", y_max);
        */
        /* DEBUG CODE END */

        count = 0;
        tb_cur = NULL;
        for (tbl = track_break_list; tbl != NULL; tbl = g_list_next(tbl)) {
            index = g_list_position(track_break_list, tbl);
            tb_cur = g_list_nth_data(track_break_list, index);
            if (tb_cur != NULL) {
                if (array_offset > tb_cur->offset) {
                    count++;
                }
            }
        }

        gdk_gc_set_foreground(gc, &sample_colors[(count - 1) % 10]);

        if (i * x_scale + leftover_count >= pixmap_offset &&
                   i * x_scale + leftover_count < pixmap_offset + width) {

            /* draw reverse background */
            gdk_gc_set_foreground(gc, &sample_colors[(count -1) % 10]);
            gdk_draw_line(summary_pixmap, gc, i, 0, i, height);

            /* draw reverse foreground */
            gdk_gc_set_foreground(gc, &bg_color);
            gdk_draw_line(summary_pixmap, gc, i, y_min, i, y_max);

        } else {
            gdk_gc_set_foreground(gc, &sample_colors[(count -1) % 10]);
            gdk_draw_line(summary_pixmap, gc, i, y_min, i, y_max);
        }
    }
//printf("leftover_count: %d\n", leftover_count);

    //cleanup
    g_object_unref(gc);
}

static gboolean
draw_summary_configure_event(GtkWidget *widget,
                             GdkEventConfigure *event,
                             gpointer user_data)
{
    draw_summary_pixmap(widget);

    return TRUE;
}

static gboolean
draw_summary_expose_event(GtkWidget *widget,
                          GdkEventExpose *event,
                          gpointer user_data)
{
    GdkGC *gc;

    gc = gdk_gc_new(summary_pixmap);

    gdk_draw_drawable(widget->window, gc, summary_pixmap, 0, 0, 0, 0, -1, -1);

    //cleanup
    g_object_unref(gc);

    return FALSE;
}

static gboolean draw_summary_button_release(GtkWidget *widget,
    GdkEventButton *event, gpointer user_data)
{
    guint start, midpoint, width;
    int x_scale, x_scale_leftover, x_scale_mod;
    int leftover_count;

    if (sample_get_playing()) {
        return TRUE;
    }

    width = widget->allocation.width;
    x_scale = graphData.numSamples / width;
    x_scale_leftover = graphData.numSamples % width;
    if (x_scale_leftover > 0) {
        x_scale_mod =  width / x_scale_leftover;
        leftover_count = event->x / x_scale_mod;
    } else {
        x_scale_mod =  0;
        leftover_count = 0;
    }

    midpoint = event->x * x_scale + leftover_count;
    start = midpoint - width / 2;
    reset_sample_display(midpoint);
    redraw();

    return TRUE;
}

void reset_sample_display(guint midpoint)
{
    int width = draw->allocation.width;
    int start = midpoint - width / 2;

    if (graphData.numSamples == 0) {
        pixmap_offset = 0;
    } else if (width > graphData.numSamples) {
        pixmap_offset = 0;
    } else if (start + width > graphData.numSamples) {
        pixmap_offset = graphData.numSamples - width;
    } else if (pixmap_offset < 0) {
        pixmap_offset = 0;
    } else {
        pixmap_offset = start;
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
    if (sample_get_playing()) {
        return;
    }
    pixmap_offset = adj->value;

    redraw();

    return TRUE;
}

static void cursor_marker_spinner_changed(GtkAdjustment *adj, gpointer data)
{
    if (sample_get_playing()) {
        return;
    }
    cursor_marker = adj->value;
    /*
    printf("adj->value: %lu\n", adj->value);
    printf("cursor_marker: %lu\n", cursor_marker);
    printf("pixmap_offset: %lu\n", pixmap_offset);
    */

    update_status();
    redraw();
}

static gboolean button_release(GtkWidget *widget, GdkEventButton *event,
    gpointer data)
{
    GtkWidget *menu;
    GtkWidget *add_item;
    GtkWidget *jump_item;

    if (event->x + pixmap_offset > graphData.numSamples) {
        return TRUE;
    }
    if (sample_get_playing()) {
        return TRUE;
    }

    if (event->button == 3) {
        menu = gtk_menu_new();

        add_item = gtk_menu_item_new_with_label("Add Track Break");
        g_signal_connect(G_OBJECT(add_item), "activate",
            G_CALLBACK(menu_add_track_break), NULL);
        gtk_widget_show(add_item);

        jump_item = gtk_menu_item_new_with_label("Jump to Cursor Marker");
        g_signal_connect(G_OBJECT(jump_item), "activate",
            G_CALLBACK(jump_to_cursor_marker), NULL);
        gtk_widget_show(jump_item);

        gtk_menu_shell_append(GTK_MENU_SHELL(menu), add_item);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), jump_item);

        gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
            event->button, event->time);

        redraw();
        return TRUE;
    }

    cursor_marker = pixmap_offset + event->x;
    gtk_adjustment_set_value(cursor_marker_spinner_adj, cursor_marker);

    /* DEBUG CODE START */
    /*
    printf("cursor_marker: %lu\n", cursor_marker);
    */
    /* DEBUG CODE END */

    update_status();
    redraw();

    return TRUE;
}

static void offset_to_duration(guint start_time, guint end_time, gchar *str) {
    guint duration = end_time - start_time;
/*
printf("start time: %d\n", start_time);
printf("end time: %d\n", end_time);
*/
    offset_to_time(duration, str);
}

static void offset_to_time(guint time, gchar *str) {
    char buf[1024];
    int min, sec, subsec;

    if (time > 0) {
        min = time / (CD_BLOCKS_PER_SEC * 60);
        sec = time % (CD_BLOCKS_PER_SEC * 60);
        subsec = sec % CD_BLOCKS_PER_SEC;
        sec = sec / CD_BLOCKS_PER_SEC;
    } else {
        min = sec = subsec = 0;
    }
    sprintf(str, "%d:%02d.%02d", min, sec, subsec);
}

static void update_status() {
    char str[1024];
    char strbuf[1024];

    sprintf(str, "cursor_marker: ");
    offset_to_time(cursor_marker, strbuf);
    strcat(str, strbuf);

    sprintf(strbuf, "\tplaying: ");
    strcat(str, strbuf);
    offset_to_time(play_marker, strbuf);
    strcat(str, strbuf);

    gtk_statusbar_push(GTK_STATUSBAR(statusbar), 0, str);
}

static void menu_play(GtkWidget *widget, gpointer user_data)
{
    play_marker = cursor_marker;
    switch (play_sample(cursor_marker, &play_marker)) {
        case 0:
            idle_func_num = gtk_idle_add(file_play_progress_idle_func, NULL);
            break;
        case 1:
            printf("error in play_sample\n");
            break;
        case 2:
//            printf("already playing\n");
//            menu_stop(NULL, NULL);
//            play_sample(cursor_marker, &play_marker);
            break;
        case 3:
//            printf("must open sample file to play\n");
            break;
    }
}

static void menu_stop(GtkWidget *widget, gpointer user_data)
{
    stop_sample();
}

static void menu_save(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    if (sample_filename == NULL) {
        return;
    }

    if (get_use_outputdir()) {
        wavbreaker_write_files(get_outputdir());
    } else {
        wavbreaker_write_files(".");
    }
}

static void menu_save_as(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    saveas_show(main_window);
}

void wavbreaker_write_files(char *dirname) {
    sample_write_files(track_break_list, &write_info, dirname);

    idle_func_num = gtk_idle_add(file_write_progress_idle_func, NULL);
}

static void menu_export(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    int write_err = -1;
    char *toc_filename;
    char *ptr = NULL;
    char *data_filename = NULL;
    char msg[128];

    if (sample_filename == NULL) {
       return;
    }

    toc_filename = g_malloc(strlen(sample_filename) + 5);

    strcpy(toc_filename, sample_filename);
    ptr = strstr(toc_filename, ".wav");
    if (ptr == NULL) {
        int n = strlen(toc_filename);
        toc_filename[n] = '.';
        toc_filename[n+1] = 't';
        toc_filename[n+2] = 'o';
        toc_filename[n+3] = 'c';
        toc_filename[n+4] = 0;
    } else {
        ptr++;
        ptr[0] = 't';
        ptr[1] = 'o';
        ptr[2] = 'c';
        ptr[3] = 0;
    }

    data_filename = basename(sample_filename);
       
    write_err = toc_write_file(toc_filename, data_filename, track_break_list);
    if (write_err) {
        sprintf(msg, "Export TOC to %s: FAILED!", toc_filename);
    } else {
        sprintf(msg, "Export TOC to %s: Successful", toc_filename);
    }

    popupmessage_show(main_window, msg);
    g_free(toc_filename);
}

void menu_delete_track_break(GtkWidget *widget, gpointer user_data)
{
    track_break_delete_entry();
}

void menu_add_track_break(GtkWidget *widget, gpointer user_data)
{
    track_break_add_entry();
}

static void
menu_open_file(gpointer callback_data, guint callback_action,
               GtkWidget *widget)
{
    open_select_file();
}

static void
menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    //g_print("menu_quit called\n");
    check_really_quit();
}

static void
menu_about(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    about_show(main_window);
}

static void
menu_config(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    appconfig_show(main_window);
}

static void
menu_autosplit(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    autosplit_show(main_window);
}

static void
menu_rename(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
    /* rename (AND overwrite) track breaks */
    track_break_rename( TRUE);
}

static void save_window_sizes()
{
    gint w, h;
    gtk_window_get_size(GTK_WINDOW(main_window), &w, &h);
    /*
    g_print("w: %d\n", w);
    g_print("h: %d\n", h);
    */
    appconfig_set_main_window_width(w);
    appconfig_set_main_window_height(h);
    appconfig_set_vpane1_position(gtk_paned_get_position(GTK_PANED(vpane1)));
    appconfig_set_vpane2_position(gtk_paned_get_position(GTK_PANED(vpane2)));
}

void wavbreaker_quit() {
    save_window_sizes();
    gtk_object_destroy(GTK_OBJECT(main_window));
}

static void check_really_quit()
{
    if (appconfig_get_ask_really_quit()) {
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

static void
destroy(GtkWidget *widget, gpointer data)
{
    //g_print("destroy event occurred\n");
    gtk_main_quit();
}

int main(int argc, char **argv)
{
    GtkWidget *vbox;
    GtkWidget *tbl_widget;
    GtkWidget *menu_widget;
    GtkWidget *toolbar;
    GtkWidget *icon;
    GtkAccelGroup *accel_group;      
    GtkItemFactory *item_factory;

    GtkWidget *vpane_vbox;
    GtkWidget *list_vbox;
    GtkWidget *frame;

    GtkWidget *cursor_marker_spinner;
    GtkWidget *hbox;
    GtkWidget *hbbox;
    GtkWidget *button;
    GtkWidget *button_hbox;
    GtkWidget *label;

    g_thread_init(NULL);
    gdk_threads_init();
    gtk_init(&argc, &argv);

    main_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_window_set_default_icon_from_file( WAVBREAKER_ICON, NULL);

    set_title( NULL);

    g_signal_connect(G_OBJECT(main_window), "delete_event",
             G_CALLBACK(delete_event), NULL);

    g_signal_connect(G_OBJECT(main_window), "destroy",
             G_CALLBACK(destroy), NULL);

    gtk_container_set_border_width(GTK_CONTAINER(main_window), 0);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(main_window), vbox);
    gtk_widget_show(vbox);

    /* Menu Items */
    accel_group = gtk_accel_group_new();
    gtk_window_add_accel_group(GTK_WINDOW(main_window), accel_group);
    g_object_unref(accel_group);

    item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>",
                                        accel_group);

    /* Set up item factory to go away with the window */
    g_object_ref (item_factory);
    gtk_object_sink (GTK_OBJECT (item_factory));
    g_object_set_data_full(G_OBJECT(main_window), "<main>", item_factory,
                          (GDestroyNotify)g_object_unref);

    /* create menu items */
    gtk_item_factory_create_items(item_factory, G_N_ELEMENTS(menu_items),
                                  menu_items, main_window);

    menu_widget = gtk_item_factory_get_widget(item_factory, "<main>");

    gtk_box_pack_start(GTK_BOX(vbox), menu_widget, FALSE, TRUE, 0);
    gtk_widget_show(menu_widget);

    /* Button Toolbar */
    toolbar = gtk_toolbar_new();
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_OPEN,
                             "Open New Wave File", NULL,
                             G_CALLBACK(menu_open_file), main_window, -1);
    gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_SAVE,
                             "Save Track Breaks", NULL,
                             G_CALLBACK(menu_save), main_window, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_SAVE_AS,
                             "Save Track Breaks To Dir", NULL,
                             G_CALLBACK(menu_save_as), main_window, -1);
    gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_CUT,
                             "AutoSplit", NULL,
                             G_CALLBACK(menu_autosplit), main_window, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_CDROM,
                             "Export to TOC", NULL,
                             G_CALLBACK(menu_export), main_window, -1);
    gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));

    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_MEDIA_PLAY,
                             "Play from cursor", NULL,
                             G_CALLBACK(menu_play), main_window, -1);
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_MEDIA_STOP,
                             "Stop playback", NULL,
                             G_CALLBACK(menu_stop), main_window, -1);

    gtk_toolbar_append_space(GTK_TOOLBAR(toolbar));
    gtk_toolbar_insert_stock(GTK_TOOLBAR(toolbar), GTK_STOCK_QUIT,
                             "Quit wavbreaker", NULL,
                             G_CALLBACK(menu_quit), main_window, -1);

    gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, TRUE, 0);
    gtk_widget_show(toolbar);

    /* Set default colors up */
    bg_color.red   = 255*(65535/255);
    bg_color.green = 255*(65535/255);
    bg_color.blue  = 255*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &bg_color);

    axis_color.red   = 0*(65535/255);
    axis_color.green = 0*(65535/255);
    axis_color.blue  = 0*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &axis_color);

    cursor_marker_color.red   = 255*(65535/255);
    cursor_marker_color.green =   0*(65535/255);
    cursor_marker_color.blue  =   0*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &cursor_marker_color);

    play_marker_color.red   =   0*(65535/255);
    play_marker_color.green = 255*(65535/255);
    play_marker_color.blue  =   0*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &play_marker_color);

    sample_colors[0].red   =  15*(65535/255);
    sample_colors[0].green = 184*(65535/255);
    sample_colors[0].blue  = 225*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[0]);

    sample_colors[1].red   = 255*(65535/255);
    sample_colors[1].green = 0*(65535/255);
    sample_colors[1].blue  = 0*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[1]);

    sample_colors[2].red   = 0*(65535/255);
    sample_colors[2].green = 255*(65535/255);
    sample_colors[2].blue  = 0*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[2]);

    sample_colors[3].red   = 255*(65535/255);
    sample_colors[3].green = 255*(65535/255);
    sample_colors[3].blue  = 0*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[3]);

    sample_colors[4].red   = 0*(65535/255);
    sample_colors[4].green = 255*(65535/255);
    sample_colors[4].blue  = 255*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[4]);

    sample_colors[5].red   = 255*(65535/255);
    sample_colors[5].green = 0*(65535/255);
    sample_colors[5].blue  = 255*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[5]);

    sample_colors[6].red   = 255*(65535/255);
    sample_colors[6].green = 128*(65535/255);
    sample_colors[6].blue  = 128*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[6]);

    sample_colors[7].red   = 128*(65535/255);
    sample_colors[7].green = 128*(65535/255);
    sample_colors[7].blue  = 255*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[7]);

    sample_colors[8].red   = 128*(65535/255);
    sample_colors[8].green = 255*(65535/255);
    sample_colors[8].blue  = 128*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[8]);

    sample_colors[9].red   = 128*(65535/255);
    sample_colors[9].green = 50*(65535/255);
    sample_colors[9].blue  = 200*(65535/255);
    gdk_color_alloc(gtk_widget_get_colormap(main_window), &sample_colors[9]);

    /* paned view */
    vpane1 = gtk_vpaned_new();
    gtk_box_pack_start(GTK_BOX(vbox), vpane1, TRUE, TRUE, 0);
    gtk_widget_show(vpane1);

    /* vbox for the vpane */
    vpane_vbox = gtk_vbox_new(FALSE, 0);
    gtk_paned_pack1(GTK_PANED(vpane1), vpane_vbox, TRUE, TRUE);
    gtk_widget_show(vpane_vbox);

    /* paned view */
    vpane2 = gtk_vpaned_new();
    gtk_box_pack_start(GTK_BOX(vpane_vbox), vpane2, TRUE, TRUE, 0);
    gtk_widget_show(vpane2);

    /* The summary_pixmap drawing area */
    draw_summary = gtk_drawing_area_new();
    gtk_widget_set_size_request(draw_summary, 500, 75);

    g_signal_connect(G_OBJECT(draw_summary), "expose_event",
             G_CALLBACK(draw_summary_expose_event), NULL);
    g_signal_connect(G_OBJECT(draw_summary), "configure_event",
             G_CALLBACK(draw_summary_configure_event), NULL);
    g_signal_connect(G_OBJECT(draw_summary), "button_release_event",
             G_CALLBACK(draw_summary_button_release), NULL);

    gtk_widget_add_events(draw_summary, GDK_BUTTON_RELEASE_MASK);
    gtk_widget_add_events(draw_summary, GDK_BUTTON_PRESS_MASK);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_widget_show(frame);

    gtk_container_add(GTK_CONTAINER(frame), draw_summary);
    gtk_paned_add1(GTK_PANED(vpane2), frame);
    gtk_widget_show(draw_summary);

    /* The sample_pixmap drawing area */
    draw = gtk_drawing_area_new();
    gtk_widget_set_size_request(draw, 500, 200);

    g_signal_connect(G_OBJECT(draw), "expose_event",
             G_CALLBACK(expose_event), NULL);
    g_signal_connect(G_OBJECT(draw), "configure_event",
             G_CALLBACK(configure_event), NULL);
    g_signal_connect(G_OBJECT(draw), "button_release_event",
             G_CALLBACK(button_release), NULL);

    gtk_widget_add_events(draw, GDK_BUTTON_RELEASE_MASK);
    gtk_widget_add_events(draw, GDK_BUTTON_PRESS_MASK);

    frame = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_IN);
    gtk_widget_show(frame);

    gtk_container_add(GTK_CONTAINER(frame), draw);
    gtk_paned_add2(GTK_PANED(vpane2), frame);
//    gtk_box_pack_start(GTK_BOX(vpane_vbox), draw, TRUE, TRUE, 5);
    gtk_widget_show(draw);

/* Add cursor marker spinner and track break add and delete buttons */
    hbox = gtk_hbox_new(FALSE, 5);
    gtk_box_pack_start(GTK_BOX(vpane_vbox), hbox, FALSE, FALSE, 0);
    gtk_widget_show(hbox);

    cursor_marker_spinner_adj = (GtkAdjustment *) gtk_adjustment_new (0.0, 0.0, 1000.0, 1.0, 74.0, 74.0);
    cursor_marker_spinner = gtk_spin_button_new(cursor_marker_spinner_adj, 1.0, 0);
    gtk_box_pack_start(GTK_BOX(hbox), cursor_marker_spinner, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(cursor_marker_spinner_adj), "value-changed",
             G_CALLBACK(cursor_marker_spinner_changed), NULL);
    gtk_widget_show(cursor_marker_spinner);

    hbbox = gtk_hbutton_box_new();
    gtk_box_pack_start(GTK_BOX(hbox), hbbox, FALSE, FALSE, 0);
    gtk_box_set_spacing(GTK_BOX(hbbox), 5);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_START);
    gtk_widget_show(hbbox);

    /* add track break button */
    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(hbbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked",
             G_CALLBACK(menu_add_track_break), NULL);
    gtk_widget_show(button);

    button_hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), button_hbox);
    gtk_widget_show(button_hbox);

    icon = gtk_image_new_from_stock( GTK_STOCK_ADD, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(button_hbox), icon, FALSE, FALSE, 0);
    gtk_widget_show(icon);

    label = gtk_label_new("Add");
    gtk_box_pack_start(GTK_BOX(button_hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    /* delete track break button */
    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(hbbox), button, FALSE, FALSE, 2);
    g_signal_connect(G_OBJECT(button), "clicked",
             G_CALLBACK(menu_delete_track_break), NULL);
    gtk_widget_show(button);

    button_hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), button_hbox);
    gtk_widget_show(button_hbox);

    icon = gtk_image_new_from_stock( GTK_STOCK_REMOVE, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(button_hbox), icon, FALSE, FALSE, 0);
    gtk_widget_show(icon);

    label = gtk_label_new("Remove");
    gtk_box_pack_start(GTK_BOX(button_hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    /* rename track breaks button */
    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(hbbox), button, FALSE, FALSE, 2);
    g_signal_connect(G_OBJECT(button), "clicked",
             G_CALLBACK(menu_rename), NULL);
    gtk_widget_show(button);

    button_hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(button), button_hbox);
    gtk_widget_show(button_hbox);

    icon = gtk_image_new_from_stock( GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(button_hbox), icon, FALSE, FALSE, 0);
    gtk_widget_show(icon);

    label = gtk_label_new("Auto-Rename");
    gtk_box_pack_start(GTK_BOX(button_hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

/* Add scrollbar */
    adj = gtk_adjustment_new(0, 0, 100, 1, 10, 100);
    g_signal_connect(G_OBJECT(adj), "value_changed",
             G_CALLBACK(adj_value_changed), NULL);

    scrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(adj));
    gtk_box_pack_start(GTK_BOX(vpane_vbox), scrollbar, FALSE, TRUE, 0);
    gtk_widget_show(scrollbar);

/* vbox for the list */
    list_vbox = gtk_vbox_new(FALSE, 0);
    gtk_paned_pack2(GTK_PANED(vpane1), list_vbox, FALSE, TRUE);
    gtk_widget_show(list_vbox);

/* Track Break List */
    tbl_widget = track_break_create_list_gui();
    gtk_box_pack_start(GTK_BOX(list_vbox), tbl_widget, TRUE, TRUE, 0);
    gtk_widget_show(tbl_widget);

/* Status Bar */
    statusbar = gtk_statusbar_new();
    gtk_box_pack_start(GTK_BOX(vbox), statusbar, FALSE, TRUE, 0);
    gtk_widget_show(statusbar);

/* Finish up */

    write_info.cur_filename = NULL;

    sample_init();
    appconfig_init();
    if (appconfig_get_main_window_width() > 0) {
        gtk_window_resize(GTK_WINDOW(main_window),
                appconfig_get_main_window_width(),
                appconfig_get_main_window_height());
        gtk_paned_set_position(GTK_PANED(vpane1), appconfig_get_vpane1_position());
        gtk_paned_set_position(GTK_PANED(vpane2), appconfig_get_vpane2_position());
    }
    gtk_widget_show(main_window);

    handle_arguments( argc, argv);

    if (!g_thread_supported ()) g_thread_init (NULL);
    gdk_threads_enter();
    gtk_main();
    gdk_threads_leave();

    appconfig_write_file();

    return 0;
}

