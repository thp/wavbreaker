INCLUDES = $(WAVBREAKER_CFLAGS)

bin_PROGRAMS = wavbreaker wavmerge

wavbreaker_SOURCES = cdda.c linuxaudio.c sample.c wav.c wavbreaker.c
wavmerge_SOURCES = wav.c wavmerge.c

noinst_HEADERS = cdda.h linuxaudio.h sample.h wav.h wavbreaker.h

wavbreaker_LDADD = $(WAVBREAKER_LIBS)
