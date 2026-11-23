/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The screen mirror (EDITOR_PROTOCOL.md "Screen mirror", 78 SCREEN / 79 SCREEN_DATA): the editor shows the FM-1's
 * screen, as big as the computer's or the phone's. The firmware keeps no frame (gfx.c draws strips straight to the
 * panel), so lcd.c hands every fill and strip it sends to scr_tap, which sends it on as SysEx: a fill as its colour,
 * a strip's pixels compressed (a run of the colour before, a colour seen lately by its hash, or the colour itself).
 * Each SCREEN_DATA frame stands alone (its own hash table): a lost frame costs its pixels until they are drawn again.
 * SCREEN 1 (on) and 3 (refresh) redraw the whole screen (ui.force), so the editor starts from a complete picture.
 * It stops by itself 4 s after the editor's last request (the editor sends SCREEN 2 every second or so), or when
 * a frame could not go out (nobody reads the USB port): the UI never waits for an editor that went away.
 * While it runs the UI's frames take as long as the USB takes to send them (a page that moves all the time draws
 * fewer frames a second); the sound does not wait (it runs in its interrupt). */
enum { ED_SCREEN = 78, ED_SCREEN_DATA };
#define SCR_W 240u
#define SCR_H 240u
#define SCR_CAP 480u                           /* the token bytes of one frame (+ 18 header bytes: a 0.5 KiB SysEx) */
#define SCR_IDLE_MS 4000u
#define SCR_RUN_MAX 1064u                      /* 40 short + 1024 long */
#ifndef SCR_HAS_LCD
#define SCR_HAS_LCD 0                          /* (a host build without lcd.c: ports/core; SCREEN answers rc 1) */
static uint8_t scr_on;
#endif
#if SCR_HAS_LCD
static struct {
    uint8_t out[24u + SCR_CAP + 4u];
    uint32_t n;                                /* bytes in out[] */
    uint16_t tab[64];
    uint16_t prev;
    uint32_t run;
    uint32_t frames, lost;                     /* (the host test's counters) */
} scr;

static uint32_t scr_hash(uint32_t c) { return ((c >> 11) * 3u + ((c >> 5) & 63u) * 5u + (c & 31u) * 7u) & 63u; }

static void scr_hdr(uint32_t type, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t start)
{
    uint32_t i;
    scr.out[0] = 0xF0; scr.out[1] = ED_HDR0; scr.out[2] = ED_HDR1; scr.out[3] = ED_HDR2; scr.out[4] = ED_SCREEN_DATA;
    scr.out[5] = (uint8_t)type;
    scr.out[6] = (uint8_t)(x & 0x7Fu); scr.out[7] = (uint8_t)(x >> 7);
    scr.out[8] = (uint8_t)(y & 0x7Fu); scr.out[9] = (uint8_t)(y >> 7);
    scr.out[10] = (uint8_t)(w & 0x7Fu); scr.out[11] = (uint8_t)(w >> 7);
    scr.out[12] = (uint8_t)(h & 0x7Fu); scr.out[13] = (uint8_t)(h >> 7);
    scr.out[14] = (uint8_t)(start & 0x7Fu); scr.out[15] = (uint8_t)((start >> 7) & 0x7Fu);
    scr.out[16] = (uint8_t)(start >> 14);
    scr.n = 17;
    for (i = 0; i < 64u; i++)
        scr.tab[i] = 0;
    scr.prev = 0;
    scr.run = 0;
}
static void scr_col(uint32_t c)                /* a colour in full: 0x70 | its top 4 bits, then 7, then 5 */
{
    scr.out[scr.n++] = (uint8_t)(0x70u | c >> 12);
    scr.out[scr.n++] = (uint8_t)((c >> 5) & 0x7Fu);
    scr.out[scr.n++] = (uint8_t)(c & 0x1Fu);
}
static void scr_flush_run(void)
{
    uint32_t r = scr.run;
    if (!r)
        return;
    if (r <= 40u)
        scr.out[scr.n++] = (uint8_t)(0x40u + r - 1u);             /* 0x40..0x67: 1..40 */
    else {
        r -= 41u;                                                 /* 0x68..0x6F, then 7 bits: 41..1064 */
        scr.out[scr.n++] = (uint8_t)(0x68u | r >> 7);
        scr.out[scr.n++] = (uint8_t)(r & 0x7Fu);
    }
    scr.run = 0;
}
static void scr_send(void)
{
    scr.out[scr.n++] = 0xF7;
    if (ota_wire_send(scr.out, scr.n)) {
        scr_on = 0;                            /* nobody reads: stop (the editor asks again) */
        scr.lost++;
    } else
        scr.frames++;
}

/* lcd.c: a fill (px 0: w x h of colour fill, as lcd_fill takes it) or a strip (w x h of px, a row every stride
 * pixels, each pixel byte-swapped as the panel takes it) about to go to the panel at x, y */
static void scr_tap(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint32_t fill)
{
    uint32_t i, j, k = 0;
    if (!scr_on || x >= SCR_W || y >= SCR_H || !w || !h)
        return;
    if (fm1_ms - ed_w.last_ms > SCR_IDLE_MS) {
        scr_on = 0;                            /* the editor went quiet */
        return;
    }
    if (x + w > SCR_W || y + h > SCR_H)
        return;                                /* (lcd.c clips before; nothing past the panel) */
    if (!px) {
        scr_hdr(1u, x, y, w, h, 0u);
        scr_col(fill & 0xFFFFu);
        scr_send();
        return;
    }
    scr_hdr(0u, x, y, w, h, 0u);
    for (j = 0; j < h && scr_on; j++) {
        const uint16_t *row = px + j * stride;
        for (i = 0; i < w; i++, k++) {
            uint32_t c = (uint32_t)(uint16_t)(row[i] >> 8 | row[i] << 8), hh;
            if (scr.n + 5u > 17u + SCR_CAP) {  /* full: send, the rest from pixel k in a new frame */
                scr_flush_run();
                scr_send();
                if (!scr_on)
                    return;
                scr_hdr(0u, x, y, w, h, k);
            }
            if (c == scr.prev) {
                if (++scr.run == SCR_RUN_MAX)
                    scr_flush_run();
                continue;
            }
            scr_flush_run();
            hh = scr_hash(c);
            if (scr.tab[hh] == c)
                scr.out[scr.n++] = (uint8_t)hh;   /* 0x00..0x3F: the colour at that hash */
            else {
                scr_col(c);
                scr.tab[hh] = (uint16_t)c;
            }
            scr.prev = (uint16_t)c;
        }
    }
    scr_flush_run();
    if (scr_on && scr.n > 17u)
        scr_send();
}

#endif

/* SCREEN (78): op 0 query, 1 on (and the whole screen again), 2 keep it on, 3 the whole screen again, 4 off.
 * Reply: op, rc (0 ok, 1 this device has no screen to mirror), on, width (2 x 7 bits), height (2 x 7 bits) */
static int ed_screen_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint32_t op;
    if (cmd != ED_SCREEN)
        return 0;
    op = na ? a[0] : 0u;
    if (na > 1u || op > 4u) {
        ed_b(op); ed_b(1); ed_b(scr_on);
    } else {
#if SCR_HAS_LCD
        if (op == 1u || (op == 3u && scr_on)) {
            ui.force = 1;                      /* the next frame redraws everything: the editor's first picture */
            scr_on = 1;
        } else if (op == 4u)
            scr_on = 0;                        /* (op 2: ed_service's last_ms, any request, keeps it on) */
#endif
        ed_b(op); ed_b(SCR_HAS_LCD ? 0u : 1u); ed_b(scr_on);
    }
    ed_b(SCR_W & 0x7Fu); ed_b(SCR_W >> 7); ed_b(SCR_H & 0x7Fu); ed_b(SCR_H >> 7);
    return 1;
}
