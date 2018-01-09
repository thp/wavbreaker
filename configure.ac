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
AC_ARG_ENABLE(mpg123,
    [  --disable-mpg123      Disable detection of libmpg123],
    [enable_mpg123=$enableval],
    [enable_mpg123=yes])

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

PKG_CHECK_MODULES(AO, [ao])
AC_SUBST(AO_CFLAGS)
AC_SUBST(AO_LIBS)

PKG_CHECK_MODULES(GTK3, gtk+-3.0)
AC_SUBST(GTK3_CFLAGS)
AC_SUBST(GTK3_LIBS)

PKG_CHECK_MODULES(GLIB, glib-2.0)
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

AM_CONDITIONAL(NEED_MPG123, test "x$enable_mpg123" = "xyes")

AC_CONFIG_FILES([Makefile po/Makefile.in
                 images/Makefile
                 src/Makefile
                 doc/Makefile])

AC_OUTPUT

echo
echo "MP3 access using libmpg123 .......   $enable_mpg123"
echo
echo "    Configure finished.  Do 'make && make install'"
echo "    to compile and install wavbreaker."
echo
