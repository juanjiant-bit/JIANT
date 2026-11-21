#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""JIANT FM as an MPC plugin: the core (ports/core, C) and the plugin (src/JiantFM.cpp, C++) for every MPC.

    python3 ports/mpc-plugin/build.py [--target all|native|arm32|arm64] [--no-test]

  native  this computer's VST2 (build/mpc-plugin/native/jiant-fm.so|.dylib), tested with the SDK's host simulator
          (the MPC's plugin host, its load sequence and quirks): build/mpc-plugin/host_sim_report.txt
  arm32   MPC X, X SE, Live, Live II, One, One+, Key 61, Key 37, Force
  arm64   MPC Live III, MPC XL and later (and the MPC Sample's 64-bit Linux)
  all     the three, then the folder the MPC installs:
          build/mpc-plugin/Juan Jiant - VST - JIANT FM/ (plugin.json, VST/*.so, Plugin Skins/, version.xml), checked
          with the SDK's validate.py, and its zip next to it

inMusic's MPC Plugin SDK (MIT) is not part of this tree: it is cloned at the commit below into build/mpc-plugin-sdk
(or $MPC_SDK). The cross builds use zig (pip install ziglang), as the SDK does; the native one zig or the system's
compiler. The generated headers (build/gen) are made first when missing.
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
HERE = ROOT / "ports" / "mpc-plugin"
OUT = ROOT / "build" / "mpc-plugin"
SDK_URL = "https://github.com/inmusic-software/mpc-plugin-builder-sdk.git"
SDK_COMMIT = "a5949cf6fe2cbfb95bcbe0ddf1f7ef555e71f2be"     # 2026-11-17
SDK = Path(os.environ.get("MPC_SDK", ROOT / "build" / "mpc-plugin-sdk"))
SLUG = "jiant-fm"

# as the SDK's tools/build.py: zig target, cpu, file name
TARGETS = {
    "arm32": ("arm-linux-gnueabihf.2.31", "cortex_a17", f"{SLUG}-arm32.so"),
    "arm64": ("aarch64-linux-gnu.2.31", "cortex_a55", f"{SLUG}-arm64.so"),
}
CORE_FLAGS = ["-O2", "-ffp-contract=off", "-std=gnu99", "-w", "-fPIC", "-fvisibility=hidden"]
CXX_FLAGS = ["-std=c++17", "-O2", "-fPIC", "-fvisibility=hidden", "-fvisibility-inlines-hidden",
             "-ffunction-sections", "-fdata-sections", "-Wall", "-Wextra"]


def run(argv, **kw):
    if os.environ.get("VERBOSE"):
        print("  $ " + " ".join(str(a) for a in argv))
    p = subprocess.run([str(a) for a in argv], **kw)
    if p.returncode:
        raise SystemExit(f"mpc-plugin: failed ({p.returncode}): {' '.join(str(a) for a in argv)}")
    return p


def ensure_sdk():
    if (SDK / "framework" / "mpc_plugin.h").is_file():
        head = subprocess.run(["git", "-C", SDK, "rev-parse", "HEAD"], capture_output=True, text=True).stdout.strip()
        if head and head != SDK_COMMIT and "MPC_SDK" not in os.environ:
            print(f"mpc-plugin: {SDK} is at {head[:10]}, not {SDK_COMMIT[:10]}; checking it out")
            run(["git", "-C", SDK, "fetch", "--depth", "1", "origin", SDK_COMMIT])
            run(["git", "-C", SDK, "checkout", "-q", SDK_COMMIT])
        return
    print(f"mpc-plugin: fetching the MPC Plugin SDK ({SDK_COMMIT[:10]}) into {SDK}")
    SDK.mkdir(parents=True, exist_ok=True)
    run(["git", "-C", SDK, "init", "-q"])
    run(["git", "-C", SDK, "remote", "add", "origin", SDK_URL])
    run(["git", "-C", SDK, "fetch", "-q", "--depth", "1", "origin", SDK_COMMIT])
    run(["git", "-C", SDK, "checkout", "-q", SDK_COMMIT])


def ensure_gen():
    if not (ROOT / "build" / "gen" / "felucca_tables.h").is_file():
        run([sys.executable, "-c", "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"], cwd=ROOT)


def zig():
    if shutil.which("zig"):
        return ["zig"]
    if subprocess.run([sys.executable, "-m", "ziglang", "version"], capture_output=True).returncode == 0:
        return [sys.executable, "-m", "ziglang"]
    return None


def version():
    m = re.search(r'^#define FELUCCA_VERSION "([^"]*)"', (ROOT / "firmware/src/felucca.c").read_text(), re.M)
    return m[1] if m else "?"


def build_one(name, cc, cxx, extra, out):
    """the core (C) and the plugin (C++) linked into one shared library exporting VSTPluginMain only"""
    obj = OUT / name / "jiant_core.o"
    obj.parent.mkdir(parents=True, exist_ok=True)
    out.parent.mkdir(parents=True, exist_ok=True)
    print(f"  [{name}] core")
    run(cc + extra + CORE_FLAGS + [f'-DFELUCCA_VERSION="{version()}"', "-I", ROOT / "build/gen",
                                   "-I", ROOT / "firmware/src", "-c", ROOT / "ports/core/jiant_core.c", "-o", obj])
    print(f"  [{name}] plugin -> {out.relative_to(ROOT)}")
    link = ["-shared", "-Wl,--gc-sections"]
    if sys.platform != "darwin":
        link += [f"-Wl,--version-script={SDK / 'tools/exports.map'}", "-Wl,-s"]
    run(cxx + extra + CXX_FLAGS + ["-I", SDK / "framework", "-I", ROOT / "ports/core",
                                   HERE / "src/JiantFM.cpp", obj, "-o", out] + link + ["-lm"])


def native(z):
    ext = ".dylib" if sys.platform == "darwin" else ".so"
    out = OUT / "native" / (SLUG + ext)
    if z:
        build_one("native", z + ["cc"], z + ["c++"], [], out)
    else:
        build_one("native", [os.environ.get("CC", "cc")], [os.environ.get("CXX", "c++")], [], out)
    return out


def host_sim(z):
    exe = OUT / "host_sim"
    src = SDK / "tools/host-sim/host_sim.cpp"
    if not exe.is_file() or exe.stat().st_mtime < src.stat().st_mtime:
        cxx = z + ["c++"] if z else [os.environ.get("CXX", "c++")]
        run(cxx + ["-std=c++17", "-O2", "-I", SDK / "framework", "-I", src.parent, "-o", exe, src, "-ldl", "-lpthread", "-lm"])
    return exe


def test(lib, z):
    exe = host_sim(z)
    params = OUT / "params.json"
    with open(params, "w") as fh:
        run([exe, lib, "dump"], stdout=fh)
    report = OUT / "host_sim_report.txt"
    ok = True
    with open(report, "w") as fh:
        for args in (["--seconds", "4", "--notes", "48,55,60,64", "--wav", OUT / "test-44k.wav"],
                     ["--seconds", "4", "--playing", "--sr", "48000", "--block", "96", "--wav", OUT / "test-48k.wav"]):
            fh.write(f"### host_sim test {' '.join(map(str, args))}\n")
            fh.flush()
            p = subprocess.run([str(exe), str(lib), "test"] + [str(a) for a in args], stdout=fh, stderr=subprocess.STDOUT)
            ok &= p.returncode == 0
    text = report.read_text()
    for line in text.splitlines():
        if re.search(r"\b(FAIL|WARN|PASS)\b|^###", line):
            print("  " + line)
    print(f"  report: {report.relative_to(ROOT)}")
    if not ok:
        raise SystemExit("mpc-plugin: host_sim failed")
    return params


def assemble(params):
    folder = OUT / "Juan Jiant - VST - JIANT FM"
    if folder.exists():
        shutil.rmtree(folder)
    (folder / "VST").mkdir(parents=True)
    shutil.copy(HERE / "plugin.json", folder / "plugin.json")
    for _t, (_zt, _cpu, f) in TARGETS.items():
        shutil.copy(OUT / "VST" / f, folder / "VST" / f)
    run([sys.executable, SDK / "tools/gen_ui.py", "--params", params, "--ui", HERE / "ui.json",
         "--out", folder / "Plugin Skins", "--kit-cache", OUT])
    m = re.match(r"(\d+)\.(\d+)\.(\d+)", "1.0.0")
    (folder / "version.xml").write_text('<?xml version="1.0" encoding="utf-8"?>\n<plugincontent version="1.0">\n'
                                        f'    <version>{m[1]}.{m[2]}.{m[3]}.0</version>\n'
                                        '    <uiversion>0.1.0.0</uiversion>\n</plugincontent>\n')
    for f in ("LICENSE",):
        shutil.copy(ROOT / f, folder / f)
    (folder / "SOURCE.txt").write_text(
        "JIANT FM is free software under the GNU GPL version 3 (LICENSE).\n"
        "Its source, and how to build this plugin: https://github.com/juanjiant-bit/JIANT (ports/mpc-plugin).\n"
        "Built on the MPC Plugin SDK framework by inMusic Brands, Inc. (MIT).\n")
    run([sys.executable, SDK / "tools/validate.py", folder, "--params", params])
    z = OUT / f"JIANT-FM-MPC-plugin-{version()}.zip"
    with zipfile.ZipFile(z, "w", zipfile.ZIP_DEFLATED) as zf:
        for p in sorted(folder.rglob("*")):
            if p.is_file():
                zf.write(p, p.relative_to(OUT))
    print(f"mpc-plugin: {folder.relative_to(ROOT)}\nmpc-plugin: {z.relative_to(ROOT)}")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--target", choices=["all", "native", "arm32", "arm64"], default="all")
    ap.add_argument("--no-test", action="store_true", help="skip the host simulator")
    a = ap.parse_args()
    ensure_sdk()
    ensure_gen()
    z = zig()
    params = None
    if a.target in ("all", "native"):
        lib = native(z)
        if not a.no_test:
            params = test(lib, z)
    for t, (zt, cpu, f) in TARGETS.items():
        if a.target in ("all", t):
            if not z:
                raise SystemExit("mpc-plugin: the ARM builds need zig (pip install ziglang)")
            build_one(t, z + ["cc"], z + ["c++"], ["-target", zt, f"-mcpu={cpu}"], OUT / "VST" / f)
    if a.target == "all" and params:
        assemble(params)


if __name__ == "__main__":
    main()
