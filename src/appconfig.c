/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2003 Timothy Robinson
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

#ifndef _WIN32
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#endif

#define APPCONFIG_FILENAME "/.wavbreaker"

static GtkWidget *window;

/* Output directory for wave files. */
static int use_outputdir = 0;
static GtkWidget *use_outputdir_toggle = NULL;
static char *outputdir = NULL;
static GtkWidget *outputdir_entry = NULL;
static GtkWidget *browse_button = NULL;
static GtkWidget *outputdir_label = NULL;

/* Device file for audio output. */
static char *outputdev = NULL;
static GtkWidget *outputdev_entry = NULL;

/* Config Filename */
static char *configfilename = NULL;

/* function prototypes */
static int appconfig_write_file();

int get_use_outputdir()
{
	return use_outputdir;
}

void set_use_outputdir(const char *val)
{
	use_outputdir = atoi(val);
}

char *get_outputdir()
{
	return outputdir;
}

void set_outputdir(const char *val)
{
	if (outputdir != NULL) {
		g_free(outputdir);
	}
	outputdir = g_strdup(val);
}

char *get_outputdev()
{
	return outputdev;
}

void set_outputdev(const char *val)
{
	if (outputdev != NULL) {
		g_free(outputdev);
	}
	outputdev = g_strdup(val);
}

char *get_configfilename()
{
	return configfilename;
}

void set_configfilename(const char *val)
{
	if (configfilename != NULL) {
		g_free(configfilename);
	}
	configfilename = g_strdup(val);
}

static void use_outputdir_toggled(GtkWidget *widget, gpointer user_data)
{
	if (get_use_outputdir()) {
		// disable the output dir widget
		gtk_widget_set_sensitive(outputdir_entry, FALSE);
		gtk_widget_set_sensitive(browse_button, FALSE);
		gtk_widget_set_sensitive(outputdir_label, FALSE);
		set_use_outputdir("0");
	} else {
		// enable the output dir widget
		gtk_widget_set_sensitive(outputdir_entry, TRUE);
		gtk_widget_set_sensitive(browse_button, TRUE);
		gtk_widget_set_sensitive(outputdir_label, TRUE);
		set_use_outputdir("1");
	}
}

static void appconfig_hide(GtkWidget *main_window)
{
	gtk_widget_destroy(main_window);
}

static void filesel_ok_clicked(GtkWidget *widget, gpointer user_data)
{
	GtkFileSelection *filesel = GTK_FILE_SELECTION(user_data);

	gtk_entry_set_text(GTK_ENTRY(outputdir_entry), gtk_file_selection_get_filename(filesel));

	gtk_widget_destroy(user_data);
}

static void filesel_cancel_clicked(GtkWidget *widget, gpointer user_data)
{
	gtk_widget_destroy(user_data);
}

static void browse_button_clicked(GtkWidget *widget, gpointer user_data)
{
	GtkWidget *filesel;

	filesel = gtk_file_selection_new("select output directory");
	gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(filesel), GTK_WINDOW(window));
	gtk_window_set_type_hint(GTK_WINDOW(filesel), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_resizable(GTK_WINDOW(filesel), TRUE);

	gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel),
		gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));

//	gtk_dialog_set_has_separator(GTK_DIALOG(filesel), TRUE);
	gtk_widget_hide(GTK_FILE_SELECTION(filesel)->file_list->parent);
	gtk_widget_hide(GTK_FILE_SELECTION(filesel)->file_list);
	gtk_widget_hide(GTK_FILE_SELECTION(filesel)->selection_entry);

	gtk_signal_connect(GTK_OBJECT( GTK_FILE_SELECTION(filesel)->ok_button),
		"clicked", (GtkSignalFunc)filesel_ok_clicked, filesel);

	gtk_signal_connect(GTK_OBJECT( GTK_FILE_SELECTION(filesel)->cancel_button),
		"clicked", (GtkSignalFunc)filesel_cancel_clicked, filesel);

	gtk_widget_show(filesel);
}

static void cancel_button_clicked(GtkWidget *widget, gpointer user_data)
{
	appconfig_hide(user_data);
}

static void ok_button_clicked(GtkWidget *widget, gpointer user_data)
{
	set_outputdir(gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));
	set_outputdev(gtk_entry_get_text(GTK_ENTRY(outputdev_entry)));
	appconfig_hide(GTK_WIDGET(user_data));
	appconfig_write_file();
}

void appconfig_show(GtkWidget *main_window)
{
	GtkWidget *vbox;
	GtkWidget *table;
	GtkWidget *hbbox;
	GtkWidget *outputdev_label;
	GtkWidget *hseparator;
	GtkWidget *ok_button, *cancel_button;

	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize(window);
	gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(main_window));
	gtk_window_set_type_hint(GTK_WINDOW(window), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ON_PARENT);
	gdk_window_set_functions(window->window, GDK_FUNC_MOVE);

	vbox = gtk_vbox_new(FALSE, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_widget_show(vbox);

	table = gtk_table_new(3, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), table);
	gtk_widget_show(table);

	use_outputdir_toggle = gtk_check_button_new_with_label("Use Selectable Output Directory");
	gtk_table_attach(GTK_TABLE(table), use_outputdir_toggle, 0, 3, 0, 1, GTK_FILL, 0, 5, 0);
	g_signal_connect(GTK_OBJECT(use_outputdir_toggle), "toggled",
		G_CALLBACK(use_outputdir_toggled), NULL);
	gtk_widget_show(use_outputdir_toggle);

	outputdir_label = gtk_label_new("Wave File Output Directory:");
	gtk_misc_set_alignment(GTK_MISC(outputdir_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), outputdir_label, 0, 1, 1, 2, GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdir_label);

	outputdir_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(outputdir_entry), outputdir);
	gtk_entry_set_width_chars(GTK_ENTRY(outputdir_entry), 32);
	gtk_table_attach(GTK_TABLE(table), outputdir_entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdir_entry);

	browse_button = gtk_button_new_with_label("Browse");
	gtk_table_attach(GTK_TABLE(table), browse_button, 2, 3, 1, 2, GTK_FILL, 0, 5, 0);
	g_signal_connect(G_OBJECT(browse_button), "clicked", (GtkSignalFunc)browse_button_clicked, window);
	gtk_widget_show(browse_button);

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
	gtk_widget_show(hseparator);

	table = gtk_table_new(3, 1, FALSE);
	gtk_container_add(GTK_CONTAINER(vbox), table);
	gtk_widget_show(table);

	outputdev_label = gtk_label_new("Audio Device:");
	gtk_misc_set_alignment(GTK_MISC(outputdev_label), 0, 0.5);
	gtk_table_attach(GTK_TABLE(table), outputdev_label, 0, 1, 0, 1, GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdev_label);

	outputdev_entry = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(outputdev_entry), outputdev);
	gtk_table_attach(GTK_TABLE(table), outputdev_entry, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, 0, 5, 0);
	gtk_widget_show(outputdev_entry);

	hseparator = gtk_hseparator_new();
	gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
	gtk_widget_show(hseparator);

	hbbox = gtk_hbutton_box_new();
	gtk_container_add(GTK_CONTAINER(vbox), hbbox);
	gtk_button_box_set_layout(GTK_BUTTON_BOX(hbbox), GTK_BUTTONBOX_END);
	gtk_box_set_spacing(GTK_BOX(hbbox), 10);
	gtk_widget_show(hbbox);

	cancel_button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
	gtk_box_pack_end(GTK_BOX(hbbox), cancel_button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(cancel_button), "clicked", (GtkSignalFunc)cancel_button_clicked, window);
	gtk_widget_show(cancel_button);

	ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
	gtk_box_pack_end(GTK_BOX(hbbox), ok_button, FALSE, FALSE, 5);
	g_signal_connect(G_OBJECT(ok_button), "clicked", (GtkSignalFunc)ok_button_clicked, window);
	gtk_widget_show(ok_button);

	// set directly so the toggled event is not emitted
	if (get_use_outputdir()) {
		//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_outputdir_toggle), TRUE);
		GTK_TOGGLE_BUTTON(use_outputdir_toggle)->active = TRUE;
	} else {
		//gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_outputdir_toggle), FALSE);
		GTK_TOGGLE_BUTTON(use_outputdir_toggle)->active = FALSE;
	}

	if (get_use_outputdir()) {
		// enable the output dir widget
		gtk_widget_set_sensitive(outputdir_entry, TRUE);
		gtk_widget_set_sensitive(browse_button, TRUE);
		gtk_widget_set_sensitive(outputdir_label, TRUE);
	} else {
		// disable the output dir widget
		gtk_widget_set_sensitive(outputdir_entry, FALSE);
		gtk_widget_set_sensitive(browse_button, FALSE);
		gtk_widget_set_sensitive(outputdir_label, FALSE);
	}

	gtk_widget_show(window);
}

static int appconfig_read_file() {
#ifndef _WIN32
	xmlDocPtr doc;
	xmlNodePtr cur;

	doc = xmlParseFile(get_configfilename());

    if (doc == NULL) {
		fprintf(stderr, "error reading wavbreaker config file\n");
        fprintf(stderr, "Document not parsed successfully.\n");
        return 1;
    }

    cur = xmlDocGetRootElement(doc);

    if (cur == NULL) {
		fprintf(stderr, "error reading wavbreaker config file\n");
        fprintf(stderr, "empty document\n");
        xmlFreeDoc(doc);
        return 2;
    }

    if (xmlStrcmp(cur->name, (const xmlChar *) "wavbreaker")) {
		fprintf(stderr, "error reading wavbreaker config file\n");
        fprintf(stderr, "wrong document type, root node != wavbreaker\n");
        fprintf(stderr, "document type is: %s\n", cur->name);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = cur->xmlChildrenNode;
    while (cur != NULL) {
		xmlChar *key;

        if (!(xmlStrcmp(cur->name, (const xmlChar *) "use_outputdir"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			set_use_outputdir(key);
			xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "outputdev"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			set_outputdev(key);
			xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "outputdir"))) {
			key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
			set_outputdir(key);
			xmlFree(key);
		}
        cur = cur->next;
    }

    return 0;
#endif
}

static int appconfig_write_file() {
#ifndef _WIN32
	xmlDocPtr doc;
	xmlNodePtr root;
	xmlNodePtr cur;
	char tmp_str[32];

	doc = xmlNewDoc((const xmlChar *)"1.0");

	if (doc == NULL) {
		fprintf(stderr, "error creating wavbreaker config file\n");
		fprintf(stderr, "error with xmlNewDoc\n");
		return 1;
	}

	root = xmlNewDocNode(doc, NULL, (const xmlChar *)"wavbreaker", "");

	if (root == NULL) {
		fprintf(stderr, "error creating wavbreaker config file\n");
		fprintf(stderr, "error creating doc node\n");
		xmlFreeDoc(doc);
		return 2;
	}

	sprintf(tmp_str, "%d", get_use_outputdir());
	cur = xmlNewChild(root, NULL, (const xmlChar *)"use_outputdir", (const xmlChar *) tmp_str);

	if (cur == NULL) {
		fprintf(stderr, "error creating wavbreaker config file\n");
		fprintf(stderr, "error creating use_outputdir node\n");
		xmlFreeNodeList(root);
		xmlFreeDoc(doc);
		return 3;
	}

	cur = xmlNewChild(root, NULL, (const xmlChar *)"outputdir", (const xmlChar *) get_outputdir());

	if (cur == NULL) {
		fprintf(stderr, "error creating wavbreaker config file\n");
		fprintf(stderr, "error creating outputdir node\n");
		xmlFreeNodeList(root);
		xmlFreeDoc(doc);
		return 3;
	}

	cur = xmlNewChild(root, NULL, (const xmlChar *)"outputdev", (const xmlChar *) get_outputdev());

	if (cur == NULL) {
		fprintf(stderr, "error creating wavbreaker config file\n");
		fprintf(stderr, "error creating outputdev node\n");
		xmlFreeNodeList(root);
		xmlFreeDoc(doc);
		return 3;
	}

	root = xmlDocSetRootElement(doc, root);
	/*
	if (root == NULL) {
		fprintf(stderr, "error setting doc root node\n");
		xmlFreeDoc(doc);
		return 4;
	}
	*/

	xmlIndentTreeOutput = 1;
	xmlKeepBlanksDefault(0);
	if (! xmlSaveFormatFile(get_configfilename(), doc, 1)) {
		fprintf(stderr, "error writing config file: %s", get_configfilename());
		xmlFreeNodeList(root);
		xmlFreeDoc(doc);
		return 5;
	}

	xmlFreeNodeList(root);
	xmlFreeDoc(doc);

	return 0;
#endif
}

void appconfig_init()
{
#ifndef _WIN32
	char str[1024];

	str[0] = '\0';
	strcat(str, getenv("HOME"));
	strcat(str, APPCONFIG_FILENAME);
	configfilename = g_strdup(str);

	if (access(get_configfilename(), W_OK | F_OK) || appconfig_read_file()) {
		set_use_outputdir("0");
		set_outputdir(getenv("PWD"));
		set_outputdev("/dev/dsp");
		appconfig_write_file();
	}
#else
	outputdir = g_strdup("c:\\MyFiles");
	outputdev = g_strdup("/dev/dsp");
#endif
}

