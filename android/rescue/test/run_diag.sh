#!/bin/sh
# The diagnosis on FM-1 Transporter console logs: android/rescue/test/run_diag.sh LOG...
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/fm1-rescue-diag-test"
rm -rf "$OUT" && mkdir -p "$OUT"
javac -d "$OUT" "$HERE/../app/src/main/java/fm/jiant/rescue/Diagnosis.java" "$HERE/DiagTest.java"
java -cp "$OUT" DiagTest "$@"
