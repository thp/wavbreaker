#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <sys/stat.h>
#include <gtk/gtk.h>

#include "wavbreaker.h"
#include "sample.h"

static GdkPixmap *pixmap;
static GdkPixmap *pixmap2;

static GtkWidget *scrollbar;
static GtkObject *adj;
static GtkWidget *draw;

static void readdata()
{
	FILE *infile;
	struct stat statBuf;
	int filesize;
	int i, j;
	char buf[BLOCK_SIZE];
	char *filename = sample_get_sample_file();

	if (stat(filename, &statBuf)) {
		printf("error stat'ing %s\n", filename);
		exit(1);
	}

	filesize = statBuf.st_size;

	nsamples = filesize / 4; /* 4 is size of left and right channel
					in bytes*/
	nsample_blocks = filesize / BLOCK_SIZE;

	printf("file size: %d\n", filesize);
	printf("nsamples: %d\n", nsamples);
	printf("nsample_blocks: %d\n", nsample_blocks);

	if ((infile = fopen(filename, "r")) == NULL) {
		printf("error opening %s\n", filename);
		exit(1);
	}

	i = 0;

	while (fread(buf, 1, BLOCK_SIZE, infile) && i < nsample_blocks) {
		short left_max, left_min;
		short right_max, right_min;

		left_max = left_min = 0;
		right_max = right_min = 0;

		samplesTmp = (SampleBlock *) malloc(sizeof(SampleBlock));
		if (samplesTmp == NULL) {
			printf("couldn't malloc enough memory for samples\n");
			exit(1);
		}
		if (i == 0) {
			samplesHead = samplesTmp;
		} else {
			samplesCur->next = samplesTmp;
		}

		samplesCur = samplesTmp;

		for (j = 0; j < BLOCK_SIZE / 4; j++) {
			char tmp;
			int left, right;

			tmp = buf[j*4+0];
			buf[j*4+0] = buf[j*4+1];
			buf[j*4+1] = tmp;

			tmp = buf[j*4+2];
			buf[j*4+2] = buf[j*4+3];
			buf[j*4+3] = tmp;

			left = buf[j*4+1] << 8 | buf[j*4+0];
			right = buf[j*4+3] << 8 | buf[j*4+2];

			if (left > left_max) {
				left_max = left;
			} else if (left < left_min) {
				left_min = left;
			}

			if (right > right_max) {
				right_max = right;
			} else if (right < right_min) {
				right_min = right;
			}
		} /* end for */

		samplesCur->left_max = left_max;
		samplesCur->left_min = left_min;
		samplesCur->right_max = right_max;
		samplesCur->right_min = right_min;
		samplesCur->next = NULL;

	/*	printf("left_max: %d\tleft_min: %d\n",
			samplesCur->left_max, samplesCur->left_min);
		printf("right_max: %d\tright_min: %d\n",
			samplesCur->right_max, samplesCur->right_min); */

		i++;
	} /* end while */
	fclose(infile);
}

static void drawsample(GtkWidget *widget)
{
	typedef struct {
		gint x, y1, y2;
	} Point;

	Point *points;

	GdkGC *gc;
	GdkColor color;

	gint i, k;
	gint xaxis;
	gint width, height;
	gint scale_factor, resolution;
	short maxsample, minsample;

	width = widget->allocation.width;
	height = widget->allocation.height;

/* sample pixmap */
	if (pixmap) {
		gdk_pixmap_unref(pixmap);
	}

	pixmap = gdk_pixmap_new(widget->window, width, height, -1);

	if (!pixmap) {
		printf("pixmap is NULL\n");
		return;
	}

/* selection pixmap */
	if (pixmap2) {
		gdk_pixmap_unref(pixmap2);
	}

	pixmap2 = gdk_pixmap_new(widget->window, 1, height, -1);

	if (!pixmap2) {
		printf("pixmap2 is NULL\n");
		return;
	}

	points = (Point *) malloc(width * sizeof(Point));
	gc = gdk_gc_new(pixmap);

	if (points == NULL) {
		printf("couldn't allocate memory for points\n");
		return;
	}

	xaxis = height / 2;
	scale_factor = SHRT_MAX / xaxis;
	resolution = nsample_blocks / width;
	if (resolution == 0) {
		/* do something */
	}


/*	printf("width: %d\n", width);
	printf("resolution: %d\n", resolution);
	printf("resolution leftover: %d\n", nsample_blocks % width); */

	/* setup points from sample data */

	maxsample = minsample = 0;
	samplesCur = samplesHead;

	for (k = 0; k < pixmap_offset; k++) {
		samplesCur = samplesCur->next;
	}

	for (i = 0; i < width && samplesCur->next != NULL; i++) {
	/*	for (k = 0; k < pixmap_offset; k++) {
			if (samplesCur->left_max > maxsample) {
				maxsample = samplesCur->left_max;
			}

			if (samplesCur->left_min < minsample) {
				minsample = samplesCur->left_min;
			}

			samplesCur = samplesCur->next;
		} */
		maxsample = samplesCur->left_max;
		minsample = samplesCur->left_min;

		points[i].x = i;

		if (minsample < 0) {
			points[i].y1 = xaxis + fabs(minsample) / scale_factor;
		} else {
			points[i].y1 = xaxis - minsample / scale_factor;
		}

		if (maxsample < 0) {
			points[i].y2 = xaxis + fabs(maxsample) / scale_factor;
		} else {
			points[i].y2 = xaxis - maxsample / scale_factor;
		}

	/*	printf("x: %d\ty1: %d\ty2: %d\n",
			points[i].x, points[i].y1, points[i].y2); */

		samplesCur = samplesCur->next;
		maxsample = minsample = 0;
	}

	/* clear pixmap before drawing */
	color.red = 255*(65535/255);
	color.green = 255*(65535/255);
	color.blue = 255*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);

	gdk_gc_set_foreground(gc, &color);

	gdk_draw_rectangle(pixmap, gc, TRUE, 0, 0,
			   width, height);

	/* set sample color */
	color.red = 15*(65535/255);
	color.green = 15*(65535/255);
	color.blue = 255*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);

	gdk_gc_set_foreground(gc, &color);

	/* draw sample graph */
	for (i = 0; i < width; i++) {
		gdk_draw_line(pixmap, gc, points[i].x, points[i].y1,
					  points[i].x, points[i].y2);
		if (((pixmap_offset + i + 1) % SEC) == 0) {
			color.red = 255*(65535/255);
			color.green = 0*(65535/255);
			color.blue = 0*(65535/255);
			gdk_color_alloc(gtk_widget_get_colormap(widget),
						&color);
			gdk_gc_set_foreground(gc, &color);

			gdk_draw_line(pixmap, gc, points[i].x, 0,
					points[i].x, height);

			color.red = 15*(65535/255);
			color.green = 15*(65535/255);
			color.blue = 255*(65535/255);
			gdk_color_alloc(gtk_widget_get_colormap(widget),
						&color);
			gdk_gc_set_foreground(gc, &color);
		}
	}

	/* draw axis */

	color.red = 0*(65535/255);
	color.green = 0*(65535/255);
	color.blue = 0*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	gdk_draw_line(pixmap, gc, 0, xaxis, width, xaxis);

	/* draw selction_start */

	color.red = 0*(65535/255);
	color.green = 255*(65535/255);
	color.blue = 0*(65535/255);
	gdk_color_alloc(gtk_widget_get_colormap(widget), &color);
	gdk_gc_set_foreground(gc, &color);

	i = sample_get_selection_start() / BLOCK_SIZE;

	if (i >= pixmap_offset && i <= pixmap_offset + width) {
		gdk_draw_line(pixmap, gc, i - pixmap_offset, 0,
			i - pixmap_offset, height);
	}

	/* finally...draw the pixmap on the window */

	gdk_draw_pixmap(widget->window, gc, pixmap, 0, 0, 0, 0, width , height);
}

static gint configure_event(GtkWidget *widget, GdkEventExpose *event)
{
	int width;
	width = widget->allocation.width;

	if (width > nsample_blocks) {
		pixmap_offset = 0;
		GTK_ADJUSTMENT(adj)->page_size = nsample_blocks;
		GTK_ADJUSTMENT(adj)->page_increment = nsample_blocks / 2;
	} else {
		if (pixmap_offset + width > nsample_blocks) {
			pixmap_offset = nsample_blocks - width;
		}
		GTK_ADJUSTMENT(adj)->page_size = width;
		GTK_ADJUSTMENT(adj)->page_increment = width / 2;
	}

	GTK_ADJUSTMENT(adj)->step_increment = 10;
	gtk_adjustment_set_value(GTK_ADJUSTMENT(adj),
				 pixmap_offset);

	return TRUE;
}

static gint expose_event(GtkWidget *widget, GdkEventExpose *event)
{
	drawsample(widget);

	return FALSE;
}

static gint adj_value_changed(GtkAdjustment *adj, gpointer user_data)
{
	pixmap_offset = adj->value;

	expose_event(draw, NULL);

	return TRUE;
}

static void play_button_clicked(GtkWidget *widget)
{
	int ret;

	if ((ret = play_sample())) {
		switch (ret) {
			case 1:
				printf("error in play_sample\n");
				break;
			case 2:
				printf("already playing\n");
				break;
		}
	}
}

static void stop_button_clicked(GtkWidget *widget)
{
	printf("Stop!!\n");

	stop_sample();
}

static void button_release(GtkWidget *widget, GdkEventMotion *event)
{
	sample_set_selection_start((event->x + pixmap_offset) * BLOCK_SIZE);
	drawsample(draw);
}

static void hello(GtkWidget *widget, gpointer data)
{
//	char *filename = "cddata.dat";
	char *filename = "tada.wav";
//	char *filename = "/mnt/music5/tmp/prine/souvenirs.bin";

	g_print ("Hello again - %s was pressed\n", (gchar *) data);
	sample_open_file(filename);
	g_print("opened %s\n", filename);
}

static gint delete_event(GtkWidget *widget, GdkEvent *event, gpointer data)
{
	g_print("delete event occurred\n");
	return FALSE;
}

static void destroy(GtkWidget *widget, gpointer data)
{
	gtk_main_quit();
}

int main(int argc, char **argv)
{
	GtkWidget *window;
	GtkWidget *button;
	GtkWidget *packer, *box2;

	sample_set_sample_file("tada.wav");
	readdata();

	gtk_init(&argc, &argv);

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

	g_signal_connect(G_OBJECT(window), "delete_event",
			 G_CALLBACK(delete_event), NULL);

	g_signal_connect(G_OBJECT(window), "destroy",
			 G_CALLBACK(destroy), NULL);

	gtk_container_set_border_width(GTK_CONTAINER(window), 10);

	packer = gtk_vbox_new(FALSE, 10);
	gtk_container_add(GTK_CONTAINER(window), packer);
	gtk_widget_show(packer);

	box2 = gtk_hbox_new(FALSE, 10);
	gtk_box_pack_end(GTK_BOX(packer), box2, FALSE, FALSE, 0);
	gtk_widget_show(box2);

/* Make button 1 */
	button = gtk_button_new_with_label("Button 1");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(hello), (gpointer) "Button 1");

	gtk_box_pack_start(GTK_BOX(box2), button, FALSE, FALSE, 0);
	gtk_widget_show(button);

/* Make button 2 */
	button = gtk_button_new_with_label("Button 2");

	g_signal_connect(G_OBJECT(button), "clicked",
			 G_CALLBACK(hello), (gpointer) "Button 2");

	gtk_box_pack_end(GTK_BOX(box2), button, FALSE, FALSE, 0);
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

/* Add scrollbar */
	adj = gtk_adjustment_new(0, 0, nsample_blocks, 50, 250, 500);
	g_signal_connect(G_OBJECT(adj), "value_changed",
			 G_CALLBACK(adj_value_changed), NULL);

	scrollbar = gtk_hscrollbar_new(GTK_ADJUSTMENT(adj));

	gtk_box_pack_end(GTK_BOX(packer), scrollbar, FALSE, FALSE, 0);
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

/* Finish up */
	gtk_widget_show(window);

	gtk_main();

	return 0;
}
