#!/bin/sh
#
# wavbreaker autogen.sh
#
# Thomas Perl <m@thp.io> 2007-03-11
#

autoreconf -ivf

if [ "X$1" = "Xrelease" ]; then
    VERSION=`grep ^PACKAGE_VERSION= configure | cut -d\' -f2`
    mkdir -p .release_tmp/wavbreaker-$VERSION
    cp -rpv * .release_tmp/wavbreaker-$VERSION/
    tar czvf wavbreaker-$VERSION.tar.gz --exclude=.git -C .release_tmp wavbreaker-$VERSION
    rm -rf .release_tmp
    echo "--> wavbreaker-$VERSION.tar.gz"
    exit 0
fi

if [ "X$1" = "Xconfigure" ]; then
    shift
    ./configure $*
    exit 0
fi
