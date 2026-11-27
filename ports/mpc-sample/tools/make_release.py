#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""The MPC Sample's release zip: an autorun folder for the top of the microSD card, a README and the licences.

    python3 ports/mpc-sample/tools/make_release.py VERSION [--binary build/jiant] [--allow-dirty]

The binary is the ARM64 build (ports/mpc-sample/build.sh on an ARM64 Linux, or build.sh docker). The SD boot is
sample-base's own (install/sd-boot.sh, sd-worker.sh: the stock firmware checked, the files checked, the binary copied
into RAM, its --self-test run, then started in place of the stock app, which comes back on exit or any failure); the
autorun is signed as the MPC wants it (sample-base device.py autorun_check). Written to ports/mpc-sample/dist/.
"""
import argparse
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path
from time import gmtime

HERE = Path(__file__).resolve().parents[1]
ROOT = HERE.parents[1]
BASE = HERE / "build" / "sample-base"
PACKAGE = "autorun/sample-base"            # (the name sample-base's SD boot scripts look for)
APP = "jiant"
SETUP_LOG = "/run/sample-base-sd.log"
SOURCE_URL = "https://github.com/juanjiant-bit/JIANT"
VERSION = re.compile(r"[vV]\d+(\.\d+){1,3}(-[0-9A-Za-z.]+)?")   # (v0.9.18-preview2, V10.8.48-mpc1)


def base_device():
    spec = importlib.util.spec_from_file_location("sample_base_device", BASE / "device.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def autorun_script(version):
    return ("#!/bin/sh\n"
            f"# JIANT FM {version} for the MPC Sample: starts it from autorun/sample-base once the stock app is up.\n"
            "# Delete this autorun folder to uninstall.\n"
            'D=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)\n'
            f'/bin/sh "$D/sample-base/sd-boot.sh" "$D/sample-base" > {SETUP_LOG} 2>&1 || true\n')


def autorun_manifest():
    script = {"path": "autorun.sh", "askQuestion": False, "message": "Start JIANT FM?",
              "yesButtonText": "Start", "noButtonText": "Cancel"}
    return json.dumps({"value0": {"scripts": [script], "files": []}}, indent=2) + "\n"


def git(*args):
    return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True, check=True).stdout.strip()


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("version", help="such as V10.8.48-mpc1")
    ap.add_argument("--binary", default=str(HERE / "build" / "jiant"))
    ap.add_argument("--allow-dirty", action="store_true", help="package uncommitted changes (not for sending)")
    a = ap.parse_args()
    if not VERSION.fullmatch(a.version):
        raise SystemExit("version: like V10.8.48 or V10.8.48-mpc1")
    if git("status", "--porcelain") and not a.allow_dirty:
        raise SystemExit("commit first: a release names the commit its source is at")
    binary = Path(a.binary)
    head = binary.read_bytes()[:20]
    if head[:4] != b"\x7fELF" or head[18] != 0xB7:       # EM_AARCH64
        raise SystemExit(f"{binary}: not an ARM64 build (ports/mpc-sample/build.sh on ARM64, or build.sh docker)")
    commit = git("rev-parse", "--short=12", "HEAD")
    dev = base_device()
    files = {f"{PACKAGE}/sample-base": binary.read_bytes()}
    for name in dev.SD_FILES[1:]:
        files[f"{PACKAGE}/{name}"] = (BASE / "install" / name).read_bytes()
    files[f"{PACKAGE}/SHA256SUMS"] = "".join(
        hashlib.sha256(data).hexdigest() + "  " + name[len(PACKAGE) + 1:] + "\n"
        for name, data in sorted(files.items())).encode()
    files[f"{PACKAGE}/app"] = f"{APP}\n".encode()
    files[f"{PACKAGE}/VERSION"] = f"{a.version} {commit}\n".encode()
    script, manifest = autorun_script(a.version), autorun_manifest()
    files["autorun/autorun.sh"] = script.encode()
    files["autorun/autorun.json"] = manifest.encode()
    files["autorun/autorun.check"] = dev.autorun_check(manifest, script).encode()
    files["README.txt"] = ((HERE / "release/README.txt").read_text()
                           .replace("{version}", a.version).replace("{commit}", commit)
                           .replace("{source}", SOURCE_URL).encode())
    files["licenses/LICENSE-GPL-3.0.txt"] = (ROOT / "LICENSE").read_bytes()
    files["licenses/LICENSE-AGPL-3.0.txt"] = (BASE / "LICENSE").read_bytes()
    files["licenses/sample-base-THIRD_PARTY.md"] = (BASE / "THIRD_PARTY.md").read_bytes()
    files["licenses/JUCE-LICENSE.md"] = (HERE / "build/deps/JUCE/LICENSE.md").read_bytes()
    files["licenses/JIANT-LICENSING.md"] = (ROOT / "LICENSING.md").read_bytes()
    out = HERE / "dist" / f"JIANT-FM-MPC-Sample-{a.version}.zip"
    out.parent.mkdir(parents=True, exist_ok=True)
    date = gmtime(int(git("log", "-1", "--format=%ct")))[:6]
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        for name in sorted(files):
            info = zipfile.ZipInfo(name, date)
            info.compress_type = zipfile.ZIP_DEFLATED
            mode = 0o755 if name.endswith(".sh") or name.endswith("/sample-base") else 0o644
            info.external_attr = (0o100000 | mode) << 16
            info.create_system = 3
            z.writestr(info, files[name])
    digest = hashlib.sha256(out.read_bytes()).hexdigest()
    out.with_name(out.name + ".sha256").write_text(f"{digest}  {out.name}\n")
    print(f"{out} ({out.stat().st_size // 1024} KiB) at {commit}\nSHA256 {digest}")


if __name__ == "__main__":
    main()
