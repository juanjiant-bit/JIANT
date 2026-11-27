#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# JIANT FM for the Akai MPC Sample.
#   ports/mpc-sample/build.sh            build here (Linux: the MPC's own build is ARM64, Debian bookworm, below)
#   ports/mpc-sample/build.sh docker     the MPC's build in sample-base's image (linux/arm64; Docker, or Podman)
#   ports/mpc-sample/build.sh test       build here, then the self-test and a screen preview (build/test.wav,
#                                        build/preview.png)
# sample-base (keremimo/sample-base, AGPL-3.0) is fetched at the commit below into build/sample-base, JUCE by its
# tools/dependencies.py into build/deps. On Linux the build needs g++, make, pkg-config and the development files of
# freetype2, fontconfig, alsa, libdrm and libsystemd (as sample-base's tools/Dockerfile lists them).
set -eu
cd "$(dirname "$0")"
BASE_URL=https://github.com/keremimo/sample-base.git
BASE_COMMIT=f18e620104fe579727c5f13f796016ba08e3a394     # 2026-11-18
if [ ! -f build/sample-base/base.mk ]; then
    mkdir -p build/sample-base
    git -C build/sample-base init -q
    git -C build/sample-base remote add origin "$BASE_URL"
fi
if [ "$(git -C build/sample-base rev-parse HEAD 2>/dev/null || true)" != "$BASE_COMMIT" ]; then
    git -C build/sample-base fetch -q --depth 1 origin "$BASE_COMMIT"
    git -C build/sample-base -c advice.detachedHead=false checkout -q "$BASE_COMMIT"
fi
python3 -c "import sys; sys.path.insert(0, 'build/sample-base/tools'); import dependencies; dependencies.prepare('.')"
[ -f ../../build/gen/felucca_tables.h ] || (cd ../.. && python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()")
case "${1:-}" in
docker)
    D=$(command -v docker || command -v podman)
    "$D" build --platform linux/arm64 -t sample-base-builder build/sample-base/tools
    # the whole repository in the container: the core is built from firmware/src
    "$D" run --rm --platform linux/arm64 -v "$PWD/../..:/work" -w /work/ports/mpc-sample sample-base-builder \
        sh -c 'make -j"$(nproc)" && build/jiant --self-test build/test.wav && build/jiant --preview build/preview.png'
    ;;
*)
    make -j"$(nproc 2>/dev/null || echo 4)"
    if [ "${1:-}" = test ]; then
        ${CXX:-g++} -std=c++17 -Wall -Wextra -Werror -Isrc -o build/panel_map_test tests/panel_map_test.cpp
        build/panel_map_test
        build/jiant --self-test build/test.wav
        build/jiant --preview build/preview.png
        rm -rf build/persist && mkdir -p build/persist
        build/jiant --persist-write build/persist && build/jiant --persist-read build/persist
        rm -rf build/samples && mkdir -p build/samples
        build/jiant --samples-write build/samples && build/jiant --samples-read build/samples
    fi
    ;;
esac
