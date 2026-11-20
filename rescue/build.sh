#!/bin/sh
# Build the FM-1 Rescue: the base firmware for each board, then the page.
#   PICO_SDK_PATH=~/pico-sdk rescue/build.sh
# Needs pico-sdk 2.2.0 (with lib/tinyusb), CMake and an Arm GCC with newlib.
# Optional host test of the rescue logic against a mock FM-1 (needs the
# official V15 and the loader, which are not in this repository):
#   FM1_V15=FM-1.fwsc FM1_LOADER=wl82loader.bin rescue/build.sh
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
: "${PICO_SDK_PATH:?set PICO_SDK_PATH to pico-sdk 2.2.0}"
BUILD="${BUILD:-$HERE/firmware/build}"
mkdir -p "$HERE/base"
for spec in zero:waveshare_rp2040_zero xiao:seeed_xiao_rp2040; do
    board=${spec%%:*}
    pico_board=${spec#*:}
    cmake -S "$HERE/firmware" -B "$BUILD/$board" -DPICO_SDK_PATH="$PICO_SDK_PATH" \
          -DPICO_BOARD="$pico_board" -DFM1T_BOARD="$board" >/dev/null
    make -C "$BUILD/$board" -j8 fm1_transporter >/dev/null
    cp "$BUILD/$board/fm1_transporter.uf2" "$HERE/base/fm1-rescue-base-$board.uf2"
    echo "base/fm1-rescue-base-$board.uf2"
done
python3 "$HERE/make_page.py"

if [ -n "$FM1_V15" ] && [ -n "$FM1_LOADER" ]; then
    T="${TMPDIR:-/tmp}"
    node "$HERE/make_uf2.mjs" "$HERE/base/fm1-rescue-base-zero.uf2" "$FM1_V15" "$FM1_LOADER" \
         "$T/fm1-rescue-test.uf2" "$T/fm1-v15-region.bin"
    "$HERE/firmware/test/run.sh" "$T/fm1-rescue-test.uf2" "$T/fm1-v15-region.bin"
fi
