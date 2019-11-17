#!/bin/bash
docker run -v "$(pwd):/build" wavbreaker-win32-build \
    sh -e -x scripts/win32/run-build-inside.sh
