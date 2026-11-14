#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""VOICE words (JIANT 0.8.5): the phoneme codes and the lyric generator's word banks as constant tables
(firmware/src/sing.c reads them), from tools/lyrics/:
  es.txt en.txt ja.txt   the generator's banks: words by theme and category, each with its syllables (Spanish by the
                same rules sing.c reads it with, sy_es below; English from en_dict.txt, the CMU Pronouncing Dictionary;
                Japanese in morae). (V10.7.33) English's words packed as their phoneme codes (PH_CODE: sing.c sy_phon reads
                them; a "." where a syllable starts only when sy_phauto would put it elsewhere)
  en_dict.txt   (V10.7.33) no longer on the device (the web editor has the whole CMU dictionary, web/lyrics_en.js): the
                English bank's phonemes, and the host tests' reference words
The phoneme list here is the order of sing.c's PHON[] (a static assert checks the count); the codes and the languages'
lists are web/lyrics.js's too (tools/gen_web_lyrics.py writes them from here).
  tools/gen_lyrics.py OUT.h"""
import re
import sys
from pathlib import Path

LY = Path(__file__).resolve().parent / "lyrics"

# code, name shown (at most 2 letters + 0)
PHONEMES = [
    ("SIL", "_"),
    ("A", "a"), ("E", "e"), ("I", "i"), ("O", "o"), ("U", "u"), ("UJ", "u"),            # ES / JA (UJ: Japanese u)
    ("IH", "ih"), ("EH", "eh"), ("AE", "ae"), ("AA", "aa"), ("AO", "ao"), ("UH", "uh"), ("AH", "ah"),
    ("ER", "er"), ("AX", "ax"),                                                           # EN
    ("Y", "y"), ("W", "w"),
    ("L", "l"), ("RT", "r"), ("RR", "rr"), ("RA", "r"),                                   # tap, trill, EN approximant
    ("M", "m"), ("N", "n"), ("NY", "ny"), ("NG", "ng"), ("NM", "N"),                      # NM: Japanese moraic n
    ("P", "p"), ("T", "t"), ("K", "k"), ("B", "b"), ("D", "d"), ("G", "g"),
    ("F", "f"), ("S", "s"), ("SH", "sh"), ("H", "h"), ("X", "x"), ("TH", "th"), ("V", "v"), ("Z", "z"),
    ("ZH", "zh"), ("DH", "dh"),
    ("CH", "ch"), ("TS", "ts"), ("J", "j"),
]
PH = {c: i for i, (c, _) in enumerate(PHONEMES)}
VOWELS = {PH[c] for c in ("A", "E", "I", "O", "U", "UJ", "IH", "EH", "AE", "AA", "AO", "UH", "AH", "ER", "AX")}
assert len(PHONEMES) < 64

# (V10.7.33) phonemes as text: each phoneme's code (upper case: the device types upper case) and the languages that
# read it (sing.c PH_CODE, PH_CM; ES also Italian, OT the languages read by sing_ext.c's rules). The longer codes only
# where none of the language's letter pairs would be taken for them (ER in Spanish is e + r); U and R are read as the
# language's (Japanese UJ, English RA), so UJ and RA have no languages of their own
PM = {"ES": 1, "EN": 2, "JA": 4, "PT": 8, "OT": 16}
ALL = 31
EN_OT = PM["EN"] | PM["OT"]
CODES = {
    "SIL": ("#", PM["JA"] | PM["OT"]),
    "A": ("A", ALL), "E": ("E", ALL), "I": ("I", ALL), "O": ("O", ALL), "U": ("U", ALL), "UJ": ("U", 0),
    "IH": ("IH", EN_OT), "EH": ("EH", EN_OT | PM["PT"]), "AE": ("AE", EN_OT), "AA": ("AA", EN_OT),
    "AO": ("AO", EN_OT | PM["PT"]), "UH": ("UH", EN_OT), "AH": ("AH", EN_OT), "ER": ("ER", EN_OT), "AX": ("AX", EN_OT),
    "Y": ("Y", ALL), "W": ("W", ALL), "L": ("L", ALL), "RT": ("R", ALL), "RR": ("RR", ALL & ~PM["EN"] & ~PM["JA"]),
    "RA": ("R", 0),
    "M": ("M", ALL), "N": ("N", ALL), "NY": ("NY", PM["ES"] | PM["PT"] | PM["OT"]), "NG": ("NG", EN_OT),
    "NM": ("NN", PM["JA"] | PM["PT"] | PM["OT"]),
    "P": ("P", ALL), "T": ("T", ALL), "K": ("K", ALL), "B": ("B", ALL), "D": ("D", ALL), "G": ("G", ALL),
    "F": ("F", ALL), "S": ("S", ALL), "SH": ("SH", ALL), "H": ("H", ALL), "X": ("X", ALL), "TH": ("TH", ALL),
    "V": ("V", ALL), "Z": ("Z", ALL), "ZH": ("ZH", ALL), "DH": ("DH", EN_OT),
    "CH": ("CH", ALL), "TS": ("TS", ALL & ~PM["EN"]), "J": ("J", ALL),
}
assert set(CODES) == {c for c, _ in PHONEMES}
# the NAME screen's lists (sing.c ph_pick: the white keys, 16 a page): each language's codes in PHONEMES' order (vowels
# first), at most 48 (three pages)
PH_LISTS = {k: " ".join(c for c, _ in PHONEMES if CODES[c][1] & m) for k, m in PM.items()}
LANG_OF = {"es": "ES", "en": "EN", "ja": "JA", "pt": "PT"}


def ph_cl(a, b):
    """sing.c ph_cl: consonants a b an onset"""
    A, B = PHONEMES[a][0], PHONEMES[b][0]
    if A == "SIL":
        return True
    if a in VOWELS or A in ("Y", "W"):
        return False
    if B in ("Y", "W"):
        return True
    return B in ("L", "RT", "RA") and A in ("P", "T", "K", "B", "D", "G", "F") and not (A == "D" and B == "L")


def ph_auto(lang, ph):
    """sing.c sy_phauto: the syllables' starts of a phoneme list without "."s"""
    starts, e, first = set(), 0, True
    for k, c in enumerate(ph):
        if not (c in VOWELS or (lang == "JA" and c == PH["NM"])):
            continue
        if lang != "JA" and k and ph[k - 1] in VOWELS and c in (PH["IH"], PH["UH"], PH["I"], PH["U"]):
            e = k + 1
            continue
        s = k - e
        if first:
            s = 0
        elif s <= 1:
            s = k - s
        elif ph_cl(ph[k - 2], ph[k - 1]):
            s = k - 3 if s >= 3 and ph[k - 3] == PH["SIL"] else k - 2
        else:
            s = k - 1
        starts.add(s)
        e = k + 1
        first = False
    if ph:
        starts.add(0)
    return starts


def ph_parse(lang, text):
    """sing.c sy_phon: a phoneme word (upper or lower case, no "/") -> [(code, syllable start)]"""
    mask, w, out, i, st = PM[lang], text.upper(), [], 0, True
    dots = "." in w
    while i < len(w):
        if w[i] == ".":
            st, i = True, i + 1
            continue
        best, bl = None, 0
        for c, (code, m) in ((PH[n], CODES[n]) for n, _ in PHONEMES):
            if m & mask and len(code) > bl and w.startswith(code, i):
                best, bl = c, len(code)
        if best is None:
            i += 1
            continue
        if best == PH["U"] and lang == "JA":
            best = PH["UJ"]
        if best == PH["RT"] and lang == "EN":
            best = PH["RA"]
        out.append([best, dots and st])
        st, i = False, i + bl
    if not dots:
        sa = ph_auto(lang, [c for c, _ in out])
        for k, o in enumerate(out):
            o[1] = k in sa
    if out:
        out[0][1] = True
    return [(c, s) for c, s in out]


def ph_text(lang, ph):
    """[(code, syllable start)] -> its shortest code text: no "."s when sy_phauto finds the same syllables, else one
    where each starts (one syllable: a "." at its end; None: no text reads back as it)"""
    plain = "".join(CODES[PHONEMES[c][0]][0] for c, _ in ph)
    if ph_parse(lang, plain) == [(c, bool(s)) for c, s in ph]:
        return plain
    dotted = "".join(("." if s and k else "") + CODES[PHONEMES[c][0]][0] for k, (c, s) in enumerate(ph))
    if "." not in dotted:                            # (one syllable that sy_phauto would split: a "." at the end)
        dotted += "."
    if ph_parse(lang, dotted) == [(c, bool(s) or k == 0) for k, (c, s) in enumerate(ph)]:
        return dotted
    return None


# ------------------------------------------------------------------------------------------ English (CMU) ---
ARPA = {"AA": ["AA"], "AE": ["AE"], "AO": ["AO"], "EH": ["EH"], "ER": ["ER"], "IH": ["IH"], "IY": ["I"], "UH": ["UH"],
        "UW": ["U"], "AW": ["AA", "UH"], "AY": ["AA", "IH"], "EY": ["E", "IH"], "OW": ["O", "UH"], "OY": ["AO", "IH"],
        "AH": ["AH"], "B": ["B"], "CH": ["CH"], "D": ["D"], "DH": ["DH"], "F": ["F"], "G": ["G"], "HH": ["H"], "JH": ["J"], "K": ["K"],
        "L": ["L"], "M": ["M"], "N": ["N"], "NG": ["NG"], "P": ["P"], "R": ["RA"], "S": ["S"], "SH": ["SH"], "T": ["T"],
        "TH": ["TH"], "V": ["V"], "W": ["W"], "Y": ["Y"], "Z": ["Z"], "ZH": ["ZH"]}
EN_ONSET2 = {("P", "L"), ("B", "L"), ("K", "L"), ("G", "L"), ("F", "L"), ("S", "L"), ("P", "RA"), ("B", "RA"),
             ("T", "RA"), ("D", "RA"), ("K", "RA"), ("G", "RA"), ("F", "RA"), ("TH", "RA"), ("SH", "RA"), ("S", "P"),
             ("S", "T"), ("S", "K"), ("S", "M"), ("S", "N"), ("S", "W"), ("T", "W"), ("K", "W"), ("D", "W"), ("G", "W"),
             ("P", "Y"), ("B", "Y"), ("K", "Y"), ("G", "Y"), ("F", "Y"), ("H", "Y"), ("M", "Y"), ("V", "Y"), ("S", "F")}
EN_ONSET3 = {("S", "P", "L"), ("S", "P", "RA"), ("S", "T", "RA"), ("S", "K", "RA"), ("S", "K", "W"), ("S", "K", "Y"),
             ("S", "P", "Y")}


def en_phones(arpa):
    """CMU phones -> [(code, syllable start)]: a syllable a vowel, consonants between two by the longest legal onset
    (maximal onset); a diphthong's second part stays in its syllable (an offglide)"""
    seq = []                                         # (code name, 1 = a syllable's vowel)
    for a in arpa:
        base = re.sub(r"\d", "", a)
        parts = ARPA[base]
        if base == "AH":
            parts = ["AX"] if a.endswith("0") else ["AH"]
        for j, c in enumerate(parts):
            seq.append((c, PH[c] in VOWELS and j == 0))
    nuc = [i for i, (c, v) in enumerate(seq) if v]
    starts = {0}
    for a, b in zip(nuc, nuc[1:]):
        cons = [i for i in range(a + 1, b) if PH[seq[i][0]] not in VOWELS]
        if not cons:
            starts.add(b)
            continue
        names = tuple(seq[i][0] for i in cons)
        k = len(names)
        if k >= 3 and names[-3:] in EN_ONSET3:
            starts.add(cons[-3])
        elif k >= 2 and names[-2:] in EN_ONSET2:
            starts.add(cons[-2])
        else:
            starts.add(cons[-1])
    if not nuc:
        return [], 0
    return [(PH[c], i in starts) for i, (c, _) in enumerate(seq)], len(nuc)


def load_en_dict():
    d = {}
    for ln in (LY / "en_dict.txt").read_text().splitlines():
        if not ln or ln.startswith("#"):
            continue
        w, *ph = ln.split()
        d[w] = ph
    return d


# ------------------------------------------------------------------------------------------------ Spanish ---
def es_tokens(w):
    """letters -> [(code, kind)], kind S strong vowel, W weak vowel, C consonant (sing.c sy_es_tok)"""
    out, i, n = [], 0, len(w)
    isv = lambda c: c != "" and c in "aeiou"
    while i < n:
        c, nx, nx2 = w[i], w[i + 1] if i + 1 < n else "", w[i + 2] if i + 2 < n else ""
        if c in "aeo":
            out.append((PH["AEO"[0] if c == "a" else "E" if c == "e" else "O"], "S"))
        elif c in "iu":
            out.append((PH["I" if c == "i" else "U"], "W"))
        elif c == "y":
            out.append((PH["Y"], "C") if isv(nx) else (PH["I"], "W"))
        elif c == "h":
            pass
        elif c == "c":
            if nx == "h":
                out.append((PH["CH"], "C"))
                i += 1
            else:
                out.append((PH["S" if nx in ("e", "i") else "K"], "C"))
        elif c == "q":
            out.append((PH["K"], "C"))
            if nx == "u":
                i += 1
        elif c == "g":
            if nx in ("e", "i"):
                out.append((PH["X"], "C"))
            else:
                out.append((PH["G"], "C"))
                if nx == "u" and nx2 in ("e", "i"):
                    i += 1
        elif c == "j":
            out.append((PH["X"], "C"))
        elif c == "x":
            out += [(PH["K"], "C"), (PH["S"], "C")]
        elif c in "zs":
            out.append((PH["S"], "C"))
        elif c in "vb":
            out.append((PH["B"], "C"))
        elif c in "dfkptmw":
            out.append((PH[c.upper()], "C"))
        elif c == "l":
            if nx == "l":
                out.append((PH["Y"], "C"))
                i += 1
            else:
                out.append((PH["L"], "C"))
        elif c == "n":
            if nx == "y" and isv(nx2):
                out.append((PH["NY"], "C"))
                i += 1
            else:
                out.append((PH["N"], "C"))
        elif c == "r":
            if nx == "r":
                out.append((PH["RR"], "C"))
                i += 1
            elif not out or out[-1][0] in (PH["N"], PH["L"], PH["S"]):
                out.append((PH["RR"], "C"))
            else:
                out.append((PH["RT"], "C"))
        i += 1
    return out


ES_CL1 = {PH[c] for c in ("P", "B", "F", "K", "G", "T", "D")}
ES_CL2 = {PH["L"], PH["RT"]}


def es_cluster(a, b):
    return a in ES_CL1 and b in ES_CL2 and not (a == PH["D"] and b == PH["L"])


def syllabify(tok):
    """[(code, kind)] -> [(code, syllable start)], syllables (sing.c sy_syl): vowel groups split between two strong
    vowels; a group's nucleus its first strong vowel (or its last); weak vowels before it glides, after it offglides;
    the consonants between two groups: one to the next, two to the next when a cluster (pl br tr ..), else split"""
    groups, i = [], 0
    while i < len(tok):
        if tok[i][1] == "C":
            i += 1
            continue
        j = i + 1
        while j < len(tok) and tok[j][1] != "C" and not (tok[j][1] == "S" and tok[j - 1][1] == "S"):
            j += 1
        groups.append((i, j))
        i = j
    out = [list(t) for t in tok]
    starts = {0}
    for g, (a, b) in enumerate(groups):
        nu = next((k for k in range(a, b) if tok[k][1] == "S"), b - 1)
        for k in range(a, nu):
            out[k][0] = PH["Y"] if tok[k][0] == PH["I"] else PH["W"]
        if g:
            pa = groups[g - 1][1]
            cons = list(range(pa, a))
            if not cons:
                starts.add(a)
            elif len(cons) == 1:
                starts.add(cons[0])
            elif len(cons) == 2:
                starts.add(cons[0] if es_cluster(tok[cons[0]][0], tok[cons[1]][0]) else cons[1])
            else:
                starts.add(cons[-2] if es_cluster(tok[cons[-2]][0], tok[cons[-1]][0]) else cons[-1])
    return [(c, k in starts) for k, (c, _) in enumerate(out)], len(groups)


def es_nsyl(w):
    return syllabify(es_tokens(w))[1]


# ----------------------------------------------------------------------------------------------- Japanese ---
def ja_nmora(w):
    """morae of romaji (sing.c sy_ja): a vowel, a consonant (+ y) + vowel, n before a consonant / at the end, - (the
    vowel again); a doubled consonant is no mora of its own (a longer closure on the next)"""
    n, i, m = len(w), 0, 0
    v = lambda c: c != "" and c in "aiueo"
    while i < n:
        c = w[i]
        nx = w[i + 1] if i + 1 < n else ""
        if v(c) or c == "-":
            m += 1
            i += 1
        elif c == "n" and not v(nx) and nx != "y":
            m += 1
            i += 2 if nx in ("n", "'") and not v(w[i + 2] if i + 2 < n else "") else 1
        elif c == nx and c != "n" or (c == "t" and nx == "c"):
            i += 1
        else:
            j = i
            while j < n and not v(w[j]) and w[j] != "-":
                j += 1
            m += 1
            i = j + 1 if j < n and v(w[j]) else j
    return m


# ------------------------------------------------------------------------------------------------- banks ---
THEMES = ["LOVE", "NIGHT", "SEA", "SKY", "ANY", "ROAD", "DREAM", "FIRE", "FN"]   # (ANY: none of its own)
CATS = ["N", "V", "A", "D", "P", "C"]


def load_bank(lang, nsyl_of):
    rows = {}
    for ln in (LY / f"{lang}.txt").read_text().splitlines():
        if not ln.strip() or ln.startswith("#"):
            continue
        head, words = ln.split(":", 1)
        theme, cat = head.split()
        for w in words.split():
            fem = w.endswith("/f")
            w = w.split("/")[0]
            ns = nsyl_of(w)
            if not 1 <= ns <= 15:
                raise SystemExit(f"gen_lyrics: {lang} {w}: {ns} syllables")
            rows.setdefault((THEMES.index(theme), CATS.index(cat)), []).append((w, ns, fem))
    return rows


def c_bytes(data, per=24):
    s = ",".join(str(b) for b in data)
    out, line = [], ""
    for tok in s.split(","):
        if len(line) + len(tok) + 1 > 116:
            out.append(line)
            line = ""
        line += tok + ","
    out.append(line)
    return "\n    ".join(out)


def c_strs(items):
    out, line = [], ""
    for tok in items:
        if line and len(line) + len(tok) + 2 > 116:
            out.append(line)
            line = ""
        line += tok + ", "
    out.append(line)
    return "\n    ".join(out)


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path("felucca_lyrics.h")
    en = load_en_dict()
    L = ["/* generated by tools/gen_lyrics.py from tools/lyrics/: do not edit */"]
    L.append("enum { " + ", ".join(f"PH_{c}" for c, _ in PHONEMES) + ", PH_COUNT };")
    L.append("static const char PH_NAME[PH_COUNT][3] = {" + ", ".join(f'"{n}"' for _, n in PHONEMES) + "};")
    # (V10.7.33) the phonemes as text: their codes, the languages reading each, the NAME screen's lists
    L.append("enum { " + ", ".join(f"PM_{k} = {v}" for k, v in PM.items()) + " };")
    L.append("static const char PH_CODE[PH_COUNT][2] = {" +
             ", ".join("{" + ", ".join(f"'{ch.lower()}'" for ch in CODES[c][0].ljust(2, chr(0))).replace(chr(39) + chr(0) + chr(39), "0") + "}"
                       for c, _ in PHONEMES) + "};")
    L.append("static const uint8_t PH_CM[PH_COUNT] = {" + ", ".join(str(CODES[c][1]) for c, _ in PHONEMES) + "};")
    lists = {k: [PH[c] for c in v.split()] for k, v in PH_LISTS.items()}
    for k, v in lists.items():
        assert len(v) <= 48, k
    # the English words (the bank's syllables and phonemes; the host tests' reference)
    nsyl_en, code_en = {}, {}
    for w in sorted(en):
        ph, ns = en_phones(en[w])
        if not ns or not re.fullmatch(r"[a-z]{1,23}", w):
            raise SystemExit(f"gen_lyrics: {w}: no vowel, or not a-z")
        if len(ph) > 32:                             # (sing.c SW_MAX)
            raise SystemExit(f"gen_lyrics: {w}: over 32 phonemes")
        nsyl_en[w] = ns
        code_en[w] = ph_text("EN", ph)
        if code_en[w] is None:
            raise SystemExit(f"gen_lyrics: {w}: no code text reads back as its phonemes")
    words = sorted(en)
    L.append(f"#define LY_EN_N {len(words)}u")
    L.append("#ifdef LY_EN_REF")                     # (tests/voice_test.c: each word's code text read back exactly)
    L.append("static const char *const LY_EN_REF_W[LY_EN_N] = {\n    " + c_strs(f'"{w}"' for w in words) + "};")
    L.append("static const char *const LY_EN_REF_CODE[LY_EN_N] = {\n    " + c_strs(f'"{code_en[w]}"' for w in words) + "};")
    L.append("static const char *const LY_EN_REF_PH[LY_EN_N] = {\n    " +
             c_strs('"' + "".join(f"\\x{0x80 | (0x40 if st else 0) | c:02x}" for c, st in en_phones(en[w])[0]) + '"'
                    for w in words) + "};")
    L.append("#endif")
    # the banks
    nsyl = {"es": es_nsyl, "en": lambda w: nsyl_en[w] if w in nsyl_en else 0, "ja": ja_nmora}
    counts = {}
    for lang in ("es", "en", "ja"):
        rows = load_bank(lang, nsyl[lang])
        bank_words = []
        txt, offs, meta, grp = bytearray(), [], [], [[[0, 0] for _ in CATS] for _ in THEMES]
        n = 0
        for t in range(len(THEMES)):
            for c in range(len(CATS)):
                r = rows.get((t, c), [])
                grp[t][c] = [n, len(r)]
                for w, ns, fem in r:
                    assert len(w) <= 19, "singer word buffer"
                    bank_words.append(w)
                    offs.append(len(txt))
                    if lang == "en":                 # (V10.7.33) its phoneme codes ("." 27), "/" put first by sing.c
                        w = code_en[w].lower()
                        assert len(w) <= 18, "singer word buffer, the / first"
                    txt += w.encode() + b"\0"
                    assert 0 <= ns < 8
                    meta.append(ns | (8 if fem else 0))
                    n += 1
        counts[lang] = n
        U = lang.upper()
        L.append(f"#ifdef LY_EN_REF\nstatic const char *const LY_{U}_BANK_REF_W[] = {{" +
                 ','.join('"'+w+'"' for w in bank_words) + "};\n#endif")
        L.append(f"#define LY_{U}_NW {n}u")
        # Exact alphabet packing: 0 is word terminator, 1..26 a..z.
        # Sparse offsets remain every 16 words, now measured in bits.
        assert all(b == 0 or 97 <= b <= 122 or b == 46 for b in txt)
        bits = ''.join(f'{0 if b == 0 else 27 if b == 46 else b-96:05b}' for b in txt)
        bits += '0' * (-len(bits) % 8)
        packed = bytes(int(bits[i:i+8],2) for i in range(0,len(bits),8)) + b'\0'
        L.append(f"static const uint8_t LY_{U}_TXT[{len(packed)}] = {{\n    {c_bytes(packed)}}};")
        offs = offs[::16]                            # (0.9.0) each 16th word's place (sing.c ly_word: then the 0s)
        L.append(f"static const uint16_t LY_{U}_OFF[{len(offs)}] = {{\n    {c_bytes(o*5 for o in offs)}}};")
        packed_meta = bytes(meta[i] | ((meta[i+1] if i+1<n else 0)<<4) for i in range(0,n,2))
        L.append(f"static const uint8_t LY_{U}_META[{len(packed_meta)}] = {{\n    {c_bytes(packed_meta)}}};")
        assert all(a < 512 and b < 128 for t in grp for a,b in t)
        rows_c = ",\n    ".join("{" + ", ".join(str(a | b<<9) for a, b in t) + "}" for t in grp)
        L.append(f"static const uint16_t LY_{U}_GRP[{len(THEMES)}][{len(CATS)}] = {{\n    {rows_c}}};")
    out.write_text("\n".join(L) + "\n")
    print(f"lyrics: {len(PHONEMES)} phonemes, codes and lists ({sum(len(v) for v in lists.values())} entries); "
          f"no English dictionary on the device; banks ES {counts['es']} EN {counts['en']} (phoneme codes) "
          f"JA {counts['ja']} words -> {out}")


if __name__ == "__main__":
    main()

