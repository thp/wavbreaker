INCLUDES = $(AO_CFLAGS) $(GTK2_CFLAGS) $(LIBXML2_CFLAGS) -I$(srcdir)
AM_CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DICONDIR="\"$(datadir)/pixmaps/\""

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = wavbreaker wavmerge wavinfo wavgen

wavbreaker_SOURCES = cdda.c sample.c wav.c wavbreaker.c about.c \
	appconfig.c autosplit.c popupmessage.c saveas.c browsedir.c \
	toc.c reallyquit.c fileutils.c overwritedialog.c \
	guimerge.c aoaudio.c

wavmerge_SOURCES = wav.c wavmerge.c fileutils.c
wavinfo_SOURCES = wavinfo.c wav.c
wavgen_SOURCES = wavgen.c wav.c fileutils.c

noinst_HEADERS = cdda.h sample.h wav.h wavbreaker.h about.h \
	appconfig.h autosplit.h popupmessage.h saveas.h browsedir.h aoaudio.h \
	toc.h reallyquit.h fileutils.h overwritedialog.h guimerge.h gettext.h

wavbreaker_LDADD = $(GTK2_LIBS) $(LIBXML2_LIBS) $(AO_LIBS) $(MPG123_LIBS)
wavgen_LDADD = $(GTK2_LIBS)
