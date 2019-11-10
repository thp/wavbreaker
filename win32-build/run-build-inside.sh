#!/bin/sh

env ac_cv_func_malloc_0_nonnull=yes \
 ./autogen.sh \
    --build=x86_64-pc-linux-gnu \
    --host=x86_64-w64-mingw32 \
    --disable-moodbar \
    --enable-mpg123 \
    --prefix=/usr/x86_64-w64-mingw32/sys-root/mingw/

make install V=1

mkdir -p win32-out/
cp -rpv /usr/x86_64-w64-mingw32/sys-root/mingw/ win32-out/
