/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2004 Timothy Robinson
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
#include <config.h>

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

/* Etree filename suffix */
/* Filename suffix (not extension) for wave files. */
static int use_etree_filename_suffix = 0;
static GtkWidget *use_etree_filename_suffix_toggle = NULL;

static char *etree_filename_suffix = NULL;
static GtkWidget *etree_filename_suffix_label = NULL;
static GtkWidget *etree_filename_suffix_entry = NULL;

static char *etree_cd_length = NULL;
static GtkWidget *etree_cd_length_label = NULL;
static GtkWidget *etree_cd_length_entry = NULL;

/* Device file for audio output. */
static char *outputdev = NULL;
static GtkWidget *outputdev_entry = NULL;

/* Config Filename */
static char *config_filename = NULL;

/* function prototypes */
static int appconfig_write_file();
static int appconfig_read_file();
static void default_all_strings();
static void open_select_outputdir_2();
static void open_select_outputdir();

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

char *default_outputdir()
{
    if (outputdir != NULL) {
        g_free(outputdir);
    }
    outputdir = g_strdup(getenv("PWD"));
    return outputdir;
}

int get_use_etree_filename_suffix()
{
    return use_etree_filename_suffix;
}

void set_use_etree_filename_suffix(const char *val)
{
    use_etree_filename_suffix = atoi(val);
}

char *get_etree_filename_suffix()
{
    return etree_filename_suffix;
}

void set_etree_filename_suffix(const char *val)
{
    if (etree_filename_suffix != NULL) {
        g_free(etree_filename_suffix);
    }
    etree_filename_suffix = g_strdup(val);
}

char *default_etree_filename_suffix()
{
    if (etree_filename_suffix != NULL) {
        g_free(etree_filename_suffix);
    }
    etree_filename_suffix = g_strdup("-");
    return etree_filename_suffix;
}

char *get_etree_cd_length()
{
    return etree_cd_length;
}

void set_etree_cd_length(const char *val)
{
    if (etree_cd_length != NULL) {
        g_free(etree_cd_length);
    }
    etree_cd_length = g_strdup(val);
}

char *default_etree_cd_length()
{
    if (etree_cd_length != NULL) {
        g_free(etree_cd_length);
    }
    etree_cd_length = g_strdup("80");
    return etree_cd_length;
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

char *default_outputdev()
{
    if (outputdev != NULL) {
        g_free(outputdev);
    }
    outputdev = g_strdup("/dev/dsp");
    return outputdev;
}

char *get_config_filename()
{
    return config_filename;
}

void set_config_filename(const char *val)
{
    if (config_filename != NULL) {
        g_free(config_filename);
    }
    config_filename = g_strdup(val);
}

char *default_config_filename()
{
    char str[1024];

    if (config_filename != NULL) {
        g_free(config_filename);
    }

    str[0] = '\0';
    strncat(str, getenv("HOME"), 1024);
    strncat(str, APPCONFIG_FILENAME, 1024);
    config_filename = g_strdup(str);

    return config_filename;
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

static void use_etree_filename_suffix_toggled(GtkWidget *widget, gpointer user_data)
{
    if (get_use_etree_filename_suffix()) {
        gtk_widget_set_sensitive(etree_filename_suffix_entry, TRUE);
        gtk_widget_set_sensitive(etree_filename_suffix_label, TRUE);
        gtk_widget_set_sensitive(etree_cd_length_entry, FALSE);
        gtk_widget_set_sensitive(etree_cd_length_label, FALSE);
        set_use_etree_filename_suffix("0");
    } else {
        gtk_widget_set_sensitive(etree_filename_suffix_entry, FALSE);
        gtk_widget_set_sensitive(etree_filename_suffix_label, FALSE);
        gtk_widget_set_sensitive(etree_cd_length_entry, TRUE);
        gtk_widget_set_sensitive(etree_cd_length_label, TRUE);
        set_use_etree_filename_suffix("1");
    }
}

static void appconfig_hide(GtkWidget *main_window)
{
    gtk_widget_destroy(main_window);
}

static void filesel_ok_clicked(GtkWidget *widget, gpointer user_data)
{
    GtkFileSelection *filesel = GTK_FILE_SELECTION(user_data);

    gtk_entry_set_text(GTK_ENTRY(outputdir_entry),
        gtk_file_selection_get_filename(filesel));

    //printf("file: %s\n", gtk_file_selection_get_filename(filesel));

    gtk_widget_destroy(user_data);
}

static void filesel_cancel_clicked(GtkWidget *widget, gpointer user_data)
{
    gtk_widget_destroy(user_data);
}

static void browse_button_clicked(GtkWidget *widget, gpointer user_data)
{
#ifdef HAVE_GTK_2_3
    open_select_outputdir_2();
#else
    open_select_outputdir();
#endif
}

static void open_select_outputdir_2() {
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialog_new("Select Output Directory",
        GTK_WINDOW(window), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, GTK_STOCK_OPEN,
        GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog),
        gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename;

        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        printf("filename: %s\n", filename);
        gtk_entry_set_text(GTK_ENTRY(outputdir_entry), filename);
        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

static void open_select_outputdir() {
    GtkWidget *filesel;
    gchar filename[4096];

    filesel = gtk_file_selection_new("select output directory");
    gtk_window_set_modal(GTK_WINDOW(filesel), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(filesel), GTK_WINDOW(window));
    gtk_window_set_type_hint(GTK_WINDOW(filesel), GDK_WINDOW_TYPE_HINT_DIALOG);
    gtk_window_set_resizable(GTK_WINDOW(filesel), TRUE);

    strcpy(filename, gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));
    strcat(filename, "/");
    gtk_file_selection_set_filename(GTK_FILE_SELECTION(filesel), filename);

//    gtk_dialog_set_has_separator(GTK_DIALOG(filesel), TRUE);
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
    appconfig_read_file();
}

static void ok_button_clicked(GtkWidget *widget, gpointer user_data)
{
    set_outputdir(gtk_entry_get_text(GTK_ENTRY(outputdir_entry)));
    set_outputdev(gtk_entry_get_text(GTK_ENTRY(outputdev_entry)));
    set_etree_filename_suffix(gtk_entry_get_text(GTK_ENTRY(etree_filename_suffix_entry)));
    set_etree_cd_length(gtk_entry_get_text(GTK_ENTRY(etree_cd_length_entry)));

    track_break_rename();
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

    /* Selectable Output Directory */
    table = gtk_table_new(2, 3, FALSE);
    gtk_container_add(GTK_CONTAINER(vbox), table);
    gtk_widget_show(table);

    use_outputdir_toggle = gtk_check_button_new_with_label("Use Selectable Output Directory");
    gtk_table_attach(GTK_TABLE(table), use_outputdir_toggle,
        0, 2, 0, 1, GTK_FILL, 0, 5, 0);
    g_signal_connect(GTK_OBJECT(use_outputdir_toggle), "toggled",
        G_CALLBACK(use_outputdir_toggled), NULL);
    gtk_widget_show(use_outputdir_toggle);

    outputdir_label = gtk_label_new("Wave File Output Directory:");
    gtk_misc_set_alignment(GTK_MISC(outputdir_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), outputdir_label,
        0, 2, 1, 2, GTK_FILL, 0, 5, 0);
    gtk_widget_show(outputdir_label);

    outputdir_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(outputdir_entry), outputdir);
    gtk_entry_set_width_chars(GTK_ENTRY(outputdir_entry), 40);
    gtk_table_attach(GTK_TABLE(table), outputdir_entry,
        0, 1, 2, 3, GTK_EXPAND | GTK_FILL, 0, 5, 0);
    gtk_widget_show(outputdir_entry);

    browse_button = gtk_button_new_with_label("Browse");
    gtk_table_attach(GTK_TABLE(table), browse_button,
        1, 2, 2, 3, GTK_FILL, 0, 5, 0);
    g_signal_connect(G_OBJECT(browse_button), "clicked", (GtkSignalFunc)browse_button_clicked, window);
    gtk_widget_show(browse_button);

    /* Etree Filename Suffix */

    hseparator = gtk_hseparator_new();
    gtk_box_pack_start(GTK_BOX(vbox), hseparator, FALSE, TRUE, 5);
    gtk_widget_show(hseparator);

    table = gtk_table_new(2, 5, FALSE);
    gtk_container_add(GTK_CONTAINER(vbox), table);
    gtk_widget_show(table);

    use_etree_filename_suffix_toggle = gtk_check_button_new_with_label("Use Etree Style Filename Suffix (d#t##)");
    gtk_table_attach(GTK_TABLE(table), use_etree_filename_suffix_toggle,
        0, 2, 0, 1, GTK_FILL, 0, 5, 2);
    g_signal_connect(GTK_OBJECT(use_etree_filename_suffix_toggle), "toggled",
        G_CALLBACK(use_etree_filename_suffix_toggled), NULL);
    gtk_widget_show(use_etree_filename_suffix_toggle);

    etree_filename_suffix_label = gtk_label_new("Filename Suffix:");
    gtk_misc_set_alignment(GTK_MISC(etree_filename_suffix_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), etree_filename_suffix_label,
        0, 1, 1, 2, GTK_FILL, 0, 5, 2);
    gtk_widget_show(etree_filename_suffix_label);

    etree_filename_suffix_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(etree_filename_suffix_entry), etree_filename_suffix);
    gtk_entry_set_width_chars(GTK_ENTRY(etree_filename_suffix_entry), 10);
    gtk_table_attach(GTK_TABLE(table), etree_filename_suffix_entry,
        1, 2, 1, 2, GTK_EXPAND | GTK_FILL, 0, 5, 2);
    gtk_widget_show(etree_filename_suffix_entry);

    etree_cd_length_label = gtk_label_new("CD Length:");
    gtk_misc_set_alignment(GTK_MISC(etree_cd_length_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), etree_cd_length_label,
        0, 1, 2, 3, GTK_FILL, 0, 5, 2);
    gtk_widget_show(etree_cd_length_label);

    etree_cd_length_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(etree_cd_length_entry), etree_cd_length);
    gtk_entry_set_width_chars(GTK_ENTRY(etree_cd_length_entry), 10);
    gtk_table_attach(GTK_TABLE(table), etree_cd_length_entry, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, 0, 5, 2);
    gtk_widget_show(etree_cd_length_entry);

    /* Audio Output Device */
    hseparator = gtk_hseparator_new();
    gtk_table_attach(GTK_TABLE(table), hseparator,
        0, 2, 3, 4, GTK_FILL, 0, 0, 5);
    gtk_widget_show(hseparator);

    outputdev_label = gtk_label_new("Audio Device:");
    gtk_misc_set_alignment(GTK_MISC(outputdev_label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), outputdev_label,
        0, 1, 4, 5, GTK_FILL, 0, 5, 0);
    gtk_widget_show(outputdev_label);

    outputdev_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(outputdev_entry), outputdev);
    gtk_table_attach(GTK_TABLE(table), outputdev_entry,
        1, 2, 4, 5, GTK_EXPAND | GTK_FILL, 0, 5, 0);
    gtk_widget_show(outputdev_entry);

    /* OK and Cancel Buttons */
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

    if (get_use_outputdir()) {
        // enable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, TRUE);
        gtk_widget_set_sensitive(browse_button, TRUE);
        gtk_widget_set_sensitive(outputdir_label, TRUE);

        //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_outputdir_toggle), TRUE);
        GTK_TOGGLE_BUTTON(use_outputdir_toggle)->active = TRUE;
    } else {
        // disable the output dir widget
        gtk_widget_set_sensitive(outputdir_entry, FALSE);
        gtk_widget_set_sensitive(browse_button, FALSE);
        gtk_widget_set_sensitive(outputdir_label, FALSE);

        //gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(use_outputdir_toggle), FALSE);
        GTK_TOGGLE_BUTTON(use_outputdir_toggle)->active = FALSE;
    }

    if (get_use_etree_filename_suffix()) {
        gtk_widget_set_sensitive(etree_filename_suffix_entry, FALSE);
        gtk_widget_set_sensitive(etree_filename_suffix_label, FALSE);
        gtk_widget_set_sensitive(etree_cd_length_entry, TRUE);
        gtk_widget_set_sensitive(etree_cd_length_label, TRUE);

        // set directly so the toggled event is not emitted
        GTK_TOGGLE_BUTTON(use_etree_filename_suffix_toggle)->active = TRUE;
    } else {
        gtk_widget_set_sensitive(etree_filename_suffix_entry, TRUE);
        gtk_widget_set_sensitive(etree_filename_suffix_label, TRUE);
        gtk_widget_set_sensitive(etree_cd_length_entry, FALSE);
        gtk_widget_set_sensitive(etree_cd_length_label, FALSE);

        // set directly so the toggled event is not emitted
        GTK_TOGGLE_BUTTON(use_etree_filename_suffix_toggle)->active = FALSE;
    }

    gtk_widget_show(window);
}

static int appconfig_read_file() {
#ifndef _WIN32
    xmlDocPtr doc;
    xmlNodePtr cur;

    doc = xmlParseFile(get_config_filename());

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
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "use_etree_filename_suffix"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_use_etree_filename_suffix(key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "etree_filename_suffix"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_etree_filename_suffix(key);
            xmlFree(key);
        } else if (!(xmlStrcmp(cur->name, (const xmlChar *) "etree_cd_length"))) {
            key = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
            set_etree_cd_length(key);
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

    cur = xmlNewChild(root, NULL, (const xmlChar *)"outputdir",
        (const xmlChar *) get_outputdir());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating outputdir node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"outputdev",
        (const xmlChar *) get_outputdev());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating outputdev node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    sprintf(tmp_str, "%d", get_use_etree_filename_suffix());
    cur = xmlNewChild(root, NULL, (const xmlChar *)"use_etree_filename_suffix",
        (const xmlChar *) tmp_str);

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating use_etree_filename_suffix node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"etree_filename_suffix",
        (const xmlChar *) get_etree_filename_suffix());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating etree_filename_suffix node\n");
        xmlFreeNodeList(root);
        xmlFreeDoc(doc);
        return 3;
    }

    cur = xmlNewChild(root, NULL, (const xmlChar *)"etree_cd_length",
        (const xmlChar *) get_etree_cd_length());

    if (cur == NULL) {
        fprintf(stderr, "error creating wavbreaker config file\n");
        fprintf(stderr, "error creating etree_cd_length node\n");
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
    /*
    xmlThrDefIndentTreeOutput(1);
    xmlKeepBlanksDefault(0);
    */
    if (! xmlSaveFormatFile(get_config_filename(), doc, 1)) {
        fprintf(stderr, "error writing config file: %s", get_config_filename());
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
    default_config_filename();

    /*
     * See if we can read and write the config file,
     * then try to read in the values.
     */
    if (access(get_config_filename(), W_OK | F_OK) || appconfig_read_file()) {
        default_all_strings();
        appconfig_write_file();
    } else {
        default_all_strings();
    }

#else
    outputdir = g_strdup("c:\\MyFiles");
    outputdev = g_strdup("/dev/dsp");
    etree_filename_suffix = g_strdup("-");
    etree_cd_length = g_strdup("80");
#endif
}

void default_all_strings() {
    /* default any values that where not in the config file */
    if (get_outputdir() == NULL) {
        default_outputdir();
    }
    if (get_outputdev() == NULL) {
        default_outputdev();
    }
    if (get_etree_filename_suffix() == NULL) {
        default_etree_filename_suffix();
    }
    if (get_etree_cd_length() == NULL) {
        default_etree_cd_length();
    }
}

