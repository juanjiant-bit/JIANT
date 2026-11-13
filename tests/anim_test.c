/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (codex/anim) The animation under load, on the host (tests/ui_render.c's screens, firmware/src/ui*.c):
 *   time     the same moment draws the same picture whatever the frame rate: HOME's ECO, BEING, EDIT's beings and
 *            creatures, DRUM-X's kit, GEN's helix drawn at 66 frames a second and at 10 (frames dropped, long gaps):
 *            the panel identical at the same time (ui.c anim_at, the clock; ui_lzhome.c eco_swim / eco_at, the swim in
 *            fixed steps drawn between them)
 *   jumps    LITE (75 %: a frame each 100 ms) the beings move on, no step between two frames past what they swim in it,
 *            and no stop (until codex/anim this load froze them, then they jumped)
 *   lod      a change of detail glides over a few frames (ui.c ui_lod, ui_organic.c pl_pt, ui_graph.c scope_lod): each
 *            step of it changes the picture by a part of the whole change; at half detail fewer lines are drawn
 *   still    FREEZE (85 %, the last resort): nothing is drawn or sent; a still page in COLORS sends nothing (gfx.c
 *            cv_blit_cells on the panel)
 *   cost     a frame's cost (us on the host, only the ratios mean anything) and the LCD pixels sent, at full pace and at
 *            LITE (a frame each 100 ms, half the detail), HOME, BEING, EDIT per engine, GEN, the kit, the scope
 *   anim_test [OUTDIR]   OUTDIR: PPM frames of HOME and EDIT at 66 and at 10 frames a second (tests/anim_strip.py)
 * Built against a tree without codex/anim (no ANIM_TICK) it measures the cost only (the before). Exit 1 on a failure. */
#define UI_RENDER_NO_MAIN 1
#include "ui_render.c"
#include <unistd.h>
#include <sys/wait.h>

static int nbad;
static void ck(const char *what, int ok)
{
    printf("anim: %-86s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static uint32_t cdiff(uint16_t a, uint16_t b)          /* how far two (byte-swapped RGB565) pixels are: |dR| + |dG| + |dB| */
{
    int32_t d, r = 0;
    a = (uint16_t)(a >> 8 | a << 8); b = (uint16_t)(b >> 8 | b << 8);
    d = (a >> 11) - (b >> 11); r += (d < 0 ? -d : d) * 2;
    d = ((a >> 5) & 63) - ((b >> 5) & 63); r += d < 0 ? -d : d;
    d = (a & 31) - (b & 31); r += (d < 0 ? -d : d) * 2;
    return (uint32_t)r;
}
static int scr_id(const char *n)
{
    int s;
    for (s = 0; s < S_COUNT; s++)
        if (S_NAME[s] && !strcmp(S_NAME[s], n))
            return s;
    fprintf(stderr, "anim: no screen %s\n", n);
    exit(2);
}
static uint64_t sent;                                   /* LCD px sent (gfx.c GFX_HOOK_SENT: tests/ui_test.c ui_sent_px) */
static void at(uint32_t ms)                             /* a frame drawn at fm1_ms = ms */
{
    host_ticks += (ms - fm1_ms) * 1000u;
    fm1_ms = ms;
    ui_sent_px = 0;
    ui_draw();
    sent += ui_sent_px;
    nscr = npend = 0;
}
static void quiet(void)                                 /* (the sound held: only the time moves the pictures) */
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) trk[k].peak = 0;
    memset(scope_buf, 0, sizeof scope_buf);
}
static void prep(int s)
{
    setup(s); pal(1); draw(s);
    quiet();
}
#define PY0 ((uint32_t)graph_y())
#define PH ((uint32_t)graph_h())

#ifdef ANIM_TICK
/* the panel at time t0 + T, drawn every step ms from t0 (a child process: the same start for both) */
static void steady(void)                                /* (a steady sound: every part at one level, the scope one wave) */
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) trk[k].peak = 4000;
    for (k = 0; k < SCOPE_N; k++) scope_buf[k] = (int16_t)(sin(k * 0.19) * 7000);
}
static void run_to(int s, uint32_t step, uint32_t T, uint16_t *out)
{
    uint32_t t0 = fm1_ms, t;
    (void)s;
    for (t = step; t <= T; t += step) { steady(); at(t0 + t); }
    if (T % step) { steady(); at(t0 + T); }
    memcpy(out, host_screen, sizeof host_screen);
}
/* (the first NSTRICT pixel for pixel; the others keep a state of their own stepped once a frame drawn (WHEEL's rotors,
 * VOICE's articulation and nod, ANIMAL's flap and glide, NOISE's swarm, LOFI's bits, the kit's beings: their time is the
 * clock's, those steps the frames'; no room in the flash to step them by the ticks): their difference reported only) */
#define NSTRICT 7u
static void test_time(void)
{
    static const char *const SC[] = {"home", "home_macros", "v6_analog", "v6_phase", "gen", "edit_analog", "v6_fm6_op",
                                     "v6_wheel", "vx_head_s", "vx_bird", "v6_noise", "v6_lofi", "vx_kit_hits"};
    uint32_t k;
    for (k = 0; k < sizeof SC / sizeof SC[0]; k++) {
        static uint16_t a[240 * 240], b[240 * 240];
        int fd[2], st;
        uint32_t i, d = 0, moved = 0, y0;
        pid_t p;
        prep(scr_id(SC[k]));
        at((fm1_ms - anim_off) / 30u * 30u + 30u + anim_off);   /* (on an even tick: the panels move every other one) */
        run_to(0, 30u, 600u, b);                        /* (the sound steady first) */
        y0 = PY0;
        memcpy(b, host_screen, sizeof b);
        if (pipe(fd)) exit(2);
        fflush(stdout);
        p = fork();
        if (!p) {                                       /* 10 frames a second, then gaps of 230 ms */
            run_to(scr_id(SC[k]), 100u, 1500u, a);
            run_to(scr_id(SC[k]), 230u, 690u, a);
            if (write(fd[1], a, sizeof a) != (ssize_t)sizeof a) _exit(3);
            _exit(0);
        }
        run_to(scr_id(SC[k]), 15u, 1500u, a);           /* 66 a second */
        run_to(scr_id(SC[k]), 15u, 690u, a);
        for (i = y0 * 240u; i < (y0 + PH) * 240u; i++) moved += a[i] != b[i];
        memcpy(b, a, sizeof b);
        {
            size_t got = 0;
            while (got < sizeof a) { ssize_t r = read(fd[0], (char *)a + got, sizeof a - got); if (r <= 0) break; got += (size_t)r; }
        }
        waitpid(p, &st, 0);
        close(fd[0]); close(fd[1]);
        for (i = y0 * 240u; i < (y0 + PH) * 240u; i++) d += a[i] != b[i];
        {
            char w[160];
            snprintf(w, sizeof w, "time: %s at 2.19 s %s at 66 and at 10 frames/s (%u px differ; it moved %u px)", SC[k],
                     k < NSTRICT ? "the same" : "(info)", (unsigned)d, (unsigned)moved);
            if (k < NSTRICT) ck(w, d == 0u && moved > 0u);
            else printf("anim: %s\n", w);
        }
    }
}
/* LITE: a frame each 100 ms, half the detail: the beings move every frame, never more than they swim in it */
static void test_jumps(void)
{
    uint32_t f, c, t, mx = 0, still = 0, lite_lod, k;
    int32_t px[ECO_N], py[ECO_N];
    uint8_t pl0 = song.playing, h0;
    prep(scr_id("home"));
    song.playing = 1;
    for (c = 0; c < ECO_N; c++) trk[c].peak = 7000;   /* (loud: the fastest swim) */
    load_avg_q8 = 80u << 8; ui_hold_poll();
    h0 = ui_hold;
    t = fm1_ms;
    for (f = 0; f < 40u; f++) {
        t += UI_PACE();
        for (c = 0; c < ECO_N; c++) trk[c].peak = 7000;
        at(t);
        for (c = 0; c < ECO_N; c++) {
            int32_t x, y, dx, dy;
            eco_at(c, &x, &y);
            dx = (x >> 8) - px[c]; dy = (y >> 8) - py[c];
            if (f > 4u) {                               /* (after the detail's glide) */
                uint32_t m = (uint32_t)(dx < 0 ? -dx : dx) + (uint32_t)(dy < 0 ? -dy : dy);
                mx = m > mx ? m : mx;
                still += m == 0u;
            }
            px[c] = x >> 8; py[c] = y >> 8;
        }
    }
    lite_lod = ui_lod;
    /* the fastest swim: 140 + 110 Q8 px a tick at x7 speed, 100 ms = 6.7 ticks: ~46 px of x + y at most */
    ck("jumps: LITE (80 %) a frame each 100 ms, half the detail, the beings moving (not frozen)", h0 == 2u && UI_PACE() == 100u &&
       lite_lod == 256u && !ANIM_OFF());
    {
        char w[120];
        snprintf(w, sizeof w, "jumps: no being steps more than 48 px between two LITE frames (most %u), none stands (%u)",
                 (unsigned)mx, (unsigned)still);
        ck(w, mx > 0u && mx <= 48u && still < 6u);
    }
    load_avg_q8 = 90u << 8; ui_hold_poll();
    k = anim_at;
    at(t += 50u); at(t += 50u);
    ck("jumps: FREEZE (90 %) the last resort: the clock still", ui_hold == 3u && anim_at == k);
    {   /* nothing drawn when nothing changes: frozen, frames send nothing */
        uint64_t s0 = sent;
        for (f = 0; f < 10u; f++) at(t += 50u);
        ck("still: frozen, ten frames send no pixel", sent == s0);
    }
    load_avg_q8 = 70u << 8; ui_hold_poll();
    k = anim_at;
    at(t += 100u);
    ck("jumps: back to LITE, on from where it stood (100 ms on, no jump)", ui_hold == 2u && anim_at == k + 100u);
    load_avg_q8 = 0; song.playing = 0; ui_hold_poll();
    for (f = 0; f < 30u; f++) at(t + 200u + 15u * f);
    ck("lod: back to the whole detail once the load is gone", ui_lod == 0u && ui_hold == 0u);
    song.playing = pl0;
}
/* the detail glides: drawn at ui_lod 0, 32 .. 256 (the time still), each step a part of the whole change */
static void test_lod(void)
{
    static const char *const SC[] = {"home", "home_macros", "vx_bird", "vx_mammal", "vx_insect"};   /* (the beings' outlines,
                                                         * the creatures' curves: ANALOG's wave, the kit's blobs, NOISE's
                                                         * swarm are drawn as they were) */
    uint32_t k, l, y, x;
    for (k = 0; k < sizeof SC / sizeof SC[0]; k++) {
        static uint16_t p[9][240 * 140];
        uint32_t whole = 0, most = 0;
        uint64_t lines0 = 0, lines1 = 0;
        prep(scr_id(SC[k]));
        at(fm1_ms + 15u);
        for (l = 0; l <= 8u; l++) {
            ui_lod = (uint16_t)(l * 32u);
            px_visited = 0;
            ui.force = 1; draw_graph(); ui.force = 0; nscr = npend = 0;
            if (!l) lines0 = px_visited;
            if (l == 8u) lines1 = px_visited;
            for (y = 0; y < 140u && y < PH; y++)
                for (x = 0; x < 240u; x++) p[l][y * 240u + x] = host_screen[(PY0 + y) * 240u + x];
        }
        ui_lod = 0;
        for (y = 0; y < 240u * 140u; y++) whole += cdiff(p[0][y], p[8][y]);
        for (l = 1; l <= 8u; l++) {
            uint32_t d = 0;
            for (y = 0; y < 240u * 140u; y++) d += cdiff(p[l][y], p[l - 1u][y]);
            most = d > most ? d : most;
        }
        {   /* (how much the picture changes, colour by colour: a step of the glide a part of the whole. The last step is the
             * largest: the odd points' joints, drawn twice while they glide (a 1 px overlap of two antialiased lines),
             * go; the outline itself no longer moves there) */
            char w[160];
            snprintf(w, sizeof w, "lod: %s whole -> half changes %u, a step of the glide at most %u (%u %%)", SC[k], (unsigned)whole,
                     (unsigned)most, (unsigned)(most * 100u / (whole ? whole : 1u)));
            ck(w, whole > 0u && most < whole * 3u / 4u);
        }
        (void)lines0; (void)lines1;
    }
    {   /* the scope's strokes, whole and half */
        static uint16_t a[240 * 140], b[240 * 140];
        uint32_t i, d = 0;
        for (i = 0; i < SCOPE_N; i++) scope_buf[i] = (int16_t)(sin(i * 0.31) * 9000 + sin(i * 2.1) * 3000);
        cv_begin(240, 140, T_BG); ui_lod = 0; scope_at(10, 220, 70, 46, T_THEME); memcpy(a, cv_px, sizeof a);
        cv_begin(240, 140, T_BG); ui_lod = 256; scope_at(10, 220, 70, 46, T_THEME); memcpy(b, cv_px, sizeof b);
        ui_lod = 0;
        for (i = 0; i < 240u * 140u; i++) d += a[i] != b[i];
        ck("lod: the scope at half its samples differs (strokes merged) but stays the trace", d > 0u && d < 6000u);
    }
}
/* a still page in COLORS sends nothing; a panel drawn again with nothing changed in it sends only the cells mended in
 * turn (gfx.c cv_blit_cells: n / 128 + 1 a frame), not the panel (was all of it, 33600 px) */
static void test_still(void)
{
    uint64_t s0;
    uint32_t f;
    prep(scr_id("pattern"));
    song.playing = 0;
    at(fm1_ms + 15u); at(fm1_ms + 15u);
    s0 = sent;
    for (f = 0; f < 10u; f++) at(fm1_ms + 15u);
    ck("still: a still page (PATTERN), ten frames send no pixel", sent == s0);
    prep(scr_id("presets"));
    song.playing = 0;
    at(fm1_ms + 15u); at(fm1_ms + 15u);
    s0 = sent;
    for (f = 0; f < 10u; f++) at(fm1_ms + 15u);
    {
        char w[120];
        snprintf(w, sizeof w, "still: PRESETS drawn again unchanged: %llu px a frame sent (the panel 33600)",
                 (unsigned long long)((sent - s0) / 10u));
        ck(w, sent - s0 <= 10u * 400u);
    }
}
#endif

/* a frame's cost: n frames step ms apart (us a frame, LCD px a frame); lite 1: LITE (80 %), 2: SLOW (65 %) */
static void cost(const char *n, uint32_t step, int lite)
{
    uint32_t i, N = 300u, t;
    clock_t c0;
    double us;
    prep(scr_id(n));
#ifdef ANIM_TICK
    if (lite) { song.playing = 1; load_avg_q8 = (lite == 1 ? 80u : 65u) << 8; ui_hold_poll(); }
#endif
    (void)lite;
    t = fm1_ms;
    for (i = 0; i < 20u; i++) { t += step; at(t); }       /* (the detail's glide done) */
    sent = 0;
    c0 = clock();
    for (i = 0; i < N; i++) {
        uint32_t k;
        for (k = 0; k < NTRK; k++) trk[k].peak = (int16_t)(2000 + (i * 37u + k * 911u) % 5000u);   /* (it sounds) */
        for (k = 0; k < SCOPE_N; k++) scope_buf[k] = (int16_t)(sin((k + i * 7u) * 0.21) * 8000);
        t += step; at(t);
    }
    us = (double)(clock() - c0) / CLOCKS_PER_SEC / N * 1e6;
    printf("cost  %-14s %s %7.1f us/frame %8.0f us/s %7llu px/frame\n", n, lite == 1 ? "LITE 10/s" : lite ? "SLOW 20/s" : "FULL 66/s", us, us * 1000.0 / step,
           (unsigned long long)(sent / N));
#ifdef ANIM_TICK
    load_avg_q8 = 0; song.playing = 0; ui_hold_poll(); ui_lod = 0;
#endif
}
static void costs(void)
{
    static const char *const SC[] = {"home", "home_macros", "v6_analog", "v6_fm6_op", "v6_wheel", "v6_noise", "v6_phase", "v6_lofi",
                                     "v6_string", "vx_head_s", "vx_bird", "vx_kit_hits", "gen", "edit_analog"};
    uint32_t k;
    for (k = 0; k < sizeof SC / sizeof SC[0]; k++) {
        cost(SC[k], 15u, 0);
#ifdef ANIM_TICK
        cost(SC[k], 50u, 2);
        cost(SC[k], 100u, 1);
#endif
    }
    {   /* the scope alone (EDIT's well, the tech pages) */
        uint32_t i, l;
        for (i = 0; i < SCOPE_N; i++) scope_buf[i] = (int16_t)(sin(i * 0.31) * 9000 + sin(i * 2.1) * 3000);
        for (l = 0; l < 2u; l++) {
            clock_t c0 = clock();
#ifdef ANIM_TICK
            ui_lod = (uint16_t)(l * 256u);
#else
            if (l) break;
#endif
            for (i = 0; i < 2000u; i++) { cv_begin(240, 140, T_BG); scope_at(10, 220, 70, 46, T_THEME); }
            printf("cost  scope          %s %7.1f us\n", l ? "half detail" : "whole      ",
                   (double)(clock() - c0) / CLOCKS_PER_SEC / 2000 * 1e6);
        }
#ifdef ANIM_TICK
        ui_lod = 0;
#endif
    }
}

#ifdef ANIM_TICK
/* PPM frames of HOME and EDIT, 1.2 s at 66 and at 10 frames a second (the panel, every frame drawn) */
static void frames_out(const char *dir)
{
    static const char *const SC[2] = {"home", "v6_analog"};
    static const uint32_t STEP[2] = {15u, 100u};
    uint32_t k, r, f, t, x, y;
    for (k = 0; k < 2u; k++)
        for (r = 0; r < 2u; r++) {
            uint8_t pl0;
            prep(scr_id(SC[k]));
            eco.init = 0;                               /* (both runs from the same start: the places, the clock) */
            t = anim_off + 600000u;
            host_ticks += (t - fm1_ms) * 1000u;
            fm1_ms = t;
            pl0 = song.playing;
            if (r) { song.playing = 1; load_avg_q8 = 80u << 8; ui_hold_poll(); }
            for (f = 0; f * STEP[r] < 1200u; f++) {
                char p[512];
                FILE *o;
                uint32_t c;
                for (c = 0; c < NTRK; c++) trk[c].peak = (int16_t)(((f * STEP[r] / 15u) % 40u) < 3u ? 6000 : 0);   /* (a beat) */
                t += STEP[r];
                at(t);
                snprintf(p, sizeof p, "%s/%s_%s_%03u.ppm", dir, SC[k], r ? "10fps" : "66fps", (unsigned)(f * STEP[r]));
                if (!(o = fopen(p, "wb"))) continue;
                fprintf(o, "P6\n240 240\n255\n");
                for (y = 0; y < 240u; y++)
                    for (x = 0; x < 240u; x++) {
                        uint16_t v = host_screen[y * 240u + x];
                        v = (uint16_t)(v >> 8 | v << 8);
                        fputc((v >> 11) << 3, o); fputc(((v >> 5) & 63u) << 2, o); fputc((v & 31u) << 3, o);
                    }
                fclose(o);
            }
            load_avg_q8 = 0; song.playing = pl0; ui_hold_poll();
        }
}
#endif

int main(int argc, char **argv)
{
    rep = fopen("/dev/null", "w");
    al_off = 1;
#ifdef ANIM_TICK
    test_time();
    test_jumps();
    test_lod();
    test_still();
#endif
    costs();
#ifdef ANIM_TICK
    if (argc > 1) frames_out(argv[1]);
    printf("anim: %s\n", nbad ? "FAILED" : "all passed");
#endif
    (void)argc; (void)argv;
    return nbad ? 1 : 0;
}
