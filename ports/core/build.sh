#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# JIANT's core as a static library for the ports (ports/core/jiant_core.h), and its test.
#   ports/core/build.sh          ->  build/core/libjiant_core.a   (this machine)
#   ports/core/build.sh test     ->  the library, then build/core/core_test run (and the copies' test)
#   ports/core/build.sh symbols  ->  ports/core/copy_symbols.h made again from the core's object (jiant_copies.h)
# CC overrides the compiler (a cross compiler for another CPU), CORE_OUT the folder.
# The generated headers (build/gen) are made first when missing: Python 3 with Pillow, numpy and fontTools.
# -ffp-contract=off: no fused multiply-adds, as the host tests and the emulator: the same sound everywhere.
set -e
cd "$(dirname "$0")/../.."
CC="${CC:-cc}"
AR="${AR:-ar}"
OUT="${CORE_OUT:-build/core}"
[ -f build/gen/felucca_tables.h ] || python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
mkdir -p "$OUT"
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
# shellcheck disable=SC2086
$CC ${CORE_OPT:--O2} -ffp-contract=off -std=gnu99 -w -fPIC -fvisibility=hidden ${CORE_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" \
    -Ibuild/gen -Ifirmware/src -c ports/core/jiant_core.c -o "$OUT/jiant_core.o"
rm -f "$OUT/libjiant_core.a"
$AR rcs "$OUT/libjiant_core.a" "$OUT/jiant_core.o"
echo "core: $OUT/libjiant_core.a"
NM="${NM:-nm}"
if [ "${1:-}" = symbols ]; then
    python3 ports/core/copy_symbols.py "$NM" "$OUT/jiant_core.o"
fi
if [ "${1:-}" = test ]; then
    $CC -O2 -Wall -Iports/core ports/core/core_test.c "$OUT/libjiant_core.a" -lm -o "$OUT/core_test"
    "$OUT/core_test"
    # the sample engines (SAMPLE, GRAIN, DRUM-X's samples) with the firmware's internals at hand
    $CC -O2 -ffp-contract=off -std=gnu99 -w "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src -Iports/core \
        ports/core/sample_test.c -lm -o "$OUT/sample_test"
    "$OUT/sample_test" "$OUT/hp_state.bin"
    # .. and the FM-1's side: that project loaded by the core built as the FM-1 is (no host power)
    $CC -O2 -ffp-contract=off -std=gnu99 -w "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src -Iports/core \
        ports/core/fm1_load_test.c -lm -o "$OUT/fm1_load_test"
    "$OUT/fm1_load_test" "$OUT/hp_state.bin"
    # several JIANTs in one process (jiant_copies.h): the copies' symbols up to date, 8 copies beside the single library
    python3 ports/core/copy_symbols.py "$NM" "$OUT/jiant_core.o" --check
    COPIES=""
    for k in 0 1 2 3 4 5 6 7; do
        # shellcheck disable=SC2086
        $CC ${CORE_OPT:--O2} -ffp-contract=off -std=gnu99 -w -fvisibility=hidden "-DJIANT_COPY=$k" \
            "-DFELUCCA_VERSION=\"$VERSION\"" -Ibuild/gen -Ifirmware/src -Iports/core -c ports/core/jiant_copy.c \
            -o "$OUT/jiant_copy_$k.o" &
        COPIES="$COPIES $OUT/jiant_copy_$k.o"
    done
    wait
    # shellcheck disable=SC2086
    $CC -O2 -Wall -Iports/core ports/core/copies_test.c ports/core/jiant_copies.c $COPIES "$OUT/libjiant_core.a" \
        -lm -lpthread -o "$OUT/copies_test"
    "$OUT/copies_test"
fi
