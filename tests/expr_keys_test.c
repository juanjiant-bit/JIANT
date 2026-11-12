/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z4) the expression keys on the real sources (ui_test.c's): OCT- / OCT+ pressed while a key's note sounds
 * (ui_input.c oct_key, kx_frame; seq.c kx_block), MENU > AUDIO > OCT BEND, their recording and MIDI OUT.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/expr_keys_test.c -lm
 * 1. no note held: OCT- / OCT+ the octave as before (both: back to 0), no expression;
 * 2. a note held: OCT+ bends that track up along the eased curve to OCT BEND's range (2 ST by default: 512 / 256 st)
 *    in ~300 ms, smooth (no jump), the octave kept, and back to 0 once let go; OCT- a vibrato ~5.5 Hz, +-0.35 st, faded
 *    in; 12 ST reaches 12 semitones; OFF: OCT is the octave with a note held too; a DRUM track none;
 * 3. MIDI OUT ALL: CC1 up and back to 0, pitch bend to full (MSB 127) and back to the middle (64);
 * 4. recording (armed, playing): the gates into the selected track's automation (P_XPR: 125 at the press, 127 at the
 *    release), played back without the keys: the bend comes and goes there;
 * 5. MENU: the row in AUDIO, its eight values; an older setting (byte 0) reads 2 ST (the editor's id 26: editor_test.c);
 * 6. (V10.7.22) the matrix's sources OCT+ / OCT- (mod.c MS_OCTUP / MS_OCTDN, appended: the ids before them kept):
 *    no route from them: the bend, the vibrato and MIDI OUT bit for bit as V10.7.21 (a checksum, EXPR_HASH_V10721,
 *    measured with that build's sources: -DEXPR_HASH_ONLY prints it); OCT+ on CUT sweeps the filter (the same eased
 *    curve), no bend (nor MIDI's pitch bend), and eases back when let go; OCT- on CUT / LEVEL wobbles it at 5.5 Hz,
 *    the pitch still, CC1 still sent; OCT BEND OFF: OCT+ 0. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int bad;
static void ck(const char *what, int ok)
{
    printf("expr: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void blocks(uint32_t ms)                       /* the audio side: ms of blocks */
{
    uint32_t k, n = ms * (FS / 1000u) / CTL;
    for (k = 0; k < n; k++)
        events_block(CTL);
}
static uint32_t ui_frames;
static void ms_run(uint32_t ms)                       /* the UI and the audio together, a frame (16 ms) at a time */
{
    uint32_t k;
    for (k = 0; k < ms / 16u; k++) {
        frame();
        blocks(16);
        ui_frames++;
    }
}
static void setup(void)
{
    ui_power_on();
    usb.config = 1;
    mo_r = mo_w = 0;
    mo_lvl = MO_KEYS;
    song.sel = 0;
    frame();
}
static uint32_t KEY = 12u;                            /* a white key mid keyboard */

/* a fixed play of the keys (both tracks' notes, OCT+, OCT-, both, MIDI OUT ALL, a route of LFO and an inactive one
 * from OCT+ on T1): FNV-1a over every block's kx_q8 and every MIDI byte sent */
#define EXPR_HASH_V10721 0x9d661a90u
static uint32_t expr_hash(void)
{
    uint32_t h = 2166136261u, b, i, n = 0;
    setup();
    menu_put(MI_OCTBEND, 2u);
    midi_out_mode = MO_ALL;
    trk[0].p[P_M2SRC] = MS_LFO; trk[0].p[P_M2DST] = MD_CUT; trk[0].p[P_M2AMT] = 40;
    trk[0].p[P_M3SRC] = MS_T1 + NTRK; trk[0].p[P_M3DST] = MD_CUT; trk[0].p[P_M3AMT] = 0;   /* (OCT+: AMT 0, no route) */
    trk[1].p[P_M1SRC] = MS_T1 + NTRK + 1; trk[1].p[P_M1DST] = 0; trk[1].p[P_M1AMT] = 30;   /* (OCT-: no DEST, no route) */
    key_down(KEY);
    frame();
    for (b = 0; b < 2400u; b++) {
        uint32_t ph = b / 300u;
        if (b % 300u == 0u) {
            if (ph & 1u) btn_down(B_OCTUP); else btn_up(B_OCTUP);
            if (ph & 2u) btn_down(B_OCTDN); else btn_up(B_OCTDN);
            frame();
        }
        events_block(CTL);
        for (i = 0; i < NTRK; i++)
            h = (h ^ (uint32_t)kx_q8[i]) * 16777619u;
        while (mo_r != mo_w) {
            h = (h ^ midi_out_q[mo_r++ % MQ]) * 16777619u;
            n++;
        }
    }
    btn_up(B_OCTUP); btn_up(B_OCTDN); key_up(KEY);
    frame();
    blocks(500);
    for (i = 0; i < 4u; i++) trk[0].p[P_M1SRC + i] = trk[1].p[P_M1SRC + i] = 0;
    for (i = 4u; i < 12u; i++) trk[0].p[P_M1SRC + i] = trk[1].p[P_M1SRC + i] = 0;
    midi_out_mode = MO_KEYS;
    frame();
    return h ^ n;
}

int main(void)
{
    char what[160];
    int32_t mx, mn, prev, jump;
    uint32_t k, cross;
    {                                                 /* ---- 6. first: no route from OCT+ / OCT- ---- */
        uint32_t h = expr_hash();
#ifdef EXPR_HASH_ONLY
        printf("expr hash %08x\n", h);
        return 0;
    }
#else
        snprintf(what, sizeof what, "no route from OCT+ / OCT-: bend, vibrato, MIDI OUT bit for bit as V10.7.21 (%08x)", h);
        ck(what, h == EXPR_HASH_V10721);
    }
#endif
    /* ---- 5. the MENU row ---- */
    setup();
    ck("MENU > AUDIO > OCT BEND: in AUDIO, eight values, 2 ST by default (an older setting's 0)",
       MI_TAB[MI_OCTBEND] == MTAB_AUDIO && menu_n(MI_OCTBEND) == 8u && menu_get(MI_OCTBEND) == 2u && !oct_xp &&
       str_eq(menu_vname(MI_OCTBEND, 2), "2 ST") && str_eq(menu_vname(MI_OCTBEND, 0), "OFF") && kx_semi == 2u);
    ck("  AUDIO's rows still fit its tab", mtab_rows(MTAB_AUDIO) <= MTAB_ROWS);
    /* ---- 1. no note: the octave ---- */
    setup();
    press(B_OCTUP);
    blocks(100);
    ck("no note held: OCT+ the octave up, no expression", song.octave == 1 && !kx_g[0] && !kx_g[1] && !kx_q8[0]);
    press(B_OCTDN);
    press(B_OCTDN);
    ck("no note held: OCT- down again (to -1)", song.octave == -1);
    /* ---- 2. a note held: the bend ---- */
    setup();
    key_down(KEY);
    frame();
    btn_down(B_OCTUP);
    frame();
    ck("a note held: OCT+ gates the bend on its track, the octave kept", (kx_g[1] & 1u) && song.octave == 0);
    prev = 0; jump = 0; mx = 0;
    for (k = 0; k < 50u; k++) {                       /* 50 x 8 ms */
        blocks(8);
        if (kx_q8[0] - prev > jump) jump = kx_q8[0] - prev;
        prev = kx_q8[0];
        if (k == 6u) mx = kx_q8[0];                   /* (at ~56 ms) */
    }
    snprintf(what, sizeof what, "  eased in: %d / 512 at ~56 ms, 512 after 400 ms, no step over 24 (largest %d)", (int)mx, (int)jump);
    ck(what, mx > 0 && mx < 100 && kx_q8[0] == 512 && jump <= 24);
    btn_up(B_OCTUP);
    frame();
    blocks(400);
    ck("  let go: back to 0 (and no octave change)", kx_q8[0] == 0 && !kx_g[1] && song.octave == 0);
    /* OCT-: the vibrato */
    btn_down(B_OCTDN);
    frame();
    blocks(300);
    mx = mn = 0; cross = 0; prev = kx_q8[0];
    for (k = 0; k < 689u; k++) {                      /* ~1 s of blocks */
        events_block(CTL);
        if (kx_q8[0] > mx) mx = kx_q8[0];
        if (kx_q8[0] < mn) mn = kx_q8[0];
        if ((prev < 0) != (kx_q8[0] < 0)) cross++;
        prev = kx_q8[0];
    }
    snprintf(what, sizeof what, "  OCT- held: a vibrato +%d / %d (of 90: 0.35 st), %u crossings a second (5.5 Hz: 11)", (int)mx, (int)mn, cross);
    ck(what, mx >= 85 && mx <= 90 && mn <= -85 && mn >= -90 && cross >= 10u && cross <= 12u && song.octave == 0);
    btn_up(B_OCTDN);
    frame();
    blocks(300);
    ck("  let go: the vibrato fades out to 0", kx_q8[0] == 0);
    /* 12 ST */
    menu_put(MI_OCTBEND, 7u);
    frame();
    btn_down(B_OCTUP);
    frame();
    blocks(400);
    ck("OCT BEND 12 ST: an octave up (12 x 256)", kx_q8[0] == 12 * 256);
    btn_up(B_OCTUP);
    frame();
    blocks(400);
    /* OFF */
    menu_put(MI_OCTBEND, 0u);
    frame();
    press(B_OCTUP);
    blocks(100);
    ck("OCT BEND OFF: OCT+ with a note held the octave, as before", song.octave == 1 && !kx_g[1] && !kx_q8[0]);
    key_up(KEY);
    frame();
    /* a DRUM track */
    setup();
    song.sel = 3;                                     /* (TRK_OLD: T4 DRUM-X) */
    frame();
    key_down(KEY);
    frame();
    btn_down(B_OCTUP);
    frame();
    blocks(400);
    ck("a DRUM track: no bend (its pitch is the kit's)", drum_track(&trk[3]) && kx_q8[3] == 0);
    btn_up(B_OCTUP);
    key_up(KEY);
    frame();
    /* ---- 3. MIDI OUT ALL ---- */
    setup();
    midi_out_mode = MO_ALL;                           /* (ui.c sets mo_lvl from it every frame) */
    key_down(KEY);
    frame();
    btn_down(B_OCTUP);
    btn_down(B_OCTDN);
    frame();
    {
        uint32_t cc_max = 0, cc_last = 255, pb_max = 0, pb_last = 0, n = 0, b;
        for (b = 0; b < 2u * 400u * (FS / 1000u) / CTL; b++) {   /* 400 ms held, 400 let go: the queue read each block */
            if (b == 400u * (FS / 1000u) / CTL) {
                btn_up(B_OCTUP);
                btn_up(B_OCTDN);
                frame();
            }
            events_block(CTL);
            while (mo_r != mo_w) {
                uint32_t pkt = midi_out_q[mo_r++ % MQ], st = (pkt >> 8) & 0xFFu, d1 = (pkt >> 16) & 127u, d2 = pkt >> 24;
                if (st == 0xB0u && d1 == 1u) { cc_last = d2; if (d2 > cc_max) cc_max = d2; n++; }
                if (st == 0xE0u) { pb_last = d2; if (d2 > pb_max) pb_max = d2; n++; }
            }
        }
        snprintf(what, sizeof what, "MIDI OUT ALL: CC1 to %u and back to %u, pitch bend MSB to %u and back to %u (%u messages)",
                 cc_max, cc_last, pb_max, pb_last, n);
        ck(what, cc_max == 127u && cc_last == 0u && pb_max == 127u && pb_last == 64u && n <= 4u * 128u);
    }
    key_up(KEY);
    frame();
    /* ---- 4. recording ---- */
    setup();
    trk[0].p[P_SLEN] = 16; trk[0].p[P_SDIV] = 2; song.g[G_BPM] = 120;   /* (a step: 125 ms) */
    motion.on = 0;
    song.rec = 1;
    transport_req = 1;
    ms_run(32);                                       /* playing */
    key_down(KEY);
    ms_run(240);                                      /* about step 2 */
    btn_down(B_OCTUP);
    ms_run(480);                                      /* held some 4 steps */
    btn_up(B_OCTUP);
    ms_run(48);
    key_up(KEY);
    ms_run(48);
    {
        uint32_t i, on = 0, off = 0, on_at = 99, off_at = 99;
        for (i = 0; i < motion.count; i++) {
            const motion_event_t *e = &motion.event[i];
            if (MOTION_TRK(e) != 0u || MOTION_ID(e) != P_XPR) continue;
            if (MOTION_VAL(e) == 125) { on++; on_at = MOTION_STEP(e); }
            if (MOTION_VAL(e) == 127) { off++; off_at = MOTION_STEP(e); }
        }
        snprintf(what, sizeof what, "recording: the bend's gate as automation (P_XPR 125 at step %u, 127 at step %u)", on_at, off_at);
        ck(what, on == 1u && off == 1u && on_at >= 1u && on_at <= 3u && off_at >= on_at + 3u && off_at <= on_at + 5u);
        song.rec = 0;
        frame();
        mx = 0;
        prev = 9999;
        for (k = 0; k < 2u * 16u * 125u / 16u; k++) {   /* two passes, no keys: a step a 125 ms */
            ms_run(16);
            if (kx_q8[0] > mx) mx = kx_q8[0];
            if (mx == 512 && kx_q8[0] < prev) prev = kx_q8[0];   /* (after it: how far back) */
        }
        snprintf(what, sizeof what, "  played back without the keys: the bend comes (to %d of 512) and goes (back to %d)", (int)mx, (int)prev);
        ck(what, mx == 512 && prev == 0 && !kx_g[0] && !kx_g[1]);
    }
    transport_req = 2;
    ms_run(32);
    ck("stopped: the recorded gates gone (the sound's value back)", trk[0].p[P_XPR] == 127 && (blocks(400), kx_q8[0] == 0));
    /* ---- 6. (V10.7.22) the sources OCT+ / OCT- ---- */
#ifndef EXPR_HASH_ONLY                                /* (the sources: not in V10.7.21) */
    ck("OCT+ / OCT- appended (24, 25), every source before them where it was: a stored project's matrix the same",
       MS_MODW == 6 && MS_PROX == 15 && MS_FOLLOW == 17 && MS_T1 == 18 && MS_OCTUP == MS_T1 + NTRK && MS_OCTUP == 24 &&
       MS_OCTDN == 25 && MS_N == 26 && str_eq(N_MSRC[MS_OCTUP], "OCT+") && str_eq(N_MSRC[MS_OCTDN], "OCT-") &&
       str_eq(N_MSRC[MS_T1 + NTRK - 1], "T6 ENV") && TP[P_M1SRC].max == MS_N - 1 && !MS_BIPOLAR(MS_OCTDN) && !MS_VOICE(MS_OCTUP));
    setup();
    midi_out_mode = MO_ALL;
    trk[0].p[P_M1SRC] = MS_OCTUP; trk[0].p[P_M1DST] = MD_CUT; trk[0].p[P_M1AMT] = 63;
    key_down(KEY);
    frame();
    {
        uint32_t b, pb = 0, cc = 0;
        int32_t c56 = 0, cmax = 0, cj = 0, cprev = 0, rel = 0, rj = 0, q8 = 0;
        btn_down(B_OCTUP);
        frame();
        for (b = 0; b < 400u * (FS / 1000u) / CTL; b++) {
            events_block(CTL);
            mod_begin(&trk[0]);
            if (MOD.cut - cprev > cj) cj = MOD.cut - cprev;
            cprev = MOD.cut;
            mod_end(&trk[0]);
            if (b == 56u * (FS / 1000u) / CTL) c56 = cprev;
            if (cprev > cmax) cmax = cprev;
            if (kx_q8[0] != 0) q8 = 1;
            while (mo_r != mo_w) {
                uint32_t pkt = midi_out_q[mo_r++ % MQ], st = (pkt >> 8) & 0xF0u;
                pb += st == 0xE0u; cc += st == 0xB0u;
            }
        }
        snprintf(what, sizeof what, "OCT+ on CUT (AMT 63): the filter eased up (%d at ~56 ms, %d of 63 steps), no step over %d, no bend, no pitch bend out",
                 (int)c56, (int)cmax, (int)cj);
        ck(what, c56 > 0 && c56 < cmax / 4 && cmax >= 62 * 256 && cmax <= 63 * 256 && cj <= 63 * 256 / 8 && !q8 && !pb && !cc);
        btn_up(B_OCTUP);
        frame();
        rj = 0; cprev = cmax;
        for (b = 0; b < 400u * (FS / 1000u) / CTL; b++) {
            events_block(CTL);
            mod_begin(&trk[0]);
            if (cprev - MOD.cut > rj) rj = cprev - MOD.cut;
            if (MOD.cut > cprev) rj = 99999;          /* (only down) */
            cprev = MOD.cut;
            mod_end(&trk[0]);
            if (b == 150u * (FS / 1000u) / CTL) rel = cprev;
        }
        snprintf(what, sizeof what, "  let go: eases back (%d at ~150 ms) to %d, down only, no step over %d", (int)rel, (int)cprev, (int)rj);
        ck(what, rel > 0 && rel < cmax && cprev == 0 && rj <= 63 * 256 / 8 && mod_tsrc(&trk[0], MS_OCTUP, 0) == 0);
        trk[1].p[P_M1SRC] = MS_OCTUP; trk[1].p[P_M1DST] = MD_CUT; trk[1].p[P_M1AMT] = 63;   /* (another track's route: */
        trk[0].p[P_M1SRC] = 0;                                                              /* T1's OCT+ bends again) */
        btn_down(B_OCTUP);
        frame();
        blocks(400);
        ck("  the route on another track only: this one bends as before (512)", kx_q8[0] == 512);
        btn_up(B_OCTUP);
        frame();
        blocks(400);
        trk[1].p[P_M1SRC] = 0;
        trk[0].p[P_M1SRC] = MS_OCTUP;
        menu_put(MI_OCTBEND, 0u);                    /* OFF: the octave, OCT+ stays 0 */
        frame();
        press(B_OCTUP);
        blocks(100);
        ck("  OCT BEND OFF: OCT+ the octave, the source 0", song.octave == 1 && mod_tsrc(&trk[0], MS_OCTUP, 0) == 0);
        press(B_OCTDN);
        menu_put(MI_OCTBEND, 2u);
        frame();
    }
    {
        uint32_t b, dst, cc = 0, cross = 0;
        for (dst = 0; dst < 2u; dst++) {
            int32_t vmax = 0, vmin = 0, base, pv, q8 = 0, x, prevx = 0;
            trk[0].p[P_M1SRC] = MS_OCTDN; trk[0].p[P_M1DST] = dst ? MD_LEVEL : MD_CUT; trk[0].p[P_M1AMT] = 63;
            base = trk[0].p[P_LEVEL] = 64;
            cross = 0;
            btn_down(B_OCTDN);
            frame();
            for (b = 0; b < 1300u * (FS / 1000u) / CTL; b++) {
                events_block(CTL);
                mod_begin(&trk[0]);
                pv = dst ? trk[0].p[P_LEVEL] - base : MOD.cut;
                mod_end(&trk[0]);
                if (b >= 300u * (FS / 1000u) / CTL) {
                    if (pv > vmax) vmax = pv;
                    if (pv < vmin) vmin = pv;
                    x = mod_tsrc(&trk[0], MS_OCTDN, 0);
                    if ((x < 0) != (prevx < 0)) cross++;
                    prevx = x;
                }
                q8 |= kx_q8[0] != 0;
                while (mo_r != mo_w) {
                    uint32_t pkt = midi_out_q[mo_r++ % MQ];
                    cc += ((pkt >> 8) & 0xF0u) == 0xB0u && ((pkt >> 16) & 127u) == 1u;
                }
            }
            snprintf(what, sizeof what, "OCT- on %s (AMT 63): a vibrato of it %+d / %+d, %u crossings a second (11), the pitch still, CC1 sent",
                     dst ? "LEVEL" : "CUT", (int)vmax, (int)vmin, cross);
            ck(what, (dst ? vmax >= 50 && vmin <= -50 && trk[0].p[P_LEVEL] == base
                          : vmax >= 60 * 256 && vmin <= -60 * 256) && cross >= 10u && cross <= 12u && !q8 && cc > 0u);
            btn_up(B_OCTDN);
            frame();
            blocks(300);
            ck("  let go: faded out to 0", mod_tsrc(&trk[0], MS_OCTDN, 0) == 0 && kx_q8[0] == 0);
        }
    }
    key_up(KEY);
    trk[0].p[P_M1SRC] = trk[0].p[P_M1DST] = trk[0].p[P_M1AMT] = 0;
    midi_out_mode = MO_KEYS;
    frame();
#endif
    printf("expr: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
