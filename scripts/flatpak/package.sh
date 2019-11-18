#!/bin/sh

set -e -x

VERSION=$(sed -n "s/^  version : '\(.*\)',/\1/p" meson.build)

# This is a workaround for the old flatpak-builder scripts in Ubuntu 18.04
# (in Travis CI) not supporting the "dir" source type, after Travis CI uses
# a recent enough flatpak-builder script, simply use this instead:
# "type": "dir",
# "path": "../.."
tar czvf scripts/flatpak/wavbreaker.tgz \
    --exclude=scripts \
    --exclude=.git \
    --exclude=build \
    --exclude=.flatpak-builder \
    --exclude=flatpak_build \
    --exclude=flatpak_repo \
    .

flatpak-builder \
    --verbose --verbose \
    --force-clean \
    --repo=flatpak_repo \
    flatpak_build \
    scripts/flatpak/net.sourceforge.wavbreaker.json

rm -f scripts/flatpak/wavbreaker.tgz

mkdir -p dist

flatpak build-bundle \
    --verbose \
    flatpak_repo \
    "dist/wavbreaker-$VERSION.flatpak" \
    net.sourceforge.wavbreaker
