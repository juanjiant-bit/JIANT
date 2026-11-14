#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(V10.7.33) The web editor's English dictionary: the whole CMU Pronouncing Dictionary as VOICE's phoneme codes
(web/lyrics_en.js; web/lyrics.js reads it). The device has no English dictionary since V10.7.33: the editor turns the
words a user types into the codes the device sings (firmware/src/sing.c sy_phon), each word's first pronunciation,
syllabified as tools/gen_lyrics.py does it (en_phones: the longest legal onset), its code text the shortest that reads
back exactly (gen_lyrics.ph_text: "." only where the device would put a syllable elsewhere).

  tools/gen_web_lyrics.py CMUDICT [OUT.js]

CMUDICT: cmudict.dict (https://github.com/cmusphinx/cmudict, BSD-2-Clause: LICENSES/BSD-2-Clause-CMUdict.txt; the
"cmudict" Python package carries it as cmudict/data/cmudict.dict). Words of a .. z and an apostrophe (dropped: the
device drops it too), at most 23 letters and 32 phonemes. The output: one string, a word a line, sorted, front-coded
(a character for the letters it shares with the word before: "0" + n), then its letters, a space, its code text."""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import gen_lyrics  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]


def main():
    src = Path(sys.argv[1])
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else ROOT / "web" / "lyrics_en.js"
    words, skipped = {}, 0
    for ln in src.read_text(encoding="utf-8", errors="replace").splitlines():
        ln = ln.split("#", 1)[0].strip()
        if not ln:
            continue
        w, *ph = ln.split()
        if "(" in w:                                  # (an alternate pronunciation: the first kept)
            continue
        key = w.replace("'", "")
        if not re.fullmatch(r"[a-z]{1,23}", key) or key in words:
            continue
        try:
            seq, ns = gen_lyrics.en_phones(ph)
        except KeyError:
            skipped += 1
            continue
        if not ns or len(seq) > 32:
            skipped += 1
            continue
        code = gen_lyrics.ph_text("EN", seq)
        if code is None:
            skipped += 1
            continue
        words[key] = code
    lines, prev = [], ""
    for w in sorted(words):
        n = 0
        while n < min(len(w), len(prev), 35) and w[n] == prev[n]:
            n += 1
        lines.append("0123456789abcdefghijklmnopqrstuvwxyz"[n] + w[n:] + " " + words[w])
        prev = w
    body = "\\n".join(lines)
    lic = "".join("// " + ln + "\n" if ln else "//\n" for ln in (ROOT / "LICENSES" / "BSD-2-Clause-CMUdict.txt").read_text().splitlines())
    out.write_text(
        "// SPDX-License-Identifier: BSD-2-Clause\n"
        "// The CMU Pronouncing Dictionary (cmudict, https://github.com/cmusphinx/cmudict), under its licence:\n//\n" + lic +
        "//\n// Converted to VOICE's phoneme codes by tools/gen_web_lyrics.py (JIANT V10.7.33): generated, do not edit.\n"
        f"// {len(words)} words, a line each: the letters shared with the word before (0-9a-z), the rest, a space, the codes.\n"
        f'var LY_EN_DICT = "{body}";\n', encoding="utf-8")
    print(f"web lyrics: {len(words)} English words ({skipped} skipped) -> {out} ({out.stat().st_size} B)")


if __name__ == "__main__":
    main()
