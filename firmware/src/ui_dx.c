/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT (FELUCCA TONIC): EDIT > SOUND and SOUND 2, a sound of the DRUM-X kit (docs/TONIC-DRUMX.md). */
/* Shown in the EDIT family on a DRUM track (ui.c page_visible). They edit the section's kit
 * (drumx_voice.c dx_kit, saved with the section: project.c FUNA):
 *   SOUND    LANE (the sound: BD .. CB; a key struck on the track picks its lane too), SIDE (patch A or B), PTCH, PMOD
 *   SOUND 2  DCAY, NOIS, COLR, MODE (the oscillator's wave, the noise filter, SNAP: 24 combinations)
 * The values are the side's; the MORPH plays between the two (EDIT 2 MRPH). The panel shows the kit's specimen
 * with the sound's organ singled out (ui_graph.c graph_drumx). */
static uint8_t dx_ui_seen = 0xFFu;                   /* the last key note followed */
static const char *const DX_LANE_ABBR[8] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};

/* a key struck on the track (seq.c key_on: last_note) picks the sound it plays */
static void dx_follow(void)
{
    if (last_note != dx_ui_seen) {
        if (dx_ui_seen != 0xFFu)                     /* (the first look only takes note of it) */
            dx_ui_lane = (uint8_t)drum_lane(last_note);
        dx_ui_seen = last_note;
    }
}

/* the mode as one value 0..23: wave x 6 + filter x 2 + SNAP, and back */
static uint32_t dx_mode_ix(uint32_t m) { return (m & 3u) * 6u + ((m >> 2) & 3u) * 2u + ((m >> 4) & 1u); }
static uint8_t dx_mode_of(uint32_t ix) { return (uint8_t)(ix / 6u | (ix % 6u / 2u) << 2 | (ix & 1u) << 4); }
static void dx_mode_name(char *b, uint32_t m)        /* "FM BP+" (b: 8 bytes) */
{
    static const char *const W[4] = {"SIN", "FM", "MTL", "BEL"}, *const F[3] = {"LP", "BP", "HP"};
    str_cpy(b, W[m & 3u], 8);
    str_cpy(b + str_len(b), " ", 8 - str_len(b));
    str_cpy(b + str_len(b), F[((m >> 2) & 3u) % 3u], 8 - str_len(b));
    if ((m >> 4) & 1u)
        str_cpy(b + str_len(b), "+", 8 - str_len(b));
}

/* KNOB k (0..3) turned s on the page */
static void dx_knob(const page_t *pg, uint32_t k, int32_t s)
{
    dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    uint8_t *v = dx_ui_side ? L->b : L->a;
    if (!dx_page2(pg)) {
        if (k == 0u)
            dx_ui_lane = (uint8_t)clamp((int32_t)dx_ui_lane + (s > 0 ? 1 : -1), 0, 7);
        else if (k == 1u)
            dx_ui_side = s > 0;
        else
            v[k == 2u ? DXP_PITCH : DXP_PMOD] = (uint8_t)clamp(v[k == 2u ? DXP_PITCH : DXP_PMOD] + accel(EN_K1 + k, s, 127),
                                                              0, 127);
        return;
    }
    if (k < 3u) {
        uint32_t i = k == 0u ? DXP_DECAY : k == 1u ? DXP_NOISE : DXP_COLOR;
        v[i] = (uint8_t)clamp(v[i] + accel(EN_K1 + k, s, 127), 0, 127);
    } else {
        L->mode = dx_mode_of((uint32_t)clamp((int32_t)dx_mode_ix(L->mode) + (s > 0 ? 1 : -1), 0, 23));
    }
}

/* the four cards */
static void dx_cards(const page_t *pg)
{
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    const uint8_t *v = dx_ui_side ? L->b : L->a;
    char val[12];
    dx_follow();
    if (!dx_page2(pg)) {
        draw_column(0, "LANE", DX_LANE_ABBR[dx_ui_lane & 7u], "", VAL(0u), (int32_t)(dx_ui_lane & 7u) * 1000 / 7,
                    ICON_X_PATTERN);
        draw_column(1, "SIDE", dx_ui_side ? "B" : "A", "", VAL(1u), dx_ui_side ? 1000 : 0, ICON_MIX);
        fmt_int(val, v[DXP_PITCH]);
        draw_column(2, "PTCH", val, "", VAL(2u), v[DXP_PITCH] * 1000 / 127, ICON_PITCH);
        fmt_int(val, v[DXP_PMOD]);
        draw_column(3, "PMOD", val, "", VAL(3u), v[DXP_PMOD] * 1000 / 127, ICON_ENV);
        return;
    }
    fmt_int(val, v[DXP_DECAY]);
    draw_column(0, "DCAY", val, "", VAL(0u), v[DXP_DECAY] * 1000 / 127, ICON_DECAY);
    fmt_int(val, v[DXP_NOISE]);
    draw_column(1, "NOIS", val, "", VAL(1u), v[DXP_NOISE] * 1000 / 127, ICON_NOISE);
    fmt_int(val, v[DXP_COLOR]);
    draw_column(2, "COLR", val, "", VAL(2u), v[DXP_COLOR] * 1000 / 127, ICON_TONE);
    dx_mode_name(val, L->mode);
    draw_column(3, "MODE", val, "", VAL(3u), (int32_t)dx_mode_ix(L->mode) * 1000 / 23, ICON_WAVE);
}
/* what the cards and the panel show changed */
static uint32_t dx_sig(void)
{
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    uint32_t h = (uint32_t)dx_ui_lane * 31u + dx_ui_side * 977u + L->mode * 131u, i;
    for (i = 0; i < DXP_N; i++)
        h = (h ^ (uint32_t)L->a[i] << 8 ^ L->b[i]) * 16777619u;
    return h;
}
