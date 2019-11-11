#!/bin/bash
docker run -v "$(pwd):/build" wavbreaker-win32-build \
    sh -e -x win32-build/run-build-inside.sh

rm -rf win32-out/mingw/{lib,sbin,include}
