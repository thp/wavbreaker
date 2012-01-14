#!/bin/sh
#
# wavbreaker autogen.sh
#
# Thomas Perl <m@thp.io> 2007-03-11
#

echo -n "Cleaning up..."
make distclean || make clean
rm -rf aclocal.m4 autom4te.cache config.h.in config.h config.status configure depcomp INSTALL install-sh missing po/stamp-po po/*.gmo

find . -name 'Makefile.in' | xargs rm -f
find . -name 'Makefile' | xargs rm -f
echo "done."

if [ "X$1" = "Xclean" ]; then
    exit 0
fi

echo -n "Running automake stuff..."
aclocal -I m4
autoheader
automake --add-missing --copy
automake
autoconf
echo "done."

if [ "X$1" = "Xrelease" ]; then
    VERSION=`grep ^PACKAGE_VERSION= configure | cut -d\' -f2`
    mkdir -p .release_tmp/wavbreaker-$VERSION
    cp -rpv * .release_tmp/wavbreaker-$VERSION/
    tar czvf wavbreaker-$VERSION.tar.gz --exclude=.svn -C .release_tmp wavbreaker-$VERSION
    rm -rf .release_tmp
    echo "--> wavbreaker-$VERSION.tar.gz"
    exit 0
fi

if [ "X$1" = "Xconfigure" ]; then
    shift
    ./configure $*
    exit 0
fi

