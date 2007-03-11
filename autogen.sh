#!/bin/sh
#
# wavbreaker autogen.sh
#
# Thomas Perl <thp@perli.net> 2007-03-11
#

echo -n "Cleaning up..."
make distclean
rm -rf aclocal.m4 autom4te.cache config.h.in config.h config.status configure depcomp INSTALL install-sh missing

find . -name 'Makefile.in' | xargs rm -f
find . -name 'Makefile' | xargs rm -f
echo "done."

if [ "$1" == "clean" ]; then
    exit 0
fi

echo -n "Running automake stuff..."
aclocal
autoheader
automake --add-missing
automake
autoconf
echo "done."

if [ "$1" == "configure" ]; then
    shift
    ./configure $*
fi

