/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (SMK-37) SCREEN MIRROR: a phone or a computer shows the screen, bigger (web/mirror.html, Chrome with Web MIDI; on
 * Android a USB OTG cable). Nothing is kept of the picture here (there is no frame buffer): what lcd.c sends to the panel
 * is sent over USB-MIDI as well, as SysEx, while a page asks for it.
 *
 *   page -> device  F0 7D 46 4C 50 m F7         MIRROR (editor protocol cmd 80): m 1 on (again every second: it stays
 *                                               on 3 s after the last one), 3 on and the whole screen drawn again, 0 off
 *   device -> page  F0 7D 46 4C 50 on w(2) h(2) F7   the reply: on or not, the screen's size (14 bit, LSB first)
 *   device -> page  F0 7D 46 4C 51 x(2) y(2) w(2) h(2) o(3) tokens.. F7
 *                   RECT (cmd 81): the pixels of the panel's window x, y, w x h, from pixel o (21 bit) on, row by row
 *                   (a big window comes in several RECTs). Tokens, each frame starting afresh (no colour yet, the
 *                   cache empty):
 *                     0x00..0x3F  a pixel of the colour in cache slot t
 *                     0x40..0x5F  the last colour again, t - 0x3F times (1..32)
 *                     0x60 n(2)   the last colour again, n times (14 bit)
 *                     0x61 c(3)   a pixel of colour c (RGB565, 16 bit in 7 + 7 + 2), put in cache slot SCRM_SLOT(c)
 *
 * The frames go out through ota_wire_send as the editor's replies: the drawing waits for USB while a frame does not fit
 * (a whole page ~0.1-0.3 s; the keys and the sequencer run in the audio ISR, not here). A host that stops reading
 * (200 ms) turns it off; the page turns it on again. */
#define SCRM_ON 80u
#define SCRM_RECT 81u
#define SCRM_KEEP_MS 3000u
#define SCRM_MAX 400u                  /* token bytes in a frame before it is sent */
#define SCRM_NONE 0xFFFFFFFFu          /* no colour yet (a colour is 16 bit) */
#define SCRM_SLOT(c) (((c) ^ ((c) >> 5) ^ ((c) >> 11)) & 63u)

static struct {
    uint8_t on;
    uint32_t last_ms;                  /* the page's last MIRROR */
    uint32_t x, y, w, h, idx;          /* the window; the pixels sent of it */
    uint32_t prev, run;                /* the last colour (SCRM_NONE: none), its repeats not yet sent */
    uint32_t n;                        /* bytes in out */
} scrm;
static uint32_t scrm_cache[64];
static uint8_t scrm_out[SCRM_MAX + 24u];

static void scrm_b(uint32_t v) { scrm_out[scrm.n++] = (uint8_t)(v & 0x7Fu); }
static void scrm_b2(uint32_t v) { scrm_b(v); scrm_b(v >> 7); }

static void scrm_frame(void)           /* a RECT from pixel idx: its header, colour and cache afresh */
{
    uint32_t i;
    scrm.n = 0;
    scrm_out[scrm.n++] = 0xF0;
    scrm_b(ED_HDR0); scrm_b(ED_HDR1); scrm_b(ED_HDR2); scrm_b(SCRM_RECT);
    scrm_b2(scrm.x); scrm_b2(scrm.y); scrm_b2(scrm.w); scrm_b2(scrm.h);
    scrm_b2(scrm.idx); scrm_b(scrm.idx >> 14);
    scrm.prev = SCRM_NONE;
    for (i = 0; i < 64u; i++)
        scrm_cache[i] = SCRM_NONE;
}
static void scrm_runs(void)            /* the repeats held back */
{
    while (scrm.run) {
        uint32_t k = scrm.run > 16383u ? 16383u : scrm.run;
        if (k <= 32u) scrm_b(0x3Fu + k);
        else { scrm_b(0x60); scrm_b2(k); }
        scrm.idx += k;
        scrm.run -= k;
    }
}
static void scrm_send(void)            /* the frame out; a host not reading: off */
{
    scrm_out[scrm.n++] = 0xF7;
    if (ota_wire_send(scrm_out, scrm.n) < 0)
        scrm.on = 0;
}
static int scrm_begin(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    if (!scrm.on)
        return 0;
    if (fm1_ms - scrm.last_ms > SCRM_KEEP_MS || !usb.config) {
        scrm.on = 0;
        return 0;
    }
    scrm.x = x, scrm.y = y, scrm.w = w, scrm.h = h, scrm.idx = 0, scrm.run = 0;
    scrm_frame();
    return 1;
}
static void scrm_px(uint32_t c)
{
    if (c == scrm.prev) {
        scrm.run++;
        return;
    }
    scrm_runs();
    if (scrm.n >= SCRM_MAX) {          /* full: out, the next frame from here */
        scrm_send();
        if (!scrm.on)
            return;
        scrm_frame();
    }
    {
        uint32_t s = SCRM_SLOT(c);
        if (scrm_cache[s] == c)
            scrm_b(s);
        else {
            scrm_cache[s] = c;
            scrm_b(0x61); scrm_b(c); scrm_b(c >> 7); scrm_b(c >> 14);
        }
    }
    scrm.prev = c;
    scrm.idx++;
}
static void scrm_end(void)
{
    if (!scrm.on)
        return;
    scrm_runs();
    scrm_send();
}

/* lcd.c's three ways to the panel */
static void scrm_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    if (!scrm_begin(x, y, w, h))
        return;
    scrm_px(c);
    scrm.run = w * h - 1u;
    scrm_end();
}
static void scrm_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride)
{
    uint32_t r, i;
    if (!scrm_begin(x, y, w, h))
        return;
    for (r = 0; r < h && scrm.on; r++)
        for (i = 0; i < w && scrm.on; i++) {
            uint32_t v = px[r * stride + i];
            scrm_px(((v >> 8) | (v << 8)) & 0xFFFFu);   /* (stored byte-swapped for the panel) */
        }
    scrm_end();
}

/* MIRROR (80) from the page; the reply as every editor command's */
static void ed_b(uint32_t v);
static int ed_mirror_handle(uint32_t cmd, const uint8_t *a, uint32_t n)
{
    if (cmd != SCRM_ON)
        return 0;
    if (n == 1u && a[0]) {
        if (!scrm.on || (a[0] & 2u))
            ui.force = 1;                  /* the whole screen once */
        scrm.on = 1;
        scrm.last_ms = fm1_ms;
    } else if (n == 1u) {
        scrm.on = 0;
    }
    ed_b(scrm.on);
    ed_b(240); ed_b(240 >> 7);
    ed_b(240); ed_b(240 >> 7);
    return 1;
}
