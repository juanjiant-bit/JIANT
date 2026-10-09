/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* JIANT FM's pages drawn with the user's plates (assets/ui-shapes/plates.svg, docs/TONIC-UI.md "Qué lámina va en
 * cada pantalla"): each page's graph panel holds a plate (ui_organic.c og_plate) that lives with the sound: a role's
 * colour lit or quieted by a value, dots lit by voices or hits, the line swaying with an LFO or a delay. The panel
 * is 234 x 122 px (cv_oy 0). Numbers in the plates' order (SH_PLATES_1..17). */
enum { PL_HOME = 1, PL_VOICES, PL_FM, PL_DRUM, PL_BUD, PL_ARP, PL_MACRO, PL_FX, PL_SONG, PL_LFO3, PL_LFO2, PL_MAP,
       PL_MIXER, PL_MOD, PL_ENV, PL_PUNCH, PL_CHORD };

/* plate n at (cx, cy) px, size px across (its +-63 units) */
static void plate(uint32_t n, int32_t cx, int32_t cy, int32_t size, const int16_t *glow, const uint8_t *lit,
                  uint32_t dot_role, int32_t sway, uint32_t ph)
{
    if (n < 1u || n > SH_PLATES_COUNT)
        return;
    og_plate(SH_PLATES_ILLS[n - 1u], SH_PLATES_LENS[n - 1u], cx * OG_Q, cy * OG_Q, size * OG_Q / 126, glow, lit,
             dot_role, sway, ph);
}

/* a small caption in the panel's corner (the plate's subject) */
static void plate_caption(const char *l, const char *r)
{
    if (l)
        cv_text(10, 106, &AF_S, l, T_MID);
    if (r)
        cv_text_r(230, 106, &AF_S, r, T_MID, T_SURF);
}

/* the track's activity 0..256: its loudest voice now (the mix's peak, decaying) */
static int32_t plate_level(const track_t *t)
{
    int32_t p = t->peak >> 6;
    return p > 256 ? 256 : p;
}

/* LFO (LFO, LFO DEST): the five swimmers are the five waves, SIN TRI SAW SQR S&H (their rings cyan violet red orange
 * yellow); the one chosen bright, the others quiet; every tail swims at the LFO's rate, wider with its depth */
static void graph_p_lfo(const track_t *t)
{
    static const uint8_t ROLE[5] = {3, 7, 1, 6, 5};
    int16_t g[8] = {0, -150, 0, -150, 0, -150, -150, -150};
    uint32_t w = (uint32_t)t->p[P_LWAVE] % 5u, ph = t->lfo_ph >> 16;
    int32_t amp = (2 + (t->p[P_LD_PIT] + t->p[P_LD_FLT] + t->p[P_LD_SHP] + t->p[P_LD_AMP]) / 64) * OG_Q;
    g[ROLE[w]] = 220;
    plate(PL_LFO3, 82, 58, 118, g, 0, 0, amp, ph);
    plate(PL_LFO2, 180, 58, 118, g, 0, 0, amp, ph + 16384u);
}

/* ENV: the curve plate, its curves quiet; the envelope itself drawn white in its box (ATK DEC SUS REL) */
static void graph_p_env(const track_t *t)
{
    static const int16_t G[8] = {-40, -120, -120, -120, -120, -120, -120, -120};
    int32_t cx = 120, cy = 62, sz = 150, u = sz * OG_Q / 126;
    int32_t a = t->p[P_ATK], d = t->p[P_DEC], s = t->p[P_SUS], r = t->p[P_REL];
    int32_t x0 = -48, x1, x2, x3, x4 = 50, yb = 8, yt = -36, ys;
    plate(PL_ENV, cx, cy, sz, G, 0, 0, 0, 0);
    x1 = x0 + 2 + a * 26 / 127;
    x2 = x1 + 2 + d * 26 / 127;
    x3 = x4 - 2 - r * 26 / 127;
    ys = yb + (yt - yb) * s / 127;
    og_line(cx * OG_Q + x0 * u, cy * OG_Q + yb * u, cx * OG_Q + x1 * u, cy * OG_Q + yt * u, T_TEXT);
    og_line(cx * OG_Q + x1 * u, cy * OG_Q + yt * u, cx * OG_Q + x2 * u, cy * OG_Q + ys * u, T_TEXT);
    og_line(cx * OG_Q + x2 * u, cy * OG_Q + ys * u, cx * OG_Q + x3 * u, cy * OG_Q + ys * u, T_TEXT);
    og_line(cx * OG_Q + x3 * u, cy * OG_Q + ys * u, cx * OG_Q + x4 * u, cy * OG_Q + yb * u, T_TEXT);
    og_node(cx * OG_Q + x1 * u, cy * OG_Q + yt * u, 2, T_ACCENT);
    og_node(cx * OG_Q + x3 * u, cy * OG_Q + ys * u, 2, T_THEME);
}

/* FX: the wing; its cyan membrane the sends (chorus, delay, reverb), its red veins the distortion; it trembles
 * with the delay */
static void graph_p_fx(const track_t *t)
{
    int16_t g[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int32_t send = (t->p[P_CHOR] + t->p[P_DLY] + t->p[P_REV]) / 3;
    g[3] = (int16_t)(send * 4 - 180);
    g[1] = (int16_t)(t->p[P_DIST] * 3 - 160);
    plate(PL_FX, 120, 58, 112, g, 0, 0, t->p[P_DLY] * OG_Q / 48, ui.frame * 1100u);
}

/* ARP: the small plant; its nodes light one after another with the arpeggio's steps (a note sounding), quiet when
 * the arp is off */
static void graph_p_arp(const track_t *t)
{
    uint8_t lit[16];
    int16_t g[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint32_t i, on = t->p[P_AMODE] != 0, k = t->arp_idx % 8u;
    for (i = 0; i < 16u; i++)
        lit[i] = (uint8_t)(on && t->arp_note && i % 8u == k ? 255u : 0u);
    if (!on)
        g[2] = g[1] = -160;
    plate(PL_ARP, 120, 58, 120, g, lit, 2, on ? 2 * OG_Q : 0, ui.frame * 700u);
}

/* VOICE: the vase of eight nodes, the eight shared voices; a node lit while this track's voice sounds */
static void graph_p_voices(const track_t *t)
{
    uint8_t lit[16];
    uint32_t i;
    memset(lit, 0, sizeof lit);
    for (i = 0; i < NVOICE && i < 16u; i++)
        lit[i] = (uint8_t)(t->v[i].active ? 128u + (uint32_t)(t->v[i].env_out >> 8) : 0u);
    plate(PL_VOICES, 120, 58, 124, 0, lit, 2, 0, 0);
}

/* an engine's plate (0: its own graph stays: SAMPLE and SLICE their wave; FM6's algorithm and WHEEL's drawbars are
 * drawn before this is asked) */
static uint32_t plate_of(const track_t *t)
{
    static const uint8_t PL_OF[14] = {PL_MIXER, PL_FM, PL_HOME, PL_VOICES, 0, PL_CHORD, PL_MACRO, PL_MAP, PL_PUNCH,
                                      PL_BUD, PL_DRUM, PL_BUD, PL_FM, 0};
    uint32_t e = t->eng_req % NENGINES;
    return e < 14u ? PL_OF[e] : 0u;
}

/* EDIT (a synth's own pages): the engine's plate; it glows with the track's level and sways with its LFO */
static void graph_p_engine(const track_t *t)
{
    int16_t g[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int32_t lv = plate_level(t);
    uint32_t n = plate_of(t);
    g[1] = (int16_t)(lv - 120);
    g[2] = g[3] = (int16_t)(lv - 60);
    plate(n ? n : PL_BUD, 120, 58, 124, g, 0, 0, mulq15(t->lfo_val, 3 * OG_Q), ui.frame * 500u);
}

/* DRUM (DRUM-X): the iris; its eight cyan nodes are the eight sounds, each lit by its hit as it decays (a muted
 * one dark); MORPH opens it (A closed .. B open), WARP makes it shiver. sel: the sound edited (EDIT > SOUND), -1 */
static void graph_p_drum(const track_t *t, int32_t sel)
{
    const drum_lane_t *K = drum_kit_of(t);
    int32_t morph = clamp(t->p[P_E0], 0, 127), warp = clamp(t->p[P_E6], 0, 127), size = 104 + morph * 24 / 127;
    uint8_t lit[16];
    int16_t g[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    uint16_t faint = ux_mix(T_SURF, T_TEXT, 30);
    uint32_t l;
    char b[16];
    memset(lit, 0, sizeof lit);
    for (l = 0; l < 8u; l++) {
        int32_t e = K && K[l].x.live ? K[l].x.ea >> 22 : 0;   /* 0..256 */
        lit[l] = (uint8_t)(sel >= 0 ? (l == (uint32_t)sel ? 255 : 0) : dx_lane_muted(l) ? 0 : 60 + (e > 195 ? 195 : e));
    }
    if (sel >= 0)
        g[0] = -100;
    plate(PL_DRUM, 120, 60, size, g, lit, 3, warp * OG_Q / 24, ui.frame * (2000u + (uint32_t)warp * 60u));
    og_dash(38 * OG_Q, 8 * OG_Q, 202 * OG_Q, 8 * OG_Q, faint, 2, 3);   /* the MORPH scale, A .. B */
    og_node((38 + morph * 164 / 127) * OG_Q, 8 * OG_Q, 2, T_ACCENT);
    og_label(22, 8, "A", morph < 64 ? T_THEME : T_DIM);
    og_label(218, 8, "B", morph >= 64 ? T_THEME : T_DIM);
    if (sel >= 0) {
        str_cpy(b, drum_lane_name(t, (uint32_t)sel), sizeof b);
        str_cpy(b + str_len(b), dx_ui_side ? " B" : " A", 4);
        plate_caption("SOUND", b);
        return;
    }
    str_cpy(b, "MORPH ", sizeof b);
    fmt_int(b + 6, morph * 100 / 127);
    str_cpy(b + str_len(b), "%", sizeof b - str_len(b));
    plate_caption("DRUM-X", b);
}
