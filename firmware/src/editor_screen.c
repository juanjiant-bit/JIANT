/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The screen mirror (EDITOR_PROTOCOL.md "Screen mirror", 78 SCREEN / 79 SCREEN_DATA): the editor shows the FM-1's
 * screen, as big as the computer's or the phone's. The firmware keeps no frame (gfx.c draws strips straight to the
 * panel), so lcd.c hands every fill and strip it sends to scr_tap, which sends it on as SysEx, compressed: a run of the
 * colour before, a colour seen lately by its hash, or the colour itself (a fill: one colour and its runs).
 * Each SCREEN_DATA frame stands alone (its own hash table): a lost frame costs its pixels until they are drawn again.
 * SCREEN 1 (on) and 3 (refresh) redraw the whole screen (ui.force), so the editor starts from a complete picture.
 * It stops by itself 4 s after the editor's last request (ed_service; the editor sends SCREEN 2 every second or so),
 * or when a frame could not go out (nobody reads the USB port): the UI never waits for an editor that went away.
 * While it runs the UI's frames take as long as the USB takes to send them (a page that moves all the time draws
 * fewer frames a second); the sound does not wait (it runs in its interrupt).
 * Small on purpose (JIANT BYTE's image is nearly full), and its state is not static and lives after the pool (app.ld):
 * JIANT BYTE pins its sound's code, and the compiler merges the static variables functions use together, so new
 * functions touching several of the existing ones would move it. scr_tap touches only its own. */
enum { ED_SCREEN = 78, ED_SCREEN_DATA };
#ifndef FELUCCA_SCREEN_MIRROR
#define FELUCCA_SCREEN_MIRROR 1                /* (lcd.c's default; 0: no mirror at all, SCREEN gets no reply as before) */
#endif
#if !FELUCCA_SCREEN_MIRROR
static int ed_screen_handle(uint32_t cmd, const uint8_t *a, uint32_t na) { (void)cmd; (void)a; (void)na; return 0; }
#else
#define SCR_W 240u
#define SCR_H 240u
#define SCR_CAP 160u                           /* the token bytes of one frame (+ 17 header bytes: a 0.3 KiB SysEx; 480 until V10.8.20's pool
                                                * left 8 KiB to spare and no more) */
#define SCR_IDLE_MS 4000u
#define SCR_RUN_MAX 1064u                      /* 40 short + 1024 long */
#ifndef SCR_HAS_LCD
#define SCR_HAS_LCD 0                          /* (a host build without lcd.c: ports/core; SCREEN answers rc 1) */
static uint8_t scr_on;
#endif
#if SCR_HAS_LCD
struct scr_state {
    uint8_t out[17u + SCR_CAP + 8u];
    uint32_t n;                                /* bytes in out[] */
    uint16_t tab[64];
    uint32_t prev, run;
    uint32_t frames, lost;                     /* (the host test's counters) */
};
struct scr_state scr __attribute__((section(".pool.mirror")));

static void mir_put(uint32_t b) { scr.out[scr.n++] = (uint8_t)(b & 0x7Fu); }
static void mir_flush_run(void)
{
    uint32_t r = scr.run;
    if (r > 40u) {                             /* 0x68..0x6F, then 7 bits: 41..1064 */
        mir_put(0x68u | (r - 41u) >> 7);
        mir_put(r - 41u);
    } else if (r)
        mir_put(0x40u + r - 1u);               /* 0x40..0x67: 1..40 */
    scr.run = 0;
}
/* the frame so far out (if it holds pixels), then a new one for the box from pixel k on */
static void mir_frame(const uint32_t *box, uint32_t k)
{
    uint32_t i;
    mir_flush_run();
    if (scr.n > 17u) {
        scr.out[scr.n++] = 0xF7;
        if (ota_wire_send(scr.out, scr.n)) {
            scr_on = 0;                        /* nobody reads: stop (the editor asks again) */
            scr.lost++;
        } else
            scr.frames++;
    }
    scr.out[0] = 0xF0; scr.out[1] = ED_HDR0; scr.out[2] = ED_HDR1; scr.out[3] = ED_HDR2; scr.out[4] = ED_SCREEN_DATA;
    scr.n = 5;
    mir_put(0);                                /* type 0: pixels (the only type so far) */
    for (i = 0; i < 4u; i++) {                 /* x y w h, 2 x 7 bits each */
        mir_put(box[i]);
        mir_put(box[i] >> 7);
    }
    mir_put(k); mir_put(k >> 7); mir_put(k >> 14);
    for (i = 0; i < 64u; i++)
        scr.tab[i] = 0;
    scr.prev = 0;
}

/* lcd.c: a fill (px 0: w x h of colour fill, as lcd_fill takes it) or a strip (w x h of px, a row every stride
 * pixels, each pixel byte-swapped as the panel takes it) about to go to the panel at x, y */
static void scr_tap(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint32_t fill)
{
    uint32_t i = 0, k = 0, c, hh, box[4];
    if (!scr_on || !w || !h || x + w > SCR_W || y + h > SCR_H)
        return;                                /* (lcd.c clips before; nothing past the panel) */
    box[0] = x; box[1] = y; box[2] = w; box[3] = h;
    scr.n = 0;
    mir_frame(box, 0);
    for (; k < w * h && scr_on; k++) {
        c = px ? (uint16_t)(px[i] >> 8 | px[i] << 8) : fill & 0xFFFFu;
        if (++i == w) {                        /* (the next row) */
            i = 0;
            px = px ? px + stride : px;
        }
        if (scr.n + 5u > 17u + SCR_CAP)
            mir_frame(box, k);                 /* full: send, the rest from pixel k in a new frame */
        if (c == scr.prev) {
            if (++scr.run == SCR_RUN_MAX)
                mir_flush_run();
            continue;
        }
        mir_flush_run();
        hh = ((c >> 11) * 3u + ((c >> 5) & 63u) * 5u + (c & 31u) * 7u) & 63u;
        if (scr.tab[hh] == c)
            mir_put(hh);                       /* 0x00..0x3F: the colour at that hash */
        else {
            mir_put(0x70u | c >> 12);          /* 0x70..0x7F then 7 + 5 bits: a colour in full, into the table */
            mir_put(c >> 5);
            mir_put(c & 0x1Fu);
            scr.tab[hh] = (uint16_t)c;
        }
        scr.prev = c;
    }
    if (scr_on)
        mir_frame(box, 0);                     /* (the last one out; the new header is never sent) */
}
#endif

/* SCREEN (78): op 0 query, 1 on (and the whole screen again), 2 keep it on, 3 the whole screen again, 4 off.
 * Reply: op, rc (0 ok, 1 an unknown op or no screen to mirror), on, width, height (2 x 7 bits each, LSB first).
 * (Built and sent here, not through ed_b / ed_send: no more uses of their statics, as above) */
static int ed_screen_handle(uint32_t cmd, const uint8_t *a, uint32_t na)
{
    uint32_t op = na ? a[0] : 0u, bad = na > 1u || op > 4u || !SCR_HAS_LCD;
    if (cmd != ED_SCREEN)
        return 0;
#if SCR_HAS_LCD
    if (!bad && (op == 1u || (op == 3u && scr_on))) {
        ui.force = 1;                          /* the next frame redraws everything: the editor's first picture */
        scr_on = 1;
    } else if (!bad && op == 4u)
        scr_on = 0;                            /* (op 2: ed_service's last_ms, any request, keeps it on) */
    scr.out[0] = 0xF0; scr.out[1] = ED_HDR0; scr.out[2] = ED_HDR1; scr.out[3] = ED_HDR2; scr.out[4] = ED_SCREEN;
    scr.out[5] = (uint8_t)(op & 0x7Fu); scr.out[6] = (uint8_t)bad; scr.out[7] = scr_on;
    scr.out[8] = SCR_W & 0x7Fu; scr.out[9] = SCR_W >> 7; scr.out[10] = SCR_H & 0x7Fu; scr.out[11] = SCR_H >> 7;
    scr.out[12] = 0xF7;                        /* (in the mirror's buffer: no constant table, no more uses of ed_out) */
    ota_wire_send(scr.out, 13u);
#else
    ed_b(op); ed_b(bad); ed_b(scr_on); ed_b(SCR_W & 0x7Fu); ed_b(SCR_W >> 7); ed_b(SCR_H & 0x7Fu); ed_b(SCR_H >> 7);
    ed_send();
#endif
    return 1;
}
#endif                                         /* FELUCCA_SCREEN_MIRROR */
