#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Put the firmware the FM-1 Rescue restores into its APK (android/rescue/app/src/main/assets-firmware/):

  jiant.fwsc        the package the site's installer offers (web/PUBLIC_RELEASE.json's version when listed,
                    else the first of firmware/versions.json)
  jiant.json        {"version", "product", "sha256"}
  LICENSE-GPL-3.0.txt, LICENSE-Apache-2.0.txt   (the package holds JieLi SDK files under Apache-2.0)
  FM-1-V15.fwsc     M-VAVE's official V15 (the owner's decision, 2026-11-20): Quixotic7/FM1Backups at a pinned
                    commit, accepted only with the genuine file's sha256

  python3 android/rescue/fetch_firmware.py [SITE_URL]       (the android workflow runs it before gradle)
  python3 android/rescue/fetch_firmware.py --file PKG.fwsc VERSION   (a local JIANT package instead)

Same checks as web/make_site.py: identity FM-1_9xx and Felucca's own loader in it (no vendor files).
"""
import hashlib
import json
import re
import shutil
import sys
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent.parent
OUT = HERE / "app" / "src" / "main" / "assets-firmware"
V15_URL = ("https://raw.githubusercontent.com/Quixotic7/FM1Backups/"
           "8cbc3b066ed15ec6a0d5a990efb78b56f6c17183/V15-FM-1.fwsc")
V15_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"   # M-VAVE's FM-1.fwsc, V15
SITE = "https://juanjiant-bit.github.io/JIANT/"
BLOCKS, BLK, KEEP, NO_CHAR = 20, 0x30, 0x2F, 0x7D


def product_of(raw):
    chars = []
    for i in range(BLOCKS):
        m = raw[i * BLK + KEEP]
        if m != NO_CHAR:
            chars.append(chr((m - i - 1) & 0xFF))
    return "".join(chars)


def get(url):
    req = urllib.request.Request(url, headers={"User-Agent": "fm1-rescue-build"})
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read()


def write(raw, version):
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_9\d\d", product):
        raise SystemExit("identity %r is not a JIANT package (FM-1_9xx)" % product)
    if b"FELUCCA-LOADER-1" not in raw:
        raise SystemExit("no Felucca loader in the package: not one made by tools/fm1pkg_make.py")
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "jiant.fwsc").write_bytes(raw)
    sha = hashlib.sha256(raw).hexdigest()
    (OUT / "jiant.json").write_text(json.dumps({"version": version, "product": product, "sha256": sha}) + "\n")
    shutil.copy(ROOT / "LICENSE", OUT / "LICENSE-GPL-3.0.txt")
    shutil.copy(ROOT / "LICENSES" / "Apache-2.0.txt", OUT / "LICENSE-Apache-2.0.txt")
    print("assets-firmware: JIANT %s (%s), %d bytes, sha256 %s" % (version, product, len(raw), sha))


def write_v15():
    raw = get(V15_URL)
    sha = hashlib.sha256(raw).hexdigest()
    if sha != V15_SHA256:
        raise SystemExit("V15: sha256 %s is not the genuine FM-1.fwsc (%s)" % (sha, V15_SHA256))
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "FM-1-V15.fwsc").write_bytes(raw)
    print("assets-firmware: official V15, %d bytes, sha256 %s" % (len(raw), sha))


def main():
    if len(sys.argv) == 4 and sys.argv[1] == "--file":
        write(Path(sys.argv[2]).read_bytes(), sys.argv[3])
        write_v15()
        return
    site = sys.argv[1] if len(sys.argv) > 1 else SITE
    versions = json.loads(get(site + "firmware/versions.json"))
    want = json.loads((ROOT / "web" / "PUBLIC_RELEASE.json").read_text()).get("version")
    pick = next((v for v in versions if v.get("version") == want), versions[0])
    if not re.fullmatch(r"felucca-[A-Za-z0-9.-]+\.fwsc", pick["file"]):
        raise SystemExit("unexpected file name %r" % pick["file"])
    write(get(site + "firmware/" + pick["file"]), pick["version"])
    write_v15()


if __name__ == "__main__":
    main()
