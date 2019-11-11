INCLUDES = $(AO_CFLAGS) $(GTK3_CFLAGS) $(GLIB_CFLAGS) -I$(srcdir)
AM_CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE \
	    -DGTK_DISABLE_SINGLE_INCLUDES -DGSEAL_ENABLE \
	    -DGDK_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = wavbreaker wavmerge wavinfo wavgen

wavbreaker_SOURCES = cdda.c cue.c sample.c wav.c wavbreaker.c about.c \
	appconfig.c autosplit.c popupmessage.c saveas.c \
	toc.c reallyquit.c overwritedialog.c moodbar.c draw.c \
	guimerge.c aoaudio.c

wavmerge_SOURCES = wav.c wavmerge.c
wavinfo_SOURCES = wavinfo.c wav.c
wavgen_SOURCES = wavgen.c wav.c

noinst_HEADERS = cdda.h sample.h wav.h wavbreaker.h about.h \
	appconfig.h autosplit.h popupmessage.h saveas.h aoaudio.h \
	toc.h reallyquit.h overwritedialog.h guimerge.h gettext.h moodbar.h draw.h

wavbreaker_LDADD = $(GTK3_LIBS) $(AO_LIBS) $(MPG123_LIBS)
wavmerge_LDADD = $(GLIB_LIBS)
wavgen_LDADD = $(GLIB_LIBS)
wavinfo_LDADD = $(GLIB_LIBS)
