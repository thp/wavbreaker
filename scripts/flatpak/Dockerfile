FROM ubuntu:rolling

RUN mkdir /build

WORKDIR /build

RUN apt-get update

RUN apt-get install -y flatpak-builder

RUN flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

RUN flatpak install -y org.freedesktop.Sdk//19.08
RUN flatpak install -y org.freedesktop.Platform//19.08
