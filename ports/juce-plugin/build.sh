#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# JIANT FM as VST3 / AU / AUv3 / LV2 / standalone (ports/juce-plugin/CMakeLists.txt), for this computer:
#   ports/juce-plugin/build.sh            -> build/juce-plugin/JiantFM_artefacts/Release/{VST3,AU,AUv3,LV2,Standalone}
#   JUCE_DIR=... reuses a JUCE checkout (else JUCE 9.0.5 is fetched); extra arguments go to cmake's configure step
#   (iOS: -G Xcode -DCMAKE_SYSTEM_NAME=iOS; .github/workflows/ports.yml shows each platform's line)
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
GEN=""
command -v ninja >/dev/null 2>&1 && GEN="-G Ninja"
# shellcheck disable=SC2086
cmake -S ports/juce-plugin -B build/juce-plugin $GEN -DCMAKE_BUILD_TYPE=Release ${JUCE_DIR:+-DJUCE_DIR="$JUCE_DIR"} "$@"
cmake --build build/juce-plugin --config Release --parallel
echo "juce-plugin: build/juce-plugin/JiantFM_artefacts"
