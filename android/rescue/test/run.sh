#!/bin/sh
# android/rescue/test/run.sh V15.fwsc V16.fwsc wl82loader.bin WEB.uf2 BASE_ZERO.uf2
# WEB.uf2: node rescue/make_uf2.mjs BASE_ZERO.uf2 V15.fwsc wl82loader.bin WEB.uf2
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/fm1-rescue-app-test"
rm -rf "$OUT" && mkdir -p "$OUT"
javac -d "$OUT" "$HERE/../app/src/main/java/fm/jiant/rescue/RescueImage.java" \
      "$HERE/../app/src/main/java/fm/jiant/rescue/Picoboot.java" \
      "$HERE/../app/src/main/java/fm/jiant/rescue/RescueStatus.java" "$HERE/RescueTest.java"
java -cp "$OUT" RescueTest "$@"
