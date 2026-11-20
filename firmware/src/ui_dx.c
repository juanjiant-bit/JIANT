/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): EDIT > SOUND and SOUND 2, a sound of the DRUM-X kit (docs/TONIC-DRUMX.md). */
/* Shown in the EDIT family on a DRUM track (ui.c page_visible). They edit the section's kit
 * (drumx_voice.c dx_kit, saved with the section: project.c FUNA):
 *   SOUND    LANE (the sound: BD .. CB; a key struck on the track picks its lane too), SIDE (patch A or B), PTCH, PMOD
 *   SOUND 2  DCAY, NOIS, COLR, MODE (the oscillator's wave, the noise filter, SNAP: 32 combinations; V10.7.8 the
 *            filter GR, the guiro's scrape: PMOD its SCRAPE, the stroke's length)
 *   SOUND 3  (JIANT 0.4) LANE again, PMOD's mode (DECAY LONG NOISE SINE), DRIVE (OFF / ON), the wave alone
 * The values are the side's; the MORPH plays between the two (EDIT 2 MRPH). The panel shows the kit's specimen
 * with the sound's organ singled out (ui_plates.c graph_p_drum: the iris, its node lit). */
static uint8_t dx_ui_seen = 0xFFu;                   /* the last key note followed */
static const char *const DX_LANE_ABBR[8] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};

/* a key struck on the track (seq.c key_on: last_note) picks the sound it plays */
static void dx_follow(void)
{
    if (last_note != dx_ui_seen) {
        if (dx_ui_seen != 0xFFu)                     /* (the first look only takes note of it) */
            dx_ui_lane = (uint8_t)drum_note_lane(TSEL,last_note);
        dx_ui_seen = last_note;
    }
}

/* the mode as one value 0..31: wave x 6 + filter x 2 + SNAP, and back; (V10.7.8) the filter GR's eight after them */
static uint32_t dx_mode_ix(uint32_t m)
{
    return ((m >> 2) & 3u) == DXF_GR ? 24u + (m & 3u) * 2u + ((m >> 4) & 1u) : (m & 3u) * 6u + ((m >> 2) & 3u) * 2u + ((m >> 4) & 1u);
}
static uint8_t dx_mode_of(uint32_t ix)
{
    return ix >= 24u ? (uint8_t)((ix - 24u) / 2u | DXF_GR << 2 | (ix & 1u) << 4) : (uint8_t)(ix / 6u | (ix % 6u / 2u) << 2 | (ix & 1u) << 4);
}
static void dx_mode_name(char *b, uint32_t m)        /* "FM BP+" (b: 8 bytes) */
{
    static const char *const W[4] = {"SIN", "FM", "MTL", "BEL"}, *const F[4] = {"LP", "BP", "HP", "GR"};
    str_cpy(b, W[m & 3u], 8);
    str_cpy(b + str_len(b), " ", 8 - str_len(b));
    str_cpy(b + str_len(b), F[(m >> 2) & 3u], 8 - str_len(b));
    if ((m >> 4) & 1u)
        str_cpy(b + str_len(b), "+", 8 - str_len(b));
}

/* (JIANT 0.6.54) the pages, the whole kit first, then a sound: KIT (EDIT 1), KIT 2 (EDIT 2), KIT 3 (TONE's BP and FEEDBK,
 * IMAGE: dx_kit3_*), X-MOD, FILTER, LEVELS; then SOUND 1..4, KNOB 1 the sound on each:
 *   (V10.7.63, the owner: "too many pages") SOUND LANE PITCH DECAY LEVEL     SOUND 3  LANE MODE PM.TYPE SEND
 *                                         SOUND 2  LANE NOISE COLOR PMOD    SOUND 4  LANE SIDE LAYER FEEDBK
 *   (LEVELS and STEREO gone: a sound's LEVEL here, IMAGE on FILTER; a sound's DRIVE off the panel, kept) */
static void knob_cap(track_t *t, uint32_t id);      /* (ui_input.c) */
static void dx_lane_turn(int32_t s) { dx_ui_lane = (uint8_t)clamp((int32_t)dx_ui_lane + (s > 0 ? 1 : -1), 0, 7); }
/* SOUND 4's SEND as one value: 0 OFF, then CHORUS, DELAY, REVERB 16 amounts each (1..48) */
static int32_t dx_send_ix(uint32_t sn)
{
    int32_t a = (DXS_AMT(sn) + 1) / 4;
    return DXS_BUS(sn) ? (int32_t)(DXS_BUS(sn) - 1u) * 16 + (a < 1 ? 1 : a > 16 ? 16 : a) : 0;
}
static uint8_t dx_send_of(int32_t ix)
{
    return ix <= 0 ? 0u : (uint8_t)((uint32_t)((ix - 1) / 16 + 1) << 6 | (uint32_t)(((ix - 1) % 16 + 1) * 4 - 1));
}
/* KNOB k (0..3) turned s on a SOUND page or X-MOD */
static __attribute__((noinline)) void dx_knob(const page_t *pg, uint32_t k, int32_t s)
{
    dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    uint8_t *v = dx_ui_side ? L->b : L->a;
    uint32_t pn = dx_pagen(pg);
    if (pn == 4u) {                                     /* (JIANT 0.5.1) X-MOD: the kit's, not a sound's */
        static const uint8_t MX[4] = {8, 127, 4, 127};
        int32_t v0 = dx_mot[k & 3u];
        dx_mot[k & 3u] = (uint8_t)clamp(v0 + (MX[k & 3u] > 8 ? accel(EN_K1 + k, s, 127) : (s > 0 ? 1 : -1)), 0, MX[k & 3u]);
        return;
    }
    if (k == 0u) {                                      /* every SOUND page: KNOB 1 the sound */
        dx_lane_turn(s);
        return;
    }
    if (pn == 8u) {                                     /* (V10.7.63) SOUND 4: SIDE, LAYER, FEEDBK */
        uint8_t *ly = &dx_lay[dx_ui_lane & 7u];
        int32_t la = (int32_t)DX_LAY_L(*ly), fb = (int32_t)DX_LAY_FB(*ly);
        if (k == 1u) { dx_ui_side = s > 0; return; }
        if (k == 2u) la = clamp(la + (s > 0 ? 1 : -1), 0, 15);
        else fb = clamp(fb + (s > 0 ? 1 : -1), 0, 15);
        *ly = (uint8_t)(la | fb << 4);
        return;
    }
    if (pn == 3u) {                                     /* SOUND 3: MODE, the pitch modulation's type, (V10.7.63) SEND (its
                                                         * DRIVE off the panel: the track's DIST, SENDS) */
        if (k == 1u)
            L->mode = (uint8_t)((L->mode & 0xE0u) |
                                (dx_mode_of((uint32_t)clamp((int32_t)dx_mode_ix(L->mode) + (s > 0 ? 1 : -1), 0, 31)) & 0x1Fu));
        else if (k == 2u)
            L->mode = (uint8_t)((L->mode & ~0x60u) | (uint32_t)clamp((int32_t)((L->mode >> 5) & 3u) + (s > 0 ? 1 : -1), 0, 3) << 5);
        else
            dx_snd[dx_ui_lane & 7u] = dx_send_of(clamp(dx_send_ix(dx_snd[dx_ui_lane & 7u]) + (s > 0 ? 1 : -1), 0, 48));
        return;
    }
    if (pn == 2u) {                                     /* (V10.7.63) SOUND 2: NOISE COLOR PMOD (the side's) */
        uint32_t i = k == 1u ? DXP_NOISE : k == 2u ? DXP_COLOR : DXP_PMOD;
        v[i] = (uint8_t)clamp(v[i] + accel(EN_K1 + k, s, 127), 0, 127);
        return;
    }
    if (k == 3u) {                                      /* (V10.7.63) SOUND: PITCH DECAY (the side's), LEVEL (LEVELS' gone) */
        uint32_t id = P_LN0 + (dx_ui_lane & 7u);
        TSEL->p[id] = (int16_t)clamp(TSEL->p[id] + accel(EN_K1 + k, s, 127), 0, 127);
        knob_cap(TSEL, id);
        return;
    }
    v[k == 1u ? DXP_PITCH : DXP_DECAY] = (uint8_t)clamp(v[k == 1u ? DXP_PITCH : DXP_DECAY] + accel(EN_K1 + k, s, 127), 0, 127);
}
/* KIT 3 (the whole kit): KNOB 1 BP, 2 FEEDBK (TONE's, dx_tone), (0.7.9) 3 HANDS (the CLAP's, dx_hands), (0.9.9-snd1)
 * 4 GLUE (the bus's, eng_drum.c dx_glue: KIT 3's free knob) */
static void dx_kit3_knob(uint32_t k, int32_t s)
{
    if(cur_page()->title[4]=='4') {                    /* KIT 4: KNOB 1 the KEY MAP (V10.6o: KNOB 1's ACOUST gone; codex/
                                                         * coherence: was KNOB 2) */
        if(!k) TSEL->p[P_DKEYMODE]=(int16_t)clamp(TSEL->p[P_DKEYMODE]+(s>0?1:-1),0,9);   /* (V10.6r) 2..9: OCT -1 a lane's chromatic run */
        return;
    }
    uint32_t bp = DX_TONE_BP(dx_tone), fb = DX_TONE_FB(dx_tone);
    if (k == 0u) bp = (uint32_t)clamp((int32_t)bp + (s > 0 ? 1 : -1), 0, 15);
    else if (k == 1u) fb = (uint32_t)clamp((int32_t)fb + (s > 0 ? 1 : -1), 0, 15);
    else if (k == 2u) dx_hands = (uint8_t)clamp((int32_t)dx_hands + (s > 0 ? 1 : -1), 0, 15);
    else dx_glue = (uint8_t)clamp((int32_t)dx_glue + (s > 0 ? 1 : -1), 0, 15);
    dx_tone = (uint8_t)(bp | fb << 4);
}
static __attribute__((noinline)) void dx_kit3_cards(void)
{
    if(cur_page()->title[4]=='4') {
        empty_column(1);
        hot_unit_column(0,"KEY MAP",drum_ext(TSEL)?"EXTENDED":"CLASSIC",!drum_ext(TSEL)?"":drum_chrom(TSEL)<0?"KIK/TOM":drum_lane_name(TSEL,(uint32_t)drum_chrom(TSEL)&7u));
        empty_column(2);
        empty_column(3);
        return;
    }
    uint32_t bp = DX_TONE_BP(dx_tone), fb = DX_TONE_FB(dx_tone);
    char val[12];
    if (bp) {                                           /* (its centre, as the filter sets it) */
        uint32_t hz = CUTOFF_HZ[78u + bp * 2u];
        if (hz < 1000u) fmt_int(val, (int32_t)hz);
        else fmt_fix(val, (int32_t)(hz / 100u), 1);
    } else
        str_cpy(val, "OFF", sizeof val);
    draw_column(0, "BP", val, bp ? (CUTOFF_HZ[78u + bp * 2u] < 1000u ? "Hz" : "kHz") : "", bp ? VAL(0u) : T_DIM,
                (int32_t)bp * 1000 / 15, ICON_TONE);
    fmt_int(val, (int32_t)fb);
    gauge_column(1, "FEEDBK", fb ? val : "OFF", fb ? VAL(1u) : T_DIM, (int32_t)fb * 1000 / 15);
    fmt_int(val, (int32_t)dx_hands);                    /* (0.7.9) the CLAP's HANDS */
    gauge_column(2, "HANDS", dx_hands ? val : "OFF", dx_hands ? VAL(2u) : T_DIM, (int32_t)dx_hands * 1000 / 15);
    fmt_int(val, (int32_t)dx_glue);                     /* (0.9.9-snd1) the bus's GLUE */
    gauge_column(3, "GLUE", dx_glue ? val : "OFF", dx_glue ? VAL(3u) : T_DIM, (int32_t)dx_glue * 1000 / 15);
}

/* the four cards of a SOUND page or X-MOD */
static void dx_cards(const page_t *pg)
{
    static const char *const BUS[4] = {"OFF", "CHO", "DLY", "REV"};
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    const uint8_t *v = dx_ui_side ? L->b : L->a;
    uint32_t pn = dx_pagen(pg);
    char val[12], ln[8];
    dx_follow();
    if (pn == 4u) {                                     /* (JIANT 0.5.1) X-MOD */
        static const char *const RT[9] = {"OFF", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
        static const char *const SH[5] = {"SINE", "TRI", "SAW", "RAMP", "S&H"};
        gauge_column(0, "RATE", RT[dx_mot[XM_RATE] % 9u], dx_mot[XM_RATE] ? VAL(0u) : T_DIM, dx_mot[XM_RATE] * 125);
        fmt_int(val, dx_mot[XM_DPTH]);
        gauge_column(1, "DEPTH", val, dx_mot[XM_DPTH] ? VAL(1u) : T_DIM, dx_mot[XM_DPTH] * 1000 / 127);
        hot_gauge_column(2, "SHAPE", SH[dx_mot[XM_SHPE] % 5u], dx_mot[XM_SHPE] * 250);
        fmt_int(val, dx_mot[XM_RAND]);
        gauge_column(3, "RAND", val, dx_mot[XM_RAND] ? VAL(3u) : T_DIM, dx_mot[XM_RAND] * 1000 / 127);
        return;
    }
    str_cpy(ln, DX_LANE_ABBR[dx_ui_lane & 7u], sizeof ln);   /* KNOB 1: the sound (SOUND, SOUND 2, SOUND 4 its side too: the
                                                         * values') */
    if (pn != 3u) {
        ln[2] = ' ';
        ln[3] = dx_ui_side ? 'B' : 'A';
        ln[4] = 0;
    }
    hot_gauge_column(0, "SOUND", ln, (int32_t)(dx_ui_lane & 7u) * 1000 / 7);
    if (pn == 8u) {                                     /* (V10.7.63) SOUND 4: SIDE LAYER FEEDBK */
        uint32_t ly = dx_lay[dx_ui_lane & 7u], la = DX_LAY_L(ly), fb = DX_LAY_FB(ly);
        hot_gauge_column(1, "SIDE", dx_ui_side ? "B" : "A", dx_ui_side ? 1000 : 0);
        fmt_int(val, (int32_t)la);
        gauge_column(2, "LAYER", la ? val : "OFF", la ? VAL(2u) : T_DIM, (int32_t)la * 1000 / 15);
        fmt_int(val, (int32_t)fb);
        gauge_column(3, "FEEDBK", fb ? val : "OFF", fb ? VAL(3u) : T_DIM, (int32_t)fb * 1000 / 15);
        return;
    }
    if (pn == 3u) {                                     /* SOUND 3: MODE PM.TYPE SEND */
        static const char *const PM[4] = {"DECAY", "LONG", "NOISE", "SINE"};
        uint32_t sn = dx_snd[dx_ui_lane & 7u];
        int32_t ix = dx_send_ix(sn);
        char un[8];
        dx_mode_name(val, L->mode);
        hot_gauge_column(1, "MODE", val, (int32_t)dx_mode_ix(L->mode) * 1000 / 31);
        hot_gauge_column(2, "PM.TYPE", PM[(L->mode >> 5) & 3u], (int32_t)((L->mode >> 5) & 3u) * 333);
        fmt_int(un, ix ? ((ix - 1) % 16 + 1) * 100 / 16 : 0);
        str_cpy(un + str_len(un), "%", 2);
        draw_column(3, "SEND", BUS[DXS_BUS(sn)], ix ? un : "", ix ? VAL(3u) : T_DIM, ix * 1000 / 48, ICON_MIX);
        return;
    }
    if (pn == 2u) {                                     /* SOUND 2: NOISE COLOR PMOD */
        fmt_int(val, v[DXP_NOISE]);
        hot_gauge_column(1, "NOISE", val, v[DXP_NOISE] * 1000 / 127);
        fmt_int(val, v[DXP_COLOR]);
        hot_gauge_column(2, "COLOR", val, v[DXP_COLOR] * 1000 / 127);
        fmt_int(val, v[DXP_PMOD]);
        hot_gauge_column(3, ((L->mode >> 2) & 3u) == DXF_GR ? "SCRAPE" : "PMOD", val, v[DXP_PMOD] * 1000 / 127);   /* (V10.7.8: GR's) */
        return;
    }
    fmt_int(val, v[DXP_PITCH]);                         /* SOUND: PITCH DECAY LEVEL */
    hot_gauge_column(1, "PITCH", val, v[DXP_PITCH] * 1000 / 127);
    fmt_int(val, v[DXP_DECAY]);
    hot_gauge_column(2, "DECAY", val, v[DXP_DECAY] * 1000 / 127);
    {
        int32_t lv = TSEL->p[P_LN0 + (dx_ui_lane & 7u)];
        fmt_int(val, lv * 100 / 127);
        hot_gauge_column(3, "LEVEL", val, lv * 1000 / 127);
    }
}
/* what the cards and the panel show changed */
static uint32_t dx_sig(void)
{
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    uint32_t h = (uint32_t)dx_ui_lane * 31u + dx_ui_side * 977u + L->mode * 131u + (uint32_t)TSEL->p[P_LN0 + (dx_ui_lane & 7u)] * 7919u, i;
    h ^= (uint32_t)dx_mot[0] << 24 ^ (uint32_t)dx_mot[1] << 16 ^ (uint32_t)dx_mot[2] << 8 ^ dx_mot[3];   /* (X-MOD) */
    h = (h ^ (uint32_t)TSEL->p[P_DKEYMODE] << 24 ^ dx_tone ^ (uint32_t)dx_hands << 8 ^ (uint32_t)dx_glue << 12) * 2654435761u;   /* (TONE, HANDS, GLUE) */
    for (i = 0; i < DV_NLANE; i++)                      /* (0.6.48) the SENDs, (0.6.49) the LAYERs */
        h = (h ^ dx_snd[i] ^ (uint32_t)dx_lay[i] << 8) * 16777619u;
    for (i = 0; i < DXP_N; i++)
        h = (h ^ (uint32_t)L->a[i] << 8 ^ L->b[i]) * 16777619u;
    return h;
}



/* (V10.6z6) EDIT > SWARM, SWARM 2 on a SWARM track (eng_input.c sw_sv): KNOB 1 SIDE (FAR / NEAR, as SOUND's A / B), then
 * the side's values: RATE MOVE TONE, NOISE MOD; SWARM 2's KNOB 4 NOW, where the morph is (0 FAR .. 128 NEAR), named by
 * what gives it (PROX, AIR, SELF: none). In the Bluetooth segment (its pages are a SWARM track's; without the segment
 * they show nothing and turn nothing) */
static uint8_t sw_ui_side;
static BTAPP_FN void sw_ui_knob_bt(const page_t *pg, uint32_t k, int32_t s)
{
    uint32_t q = pg->title[5] == ' ' ? 3u : 0u;
    if (k == 0u)
        sw_ui_side = s > 0;
    else if (k + q - 1u < SWS_N)
        sw_sv_put(TSEL, sw_ui_side, k + q - 1u, sw_sv(TSEL, sw_ui_side, k + q - 1u) + accel(EN_K1 + k, s, 127));
}
static BTAPP_FN void sw_ui_cards_bt(const page_t *pg)
{
    static const char *const LB[8] = {"RATE", "MOVE", "TONE", "NOISE", "MOD", "SELF", "AIR", "PROX"};
    uint32_t q = pg->title[5] == ' ' ? 3u : 0u, c, i;
    char val[8];
    hot_gauge_column(0, "SIDE", sw_ui_side ? "NEAR" : "FAR", sw_ui_side ? 1000 : 0);
    for (c = 1; c < 4u; c++) {                          /* (RATE as the note, 1/1 .. 1/32; NOW the morph, by its source) */
        int32_t v = (i = c + q - 1u) < SWS_N ? sw_sv(TSEL, sw_ui_side, i) : sm.mq;
        val[0] = '1', val[1] = '/';
        fmt_int(val + (i ? 0 : 2), i ? v : 1 << ((uint32_t)v * 6u >> 7));
        hot_gauge_column(c, LB[i < SWS_N ? i : 5u + sm.src % 3u], val, v * 1000 / 128);
    }
}
static void sw_ui_knob(const page_t *pg, uint32_t k, int32_t s) { if (bt_hw_seg()) sw_ui_knob_bt(pg, k, s); }
