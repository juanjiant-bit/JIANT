#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""JIANT FM's line art: SVG paths -> ui_shapes.h (run by tools/build.py generate()).

  gen_ui_shapes.py OUT.h [SVG_DIR]          (default: assets/ui-shapes)

Every SVG in SVG_DIR is a screen; every <path> with an id in it is a shape. Conventions (docs/TONIC-UI.md):
  - the canvas is viewBox="-64 -64 128 128": one unit is one stored step (int8), the origin the shape's anchor on
    screen (the firmware places, scales and mirrors it; y grows downwards as in SVG)
  - a shape that morphs is two paths, NAME_a and NAME_b, with the same number of segments (the firmware moves every
    point from A to B); a path without _a / _b does not morph
  - data-mirror="1" on a path (or its _a): the firmware also draws it mirrored left-right (one half of a symmetric
    organ, or the right one of a pair)
  - one subpath per path: M, then C S Q T L H V (absolute or relative), Z. Lines and quadratics become cubics.
    A closed path (Z) ends where it started
Out: per shape, static const int8_t SH_<SCREEN>_<NAME>_PTS[2][n] (A then B: a start point, three points a cubic
segment) and static const og_shape_t SH_<SCREEN>_<NAME> = {segments, mirror, A, B}.
Illustrations (a drawing exported as it is, no ids: the specimens of the plates): every path is a part, its
colour its role (white / cream: the outline, red: tips and accents, teal: nodes and vessels, light cyan: highlights);
a filled outline of a stroke is drawn as its contour (at the screen's scale both sides fall on one pixel), a small
round closed path becomes a dot. Drawings far apart in x are separate specimens, numbered from the left. Out per
SVG: SH_<SCREEN>_<n>_ILL[] (bytes: per part a head (kind << 4 | colour), its segment count (2 bytes) or a dot's
radius, then int8 points, each specimen centred and scaled to +-63 units), SH_<SCREEN>_<n>_LEN, SH_<SCREEN>_COUNT.
"""
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

TOK = re.compile(r"[MmCcSsQqTtLlHhVvZz]|-?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?")


def cubics(d):
    """path data -> start point, list of cubic segments ((x1, y1), (x2, y2), (x, y))"""
    toks = TOK.findall(d)
    i, cmd, cur, start, segs, last_c, last_q = 0, None, (0.0, 0.0), None, [], None, None

    def num():
        nonlocal i
        v = float(toks[i])
        i += 1
        return v

    while i < len(toks):
        if re.match(r"[A-Za-z]", toks[i]):
            cmd = toks[i]
            i += 1
            if cmd in "Zz":
                if start and (abs(cur[0] - start[0]) > 1e-6 or abs(cur[1] - start[1]) > 1e-6):
                    a, b = cur, start
                    segs.append(((a[0] + (b[0] - a[0]) / 3, a[1] + (b[1] - a[1]) / 3),
                                 (a[0] + 2 * (b[0] - a[0]) / 3, a[1] + 2 * (b[1] - a[1]) / 3), b))
                cur = start
                last_c = last_q = None
                continue
        rel = cmd.islower()
        c = cmd.upper()
        ox, oy = cur if rel else (0.0, 0.0)
        if c == "M":
            if start is not None:
                raise ValueError("one subpath per path")
            cur = (ox + num(), oy + num())
            start = cur
            cmd = "l" if rel else "L"                    # (more pairs after M are lines)
            last_c = last_q = None
            continue
        if c in "LHV":
            if c == "L":
                p = (ox + num(), oy + num())
            elif c == "H":
                p = (ox + num() if rel else num(), cur[1])
            else:
                p = (cur[0], oy + num() if rel else num())
            a = cur
            segs.append(((a[0] + (p[0] - a[0]) / 3, a[1] + (p[1] - a[1]) / 3),
                         (a[0] + 2 * (p[0] - a[0]) / 3, a[1] + 2 * (p[1] - a[1]) / 3), p))
            cur, last_c, last_q = p, None, None
        elif c in "CS":
            if c == "C":
                p1 = (ox + num(), oy + num())
            else:
                p1 = (2 * cur[0] - last_c[0], 2 * cur[1] - last_c[1]) if last_c else cur
            p2 = (ox + num(), oy + num())
            p = (ox + num(), oy + num())
            segs.append((p1, p2, p))
            cur, last_c, last_q = p, p2, None
        elif c in "QT":
            if c == "Q":
                q = (ox + num(), oy + num())
            else:
                q = (2 * cur[0] - last_q[0], 2 * cur[1] - last_q[1]) if last_q else cur
            p = (ox + num(), oy + num())
            a = cur
            segs.append(((a[0] + 2 * (q[0] - a[0]) / 3, a[1] + 2 * (q[1] - a[1]) / 3),
                         (p[0] + 2 * (q[0] - p[0]) / 3, p[1] + 2 * (q[1] - p[1]) / 3), p))
            cur, last_c, last_q = p, None, q
        else:
            raise ValueError(f"unsupported path command {cmd}")
    if start is None or not segs:
        raise ValueError("empty path")
    return start, segs


def flat(start, segs):
    out = [start] + [p for s in segs for p in s]
    vals = []
    for x, y in out:
        for v in (x, y):
            r = int(round(v))
            if not -128 <= r <= 127:
                raise ValueError(f"point {v} outside the int8 canvas (-64..64 recommended)")
            vals.append(r)
    return vals


def colour_of(el, parents):
    """the fill (or stroke) of a path, its own or inherited -> 0 cream, 1 coral, 2 teal, 4 mint"""
    for e in [el] + parents[::-1]:
        st = (e.get("style") or "") + ";fill:" + (e.get("fill") or "") + ";stroke:" + (e.get("stroke") or "")
        m = re.search(r"(?:fill|stroke)\s*:\s*(?:rgb\((\d+),\s*(\d+),\s*(\d+)\)|#([0-9a-fA-F]{6}))", st)
        if m:
            if m.group(4):
                r, g, b = (int(m.group(4)[i:i + 2], 16) for i in (0, 2, 4))
            else:
                r, g, b = (int(m.group(i)) for i in (1, 2, 3))
            if r > 180 and g > 180 and b > 180:
                return 0
            if r > 180 and g < 120:
                return 1
            if g > 200 and b > 180:
                return 4
            if g > 90 and b > 90:
                return 2
            return 0
    return 0


def illustration(svg, screen, lines):
    """an SVG without ids: its parts, grouped into specimens (see the top); returns the bytes written"""
    parts = []
    def walk(el, parents):
        for ch in el:
            tag = ch.tag.split("}")[-1]
            if tag == "path" and ch.get("d"):
                start, segs = cubics(ch.get("d"))
                segs = [g for g in segs if not all(abs(p[0] - start[0]) < 1e-6 and abs(p[1] - start[1]) < 1e-6
                                                   for p in g)] or segs
                pts = [start] + [p for g in segs for p in g]
                xs, ys = [p[0] for p in pts], [p[1] for p in pts]
                parts.append(dict(start=start, segs=segs, col=colour_of(ch, parents),
                                  bb=(min(xs), min(ys), max(xs), max(ys))))
            walk(ch, parents + [ch])
    walk(ET.parse(svg).getroot(), [])
    parts.sort(key=lambda q: q["bb"][0])
    groups = []
    for q in parts:                                      # specimens: overlapping x extents (a margin of 12)
        if groups and q["bb"][0] <= groups[-1]["bb"][2] + 12:
            g = groups[-1]
            g["parts"].append(q)
            g["bb"] = (min(g["bb"][0], q["bb"][0]), min(g["bb"][1], q["bb"][1]), max(g["bb"][2], q["bb"][2]),
                       max(g["bb"][3], q["bb"][3]))
        else:
            groups.append(dict(parts=[q], bb=q["bb"]))
    total = 0
    for n, g in enumerate(groups):
        x0, y0, x1, y1 = g["bb"]
        cx, cy, half = (x0 + x1) / 2, (y0 + y1) / 2, max(x1 - x0, y1 - y0) / 2
        k = 63.0 / half
        def q8(v):
            return max(-127, min(127, int(round(v))))
        out = []
        for q in g["parts"]:
            bx0, by0, bx1, by1 = q["bb"]
            w, h = (bx1 - bx0) * k, (by1 - by0) * k
            if w < 14 and h < 14 and 0.7 < (w + 1e-6) / (h + 1e-6) < 1.4 and len(q["segs"]) >= 3:   # a dot
                out += [1 << 4 | q["col"], q8(max(w, h) / 2 * 2) & 255, 0,
                        q8(((bx0 + bx1) / 2 - cx) * k) & 255, q8(((by0 + by1) / 2 - cy) * k) & 255]
                continue
            pts = [q["start"]] + [p for s in q["segs"] for p in s]
            out += [q["col"], len(q["segs"]) & 255, len(q["segs"]) >> 8]
            for x, y in pts:
                out += [q8((x - cx) * k) & 255, q8((y - cy) * k) & 255]
        name = f"SH_{screen}_{n + 1}"
        lines.append(f"static const uint8_t {name}_ILL[{len(out)}] = {{")
        for i in range(0, len(out), 24):
            lines.append("    " + ", ".join(map(str, out[i:i + 24])) + ",")
        lines.append("};")
        lines.append(f"#define {name}_LEN {len(out)}u")
        total += len(out)
    lines.append(f"#define SH_{screen}_COUNT {len(groups)}u")
    return total


def main():
    out = Path(sys.argv[1])
    src = Path(sys.argv[2]) if len(sys.argv) > 2 else Path(__file__).resolve().parent.parent / "assets" / "ui-shapes"
    lines = ["/* JIANT FM's line art: generated by tools/gen_ui_shapes.py from the SVGs in assets/ui-shapes. Do not edit:",
             " * edit the SVGs (docs/TONIC-UI.md). og_shape_t is ui_organic.c's. */"]
    total = 0
    for svg in sorted(src.glob("*.svg")):
        screen = re.sub(r"\W", "_", svg.stem).upper()
        paths = {}
        for el in ET.parse(svg).iter():
            if el.tag.split("}")[-1] == "path" and el.get("id"):
                paths[el.get("id")] = el
        if not paths:                                    # no ids: an illustration
            try:
                total += illustration(svg, screen, lines)
            except ValueError as e:
                raise SystemExit(f"{svg.name}: {e}")
            continue
        names = []
        for pid in paths:
            base = pid[:-2] if pid.endswith(("_a", "_b")) else pid
            if base not in names:
                names.append(base)
        for base in names:
            a = paths.get(base + "_a", paths.get(base))
            b = paths.get(base + "_b", a)
            if a is None:
                raise SystemExit(f"{svg.name}: {base}_b without {base}_a")
            try:
                sa, ga = cubics(a.get("d"))
                sb, gb = cubics(b.get("d"))
                if len(ga) != len(gb):
                    raise ValueError(f"A has {len(ga)} segments, B {len(gb)}: they must match")
                va, vb = flat(sa, ga), flat(sb, gb)
            except ValueError as e:
                raise SystemExit(f"{svg.name}: {base}: {e}")
            name = "SH_" + screen + "_" + re.sub(r"\W", "_", base).upper()
            mirror = 1 if a.get("data-mirror") == "1" else 0
            lines.append(f"static const int8_t {name}_PTS[2][{len(va)}] = {{")
            lines.append("    {" + ", ".join(map(str, va)) + "},")
            lines.append("    {" + ", ".join(map(str, vb)) + "},")
            lines.append("};")
            lines.append(f"static const og_shape_t {name} = {{{len(ga)}, {mirror}, {name}_PTS[0], {name}_PTS[1]}};")
            total += 2 * len(va)
    lines.append(f"/* {total} bytes of points */")
    out.write_text("\n".join(lines) + "\n")
    print(f"gen_ui_shapes: {out} ({total} bytes of points)")


if __name__ == "__main__":
    main()
