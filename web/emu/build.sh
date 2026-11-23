#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Felucca in the browser: the firmware's sources as the host tests build them (web/emu/felucca_web.c), compiled
# to WebAssembly with Emscripten, plus the page. After X0X's web/emu/build.sh (charlesvestal/fm1-x0x, GPL-3.0).
#   web/emu/build.sh   ->  build/emu/{index.html, devices.html, worklet.js, felucca.wasm, felucca_hp.wasm, fonts/}
# felucca_hp.wasm: the big CPUs' build (web/emu/felucca_hp.c: ports/core with FELUCCA_HOST_POWER, the sample engines,
# the slots, the recorder) for devices.html's MPC Sample panel; its memory grows with the samples loaded (2 GiB at most).
# Run after one ./build.sh (build/gen: the tables, the fonts, the CC0 samples).
# -ffp-contract=off: no fused multiply-adds, as the host build. EMU_OPT replaces -O2, EMU_CFLAGS adds flags
# (EMU_CFLAGS=-msimd128: let the compiler vectorise; every browser with AudioWorklet has wasm SIMD).
# The page shows the version (FELUCCA_VERSION) marked "(preview)" before its release; EMU_PREVIEW=0 drops the mark.
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu: run ./build.sh once first (build/gen)"; exit 1; }
command -v emcc >/dev/null 2>&1 || { echo "emu: no emcc (Emscripten)"; exit 1; }
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
mkdir -p build/emu/fonts
hp() {
    # shellcheck disable=SC2086
    emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
        "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
        --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=67108864 -sALLOW_MEMORY_GROWTH=1 \
        -sMAXIMUM_MEMORY=2147483648 -sFILESYSTEM=0 \
        -o build/emu/felucca_hp.wasm web/emu/felucca_hp.c
}
# EMU_ONLY=hp: felucca_hp.wasm alone (the site's workflow: devices.html and its wasm from this tree beside a pinned
# felucca.wasm, .github/workflows/emulator.yml)
if [ "${EMU_ONLY:-}" = hp ]; then
    hp
    echo "emu: build/emu/felucca_hp.wasm ($(wc -c < build/emu/felucca_hp.wasm | tr -d ' ') B, $VERSION)"
    exit 0
fi
# shellcheck disable=SC2086
emcc ${EMU_OPT:--O2} -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src \
    --no-entry -sSTANDALONE_WASM -sSTACK_SIZE=1048576 -sINITIAL_MEMORY=33554432 -sFILESYSTEM=0 -DFELUCCA_WEB_EDITOR=1 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c
hp
[ "${EMU_PREVIEW:-1}" = 0 ] && PREVIEW='' || PREVIEW=' (preview)'
sed -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/$PREVIEW/g" web/emu/index.html > build/emu/index.html
cp web/emu/devices.html web/emu/worklet.js web/emu/stems.js web/emu/transfer.js web/emu/transfer-ui.js web/fm1backup.js build/emu/
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
echo "emu: build/emu ($(wc -c < build/emu/felucca.wasm | tr -d ' ') B wasm, $(wc -c < build/emu/felucca_hp.wasm | tr -d ' ') B felucca_hp.wasm, $VERSION)"
