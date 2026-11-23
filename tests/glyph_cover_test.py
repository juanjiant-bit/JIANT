# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
"""(V10.8.13) every character a string of the firmware draws is a glyph of S and M (AF_X is S): none blank (BLANK), none
missing (gfx.c draws '?' for it). Scans the string and character literals of firmware/src and build/gen (comments out),
lowercase as the UI folds it to capitals (gfx.c fold), and the Latin-1 codes written as numbers"""
import glob, re, sys
sys.path.insert(0, "tools")
import gen_aa_font as g

def literals():
    out = []
    gen = [h for h in glob.glob("build/gen/*.h") if not h.endswith("felucca_lyrics.h")]   # (the lyrics: coded bytes)
    for f in glob.glob("firmware/src/*.[ch]") + gen:
        s = open(f, encoding="latin-1").read()
        s = re.sub(r"/\*.*?\*/", "", s, flags=re.S)
        s = re.sub(r"//[^\n]*", "", s)
        for m in re.finditer(r'"((?:[^"\\\n]|\\.)*)"', s):
            t = re.sub(r"\\x([0-9A-Fa-f]{2})", lambda q: chr(int(q.group(1), 16)), m.group(1))
            t = re.sub(r"\\(.)", lambda q: {"n": "\n", "t": "\t", "0": "\0", "\\": "\\", '"': '"'}.get(q.group(1), q.group(1)), t)
            out.append((f, t))
    return out

# not text: '|' a card's label / hint separator (ui.col), '~' the singer's phoneme marks (sing_ext.c), \xFD..\xFF gen.c's
# byte strings (markers)
NOT_TEXT = set("|~\xfd\xfe\xff")
face = {c for c in map(chr, range(32, 96))} | {chr(c) for c in g.EXTRAS} | {"\x85"}
bad = []
for name in ("S", "M"):
    have = face - set(g.BLANK[name])
    for f, t in literals():
        for c in t:
            c = c.upper() if "a" <= c <= "z" else c
            if c < " " or c in have or c in NOT_TEXT:
                continue
            bad.append((name, f, t, c))
seen = set()
for name, f, t, c in bad:
    if (name, c) not in seen:
        seen.add((name, c))
        print("glyph-cover: %s lacks %r, drawn by %s: %r" % (name, c, f, t[:40]))
print("glyph-cover: %s" % ("all covered" if not bad else "%d missing" % len(seen)))
sys.exit(1 if bad else 0)
