#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# Every version the public installer offers (web/STABLE, then web/CLASSIC, less web/WITHDRAWN), as an emulator of its
# own for the beta emulator's version picker (web/emu/picker.js): OUT/<sha7>/ holds that commit's own page, worklet,
# fonts and felucca.wasm (its own web/emu/build.sh, its flash under the key felucca-emu-flash-<sha7>), OUT/index.json
# lists them ({version, sha, sha7, public, ok, wasm_sha256}). CACHE keeps each commit's build: a commit already there
# is not built again. A commit that does not build is listed with ok false, not a failure.
#   web/emu/build_versions.sh OUT CACHE        (emcc on the PATH; run from the repository's root, fetch-depth 0)
set -u
OUT=$1; CACHE=$2
mkdir -p "$OUT" "$CACHE"
ROOT=$(pwd)
W=$(sed 's/#.*//' web/WITHDRAWN 2>/dev/null | awk 'NF{print $1}' | tr '\n' ' ')
{ tr -d ' \n' < web/STABLE; echo " PUBLIC"; sed 's/#.*//' web/CLASSIC | awk 'NF{print $1}'; } > /tmp/emu-v-list
: > /tmp/emu-v-rows
SEEN=" "
while read -r C TAG; do
  [ -n "$C" ] || continue
  case "$SEEN" in *" $C "*) continue;; esac; SEEN="$SEEN$C "
  S7=${C::7}; PUB=false; [ "${TAG:-}" = PUBLIC ] && PUB=true
  if ! git cat-file -e "$C^{commit}" 2>/dev/null; then echo "emu-v: $S7 not a commit here, skipped"; continue; fi
  V=$(git show "$C:firmware/src/felucca.c" 2>/dev/null | sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' | head -1)
  case " $W " in *" ${V#v} "*) echo "emu-v: $V withdrawn, skipped"; continue;; esac
  D="$CACHE/$C"
  if [ ! -s "$D/felucca.wasm" ]; then
    rm -rf /tmp/emu-v-src "$D"; git worktree prune
    if git worktree add -f --detach /tmp/emu-v-src "$C" >/dev/null 2>&1 \
       && (cd /tmp/emu-v-src && python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()" \
           && EMU_PREVIEW=0 sh web/emu/build.sh) > "/tmp/emu-v-$S7.log" 2>&1; then
      mkdir -p "$D"; cp -r /tmp/emu-v-src/build/emu/. "$D/"
      rm -f "$D/felucca_hp.wasm" "$D/devices.html"
      echo "emu-v: $V ($S7) built"
    else
      echo "emu-v: $V ($S7) did not build"; tail -5 "/tmp/emu-v-$S7.log"
    fi
    (cd "$ROOT" && git worktree remove -f /tmp/emu-v-src >/dev/null 2>&1 || true)
  else
    echo "emu-v: $V ($S7) from the cache"
  fi
  OK=false; H=""
  if [ -s "$D/felucca.wasm" ] && [ -s "$D/index.html" ]; then
    rm -rf "${OUT:?}/$S7"; cp -r "$D" "$OUT/$S7"
    python3 - "$OUT/$S7/index.html" "$S7" <<'PY'
import re, sys
p, s7 = sys.argv[1], sys.argv[2]
h = open(p, encoding="utf-8").read()
h = h.replace('"felucca-emu-flash"', '"felucca-emu-flash-' + s7 + '"')          # its own flash in the browser
h = h.replace("jiant-emu-additive-flash-v1", "jiant-emu-additive-flash-v1-" + s7)
h = re.sub(r'href="\.\./', 'href="../../', h)                                   # one level deeper than emu/
h = h.replace(" (preview)</small>", "</small>")
h = h.replace("</body>", '<script src="../picker.js"></script>\n</body>', 1)
open(p, "w", encoding="utf-8").write(h)
PY
    OK=true; H=$(sha256sum "$OUT/$S7/felucca.wasm" | cut -d' ' -f1)
  fi
  printf '%s\t%s\t%s\t%s\t%s\n' "$V" "$C" "$PUB" "$OK" "$H" >> /tmp/emu-v-rows
done < /tmp/emu-v-list
python3 - "$OUT/index.json" <<'PY'
import json, sys
rows = [l.rstrip("\n").split("\t") for l in open("/tmp/emu-v-rows") if l.strip()]
json.dump([{"version": v, "sha": c, "sha7": c[:7], "public": p == "true", "ok": o == "true", "wasm_sha256": h}
           for v, c, p, o, h in rows], open(sys.argv[1], "w"), indent=1)
print("emu-v:", ", ".join(v + ("" if o == "true" else " (no emulator)") for v, c, p, o, h in rows))
PY
cp web/emu/picker.js "$OUT/picker.js"
