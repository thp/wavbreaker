#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(wavbreaker, 0.11)
AC_CONFIG_SRCDIR(src/wavbreaker.h)
AM_INIT_AUTOMAKE(1.9)
AM_CONFIG_HEADER(config.h)

AM_GNU_GETTEXT([external])

AM_SILENT_RULES([yes])

# Parameters to configure
AC_ARG_ENABLE(alsa,
    [  --disable-alsa      Disable detection of ALSA],
    [enable_alsa=$enableval],
    [enable_alsa=yes])

AC_ARG_ENABLE(pulse,
    [  --disable-pulse      Disable detection of PulseAudio],
    [enable_pulse=$enableval],
    [enable_pulse=yes])

AC_ARG_ENABLE(ao,
    [  --disable-ao      Disable detection of libao],
    [enable_ao=$enableval],
    [enable_ao=yes])

AC_ARG_ENABLE(mpg123,
    [  --disable-mpg123      Disable detection of libmpg123],
    [enable_mpg123=$enableval],
    [enable_mpg123=yes])

AC_ARG_ENABLE(oss,
    [  --disable-oss       Disable detection of OSS],
    [enable_oss=$enableval],
    [enable_oss=yes])

# Checks for programs.
AC_PROG_AWK
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S

# Checks for libraries.
AC_CHECK_LIB([m], [cos])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h sys/ioctl.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_FUNC_MALLOC
AC_FUNC_MEMCMP
AC_FUNC_STAT
AC_CHECK_FUNCS([strchr strdup strrchr strstr])


if test "x$enable_alsa" = "xyes"
then
    AC_CHECK_LIB([asound], [snd_pcm_open],
        [found_alsa=yes; ALSA_LIBS=-lasound],
        [found_alsa=no])
    if test "x$found_alsa" = "xyes"
    then
        AC_DEFINE([HAVE_ALSA], 1, [Define if you have Alsa installed])
    else
        enable_alsa=no
    fi
fi
AC_SUBST(ALSA_LIBS)

if test "x$enable_pulse" = "xyes"
then
    PKG_CHECK_MODULES(PULSE, libpulse-simple,
                      [found_pulse=yes],
                      [found_pulse=no])
    if test "x$found_pulse" = "xyes"
    then
        AC_DEFINE([HAVE_PULSE], 1, [Define if you have PulseAudio installed])
    else
        enable_pulse=no
    fi
fi
AC_SUBST(PULSE_LIBS)

if test "x$enable_ao" = "xyes"
then
    PKG_CHECK_MODULES(AO, ao,
                      [found_ao=yes],
                      [found_ao=no])
    if test "x$found_ao" = "xyes"
    then
        AC_DEFINE([HAVE_AO], 1, [Define if you have libao installed])
    else
        enable_ao=no
    fi
fi
AC_SUBST(AO_LIBS)

if test "x$enable_mpg123" = "xyes"
then
    PKG_CHECK_MODULES(MPG123, libmpg123,
                      [found_mpg123=yes],
                      [found_mpg123=no])
    if test "x$found_mpg123" = "xyes"
    then
        AC_DEFINE([HAVE_MPG123], 1, [Define if you have libmpg123 installed])
    else
        enable_mpg123=no
    fi
fi
AC_SUBST(MPG123_LIBS)

if test "x$enable_oss" = "xyes"
then
    AC_CHECK_HEADERS(sys/soundcard.h machine/soundcard.h, found_oss=yes)

    if test "x$found_oss" = "xyes"
    then
        AC_DEFINE([HAVE_OSS], 1, [Define if you have OSS installed])
    else
        enable_oss=no
    fi
fi

PKG_CHECK_MODULES(GTK2, gtk+-2.0)
AC_SUBST(GTK2_CFLAGS)
AC_SUBST(GTK2_LIBS)

PKG_CHECK_MODULES(LIBXML2, libxml-2.0)
AC_SUBST(LIBXML2_CFLAGS)
AC_SUBST(LIBXML2_LIBS)

AM_CONDITIONAL(NEED_ALSA, test "x$enable_alsa" = "xyes")
AM_CONDITIONAL(NEED_PULSE, test "x$enable_pulse" = "xyes")
AM_CONDITIONAL(NEED_AO, test "x$enable_ao" = "xyes")
AM_CONDITIONAL(NEED_MPG123, test "x$enable_mpg123" = "xyes")
AM_CONDITIONAL(NEED_OSS, test "x$enable_oss" = "xyes")

AC_CONFIG_FILES([Makefile po/Makefile.in
                 images/Makefile
                 src/Makefile
                 doc/Makefile])

AC_OUTPUT

echo
echo "ALSA Driver.......................   $enable_alsa"
echo "PulseAudio Driver.................   $enable_pulse"
echo "libao Driver......................   $enable_ao"
echo "OSS Driver........................   $enable_oss"
echo ""
echo "MP3 access using libmpg123 .......   $enable_mpg123"
echo
echo "    Configure finished.  Do 'make && make install'"
echo "    to compile and install wavbreaker."
echo

