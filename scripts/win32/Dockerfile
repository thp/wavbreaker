FROM fedora:latest

RUN mkdir /build

WORKDIR /build

RUN dnf -y install make mingw32-gcc mingw64-gtk3 diffutils findutils automake autoconf meson gettext libtool git wget bzip2 xz

RUN git clone https://github.com/xiph/libao.git && \
    cd libao && \
    git checkout 1.2.2 && \
    ./autogen.sh && \
    env LIBS="-lksuser -lwinmm" ./configure \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-w64-mingw32 \
        --enable-shared --disable-pulse --enable-wmm \
        --prefix=/usr/x86_64-w64-mingw32/sys-root/mingw/ && \
    make install V=1

RUN wget https://www.mpg123.de/download/mpg123-1.29.3.tar.bz2 && \
    tar xvf mpg123-1.29.3.tar.bz2 && \
    cd mpg123-1.29.3 && \
    ./configure \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-w64-mingw32 \
        --enable-shared \
        --prefix=/usr/x86_64-w64-mingw32/sys-root/mingw/ && \
    make install V=1

RUN wget https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.xz && \
    tar xvf libogg-1.3.5.tar.xz && \
    cd libogg-1.3.5 && \
    ./configure \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-w64-mingw32 \
        --enable-shared \
        --prefix=/usr/x86_64-w64-mingw32/sys-root/mingw/ && \
    make install V=1

RUN wget https://downloads.xiph.org/releases/vorbis/libvorbis-1.3.7.tar.xz && \
    tar xvf libvorbis-1.3.7.tar.xz && \
    cd libvorbis-1.3.7 && \
    ./configure \
        --build=x86_64-pc-linux-gnu \
        --host=x86_64-w64-mingw32 \
        --enable-shared \
        --prefix=/usr/x86_64-w64-mingw32/sys-root/mingw/ && \
    make install V=1
