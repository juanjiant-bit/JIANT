/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.43) The packed blocks of the app image: the RAM code's and the initialised data's load images (main.c
 * fm1_cstart), the main loop's packed tables (gz.h). LZB blocks (tools/lzb.py: an LZ77 of bits, the format there),
 * which replace V10.6z5's LZ4 blocks (~2.8 KB of flash less: 28.3 KB of them in ~25.5 KB).
 *
 * app.ld links the sections' load images outside the flash (region PACKED) and tools/build.py stores each one as a
 * block, the four one after the other from _etext on (the end of .text), in the order .ram_text, .c1_text, .hot_text,
 * .data; fm1_cstart unpacks them with boot_lzb into their sections. What is in RAM after the start is byte for byte
 * what a copy would put there (tools/build.py unpacks every block with the same algorithm in Python and compares
 * before it writes the image; tests/boot_lzb_test.c runs this C code on the build's own blocks).
 *
 * A block ends where its output is full (its section's size, from the linker's symbols). Runs before .data and .bss
 * are set up (fm1_cstart): only locals, no tables, no calls. Out-of-range input (a match before the section's start,
 * a gamma code past 16 bits) stops: nothing is written outside [d, d_end). Returns where the next block starts. */
#pragma once
#include <stdint.h>

static const uint8_t *boot_lzb(const uint8_t *s, uint8_t *d, uint8_t *d_end)
{
    uint8_t *const d0 = d;
    uint32_t c = 0, b, k, z, g[2];                  /* c: the bits left of a byte, MSB first, a 1 under them */
#define LZB_BIT()                                   \
    do {                                            \
        if (!(c & 0xFFu))                           \
            c = (uint32_t)*s++ << 1 | 1u;           \
        b = c >> 8 & 1u;                            \
        c = c << 1 & 0x1FFu;                        \
    } while (0)
    while (d < d_end) {
        const uint8_t *r;
        LZB_BIT();
        if (!b) {                                   /* a literal */
            *d++ = *s++;
            continue;
        }
        for (k = 0; k < 2u; k++) {                  /* a match: gamma(length - 1), gamma(hi + 1) */
            for (z = 0;; z++) {
                LZB_BIT();
                if (b)
                    break;
                if (z >= 16u)
                    return s;                       /* (broken: build.py never makes one) */
            }
            for (g[k] = 1; z; z--) {
                LZB_BIT();
                g[k] = g[k] << 1 | b;
            }
        }
        z = (((g[1] - 1u) << 8) | *s++) + 1u;       /* the offset back */
        if (z > (uint32_t)(d - d0))
            return s;                               /* (a match before the start: nothing outside the section) */
        r = d - z;
        for (k = g[0] + 1u; k-- && d < d_end;)      /* (byte by byte: a match may overlap what it writes) */
            *d++ = *r++;
    }
#undef LZB_BIT
    return s;
}
