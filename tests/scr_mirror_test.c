/* SPDX-License-Identifier: GPL-3.0-only */
/* (SMK-37) SCREEN MIRROR (firmware/src/scr_mirror.c): fills and blits of every kind, drawn on a 240 x 240 picture as the
 * panel draws them and sent as the RECT frames, decoded as web/mirror.html decodes them: the two pictures the same, every
 * frame a well-formed SysEx (7-bit inside, F0 .. F7, at most SCRM_MAX + 24 bytes); MIRROR on / keep alive / off; a host
 * that stops reading turns it off; nothing sent while off.
 *   cc -O2 -o build/scr_mirror_test tests/scr_mirror_test.c && build/scr_mirror_test */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ED_HDR0 0x7D
#define ED_HDR1 0x46
#define ED_HDR2 0x4C
static struct { uint8_t config; } usb = {1};
static struct { uint8_t force; } ui;
static uint32_t fm1_ms;
static int fails, sent_frames, wire_fail;
static uint8_t ed_reply[32];
static uint32_t ed_rn;
static void ed_b(uint32_t v) { ed_reply[ed_rn++] = (uint8_t)(v & 0x7Fu); }

static uint16_t pic[240][240], ref[240][240];
static uint32_t max_frame;

/* the page's decoder (web/mirror.html), on what the device sends */
static void decode(const uint8_t *f, uint32_t n)
{
    uint32_t i, x, y, w, h, o, prev = 0, have = 0, cache[64];
    if (n < 18u || f[0] != 0xF0 || f[n - 1] != 0xF7 || f[1] != 0x7D || f[2] != 0x46 || f[3] != 0x4C || f[4] != 81) {
        fails++;
        printf("FAIL a frame not a RECT\n");
        return;
    }
    for (i = 1; i + 1 < n; i++)
        if (f[i] & 0x80u) { fails++; printf("FAIL a byte over 7 bits\n"); return; }
    x = f[5] | f[6] << 7; y = f[7] | f[8] << 7; w = f[9] | f[10] << 7; h = f[11] | f[12] << 7;
    o = f[13] | f[14] << 7 | (uint32_t)f[15] << 14;
    memset(cache, 0, sizeof cache);
#define PUT(c) do { if (o >= w * h) { fails++; printf("FAIL past the window\n"); return; } \
                    pic[y + o / w][x + o % w] = (uint16_t)(c); o++; } while (0)
    for (i = 16; i + 1 < n;) {
        uint32_t t = f[i++], k;
        if (t < 0x40u) { prev = cache[t]; have = 1; PUT(prev); }
        else if (t < 0x60u) { if (!have) { fails++; return; } for (k = t - 0x3Fu; k; k--) PUT(prev); }
        else if (t == 0x60u) { k = f[i] | f[i + 1] << 7; i += 2; if (!have) { fails++; return; } for (; k; k--) PUT(prev); }
        else if (t == 0x61u) {
            uint32_t c = f[i] | f[i + 1] << 7 | (uint32_t)f[i + 2] << 14;
            i += 3;
            cache[(c ^ (c >> 5) ^ (c >> 11)) & 63u] = c;
            prev = c; have = 1; PUT(c);
        } else { fails++; printf("FAIL token %02X\n", t); return; }
    }
}
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    if (wire_fail)
        return -1;
    if (n > max_frame)
        max_frame = n;
    sent_frames++;
    decode(p, n);
    return 0;
}
#define SCR_MIRROR 1
#include "../firmware/src/scr_mirror.c"

static void check(const char *what, int ok)
{
    printf("%-100s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}
static uint32_t rnd(void) { static uint32_t s = 12345; s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }

/* as lcd.c: the panel's picture, and the mirror */
static void fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) ref[y + j][x + i] = c;
    scrm_fill(x, y, w, h, c);
}
static void blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride)
{
    uint32_t i, j;
    for (j = 0; j < h; j++) for (i = 0; i < w; i++) {
        uint16_t v = px[j * stride + i];
        ref[y + j][x + i] = (uint16_t)((v >> 8) | (v << 8));
    }
    scrm_blit(x, y, w, h, px, stride);
}
static uint16_t sw(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

int main(void)
{
    static uint16_t buf[240 * 240];
    uint8_t on1[] = {1}, on3[] = {3}, off[] = {0};
    uint32_t i, k;
    /* off: nothing */
    fill(0, 0, 240, 240, 0x1234);
    check("off: nothing sent", sent_frames == 0);
    /* MIRROR 3: on, the whole screen asked for, the reply */
    ed_rn = 0;
    check("MIRROR 3: handled, on, a full redraw asked for", ed_mirror_handle(80, on3, 1) && scrm.on && ui.force);
    check("the reply: on, 240 x 240", ed_rn == 5 && ed_reply[0] == 1 && (ed_reply[1] | ed_reply[2] << 7) == 240 &&
          (ed_reply[3] | ed_reply[4] << 7) == 240);
    check("another command is not the mirror's", !ed_mirror_handle(81, on1, 1) && !ed_mirror_handle(1, on1, 0));
    memset(pic, 0, sizeof pic);
    memset(ref, 0, sizeof ref);
    /* a full screen fill, then a page's worth of everything */
    fill(0, 0, 240, 240, 0x2104);
    check("a whole-screen fill: one small frame", sent_frames == 1 && max_frame < 40u);
    for (k = 0; k < 400; k++) {
        uint32_t x = rnd() % 240, y = rnd() % 240, w = 1 + rnd() % (240 - x), h = 1 + rnd() % (240 - y), kind = rnd() % 4;
        if (kind == 0)
            fill(x, y, w, h, (uint16_t)rnd());
        else {
            uint32_t stride = kind == 3 ? w + rnd() % 30 : w;
            uint16_t pal[20];
            for (i = 0; i < 20; i++) pal[i] = (uint16_t)rnd();
            for (i = 0; i < stride * h; i++) {
                uint32_t r = rnd() % 100;     /* text-like: runs of the background, a few ramp colours, some noise */
                buf[i] = sw(r < 60 ? pal[0] : r < 95 ? pal[1 + rnd() % 19] : (uint16_t)rnd());
            }
            blit(x, y, w, h, buf, stride);
        }
    }
    check("400 fills and blits (plain, strided, 1 px to the whole screen): the mirror's picture = the panel's",
          !memcmp(pic, ref, sizeof pic));
    check("every frame within SCRM_MAX + 24 bytes", max_frame <= SCRM_MAX + 24u);
    /* pure noise: the worst case, still exact */
    for (i = 0; i < 240u * 240u; i++) buf[i] = (uint16_t)rnd();
    blit(0, 0, 240, 240, buf, 240);
    check("a whole screen of noise (every pixel a new colour): exact, frames within the limit",
          !memcmp(pic, ref, sizeof pic) && max_frame <= SCRM_MAX + 24u);
    /* a typical page: flat background, cards, text */
    {
        uint32_t f0 = sent_frames, b0 = 0;
        (void)b0;
        fill(0, 0, 240, 240, 0x18C3);
        for (k = 0; k < 8; k++) fill(4 + (k % 4) * 59, 40 + (k / 4) * 70, 55, 64, 0x31A6);
        for (k = 0; k < 30; k++) {
            uint32_t w = 40, h = 12;
            for (i = 0; i < w * h; i++) {
                uint32_t r = rnd() % 100;
                buf[i] = sw(r < 70 ? 0x31A6 : r < 85 ? 0xFFFF : (uint16_t)(0x31A6 + (rnd() % 16) * 0x0841));
            }
            blit(rnd() % 200, rnd() % 228, w, h, buf, w);
        }
        printf("  a page: %d frames\n", sent_frames - (int)f0);
        check("a page (background, 8 cards, 30 words): exact", !memcmp(pic, ref, sizeof pic));
    }
    /* keep alive / timeout / off / a host that stops reading */
    fm1_ms += 2000;
    ed_mirror_handle(80, on1, 1);
    fm1_ms += 2900;
    k = sent_frames;
    fill(0, 0, 10, 10, 1);
    check("MIRROR 1 again 2 s later: still on 2.9 s after it", scrm.on && sent_frames == (int)k + 1);
    fm1_ms += 200;
    fill(0, 0, 10, 10, 2);
    check("3 s with no MIRROR: off, nothing sent", !scrm.on && sent_frames == (int)k + 1);
    ui.force = 0;
    ed_mirror_handle(80, on1, 1);
    check("MIRROR 1 when off: on again, the whole screen asked for", scrm.on && ui.force);
    ui.force = 0;
    ed_mirror_handle(80, on1, 1);
    check("MIRROR 1 while on: no redraw asked", scrm.on && !ui.force);
    wire_fail = 1;
    fill(0, 0, 10, 10, 3);
    check("a host that stops reading (the send fails): off", !scrm.on);
    wire_fail = 0;
    ed_mirror_handle(80, on1, 1);
    usb.config = 0;
    fill(0, 0, 10, 10, 4);
    check("USB gone: off", !scrm.on);
    usb.config = 1;
    ed_mirror_handle(80, on1, 1);
    ed_mirror_handle(80, off, 1);
    k = sent_frames;
    fill(0, 0, 10, 10, 5);
    check("MIRROR 0: off, nothing sent", !scrm.on && sent_frames == (int)k);
    printf(fails ? "scr_mirror: %d FAILED\n" : "scr_mirror: all ok\n", fails);
    return fails != 0;
}
