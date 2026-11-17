#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Exercise the real startup decoder, including bounds and malformed trees."""
import os
from pathlib import Path
import random
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import boot_huff

HARNESS = r'''
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "boot_huff.h"
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    uint32_t h[4], count = 0;
    if (!f) return 1;
    while (fread(h, sizeof h, 1, f) == 1) {
        uint8_t *in = malloc(h[0] ? h[0] : 1), *want = malloc(h[1] ? h[1] : 1);
        uint8_t *out = malloc(h[2] + 32);
        if (fread(in, 1, h[0], f) != h[0] || fread(want, 1, h[1], f) != h[1]) return 2;
        memset(out, 0xA5, h[2] + 32);
        int ok = boot_huff(in, in + h[0], out + 16, h[2]);
        if (ok != h[3] || (ok && memcmp(out + 16, want, h[1]))) { fprintf(stderr,"case %u: got %d wanted %u\n",count,ok,h[3]); return 3; }
        for (uint32_t i = 0; i < 16; i++)
            if (out[i] != 0xA5 || out[16 + h[2] + i] != 0xA5) return 4;
        free(in); free(want); free(out); count++;
    }
    fclose(f);
    printf("boot_huff: %u exact round trips and malformed/bounds cases PASS\n", count);
    return 0;
}
'''

rng = random.Random(47)
cases = []
for size in [0, 1, 2, 3, 7, 16, 257, 4096, 32768, 65536]:
    for raw in (bytes([37]) * size, bytes(rng.randrange(256) for _ in range(size)),
                bytes((i * 7 // 13) & 255 for i in range(size))):
        packed = boot_huff.pack(raw)
        assert boot_huff.unpack(packed) == raw
        cases.append((packed, raw, size, 1))
        if size:
            cases.append((packed, b"", size - 1, 0))
        for cut in sorted({0, 2, 11, len(packed) // 2, len(packed) - 1}):
            if cut < len(packed):
                cases.append((packed[:cut], b"", size, 0))

base = boot_huff.pack(b"ab" * 300)
for offset, value in [(2, 9), (3, 0), (13, 127)]:
    damaged = bytearray(base)
    damaged[offset] = value
    cases.append((damaged, b"", 600, 0))
cycle = bytearray(base)
cycle[12:16] = b"\0" * 4
cases.append((cycle, b"", 600, 0))
for offset in (4, 8):
    damaged = bytearray(base)
    damaged[offset:offset + 4] = struct.pack("<I", 65537)
    cases.append((damaged, b"", 65536, 0))

with tempfile.TemporaryDirectory(prefix="boot-huff-") as tmp:
    tmp = Path(tmp)
    source, binary, corpus = tmp / "test.c", tmp / "test", tmp / "cases.bin"
    source.write_text(HARNESS)
    with corpus.open("wb") as f:
        for packed, raw, capacity, valid in cases:
            f.write(struct.pack("<IIII", len(packed), len(raw), capacity, valid))
            f.write(packed)
            f.write(raw)
    subprocess.run(["cc", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                    "-I" + str(ROOT / "firmware/src"), str(source), "-o", str(binary)], check=True)
    subprocess.run([str(binary), str(corpus)], check=True,
                   env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})
