#!/bin/bash

DOCFILES="AUTHORS CONTRIBUTORS COPYING README.md"

VERSION=$(sed -n "s/^  version : '\(.*\)',/\1/p" meson.build)

USAGE="Usage: $0 <linux|windows|macos|flatpak|snap>"

if [ $# -eq 0 ]; then
    echo "$USAGE"
    exit 1
fi

set -e -x

case "$1" in
    linux)
        meson linux_build
        DESTDIR="$(pwd)/wavbreaker-$VERSION-linux/" ninja -C linux_build install -v
        mkdir -p dist
        zip -r "dist/wavbreaker-$VERSION-linux.zip" "wavbreaker-$VERSION-linux" $DOCFILES
        ;;
    windows)
        docker build -t wavbreaker-win32-build scripts/win32
        sh scripts/win32/run-build.sh
        rm -rf "wavbreaker-$VERSION-win32"
        mv wavbreaker-win32 "wavbreaker-$VERSION-win32"
        mkdir -p dist
        zip -r "dist/wavbreaker-$VERSION-win32.zip" "wavbreaker-$VERSION-win32" $DOCFILES
        ;;
    flatpak)
        docker build -t wavbreaker-flatpak-build scripts/flatpak
        sh scripts/flatpak/package.sh
        ;;
    snap)
        snapcraft
        mkdir -p dist
        mv wavbreaker_*.snap dist/
        ;;
    macos)
        meson --prefix=/Applications/wavbreaker.app \
              --bindir=Contents/MacOS \
              -Dmacos_app=true \
              macos_build
        DESTDIR=$(pwd)/ ninja -C macos_build install -v
        mkdir -p dist
        zip -r "dist/wavbreaker-$VERSION-macos.zip" Applications $DOCFILES
        ;;
    *)
        echo "$USAGE"
        exit 1
        ;;
esac
