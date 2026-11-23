/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.8.5) the lens panels of ENV, LFO . MOD, SCL . ARP and GLO (ui_lens.c), as the approved mock-up's; included by
 * ui_graph.c after ui_lzedit.c (ed_cap, ed_chip). The canvas is the lens's panel (240 x 140, x 4..236).
 *   ENV  ENV keeps graph_adsr (V10.8.16: its layers; TRACKS gone) / the DEST columns; STAGE: the envelope with the
 *        stage picked shaded, the stage's own curve large under it, its time */
/* track t's envelope in the box (x, y, w, h): attack straight, decay and release as three bends, sustain flat */
static void env_mini(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c, int32_t th)
{
    int32_t a = 2 + t->p[P_ATK] * (w / 4) / 127, d = 3 + t->p[P_DEC] * (w / 4) / 127, r = 3 + t->p[P_REL] * (w / 4) / 127;
    int32_t s = t->p[P_SUS] * h / 127, b = y + h, x1 = x + a, x3 = x + w - r, k, px = x1, py = y;
    cv_line_t(x, b, x1, y, c, th);
    for (k = 1; k <= 4; k++) {                          /* (1/2, 3/4, 7/8, 15/16 of the way) */
        int32_t xx = x1 + d * k / 4, yy = b - s - (h - s) / (1 << k);
        cv_line_t(px, py, xx, yy, c, th);
        px = xx;
        py = yy;
    }
    cv_line_t(px, py, x3, b - s, c, th);
    px = x3;
    py = b - s;
    for (k = 1; k <= 4; k++) {
        int32_t xx = x3 + r * k / 4, yy = b - (k == 4 ? 0 : s / (1 << k));
        cv_line_t(px, py, xx, yy, c, th);
        px = xx;
        py = yy;
    }
}
static void graph_stage(const track_t *t)
{
    static const char *const SN[4] = {"ATTACK", "DECAY", "SUSTAIN", "RELEASE"};
    uint32_t sg = ui.stage & 3u, id = P_ATK + sg;
    int32_t a = 2 + t->p[P_ATK] * 56 / 127, d = 3 + t->p[P_DEC] * 56 / 127, r = 3 + t->p[P_REL] * 56 / 127;
    int32_t x0 = 10, x1 = x0 + a, x2 = x1 + d, x3 = 226 - r, sx[5] = {x0, x1, x2, x3, 226}, x, py = 0;
    int32_t s = t->p[P_SUS] * 1000 / 127, v = t->p[id] * 1000 / 127;
    const char *u = "";
    char b[24];
    cv_rect(sx[sg], 4, sx[sg + 1u] - sx[sg], 40, T_TINT);   /* the stage picked, on the whole envelope */
    env_mini(t, 10, 6, 216, 36, T_MID, 1);
    for (x = 0; x <= 216; x++) {                        /* its own shape, large: up, down to SUS, held, down to 0 */
        int32_t q = x * 1000 / 216, e = 1000 - (int32_t)((int64_t)(1000 - q) * (1000 - q) * (1000 - q) / 1000000), y;
        y = sg == 0u ? q : sg == 1u ? 1000 - (1000 - s) * e / 1000 : sg == 2u ? s : s - s * e / 1000;
        if (sg == 0u)                                   /* (ATK: slow or fast as it is) */
            y = 1000 - (int32_t)((int64_t)(1000 - q) * (1000 - q) / 1000) * (1000 - v / 2) / 1000 - (1000 - q) * v / 2000;
        y = 122 - y * 66 / 1000;
        if (x)
            cv_line_t(10 + x - 1, py, 10 + x, y, heat_y(y, 56, 122, T_THEME), 2);
        py = y;
    }
    cv_rect(10, 124, 217, 1, T_RAISE);
    ed_cap(10, 50, SN[sg], T_THEME);
    param_format(&TP[id], t->p[id], b, &u);
    str_cpy(b + str_len(b), u, 24 - str_len(b));
    cv_text_r(226, 50 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_SURF);
    cv_text(10, 128 - AF_X_CAP_Y, &AF_X, "0", T_DIM);
    cv_text_r(226, 128 - AF_X_CAP_Y, &AF_X, sg == 2u ? "HELD" : b, T_DIM, T_SURF);
}
/* the lens's panel for ENV's page pg (1), else 0 (its own) */
static int graph_env_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_STAGE)
        graph_stage(t);
    else
        return 0;
    return 1;
}

/* LFO . MOD  MOD: the matrix as a net, the sources down the left (those a slot reads lit), a curve from each slot's
 *            source to its box D1..D4 on the right (DEST, AMT as a bar), the slot KNOB 1 picked framed, a pulse
 *            running along its curve; LFO: graph_lfo under "RATE . WAVE"; LFO DEST: the LFO small on the left, the four
 *            amounts as rows, each lit by what it sends now; MSEQ, MSEQ 2: graph_mseq under its caption */
static void graph_mod_net(const track_t *t)
{
    uint32_t k;
    for (k = 0; k < NMSLOT; k++) {
        const int16_t *p = &t->p[P_M1SRC + 3u * k];
        int32_t y = 4 + (int32_t)k * 33, on = p[0] && p[1], sel = k == mod_ui_slot, sy = y + 11, w;
        uint16_t bg = sel ? T_TINT : T_RAISE, lc = sel ? T_THEME : on ? T_MID : T_DIM;
        char b[16];
        const char *u = "";
        cv_text(6, sy - AF_X_CAP_Y, &AF_X, N_MSRC[clamp(p[0], 0, MS_N - 1)], on ? T_TEXT : T_DIM);
        if (on) {
            cv_rect(58, sy, 100, 1, lc);
            if (sel && !ANIM_OFF())
                og_node((58 + (int32_t)(anim_fr * 3u % 100u)) * 16, sy * 16, 2, T_TEXT);
        } else
            cv_text(124, y + 9 - AF_X_CAP_Y, &AF_X, "----", T_DIM);
        cv_rect(160, y, 72, 26, bg);
        if (sel) {
            cv_rect(160, y, 72, 1, T_THEME); cv_rect(160, y + 25, 72, 1, T_THEME);
            cv_rect(160, y, 1, 26, T_THEME); cv_rect(231, y, 1, 26, T_THEME);
        }
        b[0] = 'D'; b[1] = (char)('1' + k); b[2] = ' '; b[3] = 0;
        str_cpy(b + 3, on ? mod_dst_name(t, p[1]) : "OFF", 13);
        cv_clip_text(165, y + 5 - AF_X_CAP_Y, &AF_X, b, on ? T_TEXT : T_DIM, bg, 64);
        param_format(&TP[P_M1AMT], p[2], b, &u);
        w = p[2] * 30 / 128;
        cv_rect(166, y + 17, 60, 2, T_LINE);
        if (on && w)
            cv_rect(w < 0 ? 196 + w : 196, y + 16, w < 0 ? -w : w, 4, p[2] < 0 ? T_ACCENT : T_THEME);
    }
}
static void lfo_cap(const track_t *t, int32_t x, int32_t y, int mode)   /* "1.18Hz . SIN" (mode: ". FREE" / ". TRIG") */
{
    const char *u = "";
    char b[32], w[12];
    param_format(&TP[P_LRATE], t->p[P_LRATE], b, &u);
    str_cpy(b + str_len(b), u, 32 - str_len(b));
    str_cpy(b + str_len(b), " \xB7 ", 32 - str_len(b));
    param_format(&TP[P_LWAVE], t->p[P_LWAVE], w, &u);
    str_cpy(b + str_len(b), w, 32 - str_len(b));
    if (mode)                                           /* (0.9.18) LFO, LFO 2: MODE too (LFO DEST: no room) */
        str_cpy(b + str_len(b), t->p[P_LPHASE] < 0 ? " \xB7 FREE" : " \xB7 TRIG", 32 - str_len(b));
    ed_cap(x, y, b, T_TEXT);
}
static __attribute__((noinline)) void graph_lfodest(const track_t *t, const page_t *pg)
{
    int32_t x, py = 0, src = mulq15(t->lfo_val, t->lfo_fade), k;
    uint32_t span = 0xFFFFFFFFu / 40u, ph = t->lfo_ph - 20u * span;
    lfo_cap(t, 10, 6, 0);
    cv_rect(10, 70, 80, 1, T_RAISE);
    for (x = 0; x < 80; x++) {                          /* (two cycles, the phase now a third in) */
        int32_t y = 70 - lfo_wave((track_t *)t, ph + (uint32_t)x * span) * 44 / 32768;
        if (x)
            cv_line_t(10 + x - 1, py, 10 + x, y, T_THEME, 2);
        py = y;
    }
    cv_rect(30, 22, 1, 96, T_MID);
    og_node(30 * 16, (70 - src * 44 / 32768) * 16, 3, T_TEXT);
    for (k = 0; k < 4; k++) {                           /* the amounts, lit by the LFO now */
        int16_t *vp;
        const param_desc_t *d = page_desc(pg, (uint32_t)k, &vp);
        int32_t y = 8 + k * 32, w = *vp * 46 / 100, live = w * src / 32767;
        const char *u = "";
        char b[16];
        if (!d)
            continue;
        cv_rect(100, y, 132, 26, T_RAISE);
        ed_cap(105, y + 4, d->label, T_MID);
        param_format(d, *vp, b, &u);
        str_cpy(b + str_len(b), u, 16 - str_len(b));
        cv_text_r(228, y + 4 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_RAISE);
        cv_rect(106, y + 17, 120, 2, T_LINE);
        if (w)
            cv_rect(w < 0 ? 166 + w : 166, y + 16, w < 0 ? -w : w, 4, ux_mix(T_RAISE, T_THEME, 45));
        if (live)
            cv_rect(live < 0 ? 166 + live : 166, y + 16, live < 0 ? -live : live, 4, heat_col(110 + (live < 0 ? -live : live) * 3));
    }
}
static void mseq_cap(const track_t *t, const page_t *pg)
{
    const char *u = "";
    char b[24];
    if (pg->id[0] == P_MSDIV) {                         /* MSEQ 2: "DIV 1/16" */
        str_cpy(b, "DIV ", 24);
        param_format(&TP[P_MSDIV], t->p[P_MSDIV], b + 4, &u);
        ed_cap(10, 6, b, T_ACCENT);
    } else
        ed_cap(10, 6, "MOD SRC: STEP", T_TEXT);
    str_cpy(b, "LEN ", 24);
    fmt_int(b + 4, t->p[P_MSLEN]);
    cv_text_r(228, 6 - AF_X_CAP_Y, &AF_X, b, T_DIM, T_SURF);
}
/* the lens's panel for LFO . MOD's page pg (1), else 0 (its own) */
static int graph_mod_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_MOD)
        graph_mod_net(t);
    else if (pg->graph == GR_LFO) {
        lfo_cap(t, 10, 6, 1);
        cv_oy = GOY + 12;
        graph_lfo(t, T_THEME);
    } else if (pg->id[0] == P_LD_PIT)
        graph_lfodest(t, pg);
    else if (pg->graph == GR_MSEQ || pg->id[0] == P_MSDIV) {
        mseq_cap(t, pg);
        cv_oy = GOY + 12;
        graph_mseq(t, T_THEME);
    } else
        return 0;
    return 1;
}

/* SCL . ARP  KEY: the twelve notes round a circle (C on top), the scale's lit and joined, ROOT in the accent, its name
 *            and the scale's in the middle; the scale on a keyboard under it. CHORD: the
 *            chord each degree of the key plays now (chord_build, as a key would), I..VII. ARP / ARP 2: "MODE . RATE .
 *            OCT" and the pattern it plays over a triad (OFF: "ARP OFF") */
/* (V10.6i) a note's colour: each of the twelve its own heat, C coldest .. B hottest (the user's pick B: the same note
 * the same colour on KEY, CHORD, ARP, SCENE); other palettes their theme */
static uint16_t note_col(uint32_t n)
{
    return heat_col(40 + (int32_t)(n % 12u) * 200 / 11);
}
/* (V10.8.19, the owner: "big, clear, the notes it uses") KEY: "D MAJOR" large (the scale's whole name, AF_L, else M,
 * else its short one); the scale's notes as chips in their colours from ROOT (ROOT framed); an octave of keys, those
 * lit, each its degree (1 ..) on it */
static const char *const SCL_LONG[] = {"CHROMATIC", "MAJOR", "MINOR", "DORIAN", "MIXOLYDIAN", "PENTATONIC", "MINOR PENTA",
                                       "HARMONIC MIN", "PHRYGIAN", "LYDIAN", "LOCRIAN", "MELODIC MIN", "BLUES", "WHOLE TONE",
                                       "DIM HALF-WHOLE", "DIM WHOLE-HALF"};
static __attribute__((noinline)) void graph_key(const track_t *t, const page_t *pg)
{
    static const uint8_t WH[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BX[5] = {1, 2, 4, 5, 6};
    uint32_t mask = scale_mask(t) & 0xFFFu, rt = (uint32_t)clamp(t->p[P_ROOT], 0, 11), i, n = 0, dg[12];
    const aafont_t *f = &AF_L;
    const char *u = "";
    char b[32];
    (void)pg;
    str_cpy(b, NN12[rt], 4);
    str_cpy(b + str_len(b), " ", 2);
    str_cpy(b + str_len(b), SCL_LONG[(uint32_t)t->p[P_SCALE] % NELEM(SCL_LONG)], 30 - str_len(b));
    if (text_w(f, b) > 228) f = &AF_M;
    if (text_w(f, b) > 228) { b[str_len(NN12[rt]) + 1u] = 0; param_format(&TP[P_SCALE], t->p[P_SCALE], b + str_len(b), &u); }
    cv_text(6, 4 - (f == &AF_L ? AF_L_CAP_Y : AF_M_CAP_Y), f, b, T_TEXT);
    for (i = 0; i < 12u; i++)                           /* dg[note]: its degree (1 ..), 0 not in the scale */
        dg[(rt + i) % 12u] = mask >> i & 1u ? ++n : 0u;
    for (i = 0; i < 12u; i++)                           /* the chips, from ROOT */
        if (mask >> i & 1u) {
            uint32_t nt = (rt + i) % 12u, k = dg[nt] - 1u;
            int32_t w = 228 / (int32_t)n, x = 6 + (int32_t)k * w;
            cv_rect(x, 34, w - 2, 24, note_col(nt));
            cv_text_in(x, 34 + (n > 9u ? CAP_IN(X, 24) : CAP_IN(M, 24)), w - 2, n > 9u ? &AF_X : &AF_M, NN12[nt], T_INK, note_col(nt));
            if (!i) cv_frame(x - 1, 33, w, 26, T_TEXT);
        }
    for (i = 0; i < 12u; i++) {                         /* an octave of keys from C, the scale's lit, each its degree */
        uint32_t blk = i >= 7u, nt = blk ? BK[i - 7u] : WH[i];
        int32_t x = blk ? 2 + (int32_t)BX[i - 7u] * 32 - 10 : 8 + (int32_t)i * 32, w = blk ? 19 : 30, kh = blk ? 40 : 68;
        uint16_t c = dg[nt] ? note_col(nt) : blk ? 0x10A2u : 0xCE59u;
        char d[3];
        cv_rect(x, 66, w, kh, c);
        if (dg[nt]) {
            fmt_int(d, (int32_t)dg[nt]);
            cv_text_in(x, 66 + kh - (blk ? 14 : 22), w, blk ? &AF_X : &AF_M, d, T_INK, c);
        }
        if (nt == rt)
            cv_rect(x + w / 2 - 2, 66 + kh - 4, 4, 3, 0x0000u);
    }
}
/* (V10.8.19, JIANT BYTE's CHORD redesign (GPL, as BYTE has it), the owner's pick) a chord's name from its notes (pn[0]
 * the root): long "D MIN ADD9", short the quality alone "MIN9" */
static void chord_label(const uint8_t *pn, uint32_t n, char *o, int lng)
{
    static const char LQ[12][10] = {" DIM7", " HALF-DIM", " DIM", " AUG", " SUS4", " SUS2", " MIN7", " MIN MAJ7", " MIN", " MAJ7",
                                    " 7", " MAJ"};
    static const char SQ[15][5] = {"DIM7", "M7B5", "DIM", "AUG", "SUS4", "SUS2", "MIN7", "MIN", "MIN", "MAJ7", "7", "MAJ",
                                   "MIN9", "MAJ9", "9"};   /* (the last three: 6, 9, 10 with a ninth) */
    uint32_t m = 0, k, q;
    int mi, dim, aug, s4, s2, b7, M7, six, nine, d7;
    for (k = 1; k < n && k < 8u; k++) m |= 1u << (((uint32_t)pn[k] + 120u - pn[0]) % 12u);
    mi = (m >> 3 & 1) && !(m >> 4 & 1); dim = mi && (m >> 6 & 1) && !(m >> 7 & 1); aug = (m >> 4 & 1) && (m >> 8 & 1) && !(m >> 7 & 1);
    s4 = !(m >> 3 & 1) && !(m >> 4 & 1) && (m >> 5 & 1); s2 = !(m >> 3 & 1) && !(m >> 4 & 1) && !s4 && (m >> 2 & 1);
    b7 = m >> 10 & 1; M7 = m >> 11 & 1; six = (m >> 9 & 1) && !dim; nine = (m >> 2 & 1) && !s2; d7 = dim && (m >> 9 & 1);
    q = dim ? (d7 ? 0u : b7 ? 1u : 2u) : aug ? 3u : s4 ? 4u : s2 ? 5u : mi ? (b7 ? 6u : M7 ? 7u : 8u) : M7 ? 9u : b7 ? 10u : 11u;
    if (lng) {
        str_cpy(o, NN12[pn[0] % 12u], 24);
        str_cpy(o + str_len(o), LQ[q], 24);
        if (six && !d7) str_cpy(o + str_len(o), " 6", 24);
        if (nine) str_cpy(o + str_len(o), b7 || M7 ? " 9" : " ADD9", 24);
        return;
    }
    str_cpy(o, SQ[nine && (q == 6u || q == 9u || q == 10u) ? (q == 6u ? 12u : q == 9u ? 13u : 14u) : q], 12);
    if (six && !d7) str_cpy(o + str_len(o), "6", 12);
    if (nine && !b7 && !M7) str_cpy(o, mi ? "MI+9" : dim ? "DIM+9" : "ADD9", 12);
}
/* note k's interval on its key (no lowercase in the font: a flat "-", as "E-" is minor; a sharp "#") */
static void chord_ivl(int32_t x, int32_t y, const uint8_t *pn, uint32_t k)
{
    static const char IV[12][3] = {"R", "-9", "9", "-3", "3", "4", "-5", "5", "#5", "6", "-7", "7"};
    const char *s = k ? IV[((uint32_t)pn[k] + 120u - pn[0]) % 12u] : "R";
    cv_text(x + (13 - text_w(&AF_X, s)) / 2, y - AF_X_CAP_Y, &AF_X, s, 0x0000u);
}
/* CHORD: the last key's chord named large (yellow while it sounds, else the track's colour), TYPE . VOICING under it; two
 * octaves of keys, the chord's notes lit (its root the track's colour) and each its interval; the seven degrees as tags,
 * each its chord's short name, the one of the last key yellow */
static __attribute__((noinline)) void graph_chord(const track_t *t)
{
    uint32_t mask = scale_mask(t), c = 0, i, d, rt = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
    uint16_t tc = TRK_COL[(uint32_t)(t - trk) % 6u];
    uint32_t key = last_note, kd, pd = 99u, snd = 0, k, n;
    uint8_t deg[12], pn[8];
    const char *u = "";
    char b[32], v[12], nm[24];
    for (i = 0; i < 12u; i++)
        if (mask >> i & 1u && c < 7u)
            deg[c++] = (uint8_t)i;
    if (t->p[P_CHRD]) {
        param_format(&CHORD_DESC[0], t->p[P_CHRD], b, &u);
        str_cpy(b + str_len(b), " \xB7 ", 32 - str_len(b));
        param_format(&CHORD_DESC[1], t->p[P_CVOI], v, &u);
        str_cpy(b + str_len(b), v, 32 - str_len(b));
    } else
        str_cpy(b, "CHORD OFF \xB7 ONE NOTE A KEY", 32);
    kd = (key + 12u - rt) % 12u;
    for (k = 0; k < NVOICE; k++) snd |= t->v[k].active && t->v[k].gate;
    for (d = 0; d < c; d++) if (deg[d] == kd) pd = d;
    n = t->p[P_CHRD] ? chord_peek(t, key, pn) : 0u;
    if (n == 1u) {                                      /* (V10.8.20) CHORD on, one note sounds: say why (a kit, not POLY) */
        str_cpy(nm, drum_track(t) ? "DRUM TRACK" : "NOT POLY", sizeof nm);
        str_cpy(b, drum_track(t) ? "CHORDS PLAY ON SYNTH TRACKS" : "CHORDS NEED EDIT > PLAY > POLY", 32);
        n = 0;
    } else if (t->p[P_CHRD]) chord_label(pn, n, nm, 1); else str_cpy(nm, "OFF", sizeof nm);
    cv_text(10, 4 - AF_L_CAP_Y + 1, text_w(&AF_L, nm) <= 220 ? &AF_L : &AF_M, nm, snd ? HUD_YE : t->p[P_CHRD] ? tc : T_DIM);
    cv_free_text(10, 26 - AF_X_CAP_Y + 1, &AF_X, b, T_MID, cv_bg, 220);
    {   /* the keys: two octaves from the C under its lowest note */
        static const uint8_t WK[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BX[5] = {0, 1, 3, 4, 5};
        uint32_t lo = n ? pn[0] : key, base = lo / 12u * 12u, on[25] = {0}, oct, w;
        for (k = 0; k < n && k < 8u; k++) { uint32_t r = (uint32_t)pn[k] - base; while (r >= 24u) r -= 12u; on[r] = k + 1u; }
        for (oct = 0; oct < 2u; oct++)
            for (w = 0; w < 7u; w++) {
                uint32_t r = oct * 12u + WK[w], h = on[r];
                int32_t x = 8 + (int32_t)(oct * 7u + w) * 16;
                cv_rect(x, 40, 15, 52, h ? (h == 1u ? tc : snd ? HUD_YE : T_TEXT) : T_RAISE);
                if (h) chord_ivl(x + 1, 80, pn, h - 1u);
            }
        for (oct = 0; oct < 2u; oct++)
            for (w = 0; w < 5u; w++) {
                uint32_t r = oct * 12u + BK[w], h = on[r];
                int32_t x = 8 + (int32_t)(oct * 7u + BX[w]) * 16 + 10;
                cv_rect(x, 40, 11, 32, h ? (h == 1u ? tc : snd ? HUD_YE : T_TEXT) : T_BG);
                if (h) chord_ivl(x - 1, 60, pn, h - 1u);
            }
    }
    for (d = 0; d < c; d++) {                           /* the degrees: numeral tag, its chord's short name */
        uint8_t nt[8];
        int32_t x = 8 + (int32_t)d * 32, tw = text_w(&AF_X, RN7[d]) + 4;
        uint32_t m = n ? chord_peek(t, 48u + rt + deg[d], nt) : 0u, on = d == pd;
        uint16_t bg = on ? HUD_YE : d == 0u ? tc : HUD_BL;
        char sn[12];
        cv_rect(x, 100, tw, 11, bg);
        cv_text_on(x + 2, 101 - AF_X_CAP_Y, &AF_X, RN7[d], on || d == 0u ? 0x0000u : T_TEXT, bg);
        if (m) { chord_label(nt, m, sn, 0); cv_text(x, 116 - AF_X_CAP_Y, &AF_X, sn, on ? HUD_YE : T_TEXT); }
    }
}
static void graph_arp(const track_t *t)
{
    static const uint8_t TRI[3] = {0, 4, 7};
    const char *u = "";
    char b[40], v[12];
    uint32_t i, md = (uint32_t)t->p[P_AMODE], oc = (uint32_t)clamp(t->p[P_AOCT], 1, 4), n = 3u * oc;
    param_format(&TP[P_AMODE], t->p[P_AMODE], b, &u);
    str_cpy(b + str_len(b), " \xB7 ", 40 - str_len(b));
    param_format(&TP[P_ARATE], t->p[P_ARATE], v, &u);
    str_cpy(b + str_len(b), v, 40 - str_len(b));
    str_cpy(b + str_len(b), " \xB7 OCT ", 40 - str_len(b));
    fmt_int(b + str_len(b), (int32_t)oc);
    ed_cap(10, 6, b, T_TEXT);
    param_format(&TP[P_AORDER], t->p[P_AORDER], b, &u);
    cv_text_r(228, 6 - AF_X_CAP_Y, &AF_X, b, T_DIM, T_SURF);
    if (!md) {
        cv_text_in(4, 56 - AF_M_CAP_Y, 232, &AF_M, "ARP OFF", T_MID, T_SURF);
        cv_text_in(4, 76 - AF_X_CAP_Y, 232, &AF_X, "KEYS PLAY STRAIGHT", T_DIM, T_SURF);
        return;
    }
    for (i = 0; i < 16u; i++) {                         /* the pattern over a triad, OCT octaves */
        uint32_t pp = n > 1u ? 2u * n - 2u : 1u, j = i % pp;
        uint32_t k = md == 2u ? n - 1u - i % n : md == 3u ? (j < n ? j : pp - j) : md == 4u ? (i * 2654435761u >> 16) % n :
                     md == 6u ? i / 2u % n : i % n;
        int32_t semi = TRI[k % 3u] + 12 * (int32_t)(k / 3u), x = 10 + (int32_t)i * 14 + (i & 1u ? t->p[P_ASWING] * 6 / 127 : 0);
        int32_t w = 2 + t->p[P_AGATE] * 10 / 127, y = 118 - semi * 2;
        uint32_t on = (i * 40503u >> 8) % 100u < (uint32_t)t->p[P_APROB] * 100u / 127u;
        cv_rect(x, y, w, 4, on ? note_col((uint32_t)(clamp(t->p[P_ROOT], 0, 11) + semi)) : T_RAISE);
    }
    cv_rect(10, 124, 220, 1, T_RAISE);
}
/* the lens's panel for SCL . ARP's page pg (1), else 0 */
static int graph_key_lz(const track_t *t, const page_t *pg)
{
    if (pg->graph == GR_SCALE)
        graph_key(t, pg);
    else if (pg->id[0] == P_CHRD)
        graph_chord(t);
    else if (pg->fam == FAM_ARP)
        graph_arp(t);
    else
        return 0;
    return 1;
}

/* GLO  MIX: the six strips (draw_tracks, track_col6; SENDS: the three sends as cells); GLOBAL the scope;
 *      SYSTEM: the tracks that play as chips, LOAD's three settings (the one set lit), MIDI's route, (V10.7.63) the output's scope
 *      and the CPU in % */
static void scope_at(int32_t x0, int32_t pw, int32_t cy, int32_t a, uint16_t c);   /* (ui_graph.c) */
static __attribute__((noinline)) void graph_system(void)
{
    static const char *const LD[3] = {"ECO", "LITE", "FULL"};
    uint32_t c, k, ld = 3;
    const char *u = "";
    char b[24];
    int16_t *vp;
    const param_desc_t *dl = page_desc(cur_page(), 1, &vp);   /* (LOAD, as its card names it) */
    if (dl) {
        param_format(dl, *vp, b, &u);
        for (k = 0; k < 3u; k++)
            if (str_eq(b, LD[k]))
                ld = k;
    }
    for (c = 0; c < NTRK; c++) {
        char n[3] = {'T', (char)('1' + c), 0};
        uint16_t bg = c < trk_live ? T_THEME : T_RAISE;
        cv_rect(8 + (int32_t)c * 22, 8, 20, 16, bg);
        cv_text_in(8 + (int32_t)c * 22, 12 - AF_X_CAP_Y, 20, &AF_X, n, c < trk_live ? T_INK : T_DIM, bg);
    }
    fmt_int(b, (int32_t)trk_live);
    str_cpy(b + str_len(b), " TRACKS PLAY", 24 - str_len(b));
    ed_cap(144, 13, b, T_TEXT);
    for (k = 0; k < 3u; k++) {
        uint16_t bg = k == ld ? T_THEME : T_RAISE;
        cv_rect(8 + (int32_t)k * 40, 34, 38, 14, bg);
        cv_text_in(8 + (int32_t)k * 40, 38 - AF_X_CAP_Y, 38, &AF_X, LD[k], k == ld ? T_INK : T_MID, bg);
    }
    param_format(&GP[G_ROUTE], song.g[G_ROUTE], b + 5, &u);
    b[0] = 'M'; b[1] = 'I'; b[2] = 'D'; b[3] = 'I'; b[4] = ' ';
    ed_cap(134, 38, b, T_DIM);
    scope_at(8, 224, 96, 28, T_THEME);                  /* (V10.7.63, the owner: an animation that said little: the
                                                         * output's scope, the CPU's number at its right) */
    str_cpy(b, "CPU ", 24);
    fmt_int(b + 4, (int32_t)(song.cpu_q8 * 100u / 256u));
    str_cpy(b + str_len(b), "%", 2);
    cv_text_r(230, 62 - AF_X_CAP_Y, &AF_X, b, T_TEXT, T_SURF);
}

