/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.44) Host test of the boot's LZB unpacker (firmware/src/boot_lzb.h: main.c fm1_cstart, gz.h): the RAM code and
 * the initialised data are LZB blocks in the app image (tools/build.py, tools/lzb.py pack; V10.6z5..V10.7.41: LZ4).
 *   - the build's own blocks (build/boot_lzb.bin; when there is a build): unpacked one after the other into the
 *     sizes of build/{ramtext,c1text,hottext,data}.bin they are byte for byte those sections, each block ending where
 *     the next starts and the last at the stream's end;
 *   - blocks made here (a greedy packer of this file's own: matches as long as they go, overlapping ones, offsets up
 *     to 65536, lengths past 2^16): random, runs, text-like, empty and tiny inputs, 400 of them;
 *   - broken input (a match before the start, a gamma code too long) writes nothing outside its section.
 *   cc -O1 -Ifirmware/src tests/boot_lzb_test.c -o build/boot_lzb_test && build/boot_lzb_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../firmware/src/boot_lzb.h"

static int fails;
static void check(const char *what, int ok)
{
    printf("boot_lzb: %-92s %s\n", what, ok ? "ok" : "FAIL");
    fails += !ok;
}

/* the writer: bits MSB first in a byte taken when the first of them is written (tools/lzb.py pack's) */
static uint8_t *wo, *wc;
static uint32_t wn;
static void wbit(uint32_t b)
{
    if (!wn) { wc = wo++; *wc = 0; wn = 8; }
    wn--;
    *wc |= (uint8_t)(b << wn);
}
static void wgamma(uint32_t v)
{
    int nb = 31, k;
    while (!(v >> nb)) nb--;
    for (k = 0; k < nb; k++) wbit(0);
    for (k = nb; k >= 0; k--) wbit(v >> k & 1u);
}
/* greedy, the last position of each 2-byte pair */
static uint32_t pack(const uint8_t *s, uint32_t n, uint8_t *o0)
{
    static int32_t last[65536];
    uint32_t i = 0;
    wo = o0;
    wn = 0;
    memset(last, -1, sizeof last);
    while (i < n) {
        int32_t c = i + 1u < n ? last[s[i] | s[i + 1] << 8] : -1;
        if (i + 1u < n) last[s[i] | s[i + 1] << 8] = (int32_t)i;
        if (c >= 0 && i - (uint32_t)c <= 65536u) {
            uint32_t m = 0, off = i - (uint32_t)c;
            while (i + m < n && s[c + m] == s[i + m]) m++;
            if (m >= 2u) {
                wbit(1);
                wgamma(m - 1u);
                wgamma(((off - 1u) >> 8) + 1u);
                *wo++ = (uint8_t)(off - 1u);
                i += m;
                continue;
            }
        }
        wbit(0);
        *wo++ = s[i++];
    }
    return (uint32_t)(wo - o0);
}

static uint8_t *slurp(const char *path, long *n)
{
    FILE *f = fopen(path, "rb");
    uint8_t *b;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = malloc((size_t)*n + 1u);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) { fclose(f); free(b); return NULL; }
    fclose(f);
    return b;
}

int main(void)
{
    static uint8_t src[140000], blk[180000], out[140000 + 64];
    uint32_t r = 12345u, t, i;
    {   /* the build's own */
        static const char *const SECT[4] = {"build/ramtext.bin", "build/c1text.bin", "build/hottext.bin", "build/data.bin"};
        long sn, n[4];
        uint8_t *stream = slurp("build/boot_lzb.bin", &sn), *sec[4];
        if (!stream) {
            printf("boot_lzb: (no build/boot_lzb.bin: the build's blocks not checked)\n");
        } else {
            const uint8_t *z = stream;
            int same = 1;
            uint32_t total = 0;
            for (i = 0; i < 4u; i++) {
                sec[i] = slurp(SECT[i], &n[i]);
                if (!sec[i]) n[i] = 0;
                if (n[i]) {
                    uint8_t *d = malloc((size_t)n[i] + 16u);
                    memset(d, 0xA5, (size_t)n[i] + 16u);
                    z = boot_lzb(z, d, d + n[i]);
                    same &= !memcmp(d, sec[i], (size_t)n[i]);
                    for (t = 0; t < 16u; t++) same &= d[n[i] + t] == 0xA5;   /* nothing past the section */
                    free(d);
                }
                total += (uint32_t)n[i];
            }
            check("the build's blocks: the RAM code and .data back byte for byte, nothing past a section", same);
            check("the build's blocks: each ends where the next starts, the last at the stream's end", z == stream + sn);
            printf("boot_lzb: (the build: %u B of sections in %ld B)\n", total, sn);
            for (i = 0; i < 4u; i++)
                free(sec[i]);
            free(stream);
        }
    }
    {
        int ok = 1;
        for (t = 0; t < 400u && ok; t++) {
            uint32_t n = t < 20u ? t : (t * 7919u) % 130000u + 1u, kind = t % 4u, nb;
            for (i = 0; i < n; i++) {
                r ^= r << 13; r ^= r >> 17; r ^= r << 5;
                src[i] = kind == 0 ? (uint8_t)r                                   /* noise */
                       : kind == 1 ? (uint8_t)(i / (1u + r % 3u) % 7u)            /* runs */
                       : kind == 2 ? (uint8_t)"the keys of the FM-1 "[(i * 3u + (r & 1u)) % 21u]
                                   : (uint8_t)((r & 7u) ? 0u : r >> 8);           /* mostly zeros: long matches */
            }
            nb = n ? pack(src, n, blk) : 0u;               /* (an empty section: no block, build.py) */
            memset(out, 0x5A, sizeof out);
            if (boot_lzb(blk, out, out + n) != blk + nb || memcmp(out, src, n) || out[n] != 0x5A)
                ok = 0;
        }
        check("400 blocks made here (noise, runs, text, long zero runs; 0 .. 130000 B) back exactly", ok);
    }
    {   /* 300 literals, a match of 70000 back 1 (an overlapping run, its length past 2^16), one back 65536 */
        uint32_t n = 300u + 70000u + 65536u + 3u;
        for (i = 0; i < 300u; i++) src[i] = (uint8_t)(i * 7u);
        for (; i < 70300u; i++) src[i] = src[299];
        for (; i < 70300u + 65536u; i++) src[i] = (uint8_t)(i * 13u >> 3);
        src[i] = src[i - 65536u]; src[i + 1u] = src[i + 1u - 65536u]; src[i + 2u] = 9;
        wo = blk;
        wn = 0;
        for (i = 0; i < 300u; i++) { wbit(0); *wo++ = src[i]; }
        wbit(1); wgamma(70000u - 1u); wgamma(1u); *wo++ = 0;
        for (i = 70300u; i < 70300u + 65536u; i++) { wbit(0); *wo++ = src[i]; }
        wbit(1); wgamma(1u); wgamma(256u); *wo++ = 255;   /* (off 65536: hi 255, lo 255) */
        wbit(0); *wo++ = 9;
        memset(out, 0, sizeof out);
        check("an overlapping match past 2^16 (a run), an offset of 65536, the last literal",
              boot_lzb(blk, out, out + n) == wo && !memcmp(out, src, n));
    }
    {   /* broken: a match 9 back after 1 byte; a gamma code of 20 zeros */
        static const uint8_t bad1[] = {0x70, 'a', 8};             /* 0 a literal, 1 a match, gamma 1 (length 2), gamma 1 */
        static const uint8_t bad2[] = {0x40, 'a', 0, 0, 0, 0};    /* 0 a literal, 1 a match, then only zero bits */
        memset(out, 0x77, 64);
        boot_lzb(bad1, out + 8, out + 16);
        boot_lzb(bad2, out + 24, out + 32);
        check("broken blocks: nothing written outside their section",
              out[7] == 0x77 && out[16] == 0x77 && out[23] == 0x77 && out[32] == 0x77 && out[8] == 'a' && out[24] == 'a' &&
              out[9] == 0x77 && out[25] == 0x77);
    }
    printf("boot_lzb: %s\n", fails ? "FAILED" : "all ok");
    return fails != 0;
}
