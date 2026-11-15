/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): EDIT > SOUND .. SOUND 3, a sound of the DRUM-X kit (docs/TONIC-DRUMX.md). */
/* Shown in the EDIT family on a DRUM track (ui.c page_visible). They edit the section's kit
 * (drumx_voice.c dx_kit, saved with the section: project.c FUNA), the sound a key struck on the track last (dx_follow;
 * V10.7.37: no SOUND knob any more, the panel's header names it):
 *   SOUND    PITCH, DECAY, SIDE (patch A or B), PMOD (V10.7.8 the filter GR, the guiro's scrape: PMOD its SCRAPE)
 *   SOUND 2  NOISE (the kick's CLICK), COLOR, DRIVE (OFF / ON), MODE (the oscillator's wave, the noise filter, SNAP: 32
 *            combinations; V10.7.8 the filter GR)
 *   SOUND 3  PM.TYPE (DECAY LONG NOISE SINE), LAYER, FEEDBK, SEND
 * The values are the side's; the MORPH plays between the two (EDIT 1 MORPH). The panel shows the kit with the sound
 * lit (ui_engvis.c vx_kit_page). */
static uint8_t dx_ui_seen = 0xFFu;                   /* the last key note followed */

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

/* (JIANT 0.6.54) the pages, the whole kit first, then a sound; (V10.7.37, the owner) three sound pages, four values each,
 * the sound the one a key struck last on the track (dx_follow; the panel's header names it):
 *   SOUND    PITCH DECAY SIDE PMOD         SOUND 3  PM.TYPE LAYER FEEDBK SEND (the bus and how much on one knob)
 *   SOUND 2  NOISE COLOR DRIVE MODE */
/* SOUND 3's SEND as one value: 0 OFF, then CHORUS, DELAY, REVERB 16 amounts each (1..48) */
static int32_t dx_send_ix(uint32_t sn)
{
    int32_t a = (DXS_AMT(sn) + 1) / 4;
    return DXS_BUS(sn) ? (int32_t)(DXS_BUS(sn) - 1u) * 16 + (a < 1 ? 1 : a > 16 ? 16 : a) : 0;
}
static uint8_t dx_send_of(int32_t ix)
{
    return ix <= 0 ? 0u : (uint8_t)((uint32_t)((ix - 1) / 16 + 1) << 6 | (uint32_t)(((ix - 1) % 16 + 1) * 4 - 1));
}
static const uint8_t DX_PV[2][4] = {{DXP_PITCH, DXP_DECAY, 0xFF, DXP_PMOD}, {DXP_NOISE, DXP_COLOR, 0xFF, 0xFF}};   /* SOUND,
                                                         * SOUND 2: the side's value each knob turns (0xFF: not one) */
/* KNOB k (0..3) turned s on a SOUND page or KIT LFO */
static __attribute__((noinline)) void dx_knob(const page_t *pg, uint32_t k, int32_t s)
{
    dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    uint8_t *v = dx_ui_side ? L->b : L->a;
    uint32_t pn = dx_pagen(pg), i;
    int32_t d = s > 0 ? 1 : -1;
    k &= 3u;
    if (pn == 4u) {                                     /* (JIANT 0.5.1) KIT LFO: the kit's, not a sound's */
        static const uint8_t MX[4] = {8, 127, 4, 127};
        int32_t v0 = dx_mot[k];
        dx_mot[k] = (uint8_t)clamp(v0 + (MX[k] > 8 ? accel(EN_K1 + k, s, 127) : d), 0, MX[k]);
        return;
    }
    if (pn == 3u) {                                     /* SOUND 3: PM.TYPE, LAYER, FEEDBK, SEND */
        uint8_t *ly = &dx_lay[dx_ui_lane & 7u];
        int32_t la = (int32_t)DX_LAY_L(*ly), fb = (int32_t)DX_LAY_FB(*ly);
        if (k == 0u)
            L->mode = (uint8_t)((L->mode & ~0x60u) | (uint32_t)clamp((int32_t)((L->mode >> 5) & 3u) + d, 0, 3) << 5);
        else if (k == 1u) la = clamp(la + d, 0, 15);
        else if (k == 2u) fb = clamp(fb + d, 0, 15);
        else dx_snd[dx_ui_lane & 7u] = dx_send_of(clamp(dx_send_ix(dx_snd[dx_ui_lane & 7u]) + d, 0, 48));
        *ly = (uint8_t)(la | fb << 4);
        return;
    }
    if ((i = DX_PV[pn - 1u][k]) < DXP_N) {              /* SOUND, SOUND 2: the side's values */
        v[i] = (uint8_t)clamp(v[i] + accel(EN_K1 + k, s, 127), 0, 127);
        return;
    }
    if (pn == 1u)                                       /* SOUND's SIDE */
        dx_ui_side = s > 0;
    else if (k == 2u)                                   /* SOUND 2's DRIVE (OFF / ON) */
        L->mode = (uint8_t)(s > 0 ? L->mode | DX_DRIVE : L->mode & ~DX_DRIVE);
    else                                                /* .. and MODE */
        L->mode = (uint8_t)((L->mode & 0xE0u) | (dx_mode_of((uint32_t)clamp((int32_t)dx_mode_ix(L->mode) + d, 0, 31)) & 0x1Fu));
}
/* KIT 3 (the whole kit): KNOB 1 BP, 2 FEEDBK (TONE's, dx_tone), (0.7.9) 3 HANDS (the CLAP's, dx_hands), (0.9.9-snd1)
 * 4 GLUE (the bus's, eng_drum.c dx_glue: KIT 3's free knob) */
static void dx_kit3_knob(uint32_t k, int32_t s)
{
    uint32_t bp = DX_TONE_BP(dx_tone), fb = DX_TONE_FB(dx_tone);
    if (k == 0u) bp = (uint32_t)clamp((int32_t)bp + (s > 0 ? 1 : -1), 0, 15);
    else if (k == 1u) fb = (uint32_t)clamp((int32_t)fb + (s > 0 ? 1 : -1), 0, 15);
    else if (k == 2u) dx_hands = (uint8_t)clamp((int32_t)dx_hands + (s > 0 ? 1 : -1), 0, 15);
    else dx_glue = (uint8_t)clamp((int32_t)dx_glue + (s > 0 ? 1 : -1), 0, 15);
    dx_tone = (uint8_t)(bp | fb << 4);
}
/* (V10.7.37) OUT's KNOB 4, the KEY MAP (KIT 4's until then): 0 CLASSIC, 1 EXTENDED (the -1 octave KICK / TOM), 2..9
 * EXTENDED, the -1 octave one sound's chromatic run (eng_drum.c drum_chrom); its card */
static void dx_keymap_turn(int32_t s) { TSEL->p[P_DKEYMODE] = (int16_t)clamp(TSEL->p[P_DKEYMODE] + (s > 0 ? 1 : -1), 0, 9); }
static void dx_keymap_card(uint32_t c)
{
    hot_text_column(c, "KEY MAP", !drum_ext(TSEL) ? "CLASSIC" : drum_chrom(TSEL) < 0 ? "KIK/TOM" :
                    drum_lane_name(TSEL, (uint32_t)drum_chrom(TSEL) & 7u));   /* (EXTENDED: its -1 octave's) */
}
static __attribute__((noinline)) void dx_kit3_cards(void)
{
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

/* the four cards of a SOUND page or KIT LFO */
static void dx_cards(const page_t *pg)
{
    static const char *const BUS[4] = {"OFF", "CHO", "DLY", "REV"};
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    const uint8_t *v = dx_ui_side ? L->b : L->a;
    uint32_t pn = dx_pagen(pg), k;
    char val[12];
    dx_follow();
    if (pn == 4u) {                                     /* (JIANT 0.5.1) KIT LFO */
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
    if (pn == 3u) {                                     /* SOUND 3 */
        static const char *const PM[4] = {"DECAY", "LONG", "NOISE", "SINE"};
        uint32_t ly = dx_lay[dx_ui_lane & 7u], la = DX_LAY_L(ly), fb = DX_LAY_FB(ly), sn = dx_snd[dx_ui_lane & 7u];
        int32_t ix = dx_send_ix(sn);
        char un[8];
        hot_gauge_column(0, "PM.TYPE", PM[(L->mode >> 5) & 3u], (int32_t)((L->mode >> 5) & 3u) * 333);
        fmt_int(val, (int32_t)la);
        gauge_column(1, "LAYER", la ? val : "OFF", la ? VAL(1u) : T_DIM, (int32_t)la * 1000 / 15);
        fmt_int(val, (int32_t)fb);
        gauge_column(2, "FEEDBK", fb ? val : "OFF", fb ? VAL(2u) : T_DIM, (int32_t)fb * 1000 / 15);
        fmt_int(un, ix ? ((ix - 1) % 16 + 1) * 100 / 16 : 0);
        str_cpy(un + str_len(un), "%", 2);
        draw_column(3, "SEND", BUS[DXS_BUS(sn)], ix ? un : "", ix ? VAL(3u) : T_DIM, ix * 1000 / 48, ICON_MIX);
        return;
    }
    for (k = 0; k < 4u; k++) {                          /* SOUND, SOUND 2: the side's values */
        uint32_t i = DX_PV[pn - 1u][k];
        if (i < DXP_N) {
            static const char *const NM[DXP_N] = {"PITCH", "PMOD", "DECAY", "NOISE", "COLOR"};
            const char *nm = NM[i];
            if (i == DXP_PMOD && ((L->mode >> 2) & 3u) == DXF_GR)   /* (V10.7.8: GR's) */
                nm = "SCRAPE";
            if (i == DXP_NOISE && (dx_ui_lane & 7u) == DV_KICK)   /* (V10.7.37) the kick's: its click (drumx_voice.c) */
                nm = "CLICK";
            fmt_int(val, v[i]);
            hot_gauge_column(k, nm, val, v[i] * 1000 / 127);
        }
    }
    if (pn == 1u) {
        hot_gauge_column(2, "SIDE", dx_ui_side ? "B" : "A", dx_ui_side ? 1000 : 0);
        return;
    }
    gauge_column(2, "DRIVE", L->mode & DX_DRIVE ? "ON" : "OFF", L->mode & DX_DRIVE ? VAL(2u) : T_DIM, L->mode & DX_DRIVE ? 1000 : 0);
    dx_mode_name(val, L->mode);
    hot_gauge_column(3, "MODE", val, (int32_t)dx_mode_ix(L->mode) * 1000 / 31);
}
/* what the cards and the panel show changed */
static uint32_t dx_sig(void)
{
    const dx_lane_t *L = &dx_kit[dx_ui_lane & 7u];
    uint32_t h = (uint32_t)dx_ui_lane * 31u + dx_ui_side * 977u + L->mode * 131u, i;
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
