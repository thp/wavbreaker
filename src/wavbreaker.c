#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <gtk/gtk.h>
#include <pthread.h>

#include "wavbreaker.h"
#include "sample.h"

static GdkPixmap *pixmap;
static GdkPixmap *pixmap_cursor;

static GtkWidget *scrollbar;
static GtkObject *adj;
static GtkWidget *draw;

static GraphData graphData;

static unsigned long cursor_marker;
static int pixmap_offset;

static guint idle_func_num;
static gdouble progress_pct;

typedef struct TrackBreak_ TrackBreak;

struct TrackBreak_ {
	gboolean  write;
	GdkColor  color;
	guint     offset;
	gchar     *filename;
};

enum {
	COLUMN_WRITE,
	COLUMN_FILENAME,
	COLUMN_OFFSET,
	NUM_COLUMNS
};

GList *track_break_list = NULL;
GtkListStore *store = NULL;

/*
 *-------------------------------------------------------------------------
 * Function Prototypes
 *-------------------------------------------------------------------------
 */

void track_break_write_toggled(GtkWidget *widget, gchar *path_str, gpointer data);

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
                                   G_TYPE_UINT);

/* create the treeview */

        treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
//      gtk_box_pack_start(GTK_BOX(container), treeview, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(sw), treeview);
        gtk_widget_show(treeview);

/* create the columns */

        renderer = gtk_cell_renderer_toggle_new();
        g_signal_connect(G_OBJECT(renderer), "toggled",
                         G_CALLBACK(track_break_write_toggled), store);
        column = gtk_tree_view_column_new_with_attributes("Write", renderer,
                        "active", COLUMN_WRITE, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_fixed_width(GTK_TREE_VIEW_COLUMN(column), 50);
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("File Name", renderer,
                        "text", COLUMN_FILENAME, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_GROW_ONLY);
        gtk_tree_view_column_set_resizable(column, TRUE);
        GTK_CELL_RENDERER_TEXT(renderer)->editable = TRUE;
        gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

        renderer = gtk_cell_renderer_text_new();
        column = gtk_tree_view_column_new_with_attributes("Offset",
                        renderer, "text", COLUMN_OFFSET, NULL);
        gtk_tree_view_column_set_sizing(GTK_TREE_VIEW_COLUMN(column),
                                        GTK_TREE_VIEW_COLUMN_GROW_ONLY);
//      gtk_tree_view_column_set_resizable(column, TRUE);
        GTK_CELL_RENDERER_TEXT(renderer)->editable = TRUE;
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

        printf("\t%d\n", breakup->offset);
}
/* DEBUG FUNCTION END */

void track_break_free_element(gpointer data, gpointer user_data)
{
	TrackBreak *track_break;

	track_break = (TrackBreak *)data;

	printf("freeing - %d\n", track_break->offset);

	free(track_break);
}

gint track_break_compare_cursor_marker(gpointer data, gpointer user_data)
{
	TrackBreak *track_break = (TrackBreak *)data;
	unsigned long cursor_marker = (unsigned long *) user_data;

	if (cursor_marker == track_break->offset) {
		return 0;
	} else if (cursor_marker < track_break->offset) {
		return -1;
	} else {
		return 1;
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
track_break_add_button_clicked(GtkWidget *widget,
			       gpointer user_data)
{
	GtkTreeIter iter;
	GtkTreeIter sibling;
        GtkTreePath *path;
	gchar path_str[256];
	gint list_pos = 0;
	TrackBreak *track_break = NULL;

	if (! (track_break = (TrackBreak *)malloc(sizeof(TrackBreak)))) {
		printf("couldn't malloc enough memory for track_break\n");
		exit(1);
	}

//	if (g_list_foreach(track_break_list, track_break_compare_cursor_marker,
//			cursor_marker)) {
//	}

	track_break->write = 1;
	track_break->filename = "phish - stash.ogg";
	track_break->offset = cursor_marker;

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

	gtk_widget_queue_draw(scrollbar);

//	gdk_window_process_all_updates();

/* Remove FIX !!!!!!!!!!! */
	configure_event(draw, NULL, NULL);

//	draw_sample(draw);
//	draw_cursor_marker();
	gtk_widget_queue_draw(draw);

/* --------------------------------------------------- */

		gdk_threads_leave();

		return FALSE;

	} else {
		gdk_threads_enter();
		gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar),
						progress_pct);
		gdk_threads_leave();
		return TRUE;
	}
}

void
filesel_ok_clicked(GtkWidget *widget,
		   gpointer data)
{
	char *filename;
	GtkWidget *filesel;

	filesel = GTK_WIDGET(data);

	filename = g_strdup((char *)gtk_file_selection_get_filename(
		GTK_FILE_SELECTION(filesel)));

	sample_open_file(filename, &graphData, &progress_pct);

	gdk_window_hide(widget->window);

	idle_func_num = gtk_idle_add(idle_func, NULL);

	cursor_marker = 0;
	track_break_clear_list();
}

void
filesel_cancel_clicked(GtkWidget *widget,
		       gpointer data)
{
	gdk_window_hide(widget->window);
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

	if (pixmap) {
		gdk_pixmap_unref(pixmap);
	}

	pixmap = gdk_pixmap_new(widget->window, width, height, -1);

	if (!pixmap) {
		printf("pixmap is NULL\n");
		return;
	}

	gc = gdk_gc_new(pixmap);

	/* clear pixmap before drawing */

	color.red   = 255*(65535/255);
	color.green = 255*(65535/255);
	color.blue  = 255*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0, width, height);

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

		gdk_draw_line(pixmap, gc, 0, xaxis, width, xaxis);

		return;
	}

	/* draw sample graph */

	color.red   =  15*(65535/255);
	color.green =  15*(65535/255);
	color.blue  = 255*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	y_min = y_max = xaxis;

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
	printf("y_min: %d\n", y_min);
        printf("y_max: %d\n", y_max);
*/
/* DEBUG CODE END */

/*
		if (y_min < 0) {
			y_min = xaxis + fabs(y_min) / scale;
		} else {
			y_min = xaxis - y_min / scale;
		}

		if (y_max < 0) {
			y_max = xaxis + fabs(y_max) / scale;
		} else {
			y_max = xaxis - y_max / scale;
		}
*/

		gdk_draw_line(pixmap, gc, i, y_min, i, y_max);
	}

	/* draw axis */

	color.red   = 0*(65535/255);
	color.green = 0*(65535/255);
	color.blue  = 0*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	gdk_draw_line(pixmap, gc, 0, xaxis, width, xaxis);
}

static void
draw_cursor_marker()
{
	int height;
	GdkGC *gc;
	GdkColor color;

	height = draw->allocation.height;

	pixmap_cursor = gdk_pixmap_new(draw->window, 1, height, -1);

	if (!pixmap_cursor) {
		printf("pixmap_cursor is NULL\n");
		return;
	}

	gc = gdk_gc_new(pixmap_cursor);

	color.red   = 255*(65535/255);
	color.green =   0*(65535/255);
	color.blue  =   0*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(draw), &color);
	gdk_gc_set_foreground(gc, &color);

	gdk_draw_line(pixmap_cursor, gc, 0, 0, 0, height);
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
	} else  if (width > graphData.numSamples) {
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

	gc = gdk_gc_new(pixmap);

	gdk_draw_drawable(widget->window, gc, pixmap, 0, 0, 0, 0, -1, -1);

	if (cursor_marker > pixmap_offset &&
	    cursor_marker < pixmap_offset + widget->allocation.width) {
		int x = cursor_marker - pixmap_offset;
		gdk_draw_drawable(widget->window, gc, pixmap_cursor, 0, 0,
				  x, 0, -1, -1);
	}

	return FALSE;
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

	draw_sample(draw);
	draw_cursor_marker();
	gtk_widget_queue_draw(draw);
}

static void
play_button_clicked(GtkWidget *widget,
		    gpointer data)
{
	switch (play_sample(cursor_marker)) {
		case 0:
			break;
		case 1:
			printf("error in play_sample\n");
			break;
		case 2:
			printf("already playing\n");
			break;
		case 3:
			printf("must open sample file to play\n");
			break;
	}
}

static void
stop_button_clicked(GtkWidget *widget,
		    gpointer data)
{
	stop_sample();
}

static void
open_file_button_clicked(GtkWidget *widget,
			 gpointer data)
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

static gboolean
delete_event(GtkWidget *widget,
	     GdkEventAny *event,
	     gpointer data)
{
	g_print("delete event occurred\n");
	return FALSE;
}

static gboolean
destroy(GtkWidget *widget,
	GdkEventAny *event,
	gpointer data)
{
	gtk_main_quit();
	return FALSE;
}

int main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *packer, *box2;

	g_thread_init(NULL);
	gdk_threads_init();
	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);

	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(destroy), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(window), 5);

	packer = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(window), packer);
	gtk_widget_show(packer);

	box2 = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_end(GTK_BOX(packer), box2, FALSE, FALSE, 0);
	gtk_widget_show(box2);

/* Make Open File Button*/
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

/* track break button */
	button = gtk_button_new_with_label("Add Track Break");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(track_break_add_button_clicked), NULL);

	gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

/* Add scrollbar */
	adj = gtk_adjustment_new(0, 0, 100, 1, 10, 100);
	g_signal_connect(G_OBJECT(adj), "value_changed",
			 G_CALLBACK(adj_value_changed), NULL);

	scrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(adj));
	gtk_box_pack_end(GTK_BOX(packer), scrollbar, FALSE, TRUE, 0);
	gtk_widget_show(scrollbar);

/* The drawing area */
	draw = gtk_drawing_area_new();
	gtk_widget_set_size_request(draw, 500, 200);

	g_signal_connect(G_OBJECT(draw), "expose_event",
			 G_CALLBACK(expose_event), NULL);
	g_signal_connect(G_OBJECT(draw), "configure_event",
			 G_CALLBACK(configure_event), NULL);
	g_signal_connect(G_OBJECT(draw), "button_release_event",
			 G_CALLBACK(button_release), NULL);

	gtk_widget_add_events(draw, GDK_BUTTON_RELEASE_MASK);

	gtk_box_pack_start(GTK_BOX(packer), draw, TRUE, TRUE, 0);
	gtk_widget_show(draw);

	track_break_create_list_gui(packer);

/* Finish up */
	gtk_widget_show(window);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();

	return 0;
}
