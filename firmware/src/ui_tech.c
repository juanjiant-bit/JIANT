/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT 0.5: the technical graphs of the pages that had none (they showed the scope): what the four knobs do, drawn
 * as an engineer would, live where the sound is (thermal colours: heat_col / heat_y).
 *   DIST       tk_dist_y: the drive TYPE's transfer (the FX lens draws it, ui_lzfx.c; 0.8.5: DIST, DLY, REVERB, CHORUS,
 *              MASTER and AUTOPAN live there, their old drawings gone)
 *   ENV DEST   the source now (the envelope's level) into four bipolar columns under the cards, each as long as its
 *              amount and lit by what it sends now (0.9.7: LFO DEST's and VOICE's, never drawn since the lenses, gone)
 *   GLOBAL     a bar of 16 steps with SWING moving the off-beats, the playhead, the beat */
#define TK_X0 PANEL_X0
#define TK_X1 (PANEL_X0 + PANEL_W)
#define TK_Y0 8
#define TK_Y1 112
/* the current page's knob c as 0..1000 (no knob: 500) */
static int32_t tk(uint32_t c)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), c, &vp);
    int32_t r = d && d->label && d->label[0] != '-' ? RATIO(d, enum_rank(d, *vp)) : -1;
    return r < 0 ? 500 : r;
}
/* .. as an amount -500..500: a bipolar knob about its middle, a unipolar one from 0 up */
static int32_t tk_amt(uint32_t c)
{
    int16_t *vp;
    const param_desc_t *d = page_desc(cur_page(), c, &vp);
    return d && d->min >= 0 ? tk(c) / 2 : tk(c) - 500;
}

/* DIST: in -> out of dist_shape's maths (the input's full scale +-131072 as the track's mix sees it) */
static int32_t tk_dist_y(uint32_t type, int32_t x, int32_t g, int32_t d)
{
    int32_t v = ((x >> 2) * g) >> 10, a;
    switch (type) {
    case DT_HARD: return clamp(v, -22000, 22000) * 32767 / 26000;
    case DT_FOLD:
        v = clamp(v, -110000, 110000);
        for (a = 0; a < 3; a++)
            v = v > 22000 ? 44000 - v : v < -22000 ? -44000 - v : v;
        return v * 32767 / 26000;
    case DT_CRUSH: {
        uint32_t sh = 4u + (uint32_t)d / 12u;
        x = (int32_t)((uint32_t)(clamp(x, -131072, 131068) >> sh) << sh);
        return clamp(((x >> 2) * g) >> 10, -26000, 26000) * 32767 / 26000;
    }
    case DT_RECT: a = x < 0 ? -x : x; return softclip((((a - 65536) >> 2) * g) >> 10);
    case DT_AMP:                                        /* (0.7.8) the two stages, without the coupling's drift */
        a = softclip(clamp(v + (v >> 1), -400000, 400000) + AMP_B1) - softclip(AMP_B1);
        return softclip(((a * (24 + d)) >> 4) + AMP_B2) - softclip(AMP_B2);
    default: return softclip(v + 2400) - softclip(2400);
    }
}
/* ENV DEST: the source now into four bipolar columns, one under each card (0.9.7: LFO DEST, VOICE and MSEQ 2 have
 * their lenses' pictures: ui_lzpage.c, ui_lzedit.c) */
static void tech_dest(const track_t *t)
{
    int32_t src = 0, c, mid = (TK_Y0 + TK_Y1) / 2, half = 44;
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].stage && t->v[i].stage < 4u && (t->v[i].env >> 9) > src)
            src = t->v[i].env >> 9;                     /* Q15 */
    for (c = 0; c < 4; c++) {
        int32_t x = CARD_X(c) + 22, amt = tk_amt((uint32_t)c), bh = amt * half / 500, live = bh * src / 32767;
        cv_rect(x - 6, mid, 25, 1, T_RAISE);
        cv_rect(x + 6, mid - half, 1, 2 * half, T_LINE);
        if (bh > 0) cv_rect(x, mid - bh, 13, bh, ux_mix(T_SURF, T_THEME, 35));
        else if (bh < 0) cv_rect(x, mid, 13, -bh, ux_mix(T_SURF, T_THEME, 35));
        if (live > 0) cv_rect(x, mid - live, 13, live, heat_col(80 + live * 170 / half));
        else if (live < 0) cv_rect(x, mid, 13, -live, heat_col(80 - live * 170 / half));
    }
    {   /* the source: its level now as a bar at the left edge */
        int32_t sh = (src < 0 ? -src : src) * half / 32767;
        cv_rect(TK_X0, src >= 0 ? mid - sh : mid, 3, sh ? sh : 1, T_TEXT);
    }
}

/* (V10.7.72, the owner) GLOBAL (BPM SWING CLOCK TUNE): the scope, its bars gone */
/* (V10.7.75: MATH's RESTART the scope, its ramps' picture gone: flash) */

/* the page's technical graph, if it has one (1) */
/* (V10.44) pictures in the Bluetooth segment (BTAPP_FN: the app slot full), drawn only once bt_hw_seg() said it is this
 * build's (else the page keeps its old picture) */
/* FX > MASTER > EQ: the four bands' curve (LOW and HIGH shelves, the mids bells), each band's dot, the frequencies */
static BTAPP_FN void tech_eq(int32_t x, int32_t y, int32_t w, int32_t h)
{
    static const uint16_t F[4] = {120, 380, 650, 900};   /* (x / 1024: ~60 250 2k 8k on a log axis) */
    int32_t i, k, py = 0, mid = y + 20 + (h - 30) / 2, sc = (h - 34) / 2;
    cv_rect(x, mid, w, 1, T_LINE);
    if (ui_scope)                                        /* (V10.58) MENU > DISPLAY > SCOPE ON: the master's scope behind the
                                                         * curve, live (the users missed the page's picture of the sound) */
        scope_at(x, w, mid, sc, T_RAISE);
    for (i = 0; i <= w; i++) {
        int32_t f = i * 1024 / w, db = 0, yy;
        for (k = 0; k < 4; k++) {
            int32_t d = f - F[k], wt = (k == 0 && d < 0) || (k == 3 && d > 0) ? 256 : 256 - d * d / 160;
            if (wt > 0) db += eq_v[k] * wt;
        }
        yy = mid - db * sc / (12 * 256);
        yy = clamp(yy, y + 18, y + h - 10);
        if (i) cv_line(x + i - 1, py, x + i, yy, heat_col(128 + db / 24));
        py = yy;
    }
    for (k = 0; k < 4; k++) {
        int32_t cx = x + F[k] * w / 1024;
        cv_rect(cx - 2, mid - eq_v[k] * sc / 12 - 2, 5, 5, heat_col(128 + eq_v[k] * 10));
        cv_text_in(cx - 12, y + h - 7, 24, &AF_X, k == 0 ? "60" : k == 1 ? "250" : k == 2 ? "2K" : "8K", T_DIM, T_SURF);
    }
}
/* (V10.43) EDIT > LEVELS / LEVELS 2 (DRUM-X): a column a sound, its LEVEL the height, lit by the sound as it plays
 * (its envelope now, as the colony's beings), the page's four framed */
static BTAPP_FN void tech_levels(const track_t *t, uint32_t b)   /* (V10.44 in the segment) */
{
    const drum_lane_t *K = drum_kit_of(t);
    uint32_t l;
    for (l = 0; l < 8u; l++) {
        int32_t x = 6 + (int32_t)l * 29, v = clamp(t->p[P_LN0 + l], 0, 127) * 104 / 127;
        int32_t e = K && K[l].x.live ? clamp(K[l].x.ea >> 22, 0, 255) : 0;
        cv_rect(x, 8, 25, 104, T_RAISE);
        if (!dx_lane_muted(l)) cv_rect(x, 112 - v, 25, v, e ? heat_col(80 + e * 3 / 4) : T_LINE);   /* (a muted sound: empty) */
        if (l - b < 4u) cv_frame(x - 1, 7, 27, 106, T_THEME);
        cv_text_in(x, 118, 25, &AF_S, drum_lane_abbr(t, l), l - b < 4u ? T_TEXT : T_DIM, T_SURF);
    }
}
static int graph_tech(const track_t *t, const page_t *pg)
{
    if (pg->scope != SC_GLOBAL)
        switch (pg->id[0]) {
        case P_ED_FLT: tech_dest(t); return 1;
        case P_LN0: case P_LN4: if (!bt_hw_seg()) return 0; tech_levels(t, pg->id[0] - P_LN0); return 1;
        default: return 0;
        }
    switch (pg->id[0]) {
    default: return 0;
    }
}
