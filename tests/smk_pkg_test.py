#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (after Melodee's tests/smk_pkg_test.py, Copyright (C) 2026 M. Geisler (genkigenki/makkuro))
"""An SMK-37 package (tools/build.py --board smk37): its identity, flash.bin the stock PRCT size (0x9D000), the app
area carrying the stock v016 region table exactly (VM, PRCT, BTIF, USRTRIM, USRFLASH, USR), app.bin the build's
felucca.bin, the encrypted region ending inside flash.bin, JIANT's own update loader, and no update-record
look-alike. With a stock M-VAVE v016 package (read only, never linked or copied) the region table's bytes at flash
0x4060..0x411F are compared with the stock one's too: the bytes the SMK build's board check (ota.c) and a later return
to stock rely on.
  smk_pkg_test.py PACKAGE.fwsc PRODUCT APP.bin [STOCK.fwsc]"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import fm1_install as I  # noqa: E402
import fm1pkg_make as M  # noqa: E402

failed = 0
RT_OFF, RT_LEN = 0x4060, 0xC0          # the region table in flash.bin (ota.c OTA_RT_OFF / OTA_RT_LEN)
STOCK_TABLE = [(0x9D000, 0x23000, 0x12, 0x81, 0, bytes.fromhex("564d00ffffffffffffffffff00800000")),
               (0, 0x9D000, 0x92, 0x82, 0, bytes.fromhex("5052435400ffffffffffffff434f4445")),
               (0xC0000, 0x1000, 0x92, 0x82, 0, bytes.fromhex("4254494600ffffffffffffff4155544f")),
               (0xC1000, 0x1000, 0x92, 0x81, 0, bytes.fromhex("5553525452494d00ffffffffffffffff")),
               (0xC2000, 0x29000, 0x92, 0x81, 0, bytes.fromhex("555352464c41534800ffffffffffffff")),
               (0xF4000, 0xA000, 0x92, 0x81, 1, b"USR\0" + b"\xff" * 12)]


def ok(cond, what):
    global failed
    print(f"smk pkg: {what:<84} {'ok' if cond else 'FAIL'}")
    failed += not cond


def entries(image):
    """the UFW entry list: [(type, offset, length, crc16)]"""
    hdr = bytearray(image[:0x40])
    M.enc(hdr, 0, 0x40)
    n = struct.unpack_from("<H", hdr, 8)[0]
    out = []
    for i in range(n):
        e = bytearray(image[0x40 + i * 0x50:0x90 + i * 0x50])
        M.enc(e, 0, 0x50)
        typ, _, crc = struct.unpack_from("<HHH", e, 0)
        off, size = struct.unpack_from("<II", e, 8)
        out.append((typ, off, size, crc))
    return out


def logical(raw, blocks):
    """the package without its marker bytes (I.logical_image's, for any block count)"""
    out = bytearray()
    for i in range(blocks):
        out += raw[i * 0x30:i * 0x30 + 0x2F]
    return bytes(out + raw[blocks * 0x30:])


def flash_of(raw):
    """(image, entries, flash.bin, its UFW CRC16). JIANT's packages interleave 20 blocks; the stock SMK-37 Pro v016
    file 36 (the marker 0x7D past its identity; Melodee's fm1_install.py block_count): the count whose flash.bin
    passes its CRC"""
    for blocks in [20] + list(range(21, 64)):
        image = logical(raw, blocks)
        ents = {t: (o, n, c) for t, o, n, c in entries(image)}
        o, n, c = ents.get(0, (0, 0, 0))
        if n and M.crc16(image[o:o + n]) == c:
            break
    return image, ents, bytes(image[o:o + n]), c


def area_entries(flash):
    area = bytearray(flash[0x4000:0x4000 + 0x400])
    M.sfc(area, 0, len(area), 0, M.KEY)
    ents = []
    for k in range(1, 12):
        e = bytes(area[k * 32:(k + 1) * 32])
        if M.crc16(e[2:]) != struct.unpack_from("<H", e)[0]:
            break
        dcrc, eoff, esize, flags, resvd, index = struct.unpack_from("<HIIBBH", e, 2)
        ents.append((eoff, esize, flags, resvd, index, e[16:32]))
    return area, ents


raw = Path(sys.argv[1]).read_bytes()
want_product = sys.argv[2]
app = Path(sys.argv[3]).read_bytes()
image, ents, flash, fcrc = flash_of(raw)
ok(I.product_of(raw) == want_product, f"identity {I.product_of(raw)!r} (want {want_product!r})")
ok(len(flash) == 0x9D000 and M.crc16(flash) == fcrc, f"flash.bin {len(flash):#x} B (the stock PRCT 0x9D000), its CRC16")
ok(100 in ents and b"FELUCCA-LOADER-1" in raw, "ota.bin: JIANT's own update loader (FELUCCA-LOADER-1)")
area, aents = area_entries(flash)
ok([e[5][:7] for e in aents[:2]] == [b"app.bin", b"cfg_too"], "app.bin, cfg_tool.bin first")
ok(aents[2:] == STOCK_TABLE, "region table == stock v016 (VM, PRCT, BTIF, USRTRIM, USRFLASH, USR), nothing else")
ok(0x4000 + 32 * (1 + len(aents)) <= 0x4120 and aents[0][0] == 0x120 and aents[0][1] == M.APP_SLOT,
   f"the table ends by 0x4120, app.bin at +0x120, its slot {M.APP_SLOT:#x}")
plain = bytearray(flash[0x4000:0x4000 + 0x120 + M.APP_SLOT])
M.sfc(plain, 0, len(plain), 0, M.KEY)
ok(bytes(plain[0x120:0x120 + len(app)]) == app and len(app) <= M.APP_SLOT,
   f"app.bin is this build's app ({len(app)} B of the {M.APP_SLOT} B slot)")
last = max(i for i in range(len(flash)) if flash[i] != 0xFF)
ok(last < 0x93000, f"the encrypted region ends at {last + 1:#x}, inside the FM-1's app area too (< 0x93000)")
ok(flash[:0x4000] != b"\xff" * 0x4000, "head present (never written by JIANT's loader)")
look = [s - 0x100 for s in range(0x5000, len(flash) + 1, 0x1000)
        if flash[s - 0x100 + 6] == 0x41 and flash[s - 0x100 + 7] == 0x54 and
        struct.unpack_from("<H", flash, s - 0x100)[0] == M.crc16(flash[s - 0x100 + 2:s - 0x100 + 80])]
ok(not look, "no 4K-256 slot of flash.bin reads as an update record")
ok(len(raw) < 0x9D000 + 0x10000, f"package {len(raw)} B")
if len(sys.argv) > 4:                  # the stock M-VAVE v016 package of the same model: data only, read here
    sraw = Path(sys.argv[4]).read_bytes()
    _, _, sflash, _ = flash_of(sraw)
    _, sents = area_entries(sflash)
    ok(len(sflash) == len(flash), f"stock {Path(sys.argv[4]).name}: flash.bin {len(sflash):#x} B, the same size")
    ok(sents[2:] == STOCK_TABLE, "stock package: its region table decodes to the table this test expects")
    ok(sflash[RT_OFF:RT_OFF + RT_LEN] == flash[RT_OFF:RT_OFF + RT_LEN],
       "flash 0x4060..0x411F byte for byte the stock package's (the SFC-enciphered region table)")
print("SMK PACKAGE TEST FAILED" if failed else "smk package test passed")
sys.exit(1 if failed else 0)
