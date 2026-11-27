#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(V10.8.60, from JIANT BYTE byte-cpu-light devcpu2) the lines model: the FM-1's time a block, core by core, as the 32-byte lines of the flash the sound reads
(its code and its constants: each a cache line filled, ~1.25 us, the I-cache 32 KiB shared by both cores and the UI)
plus its instructions (one a cycle at 360 MHz). Fitted to MENU > ABOUT on f22f004f, the owner's project (NOW 83 %,
PEAK 132 % with the relief on; FUNC 581 .. 610 us, DRUM-W 422 .. 509, FX 212): this model reads it 89 / 99 % (mean /
worst of the busier core), FUNC 641 us, DRUM-W 425, the buses 139 (each part alone, cold).

How: tests/cpu_lines_test.c plays a scene on the host (built -fno-inline: its functions the source's) under valgrind's
lackey (tests/cpu_lines_lk.c: each block's lines of code and constants, core 0's and core 1's, each part's); callgrind
gives the host's calls. The device's functions (tools/audio_map.txt, written by tools/build.py: each one's size and
section, what it calls) say where each host function's code is on the FM-1: its own function where the device calls it
(else it was inlined into its caller's), in RAM (free) or in the flash (its host lines x RHO, at most the device
function's own). A first pass gives each part's cost, the second plays the device's split with it (fx.c mc_split, as
the FM-1 measures them). The FM-1 runs a block in 1451 us.
   python3 tests/cpu_lines_test.py [SCENE ...]      (run_byte_tests.sh: 101, the heaviest GEN song with CLOUD, the delay
                                                      and the chorus; fails past LIMIT on either core, its worst block)"""
import bisect, collections, json, os, re, subprocess, sys, tempfile

K_US, RHO, MHZ, BLOCK = 1.25, 0.77, 360.0, 64 / 44100 * 1e6
LIMIT = {}                      # (FM: numbers reported; a guard once FM passes one)
NBLK = int(os.environ.get("CL_NBLK", "16"))
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RAMDATA = {"SINE_Q", "DIV_DEN"}
HOSTMAP = {"mix_part": "mix_part_m0", "mix_part_m": "mix_part_m", "fx_run": "fx_run0"}
NAMES = {"101": "GEN EXPLORE s123 + CLOUD, delay, chorus", "102": "GEN EXPLORE s7 DANCE", "103": "GEN genre 7 s42",
         "104": "GEN genre 12 s2024 DANCE", "105": "GEN EXPLORE s999", "106": "GEN EXPLORE s321 CALM", "107": "GEN genre 3 s77 DANCE",
         "108": "GEN NEO CLASSIC s5 CALM", "0c": "CPU TEST's song, TRACKS 4"}


def sh(*a, **k):
    return subprocess.run(a, capture_output=True, text=True, **k)


def build(out):
    exe, lk = os.path.join(out, "cpu_lines"), os.path.join(out, "cpu_lines_lk")
    cc = os.environ.get("CC", "cc")
    r = sh(cc, "-std=gnu99", "-O2", "-fno-inline", "-no-pie", "-fno-pie", "-w", "-Ibuild/gen", "-Ifirmware/src", "-Itests",
           "-o", exe, "tests/cpu_lines_test.c", "-lm", cwd=ROOT)
    if r.returncode:
        sys.exit("cpu_lines: the build failed\n" + r.stderr[-3000:])
    r = sh(cc, "-O2", "-o", lk, "tests/cpu_lines_lk.c", cwd=ROOT)
    if r.returncode:
        sys.exit("cpu_lines: the filter's build failed\n" + r.stderr[-3000:])
    return exe, lk


def run(exe, lk, scene, prefix, cost):
    nm = {p[2]: p[0] for p in (ln.split() for ln in sh("nm", exe).stdout.splitlines()) if len(p) == 3}
    sec = {}
    for ln in sh("readelf", "-S", "-W", exe).stdout.splitlines():
        m = re.search(r"\]\s+(\S+)\s+\S+\s+([0-9a-f]+)\s+[0-9a-f]+\s+([0-9a-f]+)", ln)
        if m: sec[m.group(1)] = (int(m.group(2), 16), int(m.group(3), 16))
    env = dict(os.environ)
    env.pop("CL_COST", None)
    if cost:
        env["CL_COST"] = cost
    lo, n = sec[".rodata"]; tlo, tn = sec[".text"]
    args = [lk, nm["cl_mark"], nm["cl_c1"], nm["cl_part"], nm["cl_ex"], f"{lo:x}", f"{lo + n:x}", f"{tlo:x}",
            f"{tlo + tn:x}", prefix]
    v = subprocess.Popen(["valgrind", "--tool=lackey", "--trace-mem=yes", exe, scene, str(NBLK)], stdout=subprocess.DEVNULL,
                         stderr=subprocess.PIPE, env=env)
    f = subprocess.run(args, stdin=v.stderr, capture_output=True, text=True)
    v.wait()
    if v.returncode or f.returncode:
        sys.exit(f"cpu_lines: scene {scene}: the run failed ({v.returncode}, {f.returncode}) {f.stderr[-500:]}")
    cg = prefix + ".cg"
    r = subprocess.run(["valgrind", "--tool=callgrind", f"--callgrind-out-file={cg}", exe, scene, str(NBLK)],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env)
    if r.returncode:
        sys.exit(f"cpu_lines: scene {scene}: callgrind failed")
    names, callers, cur, callee = {}, collections.defaultdict(set), None, None
    def nmid(tok):
        m = re.match(r"\((\d+)\)(?: (.*))?", tok)
        if m.group(2): names[m.group(1)] = m.group(2)
        return names.get(m.group(1), "?")
    for ln in open(cg, errors="replace"):
        if ln.startswith("fn="): cur = nmid(ln[3:].strip())
        elif ln.startswith("cfn="): callee = nmid(ln[4:].strip())
        elif ln.startswith("calls=") and cur and callee: callers[callee].add(cur)
    os.remove(cg)
    json.dump({k: sorted(v) for k, v in callers.items()}, open(prefix + ".calls.json", "w"))
    return callers


class Model:
    def __init__(self, exe):
        self.syms = []
        for ln in sh("nm", "-S", "--defined-only", "-n", exe).stdout.splitlines():
            p = ln.split()
            if len(p) == 4: self.syms.append((int(p[0], 16), int(p[1], 16), p[3]))
        self.syms.sort(); self.starts = [s[0] for s in self.syms]
        self.dev, self.calls, self.taken, self.ind = {}, {}, set(), set()
        for ln in open(os.path.join(ROOT, "tools", "audio_map.txt")):
            if ln.startswith("#"): continue
            p = ln.split()
            self.dev[p[0]] = (int(p[1]), p[2])
            rest = p[3:]
            if rest and set(rest[0]) <= set("*!"):
                if "*" in rest[0]: self.taken.add(p[0])
                if "!" in rest[0]: self.ind.add(p[0])
                rest = rest[1:]
            self.calls[p[0]] = set(rest)

    def sym(self, a):
        i = bisect.bisect_right(self.starts, a) - 1
        while i >= 0 and not (self.syms[i][0] <= a < self.syms[i][0] + max(self.syms[i][1], 1)):
            i -= 1
            if i < 0 or a - self.syms[i][0] > 1 << 20: return None
        return self.syms[i] if i >= 0 else None

    def devof(self, n):
        b = re.sub(r"\.(constprop|part|isra|cold|lto_priv)\.\d+", "", n)
        b = re.sub(r"\.(constprop|part|isra|cold)\.\d+", "", b)
        for c in (b, HOSTMAP.get(b, "")):
            if c in self.dev: return c
        return None

    def homes(self, h, X, CAL, memo, depth=0):
        """the device functions h's code runs in this block: its own where a caller run calls it (directly, or through a
        register with its address taken), else inside the callers' (inlined), a share each"""
        if h in memo: return memo[h]
        memo[h] = []
        d = self.devof(h)
        cs = [g for g in CAL.get(h, ()) if g in X and g != h]
        if not cs:                                       # (a caller whose lines this block began in another's: any)
            cs = [g for g in CAL.get(h, ()) if g != h]
        r = []
        if not cs:
            r = [d] if d else []
        for g in cs:
            hg = self.homes(g, X, CAL, memo, depth + 1) if depth < 16 else []
            if d and (not hg or any(d in self.calls.get(x, ()) or (d in self.taken and x in self.ind) or x == d for x in hg)):
                r.append(d)
            elif hg: r.extend(hg)
            elif d: r.append(d)
        memo[h] = r
        return r

    def slot(self, prefix, k, CAL):
        bi_ = open(f"{prefix}.i{k}").read().split("E\n")[:-1]
        bd_ = open(f"{prefix}.d{k}").read().split("E\n")[:-1]
        rows = []
        for bi, bd in zip(bi_, bd_):
            byfn, ni = collections.Counter(), 0
            for x in bi.split("\n"):
                if not x or x.startswith("C "): continue
                if x.startswith("N "): ni = int(x[2:]); continue
                s = self.sym(int(x, 16))
                if s: byfn[s[2]] += 1
            X, memo, blk = set(byfn), {}, collections.Counter()
            for f, cnt in byfn.items():
                hs = self.homes(f, X, CAL, memo)
                for x in hs:
                    if self.dev[x][1] == "text": blk[x] += cnt * RHO / len(hs)
            fl = sum(min(w, self.dev[x][0] / 32.0 + 1.0) for x, w in blk.items())
            for x, w in blk.items():                     # (CL_TOP=N: the dearest device functions, their flash lines)
                TOP[(prefix, k)][x] += min(w, self.dev[x][0] / 32.0 + 1.0)
            dl = 0
            for x in bd.split():
                s = self.sym(int(x, 16))
                if not (s and s[2].split(".")[0] in RAMDATA): dl += 1
            rows.append((fl, dl, ni))
        return rows


TOP = collections.defaultdict(collections.Counter)


def us(r): return K_US * (r[0] + r[1]) + r[2] / MHZ


def scene(exe, lk, M, out, sc):
    pre = os.path.join(out, sc)
    CAL = run(exe, lk, sc, pre + "p", "")
    parts = [M.slot(pre + "p", 2 + k, CAL) for k in range(6)]
    fx = M.slot(pre + "p", 8, CAL)
    cost = ",".join(str(int(sum(us(r) for r in p) / max(1, len(p)))) for p in parts) + f",{int(sum(us(r) for r in fx) / max(1, len(fx)))}"
    return pre, run(exe, lk, sc, pre, cost), cost


def main():
    from concurrent.futures import ThreadPoolExecutor
    scenes = sys.argv[1:] or ["101"]
    out = tempfile.mkdtemp(prefix="cpu_lines_")
    exe, lk = build(out)
    M = Model(exe)
    bad = 0
    with ThreadPoolExecutor(min(4, len(scenes))) as ex:
        done = list(ex.map(lambda sc: scene(exe, lk, M, out, sc), scenes))
    for sc, (pre, CAL, cost) in zip(scenes, done):
        row = []
        worst = 0.0
        for c in (0, 1):
            rows = M.slot(pre, c, CAL)
            n = len(rows) or 1
            u = [us(r) for r in rows]
            lines = [r[0] + r[1] for r in rows]
            worst = max(worst, max(u) / BLOCK)
            row.append(f"core {c} {sum(lines) / n:4.0f} / {max(lines):4.0f} lines, {sum(r[2] for r in rows) / n / 1000:5.1f} k instr: "
                       f"{sum(u) / n / BLOCK * 100:3.0f} / {max(u) / BLOCK * 100:3.0f} %")
        lim = LIMIT.get(sc)
        over = lim is not None and worst > lim
        bad += over
        print(f"cpu_lines: {sc} {NAMES.get(sc, ''):34s} " + "   ".join(row) + (f"  OVER {lim * 100:.0f} %" if over else ""))
        if os.environ.get("CL_TOP"):
            for c in (0, 1):
                t = TOP[(pre, c)]
                nb = max(1, NBLK)
                print(f"cpu_lines:     core {c} dearest (flash lines a block): " + ", ".join(
                    f"{x} {w / nb:.0f}" for x, w in t.most_common(int(os.environ["CL_TOP"]))))
        if os.environ.get("CL_VERB"):
            print(f"cpu_lines:     each part alone, its lines cold (us; the split's measure): {cost.replace(',', ' ')} (the last the buses)")
    print("cpu_lines: " + ("FAILED: a scene's worst block over its limit" if bad else
                           "every scene's worst block under its limit (mean / worst, a core each)"))
    if not os.environ.get("CL_KEEP"):
        import shutil
        shutil.rmtree(out, ignore_errors=True)
    else:
        print("cpu_lines: the traces in", out)
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
