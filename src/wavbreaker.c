/* wavbreaker - A tool to split a wave file up into multiple wave.
 * Copyright (C) 2002 Timothy Robinson
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
#include <pthread.h>
#include <string.h>
#include <libgen.h>

#include "wavbreaker.h"
#include "sample.h"

static GdkPixmap *sample_pixmap;
static GdkPixmap *summary_pixmap;
static GdkPixmap *cursor_pixmap;

static GtkWidget *scrollbar;
static GtkObject *adj;
static GtkWidget *draw;
static GtkWidget *draw_summary;

static GraphData graphData;

static unsigned long cursor_marker;
static int pixmap_offset;
static char *sample_filename = NULL;

static guint idle_func_num;
static gdouble progress_pct;

typedef struct CursorData_ CursorData;

struct CursorData_ {
	unsigned long marker;
	gboolean is_equal;
};

enum {
	COLUMN_WRITE,
	COLUMN_FILENAME,
	COLUMN_OFFSET,
	COLUMN_EDITABLE,
	NUM_COLUMNS
};

static GList *track_break_list = NULL;
static GtkListStore *store = NULL;

/*
 *-------------------------------------------------------------------------
 * Function Prototypes
 *-------------------------------------------------------------------------
 */

void
track_break_write_toggled(GtkWidget *widget,
                               gchar *path_str,
                               gpointer data);

void
track_break_filename_edited(GtkCellRendererText *cell,
                                 const gchar *path_str,
                                 const gchar *new_text,
                                 gpointer user_data);

void
filesel_ok_clicked(GtkWidget *widget,
                   gpointer data);

void
filesel_cancel_clicked(GtkWidget *widget,
                       gpointer data);

static void
draw_sample(GtkWidget *widget);

static void
draw_cursor_marker();

static gboolean
configure_event(GtkWidget *widget,
                GdkEventConfigure *event,
                gpointer data);

static gboolean
expose_event(GtkWidget *widget,
             GdkEventExpose *event,
             gpointer data);

static void
draw_summary_pixmap(GtkWidget *widget);

static gboolean
draw_summary_configure_event(GtkWidget *widget,
                             GdkEventConfigure *event,
                             gpointer user_data);

static gboolean
draw_summary_expose_event(GtkWidget *widget,
                          GdkEventExpose *event,
                          gpointer user_data);

static void
draw_summary_button_release(GtkWidget *widget,
                            GdkEventButton *event,
                            gpointer user_data);

static void
menu_open_file(gpointer callback_data, guint callback_action,
               GtkWidget *widget);

static void
menu_save(gpointer callback_data, guint callback_action, GtkWidget *widget);

static void
menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget);

static GtkItemFactoryEntry menu_items[] = {
  {"/_File", NULL, 0, 0, "<Branch>"},
//  {"/File/_New", "<control>N", menuitem_cb, 0, "<StockItem>", GTK_STOCK_NEW},
  {"/File/_Open", "<control>O", menu_open_file, 0, "<StockItem>",
      GTK_STOCK_OPEN},
  {"/File/_Save", "<control>S", menu_save, 0, "<StockItem>", GTK_STOCK_SAVE},
/*
  {"/File/Save _As...", "<control>A", menuitem_cb, 0, "<StockItem>",
      GTK_STOCK_SAVE},
*/
  {"/File/sep1", NULL, NULL, 0, "<Separator>"},
  {"/File/_Quit", "<control>Q", menu_quit, 0, "<StockItem>", GTK_STOCK_QUIT},

/*
  {"/_Preferences", NULL, 0,               0, "<Branch>" },
  {"/_Preferences/_Color", NULL, 0,               0, "<Branch>" },
  {"/_Preferences/Color/_Red", NULL, menuitem_cb, 0, "<RadioItem>" },
  {"/_Preferences/Color/_Green", NULL, menuitem_cb, 0,
      "/Preferences/Color/Red" },
  {"/_Preferences/Color/_Blue", NULL, menuitem_cb, 0, "/Preferences/Color/Red"},
  {"/_Preferences/_Shape", NULL, 0,               0, "<Branch>" },
  {"/_Preferences/Shape/_Square", NULL, menuitem_cb, 0, "<RadioItem>" },
  {"/_Preferences/Shape/_Rectangle", NULL, menuitem_cb, 0,
      "/Preferences/Shape/Square" },
  {"/_Preferences/Shape/_Oval", NULL, menuitem_cb, 0,
      "/Preferences/Shape/Rectangle"},
*/

  { "/_Help",            NULL,         0,                 0, "<Branch>" },
  { "/Help/_About",      NULL,         NULL,       0 },
};

/*
 *-------------------------------------------------------------------------
 * Track Break
 *-------------------------------------------------------------------------
 */

void
track_break_create_list_gui(GtkWidget *container)
{
	GtkWidget *treeview;
	GtkTreeViewColumn *column;
	GtkCellRenderer *renderer;
	GtkWidget *sw;

/* create the scrolled window for the list */
        sw = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw),
                                            GTK_SHADOW_ETCHED_IN);
        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                        GTK_POLICY_AUTOMATIC,
                                        GTK_POLICY_AUTOMATIC);
        gtk_box_pack_start(GTK_BOX(container), sw, TRUE, TRUE, 0);
        gtk_widget_set_size_request(sw, 350, 75);
        gtk_widget_show(sw);

/* create the data store */

        store = gtk_list_store_new(NUM_COLUMNS,
                                   G_TYPE_BOOLEAN,
                                   G_TYPE_STRING,
                                   G_TYPE_UINT,
                                   G_TYPE_BOOLEAN);

/* create the treeview */

        treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
//      gtk_box_pack_start(GTK_BOX(container), treeview, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(sw), treeview);
        gtk_widget_show(treeview);

/* create the columns */

/* Write Toggle Column */
	renderer = gtk_cell_renderer_toggle_new();
	g_signal_connect(G_OBJECT(renderer), "toggled",
	                 G_CALLBACK(track_break_write_toggled), store);
	column = gtk_tree_view_column_new_with_attributes("Write", renderer,
	                    "active", COLUMN_WRITE, NULL);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
	                                GTK_TREE_VIEW_COLUMN_FIXED);
	gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 50);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

/* File Name Column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("File Name", renderer,
	                    "text", COLUMN_FILENAME,
						"editable", COLUMN_EDITABLE, NULL);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
	                                GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_column_set_resizable(column, TRUE);
	g_signal_connect(G_OBJECT(renderer), "edited",
	                 G_CALLBACK(track_break_filename_edited), store);
//	GTK_CELL_RENDERER_TEXT(renderer)->editable = TRUE;
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

/* File Offset Column */
	renderer = gtk_cell_renderer_text_new();
	column = gtk_tree_view_column_new_with_attributes("Offset",
	                    renderer, "text", COLUMN_OFFSET, NULL);
	gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
	                                GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
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

/* DEBUG FUNCTION START */
void print_element(gpointer data, gpointer user_data)
{
	TrackBreak *breakup;

	breakup = (TrackBreak *)data;

	printf("\toffset: %d\n", breakup->offset);
	printf("\tfilename: %s\n\n", breakup->filename);
}
/* DEBUG FUNCTION END */

void
track_break_free_element(gpointer data, gpointer user_data)
{
	TrackBreak *track_break;

	track_break = (TrackBreak *)data;

	free(track_break);
}

void
track_break_compare_cursor_marker(gpointer data, gpointer user_data)
{
	TrackBreak *track_break = (TrackBreak *) data;
	CursorData *cursor_data = (CursorData *) user_data;

	if (cursor_data->marker == track_break->offset) {
		cursor_data->is_equal = TRUE;
	}
}

void track_break_clear_list()
{
	gtk_list_store_clear(store);
	g_list_foreach(track_break_list, track_break_free_element, NULL);
	g_list_free(track_break_list);
	track_break_list = NULL;
}

void
track_break_add_entry()
{
	GtkTreeIter iter;
	GtkTreeIter sibling;
	GtkTreePath *path;
	gchar path_str[256];
	gint list_pos = 0;
	TrackBreak *track_break = NULL;
	CursorData cursor_data;
	char str_tmp[1024];
	char *str_ptr;

	if (sample_filename == NULL) {
		return;
	}

	cursor_data.is_equal = FALSE;
	cursor_data.marker = cursor_marker;
	g_list_foreach(track_break_list, track_break_compare_cursor_marker,
                   &cursor_data);
	if (cursor_data.is_equal == TRUE) {
		return;
	}

	if (! (track_break = (TrackBreak *)malloc(sizeof(TrackBreak)))) {
		printf("couldn't malloc enough memory for track_break\n");
		exit(1);
	}

	track_break->write = 1;
	track_break->offset = cursor_marker;
	track_break->editable = TRUE;

	/* setup the filename */
	strcpy(str_tmp, sample_filename);
	str_ptr = basename(str_tmp);
	strcpy(str_tmp, str_ptr);
	str_ptr = rindex(str_tmp, '.');
	if (str_ptr != NULL) {
		*str_ptr = '\0';
	}
	track_break->filename = strdup(strcat(str_tmp, "00"));

	track_break_list = g_list_insert_sorted(track_break_list, track_break,
											track_break_sort);

	list_pos = g_list_index(track_break_list, track_break);
	sprintf(path_str, "%d", list_pos);
	path = gtk_tree_path_new_from_string(path_str);

/* DEBUG CODE START */
/*
	g_print("gtktreepath: %s\n", path_str);
	printf("list contents:\n");
	g_list_foreach(track_break_list, print_element, NULL);
*/
/* DEBUG CODE END */

	if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &sibling, path)) {
		gtk_list_store_insert_before(store, &iter, &sibling);
	} else {
		gtk_list_store_append(store, &iter);
	}
	gtk_list_store_set(store, &iter,
                           COLUMN_WRITE, track_break->write,
                           COLUMN_FILENAME, track_break->filename,
                           COLUMN_OFFSET, track_break->offset,
						   COLUMN_EDITABLE, track_break->editable,
                           -1);

	gtk_tree_path_free(path);
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
	GtkTreeModel *model = (GtkTreeModel *)user_data;
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

/*
 *-------------------------------------------------------------------------
 * File Open Dialog Stuff
 *-------------------------------------------------------------------------
 */

gboolean
idle_func(gpointer data) {
	static GtkWidget *window;
	static GtkWidget *pbar;

	if (window == NULL) {
		gdk_threads_enter();
		window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
		gtk_widget_realize(window);
		gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
		gtk_window_set_modal(GTK_WINDOW(window), TRUE);
		gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

		pbar = gtk_progress_bar_new();
		gtk_container_add(GTK_CONTAINER(window), pbar);
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

	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj), 0);
	gtk_widget_queue_draw(scrollbar);

//	gdk_window_process_all_updates();

/* Remove FIX !!!!!!!!!!! */
	configure_event(draw, NULL, NULL);

	draw_sample(draw);
	draw_cursor_marker();
	gtk_widget_queue_draw(draw);
	draw_summary_pixmap(draw_summary);
	gtk_widget_queue_draw(draw_summary);

	track_break_add_entry();

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

void
filesel_ok_clicked(GtkWidget *widget,
                   gpointer user_data)
{
	GtkWidget *filesel;

	filesel = GTK_WIDGET(user_data);

	sample_filename = g_strdup((char *)gtk_file_selection_get_filename(
	                           GTK_FILE_SELECTION(filesel)));

	sample_open_file(sample_filename, &graphData, &progress_pct);

	gdk_window_hide(widget->window);

	stop_sample();

	idle_func_num = gtk_idle_add(idle_func, NULL);
}

void
filesel_cancel_clicked(GtkWidget *widget,
                       gpointer user_data)
{
	gdk_window_hide(widget->window);
}

static void
openfile()
{
	static GtkWidget *filesel = NULL;

	if (!filesel) {
		filesel = gtk_file_selection_new("open file");
		gtk_signal_connect(GTK_OBJECT(
			GTK_FILE_SELECTION(filesel)->ok_button),
			"clicked", (GtkSignalFunc)filesel_ok_clicked, filesel);

		gtk_signal_connect(GTK_OBJECT(
			GTK_FILE_SELECTION(filesel)->cancel_button),
			"clicked", (GtkSignalFunc)filesel_cancel_clicked,
			filesel);

		gtk_widget_show(filesel);
	} else {
		gdk_window_show(filesel->window);
	}
}

/*
 *-------------------------------------------------------------------------
 * Sample Drawing Area Stuff
 *-------------------------------------------------------------------------
 */

static void
draw_sample(GtkWidget *widget)
{
	int xaxis;
	int width, height;
	int y_min, y_max;
	int scale;
	int i;

	GdkGC *gc;
	GdkColor color;

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

	color.red   = 255*(65535/255);
	color.green = 255*(65535/255);
	color.blue  = 255*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	gdk_draw_rectangle(sample_pixmap, gc, TRUE, 0, 0, width, height);

	xaxis = height / 2;
	scale = graphData.maxSampleValue / xaxis;

	if (scale == 0) {
		scale = 1;
	}

	if (graphData.data == NULL) {
		/* draw axis */

		color.red   = 0*(65535/255);
		color.green = 0*(65535/255);
		color.blue  = 0*(65535/255);
		gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
		gdk_gc_set_foreground(gc, &color);

		gdk_draw_line(sample_pixmap, gc, 0, xaxis, width, xaxis);

		return;
	}

	/* draw sample graph */

	color.red   =  15*(65535/255);
	color.green =  184*(65535/255);
	color.blue  = 225*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

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

		gdk_draw_line(sample_pixmap, gc, i, y_min, i, y_max);
	}

	/* draw axis */

	color.red   = 0*(65535/255);
	color.green = 0*(65535/255);
	color.blue  = 0*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	if (width > graphData.numSamples) {
		gdk_draw_line(sample_pixmap, gc, 0, xaxis, graphData.numSamples, xaxis);
	} else {
		gdk_draw_line(sample_pixmap, gc, 0, xaxis, width, xaxis);
	}
}

static void
draw_cursor_marker()
{
	int height;
	GdkGC *gc;
	GdkColor color;

	height = draw->allocation.height;

	cursor_pixmap = gdk_pixmap_new(draw->window, 1, height, -1);

	if (!cursor_pixmap) {
		printf("cursor_pixmap is NULL\n");
		return;
	}

	gc = gdk_gc_new(cursor_pixmap);

	color.red   = 255*(65535/255);
	color.green =   0*(65535/255);
	color.blue  =   0*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(draw), &color);
	gdk_gc_set_foreground(gc, &color);

	gdk_draw_line(cursor_pixmap, gc, 0, 0, 0, height);
}

static gboolean
configure_event(GtkWidget *widget,
				GdkEventConfigure *event,
				gpointer data)
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

	draw_sample(widget);
	draw_cursor_marker();

	return TRUE;
}

static gboolean
expose_event(GtkWidget *widget,
			 GdkEventExpose *event,
			 gpointer data)
{
	GdkGC *gc;

	gc = gdk_gc_new(sample_pixmap);

	gdk_draw_drawable(widget->window, gc, sample_pixmap, 0, 0, 0, 0, -1, -1);

	if (cursor_marker >= pixmap_offset &&
	    cursor_marker <= pixmap_offset + widget->allocation.width) {

		int x = cursor_marker - pixmap_offset;
		gdk_draw_drawable(widget->window, gc, cursor_pixmap, 0, 0,
				  x, 0, -1, -1);
	}

	return FALSE;
}

static void
draw_summary_pixmap(GtkWidget *widget)
{
	int xaxis;
	int width, height;
	int y_min, y_max;
	int min, max, x_scale, x_scale_leftover, x_scale_mod;
	int scale;
	int i, k;
	int leftover_count, loop_end, array_offset;

	GdkGC *gc;
	GdkColor color;

	width = widget->allocation.width;
	height = widget->allocation.height;

	if (summary_pixmap) {
		gdk_pixmap_unref(summary_pixmap);
	}

	summary_pixmap = gdk_pixmap_new(widget->window, width, height, -1);

	if (!summary_pixmap) {
		printf("summary_pixmap is NULL\n");
		return;
	}

	gc = gdk_gc_new(summary_pixmap);

	/* clear summary_pixmap before drawing */

	color.red   = 255*(65535/255);
	color.green = 255*(65535/255);
	color.blue  = 255*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	gdk_draw_rectangle(summary_pixmap, gc, TRUE, 0, 0, width, height);

	xaxis = height / 2;
	scale = graphData.maxSampleValue / xaxis;

	if (scale == 0) {
		scale = 1;
	}

	if (graphData.data == NULL) {
		/* draw axis */

		color.red   = 0*(65535/255);
		color.green = 0*(65535/255);
		color.blue  = 0*(65535/255);
		gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
		gdk_gc_set_foreground(gc, &color);

		return;
	}

	/* draw sample graph */

	color.red   =  15*(65535/255);
	color.green =  184*(65535/255);
	color.blue  = 225*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

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
	x_scale_mod =  width / x_scale_leftover;
//printf("x_scale_leftover: %d\n", x_scale_leftover);

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

		/* setup colors for graph */
		if (i * x_scale + leftover_count >= pixmap_offset &&
		           i * x_scale + leftover_count < pixmap_offset + width) {

			/* draw reverse background */
			color.red   =  15*(65535/255);
			color.green =  184*(65535/255);
			color.blue  = 225*(65535/255);
			gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
			gdk_gc_set_foreground(gc, &color);

			gdk_draw_line(summary_pixmap, gc, i, 0, i, height);

			/* draw reverse foreground */
			color.red   =  255*(65535/255);
			color.green =  255*(65535/255);
			color.blue  = 225*(65535/255);
			gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
			gdk_gc_set_foreground(gc, &color);

			gdk_draw_line(summary_pixmap, gc, i, y_min, i, y_max);

		} else {
			color.red   =  15*(65535/255);
			color.green =  184*(65535/255);
			color.blue  = 225*(65535/255);
			gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
			gdk_gc_set_foreground(gc, &color);

			gdk_draw_line(summary_pixmap, gc, i, y_min, i, y_max);
		}
	}
//printf("leftover_count: %d\n", leftover_count);
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

	return FALSE;
}

static void
draw_summary_button_release(GtkWidget *widget,
                            GdkEventButton *event,
                            gpointer user_data)
{
	int start, midpoint, width;
	int x_scale, x_scale_leftover, x_scale_mod;
	int leftover_count;

	width = widget->allocation.width;
	x_scale = graphData.numSamples / width;
	x_scale_leftover = graphData.numSamples % width;
	x_scale_mod =  width / x_scale_leftover;

	leftover_count = event->x / x_scale_mod;

	midpoint = event->x * x_scale + leftover_count;
	start = midpoint - width / 2;

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

	draw_sample(draw);
	draw_cursor_marker();
	gtk_widget_queue_draw(draw);
	draw_summary_pixmap(draw_summary);
	gtk_widget_queue_draw(draw_summary);

	return;
}

/*
 *-------------------------------------------------------------------------
 * Scrollbar and Buttons
 *-------------------------------------------------------------------------
 */

static gboolean
adj_value_changed(GtkAdjustment *adj,
                  gpointer data)
{
	pixmap_offset = adj->value;

	draw_sample(draw);
	draw_cursor_marker();
	gtk_widget_queue_draw(draw);
	draw_summary_pixmap(draw_summary);
	gtk_widget_queue_draw(draw_summary);

	return TRUE;
}

static void
button_release(GtkWidget *widget,
			   GdkEventButton *event,
			   gpointer data)
{

	if (event->x + pixmap_offset > graphData.numSamples) {
		return;
	}

	cursor_marker = pixmap_offset + event->x;

	/* DEBUG CODE START */
	
	printf("cursor_marker: %lu\n", cursor_marker);
	
	/* DEBUG CODE END */

	draw_sample(draw);
	draw_cursor_marker();
	gtk_widget_queue_draw(draw);
}

static void
play_button_clicked(GtkWidget *widget,
		    gpointer user_data)
{
	switch (play_sample(cursor_marker)) {
		case 0:
			break;
		case 1:
			printf("error in play_sample\n");
			break;
		case 2:
//			printf("already playing\n");
			break;
		case 3:
//			printf("must open sample file to play\n");
			break;
	}
}

static void
stop_button_clicked(GtkWidget *widget,
                    gpointer user_data)
{
	stop_sample();
}

static void
write_button_clicked(GtkWidget *widget,
                     gpointer user_data)
{
	if (sample_filename == NULL) {
		return;
	}

	sample_write_files(sample_filename, track_break_list);
}

static void
menu_save(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	if (sample_filename == NULL) {
		return;
	}

	sample_write_files(sample_filename, track_break_list);
}

void
track_break_add_button_clicked(GtkWidget *widget, gpointer user_data)
{
	track_break_add_entry();
}

static void
open_file_button_clicked(GtkWidget *widget, gpointer data)
{
	openfile();
}

static void
menu_open_file(gpointer callback_data, guint callback_action,
               GtkWidget *widget)
{
	openfile();
}
static void
menu_quit(gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	gtk_main_quit();
}

/*
 *-------------------------------------------------------------------------
 * Main Window Events
 *-------------------------------------------------------------------------
 */

static gboolean
delete_event(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	return FALSE;
}

static gboolean
destroy(GtkWidget *widget, GdkEventAny *event, gpointer data)
{
	gtk_main_quit();
	return FALSE;
}

int main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *packer, *box2;
	GtkWidget *menu_widget;
	GtkAccelGroup *accel_group;      
	GtkItemFactory *item_factory;

	g_thread_init(NULL);
	gdk_threads_init();
	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);

	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(destroy), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(window), 0);

	packer = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), packer);
	gtk_widget_show(packer);

	box2 = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_end(GTK_BOX(packer), box2, FALSE, FALSE, 0);
	gtk_widget_show(box2);

/* Menu Items */
	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
	g_object_unref(accel_group);

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU_BAR, "<main>",
	                                    accel_group);

	/* Set up item factory to go away with the window */
	g_object_ref (item_factory);
	gtk_object_sink (GTK_OBJECT (item_factory));
	g_object_set_data_full(G_OBJECT(window), "<main>", item_factory,
	                      (GDestroyNotify)g_object_unref);

	/* create menu items */
	gtk_item_factory_create_items(item_factory, G_N_ELEMENTS(menu_items),
	                              menu_items, window);

	menu_widget = gtk_item_factory_get_widget(item_factory, "<main>");

	gtk_box_pack_start(GTK_BOX(packer), menu_widget, FALSE, TRUE, 0);
	gtk_widget_show(menu_widget);

/* Open File Button*/
	button = gtk_button_new_with_label("Open File");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(open_file_button_clicked), NULL);

	gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

/* play button */
	button = gtk_button_new_with_label("Play");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(play_button_clicked), NULL);

	gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

/* stop button */
	button = gtk_button_new_with_label("Stop");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(stop_button_clicked), NULL);

	gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

/* write button */
	button = gtk_button_new_with_label("Write");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(write_button_clicked), NULL);

	gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

/* track break button */
	button = gtk_button_new_with_label("Add Track Break");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(track_break_add_button_clicked), NULL);

	gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

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

	gtk_box_pack_start(GTK_BOX(packer), draw_summary, FALSE, TRUE, 5);
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

	gtk_box_pack_start(GTK_BOX(packer), draw, TRUE, TRUE, 5);
	gtk_widget_show(draw);

/* Add scrollbar */
	adj = gtk_adjustment_new(0, 0, 100, 1, 10, 100);
	g_signal_connect(G_OBJECT(adj), "value_changed",
			 G_CALLBACK(adj_value_changed), NULL);

	scrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(adj));
	gtk_box_pack_start(GTK_BOX(packer), scrollbar, FALSE, TRUE, 0);
	gtk_widget_show(scrollbar);

/* Finish up */
	track_break_create_list_gui(packer);

	gtk_widget_show(window);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}
