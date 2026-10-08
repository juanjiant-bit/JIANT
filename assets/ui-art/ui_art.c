/* SPDX-License-Identifier: GPL-3.0-only */
/* JIANT FM UI art: full-screen 240x240 illustrations, 16 colours each, raw-deflate compressed.
 *
 * Unity build: #include "ui_art.c" in felucca.c right after "gfx.c" (it uses cv_begin, cv_px, cv_blit).
 * Data: ui_art_data.h (tools/gen_ui_art.py). One 28.8 KB index buffer in .pool; drawing goes through
 * the canvas in two 240x120 strips (CV_MAX is 240x124), so lcd_blit always reads RAM.
 *
 *   ui_art_draw(UA_HOME)                 the whole screen
 *   ui_art_rect(UA_HOME, x, y, w, h)     part of it again (under a live value, before cv_text etc.)
 *
 * The last screen inflated stays in ua_idx: redrawing it or a part of it costs no inflate.
 */
#include "ui_art_data.h"

#define UA_STRIP 120u

static uint8_t ua_idx[UI_ART_W * UI_ART_H / 2] __attribute__((section(".pool")));
static int32_t ua_last = -1;

/* ---------------------------------------------------------- inflate --- */
/* A small raw-deflate (RFC 1951) decoder: stored, fixed and dynamic blocks, output to one flat buffer
 * (the whole image), so the window is the output itself. */
typedef struct {
    uint16_t count[16];
    uint16_t sym[288];
} ua_huff_t;

typedef struct {
    const uint8_t *in, *in_end;
    uint32_t bitbuf;
    int32_t bitcnt;
    uint8_t *out, *out_start, *out_end;
} ua_inf_t;

static ua_huff_t ua_lens, ua_dists;

static const uint16_t ua_lbase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
                                      35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
static const uint8_t ua_lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
static const uint16_t ua_dbase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769,
                                      1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
static const uint8_t ua_dext[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

static int32_t ua_bits(ua_inf_t *s, int32_t n)
{
    uint32_t v = s->bitbuf;
    while (s->bitcnt < n) {
        if (s->in >= s->in_end)
            return -1;
        v |= (uint32_t)*s->in++ << s->bitcnt;
        s->bitcnt += 8;
    }
    s->bitbuf = v >> n;
    s->bitcnt -= n;
    return (int32_t)(v & ((1u << n) - 1u));
}

static void ua_build(ua_huff_t *h, const uint8_t *len, int32_t n)
{
    uint16_t offs[16];
    int32_t i;
    for (i = 0; i < 16; i++)
        h->count[i] = 0;
    for (i = 0; i < n; i++)
        h->count[len[i]]++;
    h->count[0] = 0;
    offs[1] = 0;
    for (i = 1; i < 15; i++)
        offs[i + 1] = (uint16_t)(offs[i] + h->count[i]);
    for (i = 0; i < n; i++)
        if (len[i])
            h->sym[offs[len[i]]++] = (uint16_t)i;
}

static int32_t ua_decode(ua_inf_t *s, const ua_huff_t *h)
{
    int32_t code = 0, first = 0, index = 0, len, count, b;
    for (len = 1; len < 16; len++) {
        b = ua_bits(s, 1);
        if (b < 0)
            return -1;
        code |= b;
        count = h->count[len];
        if (code - count < first)
            return h->sym[index + (code - first)];
        index += count;
        first = (first + count) << 1;
        code <<= 1;
    }
    return -1;
}

static int32_t ua_codes(ua_inf_t *s)
{
    int32_t sym, len, dist, e;
    for (;;) {
        sym = ua_decode(s, &ua_lens);
        if (sym < 0)
            return -1;
        if (sym < 256) {
            if (s->out >= s->out_end)
                return -1;
            *s->out++ = (uint8_t)sym;
            continue;
        }
        if (sym == 256)
            return 0;
        sym -= 257;
        if (sym >= 29)
            return -1;
        e = ua_bits(s, ua_lext[sym]);
        if (e < 0)
            return -1;
        len = ua_lbase[sym] + e;
        sym = ua_decode(s, &ua_dists);
        if (sym < 0 || sym >= 30)
            return -1;
        e = ua_bits(s, ua_dext[sym]);
        if (e < 0)
            return -1;
        dist = ua_dbase[sym] + e;
        if (dist > s->out - s->out_start || len > s->out_end - s->out)
            return -1;
        while (len--) {
            *s->out = *(s->out - dist);
            s->out++;
        }
    }
}

static int32_t ua_stored(ua_inf_t *s)
{
    uint32_t len;
    s->bitbuf = 0;
    s->bitcnt = 0;
    if (s->in_end - s->in < 4)
        return -1;
    len = (uint32_t)s->in[0] | ((uint32_t)s->in[1] << 8);
    if ((len ^ 0xFFFFu) != ((uint32_t)s->in[2] | ((uint32_t)s->in[3] << 8)))
        return -1;
    s->in += 4;
    if (len > (uint32_t)(s->in_end - s->in) || len > (uint32_t)(s->out_end - s->out))
        return -1;
    while (len--)
        *s->out++ = *s->in++;
    return 0;
}

static int32_t ua_fixed(ua_inf_t *s)
{
    uint8_t len[288];
    int32_t i;
    /* rebuilt every time: a dynamic block before it may have replaced the tables (cheap) */
    for (i = 0; i < 144; i++) len[i] = 8;
    for (; i < 256; i++) len[i] = 9;
    for (; i < 280; i++) len[i] = 7;
    for (; i < 288; i++) len[i] = 8;
    ua_build(&ua_lens, len, 288);
    for (i = 0; i < 30; i++) len[i] = 5;
    ua_build(&ua_dists, len, 30);
    return ua_codes(s);
}

static int32_t ua_dynamic(ua_inf_t *s)
{
    static const uint8_t order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    uint8_t len[320];
    int32_t nlen, ndist, ncode, i, sym, rep, v;
    nlen = ua_bits(s, 5);
    ndist = ua_bits(s, 5);
    ncode = ua_bits(s, 4);
    if (nlen < 0 || ndist < 0 || ncode < 0)
        return -1;
    nlen += 257;
    ndist += 1;
    ncode += 4;
    if (nlen > 286 || ndist > 30)
        return -1;
    for (i = 0; i < 19; i++)
        len[order[i]] = 0;
    for (i = 0; i < ncode; i++) {
        v = ua_bits(s, 3);
        if (v < 0)
            return -1;
        len[order[i]] = (uint8_t)v;
    }
    ua_build(&ua_lens, len, 19);
    for (i = 0; i < nlen + ndist;) {
        sym = ua_decode(s, &ua_lens);
        if (sym < 0)
            return -1;
        if (sym < 16) {
            len[i++] = (uint8_t)sym;
            continue;
        }
        v = 0;
        if (sym == 16) {
            if (i == 0)
                return -1;
            v = len[i - 1];
            rep = ua_bits(s, 2);
            if (rep < 0) return -1;
            rep += 3;
        } else if (sym == 17) {
            rep = ua_bits(s, 3);
            if (rep < 0) return -1;
            rep += 3;
        } else {
            rep = ua_bits(s, 7);
            if (rep < 0) return -1;
            rep += 11;
        }
        if (i + rep > nlen + ndist)
            return -1;
        while (rep--)
            len[i++] = (uint8_t)v;
    }
    ua_build(&ua_lens, len, nlen);
    ua_build(&ua_dists, len + nlen, ndist);
    return ua_codes(s);
}

static int32_t ua_inflate(uint8_t *dst, uint32_t dlen, const uint8_t *src, uint32_t slen)
{
    ua_inf_t s;
    int32_t last, type, r;
    s.in = src;
    s.in_end = src + slen;
    s.bitbuf = 0;
    s.bitcnt = 0;
    s.out = s.out_start = dst;
    s.out_end = dst + dlen;
    do {
        last = ua_bits(&s, 1);
        type = ua_bits(&s, 2);
        if (last < 0 || type < 0)
            return -1;
        if (type == 0)
            r = ua_stored(&s);
        else if (type == 1)
            r = ua_fixed(&s);
        else if (type == 2)
            r = ua_dynamic(&s);
        else
            r = -1;
        if (r)
            return r;
    } while (!last);
    return s.out == s.out_end ? 0 : -1;
}

/* ------------------------------------------------------------- draw --- */
static int32_t ui_art_unpack(uint32_t id)
{
    if (id >= UI_ART_N)
        return -1;
    if (ua_last == (int32_t)id)
        return 0;
    ua_last = -1;
    if (ua_inflate(ua_idx, sizeof ua_idx, ui_art[id].z, ui_art[id].zlen))
        return -1;
    ua_last = (int32_t)id;
    return 0;
}

/* screen rect x, y, w, h (w * h <= CV_MAX) from the art, straight to the LCD */
static void ui_art_rect(uint32_t id, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    const uint16_t *pal;
    uint32_t r, c, i, b;
    uint16_t *px;
    if (ui_art_unpack(id) || x >= UI_ART_W || y >= UI_ART_H)
        return;
    if (x + w > UI_ART_W) w = UI_ART_W - x;
    if (y + h > UI_ART_H) h = UI_ART_H - y;
    if (w * h > CV_MAX) h = CV_MAX / w;
    pal = ui_art[id].pal;
    cv_begin(w, h, 0);
    px = cv_px;
    for (r = 0; r < h; r++) {
        i = (y + r) * UI_ART_W + x;
        for (c = 0; c < w; c++, i++) {
            b = ua_idx[i >> 1];
            *px++ = pal[(i & 1u) ? (b & 15u) : (b >> 4)];
        }
    }
    cv_blit(x, y);
}

static void ui_art_draw(uint32_t id)
{
    uint32_t y;
    for (y = 0; y < UI_ART_H; y += UA_STRIP)
        ui_art_rect(id, 0, y, UI_ART_W, UA_STRIP);
}
