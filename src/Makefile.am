INCLUDES = $(WAVBREAKER_CFLAGS) -I$(srcdir) -DIMAGEDIR="\"$(datadir)/wavbreaker/images/\""
#wavbreaker_CPPFLAGS = -DIMAGEDIR=$(datadir)

bin_PROGRAMS = wavbreaker wavmerge wavinfo

wavbreaker_SOURCES = cdda.c linuxaudio.c sample.c wav.c wavbreaker.c about.c \
	appconfig.c autosplit.c popupmessage.c
wavmerge_SOURCES = wav.c wavmerge.c
wavinfo_SOURCES = wavinfo.c wav.c

noinst_HEADERS = cdda.h linuxaudio.h sample.h wav.h wavbreaker.h about.h \
	appconfig.h autosplit.h popupmessage.h

wavbreaker_LDADD = $(WAVBREAKER_LIBS)
