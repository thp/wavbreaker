INCLUDES = $(WAVBREAKER_CFLAGS) -I$(srcdir)

bin_PROGRAMS = wavbreaker wavmerge wavinfo

wavbreaker_SOURCES = cdda.c linuxaudio.c sample.c wav.c wavbreaker.c about.c \
	appconfig.c
wavmerge_SOURCES = wav.c wavmerge.c
wavinfo_SOURCES = wavinfo.c wav.c

noinst_HEADERS = cdda.h linuxaudio.h sample.h wav.h wavbreaker.h about.h \
	appconfig.h

wavbreaker_LDADD = $(WAVBREAKER_LIBS)
