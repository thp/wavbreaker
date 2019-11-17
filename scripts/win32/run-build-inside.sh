#!/bin/sh

SYSROOT=/usr/x86_64-w64-mingw32/sys-root/mingw/
OUT=wavbreaker-win32

meson win32_build \
    --prefix / \
    --cross-file scripts/win32/linux-mingw-w64-64bit.txt \
    -Dbindir=/ \
    -Dwindows_app=true \
    -Dmoodbar=false

DESTDIR=/build/$OUT/ ninja -C /build/win32_build/ install -v

MINGW_BUNDLEDLLS_SEARCH_PATH=$SYSROOT/bin \
    scripts/win32/mingw-bundledlls --copy $OUT/wavbreaker.exe

mkdir -p $OUT/share/glib-2.0/
cp -rpv $SYSROOT/share/glib-2.0/schemas $OUT/share/glib-2.0/

mkdir -p $OUT/share/icons/{Adwaita,hicolor}
cp $SYSROOT/share/icons/Adwaita/index.theme $OUT/share/icons/Adwaita
sed -e 's/Adwaita/hicolor/' $OUT/share/icons/Adwaita/index.theme \
    >$OUT/share/icons/hicolor/index.theme

cd $SYSROOT/share/icons/Adwaita/
cat /build/scripts/win32/iconlist.txt | while read iconfile; do
    for filename in `find . -name "${iconfile}.*"`; do
        dest=`dirname $filename`
        mkdir -p /build/$OUT/share/icons/Adwaita/$dest
        cp -v $filename /build/$OUT/share/icons/Adwaita/$filename
    done
done
