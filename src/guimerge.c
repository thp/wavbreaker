/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2004 Timothy Robinson
 *
 * This file copyright (c) 2007 Thomas Perl
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

#include <gtk/gtk.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>

#include "sample.h"
#include "wavbreaker.h"
#include "popupmessage.h"
#include "format_wav.h"

#include "gettext.h"

enum {
    COLUMN_FILENAME,
    COLUMN_BASENAME,
    COLUMN_LENGTH,
    COLUMN_LENGTH_STRING,
    NUM_COLUMNS
};

static GtkWidget *window;
static GtkWidget *ok_button;
static GtkWidget *remove_button;

static GtkTreeView *treeview = NULL;
static GtkListStore *store = NULL;

static char folder[4096] = {0};

static SampleInfo common_sample_info;
static WriteInfo write_info;

gboolean file_merge_progress_idle_func(gpointer data);

int get_merge_files_count()
{
    GtkTreeIter iter;
    int i = 0;

    if( gtk_tree_model_get_iter_first( GTK_TREE_MODEL(store), &iter) == TRUE) {
        do {
            i++;
        } while( gtk_tree_model_iter_next( GTK_TREE_MODEL(store), &iter) == TRUE);
    }

    /* Only enable when we have more than one file to merge */
    gtk_widget_set_sensitive( GTK_WIDGET( ok_button), (i>1)?TRUE:FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET( remove_button), (i>0)?TRUE:FALSE);

    return i;
}

typedef struct MergeThreadData_ MergeThreadData;
struct MergeThreadData_ {
    char *merge_filename;
    int num_files;
    GList *filenames;
    WriteInfo *write_info;
};
MergeThreadData mtd;

static gpointer
merge_thread(gpointer data)
{
    MergeThreadData *thread_data = data;
    char *filenames[g_list_length(thread_data->filenames)];
    GList *cur, *head;
    int index, i;
    char *list_data;

    head = thread_data->filenames;
    cur = head;
    i = 0;
    while (cur != NULL) {
        index = g_list_position(head, cur);
        list_data = (char *)g_list_nth_data(head, index);

        filenames[i++] = list_data;

        cur = g_list_next(cur);
    }

    wav_merge_files(thread_data->merge_filename,
                        g_list_length(thread_data->filenames),
                        filenames,
                        DEFAULT_BUF_SIZE,
                        thread_data->write_info);

    head = thread_data->filenames;
    cur = head;
    while (cur != NULL) {
        index = g_list_position(head, cur);
        list_data = (char *)g_list_nth_data(head, index);

        free(list_data);

        cur = g_list_next(cur);
    }

    g_list_free(thread_data->filenames);

    return NULL;
}

static void
do_merge_files(char *merge_filename, GList *filenames, WriteInfo *write_info)
{
    mtd.merge_filename = g_strdup(merge_filename);
    mtd.filenames = filenames;
    mtd.write_info = write_info;

    if (write_info->merge_filename != NULL) {
        g_free(write_info->merge_filename);
    }
    write_info->merge_filename = mtd.merge_filename;

    g_thread_unref(g_thread_new("merge files", merge_thread, &mtd));
}

static void ok_button_clicked(GtkWidget *widget, gpointer user_data)
{
    GtkWidget *dialog;
    GValue value;
    GtkTreeIter iter;
    GList *filenames = NULL;
    char *tmp;
    GtkWidget *checkbutton;

    GtkFileFilter *filter_all;
    GtkFileFilter *filter_supported;

    gtk_tree_model_get_iter_first( GTK_TREE_MODEL(store), &iter);
    do {
        memset (&value, 0, sizeof (GValue));
        gtk_tree_model_get_value( GTK_TREE_MODEL(store), &iter, 0, &value);
        tmp = (char*)g_value_peek_pointer( &value);
        filenames = g_list_append(filenames, g_strdup(tmp));
    } while( gtk_tree_model_iter_next( GTK_TREE_MODEL(store), &iter) == TRUE);

    filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_all, _("All files"));
    gtk_file_filter_add_pattern( filter_all, "*");

    filter_supported = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_supported, _("Supported files"));
    gtk_file_filter_add_pattern( filter_supported, "*.wav");

    dialog = gtk_file_chooser_dialog_new( _("Select filename for merged wave file"),
                                          GTK_WINDOW(window),
                                          GTK_FILE_CHOOSER_ACTION_SAVE,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                                          _("_Save"), GTK_RESPONSE_ACCEPT,
                                          NULL);

    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_all);
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_supported);
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER(dialog), filter_supported);

    gtk_file_chooser_set_do_overwrite_confirmation( GTK_FILE_CHOOSER(dialog), TRUE);

    checkbutton = (GtkWidget*)gtk_check_button_new_with_label( _("Open file in wavbreaker after merge"));
    gtk_box_pack_end(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), checkbutton, FALSE, FALSE, 0);
    gtk_widget_show( GTK_WIDGET(checkbutton));

    if( strlen( folder) > 0) {
        gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), folder);
    }

    gtk_file_chooser_set_current_name( GTK_FILE_CHOOSER(dialog), "merged.wav");

    if( gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        tmp = gtk_file_chooser_get_filename( GTK_FILE_CHOOSER(dialog));
        write_info.pct_done = 0.0;
        do_merge_files(tmp, filenames, &write_info);
        gtk_widget_destroy(GTK_WIDGET(user_data));
        g_idle_add( file_merge_progress_idle_func, NULL);
    }

    gtk_widget_destroy( GTK_WIDGET(dialog));
}

static void add_filename( char* filename)
{
    GtkTreeIter iter;
    SampleInfo sampleinfo;
    unsigned int length = 0;
    char* length_str;
    int files = get_merge_files_count();

    wav_read_header( filename, &sampleinfo, 0);

    if( files == 0) {
        /* Adding first file, saving sample info for later comparison */
        memcpy( &common_sample_info, &sampleinfo, sizeof(SampleInfo));
    } else {
        /* Compare the format info of the first file with the current info */
        if( common_sample_info.channels != sampleinfo.channels ||
            common_sample_info.samplesPerSec != sampleinfo.samplesPerSec ||
            common_sample_info.avgBytesPerSec != sampleinfo.avgBytesPerSec ||
            common_sample_info.blockAlign != sampleinfo.blockAlign ||
            common_sample_info.bitsPerSample != sampleinfo.bitsPerSample ||
            sampleinfo.channels == 0 ||
            sampleinfo.samplesPerSec == 0 ||
            sampleinfo.bitsPerSample < 8) {
            popupmessage_show( window, _("Wrong file format - skipping file"), filename);
            return;
        }
    }

    length = sampleinfo.numBytes / (sampleinfo.channels*sampleinfo.samplesPerSec*sampleinfo.bitsPerSample/8);
    length_str = (char*)malloc( 20);
    sprintf( length_str, "%02d:%02d", length/60, length%60);

    gtk_list_store_append( store, &iter);

    gtk_list_store_set( store, &iter, COLUMN_FILENAME, filename,
                                      COLUMN_BASENAME, basename(filename),
                                      COLUMN_LENGTH, length,
                                      COLUMN_LENGTH_STRING, length_str,
                                      -1);
}

static void add_button_clicked( GtkWidget *widget, gpointer user_data)
{
    GtkWidget *dialog;

    GtkFileFilter *filter_all;
    GtkFileFilter *filter_supported;

    int i;

    filter_all = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_all, _("All files"));
    gtk_file_filter_add_pattern( filter_all, "*");

    filter_supported = gtk_file_filter_new();
    gtk_file_filter_set_name( filter_supported, _("Supported files"));
    gtk_file_filter_add_pattern( filter_supported, "*.wav");

    dialog = gtk_file_chooser_dialog_new(_("Add wave file to merge"), GTK_WINDOW(window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        _("_Cancel"), GTK_RESPONSE_CANCEL,
        _("_Open"), GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_all);
    gtk_file_chooser_add_filter( GTK_FILE_CHOOSER(dialog), filter_supported);
    gtk_file_chooser_set_filter( GTK_FILE_CHOOSER(dialog), filter_supported);

    gtk_file_chooser_set_select_multiple( GTK_FILE_CHOOSER(dialog), TRUE);

    if( strlen( folder) > 0) {
        gtk_file_chooser_set_current_folder( GTK_FILE_CHOOSER(dialog), folder);
    }

    if (gtk_dialog_run( GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        const char *current_folder = gtk_file_chooser_get_current_folder( GTK_FILE_CHOOSER(dialog));
        if (current_folder) {
            strcpy(folder, current_folder);
        }

        GSList* filenames;
        filenames = gtk_file_chooser_get_filenames( GTK_FILE_CHOOSER(dialog));

        for( i=0; i<g_slist_length( filenames); i++) {
            add_filename( (char*)g_slist_nth_data( filenames, i));
            g_free( g_slist_nth_data( filenames, i));
        }

        g_slist_free( filenames);
    }

    gtk_widget_destroy(dialog);
    get_merge_files_count();
}

void remove_selected_row( gpointer data, gpointer user_data)
{
    GtkTreePath *path = (GtkTreePath*)data;
    GtkTreeIter iter;

    gtk_tree_model_get_iter( GTK_TREE_MODEL(store), &iter, path);
    gtk_list_store_remove( store, &iter);
    gtk_tree_path_free( path);
}

static void remove_button_clicked( GtkWidget *widget, gpointer user_data)
{
    GtkTreeSelection *selection;
    GtkTreeModel *model;
    GList *list;

    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW(treeview));
    list = gtk_tree_selection_get_selected_rows( selection, &model);
    list = g_list_reverse( list);
    g_list_foreach( list, remove_selected_row, NULL);
    g_list_free( list);
    get_merge_files_count();
}

void guimerge_show( GtkWidget *main_window)
{
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *vbbox;
    GtkWidget *button;

    GtkTreeSelection *selection;
    GtkTreeViewColumn *column;
    GtkCellRenderer *renderer;
    GtkWidget *sw;

    window = gtk_window_new( GTK_WINDOW_TOPLEVEL);

    GtkWidget *header_bar = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), _("Merge wave files"));
    gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

    gtk_widget_realize( window);
    gtk_window_set_modal( GTK_WINDOW(window), TRUE);
    gtk_window_set_transient_for( GTK_WINDOW(window), GTK_WINDOW(main_window));
    gtk_window_set_type_hint( GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_position( GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_box_set_spacing( GTK_BOX(vbox), 6);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_spacing( GTK_BOX(hbox), 6);
    gtk_box_pack_start( GTK_BOX(vbox), hbox, TRUE, TRUE, 0);

    if (!store) {
        store = gtk_list_store_new( NUM_COLUMNS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
    }
    gtk_list_store_clear(store);

    /* create the scrolled window for the list */
    sw = gtk_scrolled_window_new( NULL, NULL);
    gtk_scrolled_window_set_shadow_type( GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy( GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    treeview = GTK_TREE_VIEW( gtk_tree_view_new_with_model( GTK_TREE_MODEL(store)));
    gtk_container_add( GTK_CONTAINER(sw), GTK_WIDGET(treeview));

    selection = gtk_tree_view_get_selection( GTK_TREE_VIEW(treeview));
    gtk_tree_selection_set_mode( selection, GTK_SELECTION_MULTIPLE);

    /* Basename Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title( column, _("File Name"));
    gtk_tree_view_column_set_expand(column, TRUE);
    gtk_tree_view_column_pack_start( column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_BASENAME);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), column);

    /* Length Column */
    column = gtk_tree_view_column_new();
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_set_title( column, _("Length"));
    gtk_tree_view_column_pack_start( column, renderer, TRUE);
    gtk_tree_view_column_add_attribute(column, renderer, "text", COLUMN_LENGTH_STRING);
    gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
    gtk_tree_view_column_set_resizable(column, TRUE);
    gtk_tree_view_append_column( GTK_TREE_VIEW(treeview), column);

    gtk_box_pack_start( GTK_BOX(hbox), sw, TRUE, TRUE, 0);

    vbbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start( GTK_BOX(hbox), vbbox, FALSE, FALSE, 0);
    gtk_box_set_spacing(GTK_BOX(vbbox), 5);

    button = gtk_button_new_with_mnemonic(_("_Add"));
    gtk_box_pack_start( GTK_BOX(vbbox), button, FALSE, FALSE, 0);
    g_signal_connect( G_OBJECT(button), "clicked", (GCallback)add_button_clicked, window);

    remove_button = gtk_button_new_with_mnemonic(_("_Remove"));
    gtk_box_pack_start( GTK_BOX(vbbox), remove_button, FALSE, FALSE, 0);
    g_signal_connect( G_OBJECT(remove_button), "clicked", (GCallback)remove_button_clicked, window);
    gtk_widget_set_sensitive( GTK_WIDGET(remove_button), FALSE);

    gtk_box_pack_start(GTK_BOX(vbbox), gtk_label_new(""), TRUE, TRUE, 0);

    ok_button = gtk_button_new_with_label(_("Merge"));
    g_signal_connect(G_OBJECT(ok_button), "clicked", (GCallback)ok_button_clicked, window);
    gtk_box_pack_start( GTK_BOX(vbbox), ok_button, FALSE, FALSE, 0);
    gtk_widget_set_sensitive( GTK_WIDGET(ok_button), FALSE);

    gtk_window_resize( GTK_WINDOW(window), 500, 300);
    gtk_widget_show_all(window);
}

gboolean file_merge_progress_idle_func(gpointer data) {
    static GtkWidget *window;
    static GtkWidget *pbar;
    static GtkWidget *vbox;
    static GtkWidget *label;
    static GtkWidget *status_label;
    static int cur_file_displayed = 0;
    static double fraction;

    if (window == NULL) {
        window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        gtk_widget_realize(window);
        gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
        gtk_window_set_modal(GTK_WINDOW(window), TRUE);
        gtk_window_set_transient_for(GTK_WINDOW(window),
                GTK_WINDOW(wavbreaker_get_main_window()));
        gtk_window_set_type_hint(GTK_WINDOW(window),
                GDK_WINDOW_TYPE_HINT_DIALOG);
        gtk_window_set_position(GTK_WINDOW(window),
                GTK_WIN_POS_CENTER_ON_PARENT);
        gdk_window_set_functions(gtk_widget_get_window(window), GDK_FUNC_MOVE);

        vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
        gtk_container_add(GTK_CONTAINER(window), vbox);
        gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

        gtk_window_set_title( GTK_WINDOW(window), _("Merging wave files"));

        gchar *markup = g_markup_printf_escaped("<span size=\"larger\" weight=\"bold\">%s</span>",
                gtk_window_get_title(GTK_WINDOW(window)));
        label = gtk_label_new(NULL);
        gtk_label_set_markup(GTK_LABEL(label), markup);
        g_free(markup);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 5);

        label = gtk_label_new( _("The selected files are now being merged. This can take some time."));
        gtk_label_set_line_wrap( GTK_LABEL(label), TRUE);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, TRUE, 5);

        pbar = gtk_progress_bar_new();
        gtk_box_pack_start(GTK_BOX(vbox), pbar, FALSE, TRUE, 5);

        status_label = gtk_label_new( NULL);
        g_object_set(G_OBJECT(label), "xalign", 0.0f, "yalign", 0.5f, NULL);
        gtk_label_set_ellipsize( GTK_LABEL(status_label), PANGO_ELLIPSIZE_MIDDLE);
        gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, TRUE, 5);

        gtk_widget_show_all(GTK_WIDGET(window));
        cur_file_displayed = -1;
    }

    if (write_info.sync) {
        write_info.sync = 0;
        gtk_widget_destroy(window);
        window = NULL;

        popupmessage_show(NULL, _("Operation successful"), _("The files have been merged."));

        return FALSE;
    }

    if (cur_file_displayed != write_info.cur_file) {
        gchar *bn = g_path_get_basename(write_info.cur_filename);
        gchar *tmp = g_strdup_printf(_("Adding %s"), bn);
        g_free(bn);
        gchar *msg = g_markup_printf_escaped("<i>%s</i>", tmp);
        g_free(tmp);
        gtk_label_set_markup(GTK_LABEL(status_label), msg);
        g_free(msg);

        cur_file_displayed = write_info.cur_file;
    }

    fraction = 1.00*(write_info.cur_file-1+write_info.pct_done)/write_info.num_files;
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), fraction);

    gchar *msg;
    if (write_info.num_files > 1) {
        // TODO: i18n plural forms
        msg = g_strdup_printf(_("%d of %d files merged"), write_info.cur_file-1, write_info.num_files);
    } else {
        msg = g_strdup_printf(_("%d of 1 file merged"), write_info.cur_file-1);
    }
    gtk_progress_bar_set_text( GTK_PROGRESS_BAR(pbar), msg);
    g_free(msg);

    return TRUE;
}

