# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""(V10.7.43) LZB: the packed blocks of the app image (the RAM code and data the boot unpacks, firmware/src/boot_lzb.h;
the main loop's packed tables, gz.h), an LZ77 of bits, smaller than the LZ4 blocks it replaces (~13 % on the pi32v2's
code, LZ4's optimal parse ~12 %: 28.3 KB of RAM code and data in ~26 KB). A block has no header and no end mark: it
ends where its output is full (the size is the reader's).

  token    a bit: 0 a literal, the next byte of the stream; 1 a match:
  match    gamma(length - 1) (length 2 ..), gamma(hi + 1), then a byte lo: the offset back (hi << 8 | lo) + 1 (1 .. 65536)
  gamma    Elias's: n zero bits, a one, then the n bits under the value's top one (1: "1", 2: "010", 3: "011", ..)
  bits     MSB first, eight at a time in a byte of the stream taken when the reader needs its first bit: the bytes of
           the literals and the offsets' low bytes sit between them, in the order the reader asks for them

pack() is an optimal parse (the fewest bits over every literal / match choice, the matches from a hash chain), unpack()
the reader step for step as boot_lzb.h's (and its errors where that one stops early)."""


def _gbits(v):                                    # the bits gamma(v) takes, v >= 1
    return 2 * (v.bit_length() - 1) + 1


def pack(src: bytes, chain: int = 512) -> bytes:
    n = len(src)
    head, prev, found = {}, [-1] * n, [()] * n
    for i in range(n - 1):
        key = src[i:i + 2]
        prev[i] = head.get(key, -1)
        head[key] = i
        c, best, longest, steps = prev[i], [], 1, 0
        while c >= 0 and i - c <= 65536 and steps < chain:
            ln, top = 0, n - i
            while ln < top and src[c + ln] == src[i + ln]:
                ln += 1
            if ln > longest:
                best.append((ln, i - c))
                longest = ln
            c, steps = prev[c], steps + 1
        found[i] = tuple(best)
    inf = 1 << 60
    cost, pick = [inf] * (n + 1), [None] * (n + 1)   # cost[i]: the bits of src[i:]; pick[i]: (length, offset) or None
    cost[n] = 0
    for i in range(n - 1, -1, -1):
        best, at = 9 + cost[i + 1], None
        for ln, off in found[i]:
            ob = 1 + _gbits(((off - 1) >> 8) + 1) + 8
            for k in (range(2, ln + 1) if ln <= 64 else (*range(2, 34), ln)):
                c = ob + _gbits(k - 1) + cost[i + k]
                if c < best:
                    best, at = c, (k, off)
        cost[i], pick[i] = best, at
    out, ci, bc = bytearray(), 0, 0

    def bit(b):
        nonlocal ci, bc
        if not bc:
            ci, bc = len(out), 8
            out.append(0)
        bc -= 1
        out[ci] |= b << bc

    def gamma(v):
        nb = v.bit_length() - 1
        for _ in range(nb):
            bit(0)
        for s in range(nb, -1, -1):
            bit(v >> s & 1)
    i = 0
    while i < n:
        if pick[i] is None:
            bit(0)
            out.append(src[i])
            i += 1
            continue
        k, off = pick[i]
        bit(1)
        gamma(k - 1)
        gamma(((off - 1) >> 8) + 1)
        out.append((off - 1) & 255)
        i += k
    return bytes(out)


def unpack(stream: bytes, at: int, size: int):
    """boot_lzb.h's boot_lzb, step for step: the block at stream[at:] into `size` bytes; (the bytes, where the next
    block starts). An error where boot_lzb would stop early"""
    out, s, c = bytearray(), at, 0

    def bit():
        nonlocal s, c
        if not c & 0xFF:
            c = stream[s] << 1 | 1
            s += 1
        b = c >> 8 & 1
        c = c << 1 & 0x1FF
        return b

    def gamma():
        z = 0
        while not bit():
            z += 1
            if z > 16:
                raise ValueError("boot_lzb: a gamma code too long")
        v = 1
        for _ in range(z):
            v = v << 1 | bit()
        return v
    while len(out) < size:
        if not bit():
            out.append(stream[s])
            s += 1
            continue
        k = gamma() + 1
        off = ((gamma() - 1) << 8 | stream[s]) + 1
        s += 1
        if off > len(out):
            raise ValueError(f"boot_lzb: a match {off} back at {len(out)}")
        while k and len(out) < size:
            out.append(out[-off])
            k -= 1
    return bytes(out), s


if __name__ == "__main__":                        # tools/lzb.py FILE..: each file's size packed (and checked)
    import sys
    for f in sys.argv[1:]:
        raw = open(f, "rb").read()
        z = pack(raw)
        assert unpack(z, 0, len(raw)) == (raw, len(z))
        print(f"{f}: {len(raw)} B packed to {len(z)} B")
