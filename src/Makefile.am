INCLUDES = $(WAVBREAKER_CFLAGS) -I$(srcdir) $(PULSE_CFLAGS)
AM_CFLAGS = -Wall -D_FILE_OFFSET_BITS=64 -D_LARGEFILE_SOURCE -DICONDIR="\"$(datadir)/pixmaps/\""

localedir = $(datadir)/locale
DEFS = -DLOCALEDIR=\"$(localedir)\" @DEFS@

bin_PROGRAMS = wavbreaker wavmerge wavinfo

wavbreaker_SOURCES = cdda.c sample.c wav.c wavbreaker.c about.c \
	appconfig.c autosplit.c popupmessage.c saveas.c browsedir.c \
	toc.c nosound.c reallyquit.c fileutils.c overwritedialog.c \
	guimerge.c

if NEED_ALSA
wavbreaker_SOURCES += alsaaudio.c
endif

if NEED_PULSE
wavbreaker_SOURCES += pulseaudio.c
endif

if NEED_OSS
wavbreaker_SOURCES += linuxaudio.c
endif

wavmerge_SOURCES = wav.c wavmerge.c fileutils.c
wavinfo_SOURCES = wavinfo.c wav.c

noinst_HEADERS = cdda.h linuxaudio.h sample.h wav.h wavbreaker.h about.h pulseaudio.h \
	appconfig.h autosplit.h popupmessage.h alsaaudio.h saveas.h browsedir.h \
	toc.h nosound.h reallyquit.h fileutils.h overwritedialog.h guimerge.h gettext.h

wavbreaker_LDADD = $(WAVBREAKER_LIBS) $(ALSA_LIBS) $(PULSE_LIBS)

