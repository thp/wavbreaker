/* wavbreaker - A tool to split a wave file up into multiple waves.
 * Copyright (C) 2002-2006 Timothy Robinson
 * Copyright (C) 2006-2019 Thomas Perl
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

#include "moodbar.h"
#include "popupmessage.h"

#if defined(WANT_MOODBAR)

#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libgen.h>

#include <locale.h>
#include "gettext.h"

static pid_t moodbar_pid;
static gboolean moodbar_cancelled;
static GtkWidget *moodbar_wait_dialog;

static void
cancel_moodbar_process(GtkWidget *widget, gpointer user_data)
{
    const gchar *fn = user_data;

    moodbar_cancelled = TRUE;
    kill(moodbar_pid, SIGKILL);

    /* remove (partial) .mood file */
    unlink(fn);
}

static void
hide_moodbar_process(GtkWidget *widget, gpointer user_data)
{
    gtk_widget_hide(GTK_WIDGET(user_data));
}

static gchar *
get_moodbar_filename(const gchar *filename)
{
	/* replace ".xxx" with ".mood" */
	gchar *fn = (gchar*)malloc(strlen(filename)+2);
	strcpy(fn, filename);
	strcpy((gchar*)(fn+strlen(fn)-4), ".mood");
	return fn;
}

gboolean
moodbar_generate(GtkWidget *window, const gchar *filename)
{
    struct stat st;
    int child_status;
    GtkWidget *child, *cancel_button;

    gchar *fn = get_moodbar_filename(filename);

    if (stat(fn, &st) == 0) {
		return TRUE;
	}

	if (moodbar_wait_dialog != NULL) {
		return FALSE;
	}

	moodbar_cancelled = FALSE;
	moodbar_pid = fork();

	if (moodbar_pid == 0) {
		if (execlp("moodbar", "moodbar", "-o", fn, filename, (char*)NULL) == -1) {
			fprintf(stderr, "Error running moodbar: %s (Have you installed the \"moodbar\" package?)\n", strerror(errno));
			_exit(-1);
		}
	} else if (moodbar_pid > 0) {
		moodbar_wait_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
							  GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
							  GTK_MESSAGE_INFO,
							  GTK_BUTTONS_NONE,
							  _("Generating moodbar"));

		child = gtk_progress_bar_new();
		gtk_progress_bar_set_text(GTK_PROGRESS_BAR(child), basename(fn));
		gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(moodbar_wait_dialog))), child, FALSE, TRUE, 0);

		cancel_button = gtk_button_new_with_mnemonic(_("_Hide window"));
		g_signal_connect(G_OBJECT(cancel_button), "clicked", G_CALLBACK(hide_moodbar_process), moodbar_wait_dialog);
		gtk_dialog_add_action_widget(GTK_DIALOG(moodbar_wait_dialog), cancel_button, -1);

		cancel_button = gtk_button_new_with_mnemonic(_("_Cancel"));
		g_signal_connect(G_OBJECT(cancel_button), "clicked", G_CALLBACK(cancel_moodbar_process), fn);
		gtk_dialog_add_action_widget(GTK_DIALOG(moodbar_wait_dialog), cancel_button, -1);

		gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(moodbar_wait_dialog),
                _("The moodbar tool analyzes your audio file and generates a colorful representation of the audio data."));
		gtk_window_set_title(GTK_WINDOW(moodbar_wait_dialog), _("Generating moodbar"));
		gtk_widget_show_all(moodbar_wait_dialog);

        // FIXME: Do this via g_idle_add()
		time_t t = time(NULL);
		while (waitpid(moodbar_pid, &child_status, WNOHANG) == 0) {
			while (gtk_events_pending()) {
				gtk_main_iteration();
			}

			if (time(NULL) > t) {
				gtk_progress_bar_pulse(GTK_PROGRESS_BAR(child));
				t = time(NULL);
			}
		}

		gtk_widget_destroy(moodbar_wait_dialog);
		moodbar_wait_dialog = NULL;

		if (child_status != 0 && !moodbar_cancelled) {
			popupmessage_show(NULL, _("Cannot launch \"moodbar\""), _("wavbreaker could not launch the moodbar application, which is needed to generate the moodbar. You can download the moodbar package from:\n\n      http://amarok.kde.org/wiki/Moodbar"));
            return FALSE;
		}

        return TRUE;
	}

    fprintf(stderr, "fork() failed for moodbar process: %s\n", strerror(errno));
    return FALSE;
}

MoodbarData *
moodbar_open(const gchar *filename)
{
	gchar *fn = get_moodbar_filename(filename);

    FILE *fp = fopen(fn, "rb");

    if (!fp) {
		free(fn);
		return NULL;
    }

    MoodbarData *result = calloc(sizeof(MoodbarData), 1);

    fseek(fp, 0, SEEK_END);
    long length = ftell(fp);
    fseek(fp, 0, SEEK_SET);

	result->numFrames = length/3;

    result->frames = calloc(result->numFrames, sizeof(GdkRGBA));

    unsigned long pos = 0;

    guchar tmp[3];
    while (fread(&tmp, 3, 1, fp) > 0) {
        result->frames[pos].red = (float)tmp[0]/255.f;
        result->frames[pos].green = (float)tmp[1]/255.f;
        result->frames[pos].blue = (float)tmp[2]/255.f;
        pos++;
    }

    fclose(fp);
    free(fn);

	return result;
}

void
moodbar_free(MoodbarData *data)
{
	free(data->frames);
	free(data);
}

#else
gboolean
moodbar_generate(GtkWidget *window, const gchar *filename)
{
    return FALSE;
}

MoodbarData *
moodbar_open(const gchar *filename)
{
    return NULL;
}

void
moodbar_free(MoodbarData *data)
{
}
#endif
