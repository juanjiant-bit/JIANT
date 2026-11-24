#!/bin/sh
# Host test of the stand-alone rescue logic: test/run.sh RESCUE.uf2 V15_REGION.bin [-v]
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="${TMPDIR:-/tmp}/fm1-rescue-host-test"
cc -std=c11 -O1 -Wall -Wextra -Werror -I"$HERE/stub" -I"$HERE/../src" \
   "$HERE/host_test.c" "$HERE/../src/payload.c" "$HERE/../src/autorescue.c" -o "$OUT"
"$OUT" "$@"
