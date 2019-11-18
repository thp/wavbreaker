#!/bin/sh

set -e -x

VERSION=$(sed -n "s/^  version : '\(.*\)',/\1/p" meson.build)

docker run --privileged -v "$(pwd):/build" wavbreaker-flatpak-build \
    flatpak-builder \
    --verbose --verbose \
    --disable-rofiles-fuse \
    --force-clean \
    --repo=flatpak_repo \
    flatpak_build \
    scripts/flatpak/net.sourceforge.wavbreaker.json

mkdir -p dist

docker run --privileged -v "$(pwd):/build" wavbreaker-flatpak-build \
    flatpak build-bundle \
    --verbose \
    flatpak_repo \
    "dist/wavbreaker-$VERSION.flatpak" \
    net.sourceforge.wavbreaker
