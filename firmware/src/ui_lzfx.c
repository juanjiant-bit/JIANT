/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.8.5) FX's lens (ui.c fx_*): the panels of its three levels, drawn as the approved HTML mock-up (FX, the proposal
 * "LENTE"), in the lens's panel (240 x 140 at y 84; the mock-up's coordinates less 84). Included by ui_graph.c.
 *   TRACK   the track's set (SENDS, DIST, AUTO PAN): its place as dots and its name at the top, its picture (the sends
 *           as four bars, DIST's curve, AUTO PAN's swing), the DIST curve small under it
 *   BUSES   the six modules as tiles (3 x 2, 76 x 68: DELAY REVERB CHORUS SLICER MASTER PUNCH), each its name and its
 *           picture small; the one PRESETS picked TINT with a THEME frame; one LOAD ECO took away: dark, "ECO"
 *   EFFECT  the module picked large: its pages as dots, "DELAY . DLY 2" at the right, its picture
 * The pictures (fxg_*, any size): DELAY the echoes as bars, MIX high, each FDBK lower, TIME apart (PING-PONG: L / R),
 * REVERB its decay filled (its TYPE: SPRING rippled, CLOUD wavy), CHORUS three sines DEPTH deep, STEREO apart, SLICER its
 * 16 steps (the pattern's open ones lit, the step playing framed), MASTER the output now as a bar over CLIP, the
 * DUCK's curve, PUNCH the lane's steps, SL PITCH its 16 pitches from the middle, the sends four bars, DIST its transfer
 * curve in a box with TYPE and TONE, AUTO PAN the swing's span with its dot; heat colours (JIANT's thermal ramp; another
 * palette: its theme toward its accent).
 * (0.8.5, FX live) While it sounds (playing, or the output above silence: bg_energy) they move with it: the echoes
 * swell with the level and a pulse runs along them, the tail swells and a line sweeps it, the chorus lines run at
 * RATE, the slicer's step playing is framed, MASTER's meter follows the output, PUNCH's playhead walks the lane, DIST's
 * level is a dot on its curve, AUTO PAN's dot swings at RATE. Drawn on the panel's redraws only (every other frame
 * while it sounds: graph_signature); nothing of the audio path */
#define FXG_H(v) heat_col((int32_t)(v))                 /* (0..256) */
#define FXG_TC TRK_COL[song.sel % 6u]                 /* (BYTE, the HUD: the track's colour) */
static void hud_tag(int32_t x, int32_t y, const char *s, uint16_t fg, uint16_t bg)   /* a tag: s on bg */
{
    cv_rect(x, y, text_w(&AF_X, s) + 4, 11, bg);
    cv_text_on(x + 2, y + 1 - AF_X_CAP_Y, &AF_X, s, fg, bg);
}
static int32_t fxg_en(void) { return song.playing || bg_energy() > 2 ? 32 + bg_energy() : 0; }   /* 0: still */
static void fxg_label(int32_t x, int32_t y, const char *s, uint16_t c, int right)
{
    if (right)                                          /* (AF_X 3 rows down: S's capitals' middle) */
        cv_text_r(x, y + 3, &AF_X, s, c, cv_bg);
    else
        cv_text(x, y + 3, &AF_X, s, c);
}
/* (V10.8.53) DELAY, REVERB and CHORUS drawn with the shared engine (ui_gx.c: gx_col, gx_sine; were three pictures of
 * their own: the tunnel, the cells' tail, three sines and an orbit) */
/* DELAY: each echo a column, MIX's height the first and each FDBK lower, TIME apart (the slower the wider), the one
 * sounding yellow (a pulse running along them while it sounds); "1/8 . 5 ECHOES" */
static void fxg_delay(int32_t x, int32_t y, int32_t w, int32_t h)
{
    char b[24];
    const char *u;
    int32_t n, a = 1000, fb = clamp(song.g[G_DFDBK], 0, 127), top = w > 120 ? 12 : 0, run, sp;
    for (n = 0; n < 14 && a > 40; n++) a = a * fb / 127;
    run = fxg_en() && n ? (int32_t)(anim_fr / 3u % (uint32_t)n) : -1;
    sp = clamp(w / (n + 1), 6, 8 + song.g[G_DTIME] * 2);
    for (a = 1000, fb = 0; fb < n && fb * sp < w; fb++, a = a * clamp(song.g[G_DFDBK], 0, 127) / 127)
        gx_col(x + fb * sp, y + h, sp - 2 > 12 ? 12 : sp - 2, h - top, a, fb == run ? HUD_YE : fb ? 0 : FXG_TC);
    if (top) {
        param_format(&GP[G_DTIME], song.g[G_DTIME], b, &u);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        fmt_int(b + str_len(b), n);
        str_cpy(b + str_len(b), n == 1 ? " ECHO" : " ECHOES", 8);
        hud_tag(x, y, b, T_TEXT, HUD_BL);
    }
}
/* REVERB: its tail as columns of cells, SIZE long (SPRING rippled, CLOUD wavy), a line sweeping it while it sounds */
static void fxg_reverb(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t i, sz = song.g[G_RSIZE], ty = clamp(song.g[G_RTYPE], 0, 4), e = 1000, en = fxg_en();
    int32_t k = 1000 - (5000 - sz * 4300 / 127) * 5 / (w ? w : 1), sweep = en && w > 0 ? (int32_t)(anim_fr * 3u % (uint32_t)w) / 5 : -1;
    char b[16];
    for (i = 0; i < w / 5; i++, e = e * k / 1000) {
        int32_t n = ty == 4 ? 750 + (bg_sin((uint32_t)i * 2048u) >> 7) : ty == 1 ? 850 + (bg_sin((uint32_t)i * 5000u) >> 8) : 1000;
        gx_col(x + i * 5, y + h, 4, h - 12, (i < 2 ? e / 2 : e) * n / 1000 * (en ? 168 + clamp(en, 0, 288) * 88 / 288 : 256) / 256,
               i == sweep ? HUD_YE : 0);
    }
    fxg_label(x, y, N_RTYPE[ty], T_MID, 0);
    if (w > 120) {
        str_cpy(b, "PRE ", sizeof b);
        fmt_int(b + 4, song.g[G_RPRE]);
        str_cpy(b + str_len(b), " MS", 4);
        fxg_label(x + w, y, b, T_DIM, 1);
    }
}
/* CHORUS: its voices as two sines DEPTH deep, STEREO apart, running at RATE while it sounds */
static void fxg_chorus(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t dep = song.g[G_CDEPTH], st = song.g[G_CSTER], mid = y + 8 + (h - 8) / 2, amp = (h - 8) * 35 / 100 * dep / 127;
    uint32_t ph = fxg_en() ? anim_fr * (uint32_t)(song.g[G_CRATE] + 8) * 48u : 0u;
    char b[16];
    gx_sine(x, mid, w, amp * (128 + st) / 128, 2u, ph + 21000u, T_TEXT, y + 9, y + h - 1);
    gx_sine(x, mid, w, amp, 2u, ph, FXG_TC, y + 9, y + h - 1);
    str_cpy(b, "STEREO ", sizeof b);
    b[7] = st > 0 ? '+' : 0;
    fmt_int(b + 7 + (st > 0), st);
    fxg_label(x, y, b, T_MID, 0);
}
static void fxg_slicer(int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint32_t i, pat = sl_pattern(), mode = (uint32_t)song.g[G_SLMODE], play = mode && song.playing ? sl.idx % 16u : 99u;
    int32_t cw = w / 16, x0 = x + (w - 16 * cw) / 2, lw;
    char b[32];
    const char *u;
    for (i = 0; i < 16u; i++) {
        int on = ((pat >> i) & 1u) && mode;
        int32_t cx = x0 + (int32_t)i * cw;
        cv_rect(cx + 1, y + 12, cw - 2, h - 18, i == play ? (on ? HUD_YE : T_MID) : on ? FXG_H(102 + song.g[G_SLDEP] * 128 / 127) : T_SURF);
    }
    str_cpy(b, mode == SL_OFF ? "OFF" : mode == SL_STUT ? "STUT" : "GATE", sizeof b);
    str_cpy(b + str_len(b), " ", 2);
    if (w > 120) str_cpy(b + str_len(b), "\xB7 PAT ", 8);
    fmt_int(b + str_len(b), song.g[G_SLPAT]);
    if (w > 120) {
        char r[8];
        param_format(&GP[G_SLRATE], song.g[G_SLRATE], r, &u);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        str_cpy(b + str_len(b), r, 8);
    }
    lw = text_w(&AF_X, b);
    fxg_label(x, y, b, T_MID, 0);
    if (w > 120) {                                   /* the tracks in it, FX */
        uint32_t n = 0;
        for (i = 0; i < NTRK && n + 4u < sizeof b; i++)
            if (song.g[G_SLT1 + i]) { b[n++] = 'T'; b[n++] = (char)('1' + i); b[n++] = ' '; }
        b[n] = 0;
        if (!n) str_cpy(b, "NONE ", sizeof b);
        if (song.g[G_SLFX]) str_cpy(b + str_len(b), "+FX", 4);
        {   /* (at the right, clear of the mode's words: ellipsised when it must) */
            int32_t rw = text_w(&AF_X, b), l = x + lw + 14, rx = x + w - rw < l ? l : x + w - rw;
            cv_free_text(rx, y + 3, &AF_X, b, T_DIM, cv_bg, x + w - rx);
        }
    }
}
/* (V10.8.53) a transfer curve in an s x s box at (x, y): DIST's (bip: in -1 .. 1, out across the middle) and CLIP's (0 .. 1,
 * from the bottom left), its guides dotted, the level now (0..256, < 0 none) a yellow dot on it */
static void fxg_curve(uint32_t type, int32_t x, int32_t y, int32_t s, int32_t d, int32_t lvl, int bip)
{
    int32_t i, py = 0, g = d ? 4096 + d * d * 2 : 4096, cy = bip ? y + s / 2 : y + s - 1, hx = bip ? s / 2 : 0;
    int32_t sc = bip ? (s / 2 - 1) * 9 / 10 : s - 2, lx = lvl < 0 ? -1 : hx + clamp(lvl, 0, 256) * (s - hx) / 256;
    for (i = 0; i < s; i += 3) { cv_pset(x + i, cy, T_LINE); cv_pset(x + hx, y + i, T_LINE); }
    for (i = 0; i < s; i++) {
        int32_t yy = clamp(cy - tk_dist_y(type, (i - hx) * (bip ? 131072 : 32768) / (bip ? s / 2 : s), g, d) * sc / 32767, y + 1, y + s - 2);
        if (i) cv_line_t(x + i - 1, py, x + i, yy, FXG_TC, 2);
        if (i == lx) { cv_rect(x + i, y, 1, s, HUD_YE); cv_rect(x + i - 2, yy - 2, 5, 5, HUD_YE); }
        py = yy;
    }
}
static void fxg_master(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t en = bg_energy(), lvl = song.playing || en > 2 ? 128 + en / 2 : 26, i, py = 0, duck = song.g[G_DUCK];
    int32_t rel = song.g[G_DREL], cy = y + 30, ch = h - 32;
    char b[24];
    for (i = 0; i < w / 5; i++)                         /* (the meter a matrix) */
        cv_rect(x + i * 5, y + 2, 4, 9, i * 5 < w * lvl / 256 ? FXG_H(40 + i * 5 * 216 / (w ? w : 1)) : T_SURF);
    str_cpy(b, "CLIP ", sizeof b);
    fmt_int(b + 5, song.g[G_CLIP]);
    if (w > 120) {
        str_cpy(b + str_len(b), " \xB7 PUNCH ", 10);
        fmt_int(b + str_len(b), song.g[G_PUNCH]);
        fxg_label(x + w, y + 14, "DUCK", T_DIM, 1);
    }
    fxg_label(x, y + 14, b, T_MID, 0);
    if (w > 120) {                                      /* (V10.43) the page: CLIP's curve (in -> out, the level now a dot),
                                                         * PUNCH's bar (its push on the peaks now), then the duck */
        int32_t s = ch;
        fxg_curve(DT_SOFT, x, cy, s, song.g[G_CLIP] * 127 / 100, lvl * 256 / 320, 0);
        cv_rect(x + s + 6, cy, 6, s, T_RAISE);
        i = song.g[G_PUNCH] * s / 100 * (lvl - 26) / 230;
        if (i > 0) cv_rect(x + s + 6, cy + s - i, 6, i, FXG_H(230));
        x += s + 18;
        w -= s + 18;
    }
    {   /* the duck's gain over a beat: down by DUCK, back over REL (e^-u k, a factor a column); (V10.43) the gain now a
         * line across */
        int32_t kk = clamp(8000 - rel * 50, 1600, 8000), e = 1000;
        for (i = 0; i <= w; i++) {
            int32_t d = 1000 - duck * e / 127, yy = cy + ch - d * ch / 1000;
            if (i)
                cv_line(x + i - 1, py, x + i, yy, FXG_H(154));
            py = yy;
            e = e * (1000 - kk / (w ? w : 1)) / 1000;
        }
        if (duck)
            cv_rect(x, cy + ch - duck_g1 * ch / 32767, w, 1, T_TEXT);
    }
}
static void fxg_slpitch(int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t i, cw = w / 16, mid = y + 10 + (h - 10) / 2, len = clamp(song.g[G_SLPLEN], 1, 16);
    cv_rect(x, mid, w, 1, T_LINE);
    for (i = 0; i < 16; i++) {
        int32_t v = song.g[G_SLP0 + i], bh = (v < 0 ? -v : v) * ((h - 10) / 2 - 4) / 24 + 2;
        uint16_t c = i == (int32_t)sl_ui_step ? T_THEME : i < len ? FXG_H(128 + v * 5) : T_RAISE;
        cv_rect(x + i * cw + 1, v >= 0 ? mid - bh : mid + 1, cw - 2, bh, c);
    }
    fxg_label(x, y, song.g[G_SLPQ] ? "SL PITCH \xB7 SCL" : "SL PITCH \xB7 CHR", T_MID, 0);
}
static void fxg_sends(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h)
{                                                       /* (V10.8.15, JIANT BYTE's COLUMNS: a column of thermal cells a send,
                                                         * its tag above, its level a yellow line and its percent; in the
                                                         * cards' order) */
    static const char *const N[4] = {"CHOR", "DLY", "REV", "DIST"};   /* (V10.7.58, the owner: DIST back on SENDS) */
    static const uint8_t ID[4] = {P_CHOR, P_DLY, P_REV, P_DIST};
    int32_t i, cw = w / 4 - 8, nr = (h - 12) / 6;
    char b[8];
    for (i = 0; i < 4; i++) {
        int32_t v = t->p[ID[i]], bx = x + i * (w / 4), ly = y + h - 1 - v * nr * 6 / 127;
        cv_rect(bx, y, text_w(&AF_X, N[i]) + 4, 11, HUD_BL);
        cv_text_on(bx + 2, y + 1 - AF_X_CAP_Y, &AF_X, N[i], T_TEXT, HUD_BL);
        gx_col(bx, y + h, cw, h - 12, v * 1000 / 127, 0);
        cv_rect(bx - 2, ly, cw + 4, 1, HUD_YE);
        fmt_int(b, v * 100 / 127);
        str_cpy(b + str_len(b), "%", 2);
        cv_text_on(bx + cw / 2 - 8, (ly - 12 > y + 12 ? ly - 12 : y + 12) - AF_X_CAP_Y, &AF_X, b, HUD_YE, cv_bg);   /* (above its line) */
    }
}
static void fxg_dist(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h)
{
    uint32_t type = (uint32_t)t->p[P_DTYPE] > DT_AMP ? DT_SOFT : (uint32_t)t->p[P_DTYPE];
    char b[16];
    (void)w;
    fxg_curve(type, x, y, h, t->p[P_DIST], fxg_en() && h > 20 ? clamp(bg_energy(), 0, 256) : -1, 1);
    hud_tag(x + h + 8, y + 1, N_DTYPE[type], song.sel % 6u >= 4u ? 0xFFFFu : 0x0000u, FXG_TC);
    str_cpy(b, "TONE ", sizeof b);
    fmt_int(b + 5, t->p[P_DTONE]);
    fxg_label(x + h + 8, y + 15, b, T_DIM, 0);
}
static void fxg_pan(const track_t *t, int32_t x, int32_t y, int32_t w, int32_t h)
{
    int32_t dep = t->p[P_APDEP], mid = y + 10 + (h - 10) / 2, cx = x + w / 2, sw = dep * (w / 2 - 12) / 127;
    char b[24];
    const char *u;
    cv_rect(cx, y + 10, 1, h - 10, T_LINE);
    fxg_label(x, mid - 6 - AF_X_CAP_Y, "L", T_DIM, 0);
    fxg_label(x + w, mid - 6 - AF_X_CAP_Y, "R", T_DIM, 1);
    if (sw > 0)
        cv_rect(cx - sw, mid, 2 * sw + 1, 1, T_MID);
    {   /* the dot: at the middle, or swinging at RATE while it sounds (a sine over the span: the picture's, not the
         * audio's own phase) */
        int32_t dx = fxg_en() && sw > 0 ? bg_sin(anim_fr * (uint32_t)(t->p[P_APRATE] + 6) * 96u) * sw >> 15 : 0;
        cv_rrect(cx + dx - 4, mid - 4, 9, 9, 4, FXG_H(205), T_SURF);
    }
    {   /* WAVE . RATE as AUTO PAN's cards say them (its RATE by SYNC: page_desc) */
        int16_t *vp;
        const param_desc_t *dw = page_desc(cur_page(), 3, &vp), *dr;
        char r[10];
        param_format(dw ? dw : track_desc(t, P_APWAVE), t->p[P_APWAVE], b, &u);
        str_cpy(b + str_len(b), " \xB7 ", 4);
        dr = page_desc(cur_page(), 1, &vp);
        param_format(dr ? dr : track_desc(t, P_APRATE), dr && vp ? *vp : t->p[P_APRATE], r, &u);
        str_cpy(b + str_len(b), r, 10);
    }
    fxg_label(x + 12, y, b, T_MID, 0);
}
/* module m's picture */
static void fxg_mod(uint32_t m, int32_t x, int32_t y, int32_t w, int32_t h)
{
    switch (m) {
    case 0: fxg_delay(x, y, w, h); break;
    case 1: fxg_reverb(x, y, w, h); break;
    case 2: fxg_chorus(x, y, w, h); break;
    case 3: if (cur_page()->graph == GR_SLPIT && w > 120) fxg_slpitch(x, y, w, h); else fxg_slicer(x, y, w, h); break;
    case 4: break;                                     /* (V10.8.33: PUNCH IN's lane picture gone, the owner: its room) */
    default:
        fxg_master(x, y, w, h);
        break;                                         /* (snd2: MASTER last) */
    }
}
/* the set's place: n dots (5 x 3, 7 apart) at the top left, k lit */
static void fx_dots(uint32_t n, uint32_t k, const char *a, const char *b)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        cv_rect(6 + (int32_t)i * 7, 6, i == k ? 5 : 3, 3, i == k ? FXG_TC : T_LINE);
    (void)a; (void)b;                                  /* (its name stood at the right, "FX . T1", "SLICER . SL BUS 2": the
                                                        * header, the tabs and the breadcrumb say it) */
}
static __attribute__((noinline)) void graph_fx_lz(const track_t *t, const page_t *pg)
{
    uint32_t lv = fx_level();
    if (lv == FXL_TRACK) {
        int32_t k = fx_trk_k(ui.page);
        char tn[3] = {'T', (char)('1' + song.sel % NTRK), 0};
        fx_dots(3, (uint32_t)k, k == 0 ? "FX" : PAGES[ui.page].title, tn);
        if (k == 0) fxg_sends(t, 8, 12, 224, 70);
        else if (k == 1) fxg_dist(t, 8, 12, 224, 70);
        else fxg_pan(t, 8, 12, 224, 70);
        if (k != 1) fxg_dist(t, 8, 92, 224, 40);       /* (the drive small under the others; DIST's own page: once) */
    } else if (lv == FXL_BUSES) {
        uint32_t m;
        for (m = 0; m < FX_NMOD; m++) {
            int32_t x = 4 + (int32_t)(m % 3u) * 78, y = (int32_t)(m / 3u) * 72;
            int sel = m == ui.fxmod, ok = fx_mod_ok(m);
            uint16_t bg = T_RAISE;                      /* (0.8.9: a rounded card, the one picked framed THEME) */
            lz_tile(x, y, 76, 68, sel, T_THEME);
            cv_bg = bg;
            GFX_HOOK_ALIGN(0, y + 3, 0, y + 14, AL_V | AL_PASS, "FX module name");
            cv_text_on(x + 4, y + 3, &AF_X, FX_MOD[m], sel ? T_THEME : T_TEXT, bg);
            if (ok)
                fxg_mod(m, x + 4, y + 14, 68, 50);
            else
                cv_text_on(x + 4, y + 33, &AF_X, "ECO", T_DIM, bg);
            cv_bg = T_SURF;
        }
    } else {
        uint32_t m = fx_mod_of(ui.page), k = fx_mod_k(ui.page);
        fx_dots(fx_mod_n(m), k, FX_MOD[m], pg->title);
        fxg_mod(m, 8, 12, 224, 120);
    }
}
