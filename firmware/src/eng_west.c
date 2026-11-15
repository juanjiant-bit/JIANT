/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* WEST (V10.7.36, engine 11: NOISE's slot, its number, its first eight presets' places): west-coast synthesis. A sine
 * core, phase-modulated by a second sine (MOD, RATIO), into a WAVEFOLDER (FOLD, SYMM), into a LOW-PASS GATE: a
 * low-pass filter and a VCA driven together by one envelope through a model of a vactrol (a lamp lighting a cell:
 * it opens quickly and closes late, slower the darker it gets), so a note is struck: a pluck, a bongo, a bell.
 *   FOLD   the folder's gain: the sine bent back on itself, x1 .. x9 (sin(pi/2 * g * x): its level never grows, its
 *          harmonics do, odd ones, lively and metallic). The strike folds harder (the gate's level moves it, 1/2 .. 1),
 *          and a harder note too. The gain stops short of aliasing high notes (its top harmonic under ~18 kHz).
 *   SYMM   a DC offset into the folder: asymmetric folds, even harmonics (a DC blocker after it).
 *   MOD    the modulator's depth on the core's phase (FM), up to an index of ~5.
 *   RATIO  the modulator's ratio to the note: 0.5 1 1.41 1.5 1.73 2 2.5 2.76 3 3.5 4 5 5.40 6 7 8 (2.76 and 5.40 a
 *          free bar's modes, 1.41 and 1.73 roots: bells).
 *   GATE   what the gate does: 0 a low-pass alone (its level stays: the ADSR's), 64 both (the classic), 127 a VCA
 *          alone (the folds as they are).
 *   DECAY  the strike's decay, 5 ms .. 4 s (DECAY_K); past 88 a held note keeps the gate partly open (127: open), a
 *          drone. A released note's gate closes within ~150 ms at most.
 *   RES    the gate's filter resonance.
 *   DUST   noise into the gate: 1..72 random impulses (crackle, 1 .. 600 a second) that also strike the gate a
 *          little (the dust pings it: ghost notes, drops); 72..96 the core fades out (the impulses alone); 96..127
 *          hiss comes in (white noise, at 127 full) and the impulses get denser (to ~2000 a second). 0: none at all.
 * The track's FLT moves the gate's ceiling (CUT, the matrix's CUT, LIFE's), SHP the fold, PIT the pitch; velocity
 * folds harder and opens the ceiling (+-10 semitones). The ceiling follows the key by half, the closed gate the key.
 *
 * State (the voice's own: no memory of the engine's): ph[0] the core's phase (UNISON spreads it), ph[1] the
 * modulator's, ph[2] the noise's generator (seeded by the note and the unison detune); s[0] s[1] the filter, s[2]
 * the lamp (the strike's level, decaying), s[3] the dust's strikes waiting for the next step, s[4] the DC blocker,
 * s[5] the cell (the gate's level), s[6] the VCA's gain as last ramped. The envelope and the filter's coefficients
 * step every 16 samples (WS_SUB), the gain ramps between them.
 *
 * NOISE (engine 11 until V10.7.35) is gone: a project or a user preset made with it opens as WEST in a DUST patch
 * (core.h west_from_noise, project.c proj_phys_gone, upreset.c up_migrate); its first eight presets are NOISE's names
 * remade (PROGRAM CHANGE, the generator's textures 4..7 and the stored preset numbers name what they named). */
static const char *const N_WEST_RATIO[] = {"0.5", "1", "1.41", "1.5", "1.73", "2", "2.5", "2.76",
                                           "3", "3.5", "4", "5", "5.40", "6", "7", "8"};
static const uint16_t WEST_RATIO[16] = {128, 256, 362, 384, 443, 512, 640, 706, 768, 896, 1024, 1280, 1383, 1536, 1792, 2048};
/* phase increment of pitch p (1/16 semitone) past the table's top, up to one step a sample (NOISE's, kept here:
 * eng_animal.c uses it) */
static uint32_t noise_inc(int32_t p)
{
    uint32_t sh = 0, inc;
    while (p > 2047) {
        p -= 192;
        sh++;
    }
    inc = pitch_inc((uint32_t)clamp(p, 0, 2047));
    return sh && inc >= 0x80000000u >> (sh - 1u) ? 0xFFFFFFFFu : inc << sh;
}
/* the integer square root (NOISE's, kept here: voice.c's LP24 tf_lp24 uses it) */
static uint32_t isqrt32(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x)
        b >>= 2;
    while (b) {
        if (x >= r + b) {
            x -= r + b;
            r = (r >> 1) + b;
        } else {
            r >>= 1;
        }
        b >>= 2;
    }
    return r;
}
#define WS_SUB 16u                                      /* the envelope's step (samples) */

static void west_note_on(track_t *t, voice_t *v)
{
    uint32_t seed = (v->note + 1u) * 0x9E3779B9u ^ (uint32_t)v->fine * 0x85EBCA6Bu;   /* (UNISON: each its own) */
    (void)t;
    v->ph[0] = v->ph[1] = 0;                            /* (from silence: the same strike every time; a sounding
                                                         * voice keeps its phases, voice.c) */
    v->ph[2] = seed ? seed : 1u;
    v->s[0] = v->s[1] = v->s[3] = v->s[4] = v->s[5] = v->s[6] = v->s[7] = 0;
    v->s[2] = 14000 + v->vel * 147;                     /* the strike: the lamp lit, harder brighter */
}

static void west_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t symm = clamp(p[P_E1], 0, 127) * 129, md = clamp(p[P_E2], 0, 127), gate = clamp(p[P_E4], 0, 127);
    int32_t dec = clamp(p[P_E5], 0, 127), dust = clamp(p[P_E7], 0, 127), vel = v->vel;
    int32_t ic1 = v->s[0], ic2 = v->s[1], drv = v->s[2], pend = v->s[3], dc = v->s[4], cell = v->s[5], ga = v->s[6];
    uint32_t cph = v->ph[0], mph = v->ph[1], inc = m->inc;   /* (state in locals: out[] may alias v->s[]) */
    uint32_t minc = (inc >> 8) * WEST_RATIO[(uint32_t)clamp(p[P_E3], 0, 15)], dep = (uint32_t)(md * md * 13) >> 1;
    uint32_t r = 1753000000u / (inc | 1u), k, i = 0;    /* r: ~18 kHz / the note */
    int32_t rng = (int32_t)v->ph[2], kd = 8192 - clamp(p[P_E6], 0, 127) * 7600 / 127, hold = 0, osc = 32767, hiss = 0;
    int32_t thr = 0, kick = dust * 200, fdep = (127 - gate) * 520, vdep = gate * 520, gcap, fg, ceil, flo;
    {   /* FOLD (+ SHP, velocity), squared: 0 .. 2016 (Q8 over x1) */
        int32_t f = clamp((p[P_E0] << 8) + m->shape - (64 << 8) + (vel - 64) * 40, 0, 127 << 8);
        fg = ((f >> 4) * (f >> 4)) >> 11;
    }
    gcap = r >= 16u ? 4096 : r <= 3u ? 256 : (int32_t)(r - 2u) * 160;
    /* the gate's ceiling (+ FLT, velocity, the key by half) and its floor (the key, half an octave under it) */
    ceil = clamp((112 << 8) + m->cutoff + (vel - 64) * 48 + (m->pitch16 - 60 * 16) * 9, 0, 127 << 8);
    flo = clamp((m->pitch16 - 362) * 19 - (6 << 8), 0, ceil);
    fdep = fdep > 32767 ? 32767 : fdep;
    vdep = vdep > 32767 ? 32767 : vdep;
    /* the strike's decay a step: DECAY_K (per sample, Q16) to the 16th; released, ~150 ms at most */
    k = DECAY_K[v->gate ? (dec > 126 ? 126 : dec) : (dec > 64 ? 64 : dec)];
    k = (k * k) >> 16;
    k = (k * k) >> 16;
    k = (k * k) >> 16;
    k = (k * k) >> 16;
    if (v->gate && dec > 88)
        hold = (dec - 88) * 840;
    if (dust) {                                         /* DUST: the impulses' rate 2^(e8 / 256) a second */
        int32_t e8 = dust <= 72 ? dust * 33 : dust <= 96 ? 2376 : 2376 + (dust - 96) * 15;
        uint32_t rate = 1u << (e8 >> 8);
        rate += (rate * (uint32_t)(e8 & 255)) >> 8;
        thr = (int32_t)((rate * 97391u) >> 16);         /* per 2^16 of a sample */
        osc = dust <= 72 ? 32767 : dust >= 96 ? 0 : (96 - dust) * 1365;
        hiss = dust > 96 ? (dust - 96) * 1057 : 0;
    }
    while (i < n) {
        uint32_t len = n - i < WS_SUB ? n - i : WS_SUB, end = i + len;
        int32_t lf, lv, g, dga;
        tsvf_t c;
        drv = hold + (((drv - hold) * (int32_t)(k >> 1)) >> 15);   /* the lamp fades */
        drv = clamp(drv + pend, 0, 32767);              /* .. and the dust strikes it */
        pend = 0;
        if (drv > cell)                                 /* the cell: quick to light, slow to darken */
            cell += (drv - cell + 1) >> 1;
        else
            cell += (drv - cell) >> (cell < 8192 ? 6 : 5);
        lf = 32767 - mulq15(fdep, 32767 - cell);        /* the filter's share of it, the VCA's */
        lv = 32767 - mulq15(vdep, 32767 - cell);
        tsvf_coef_k(&c, flo + mulq15(ceil - flo, lf), kd);
        g = 256 + ((fg * (128 + (cell >> 8))) >> 8);
        if (g > gcap)
            g = gcap;
        dga = len == WS_SUB ? (lv - ga) >> 4 : (lv - ga) / (int32_t)len;
        for (; i < end; i++) {
            int32_t x, y, v1, v2, v3;
            if (dep) {
                int32_t ms = sine_i(mph);
                mph += minc;
                x = sine_i(cph + (uint32_t)ms * dep);
            } else {
                x = sine_i(cph);
            }
            cph += inc;
            y = sine_i((uint32_t)(((x * g) >> 8) + symm) << 15);   /* the fold: sin(pi/2 * (g x + SYMM)) */
            dc += ((y << 8) - dc) >> 9;                 /* (~14 Hz: SYMM's DC gone within ~12 ms) */
            y -= dc >> 8;
            if (osc != 32767)
                y = mulq15(y, osc);
            if (dust) {
                uint32_t q = noise32(&rng);
                if (hiss)
                    y += mulq15((int32_t)(q >> 16) - 32768, hiss);
                if ((int32_t)(q & 0xFFFFu) < thr) {     /* an impulse, random size and sign */
                    int32_t a = 8192 + (int32_t)((q >> 18) & 0x7FFFu);
                    y += (q & 0x20000u) ? a : -a;
                    pend += (a * kick) >> 16;
                }
            }
            /* the gate's low-pass (dsp.c tsvf_lp, rounded: no DC walk at low cutoffs), at half level (the sums of
             * its products inside 32 bits with the dust's peaks) */
            v3 = (clamp(y, -65535, 65535) >> 1) - ic2;
            v1 = (c.a1 * ic1 + c.a2 * v3 + 4096) >> 13;
            v2 = ic2 + ((c.a2 * ic1 + c.a3 * v3 + 4096) >> 13);
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            ga += dga;
            out[i] += voice_amp(soft_knee(mulq15(v2 << 1, ga), 26000), m, i);
        }
        ga = lv;
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = drv;
    v->s[3] = pend;
    v->s[4] = dc;
    v->s[5] = cell;
    v->s[6] = ga;
    v->ph[0] = cph;
    v->ph[1] = mph;
    v->ph[2] = (uint32_t)rng;
}

/* {FOLD, SYMM, MOD, RATIO, GATE, DECAY, RES, DUST}. 0..7: NOISE's names in their places, remade as DUST and noise
 * through the gate (the generator's textures are 4..7); then the west coast: plucks, bongos, mallets, bells, folded
 * basses, leads, drones (WEST_ORDER: those first) */
static const preset_t WEST_PRESETS[] = {
    {"WIND", {0, 0, 0, 1, 0, 112, 100, 120}, {90, 90, 127, 90}, 0, TAG(FX), FX(0, 30, 20, 80), PAT(5),
     MODS(MMOD(1, PS_MODW, PD_E(6), 30), P_LWAVE, LW_DRIFT, P_LRATE, 20, P_LD_FLT, 30)},   /* MODW: it howls, gusts */
    /* the dust pings the gate: drops of the note, each its own height and place */
    {"RAIN", {40, 0, 0, 1, 64, 8, 60, 56}, {0, 64, 127, 50}, 0, TAG(FX), FX(0, 0, 20, 70), PAT(5),
     MODS(MMOD(1, PS_RAND, PD_CUT, 30), MMOD(2, PS_RAND, PD_PAN, 40))},
    /* a folded zap: ENV 2 drops it an octave into the note, RAND its ratio */
    {"ARCADE", {100, 40, 50, 0, 80, 30, 30, 0}, {0, 80, 0, 50}, 0, TAG(FX), FX(0, 0, 10, 20), PAT(3),
     MODS(MENV2(0, 40, 0, 30), MMOD(1, PS_ENV2, PD_PITCH, 63), MMOD(2, PS_RAND, PD_E(3), 20))},
    {"METAL", {90, 30, 70, 7, 90, 70, 40, 6}, {0, 75, 40, 60}, 10, TAG(FX), FX(0, 20, 30, 45), PAT(7),
     MODS(MMOD(1, PS_VEL, PD_CUT, 20))},
    {"SEA", {0, 0, 0, 1, 0, 104, 30, 118}, {100, 90, 120, 110}, 0, TAG(FX), FX(0, 20, 20, 80), PAT(5),
     MODS(P_LRATE, 10, P_LD_FLT, 20, P_LD_AMP, 30)},
    {"HISS PAD", {20, 0, 0, 1, 30, 120, 60, 112}, {80, 90, 115, 100}, 0, TAG(PAD), FX(0, 40, 30, 85), PAT(5),
     BREATHE(24, 0, 1), MODS(MMOD(1, PS_AT, PD_E(6), 30))},
    {"INIT NOISE", {0, 0, 0, 1, 127, 127, 0, 127}, {0, 64, 57, 20}, 0, TAG(FX), FX(0, 0, 0, 0)},
    {"TAPE AIR", {0, 0, 0, 1, 20, 100, 20, 104}, {90, 90, 120, 110}, 0, TAG(FX), FX(0, 30, 25, 70), PAT(5),
     MODS(P_LWAVE, LW_DRIFT, P_LRATE, -1, P_LD_FLT, 10)},
    /* (V10.7.36) the west coast */
    {"WEST PLUCK", {45, 0, 0, 1, 64, 40, 20, 0}, {0, 64, 127, 40}, 0, TAG(PLUCK), FX(0, 0, 20, 30), PAT(3)},
    {"BONGO", {25, 10, 20, 3, 70, 18, 60, 0}, {0, 64, 127, 30}, 0, TAG(PLUCK), FX(0, 0, 10, 25), PAT(6)},
    {"MARIMBA", {20, 0, 25, 10, 64, 50, 10, 0}, {0, 64, 127, 50}, 0, TAG(KEYS), FX(0, 0, 15, 35), PAT(3)},
    {"WEST BELL", {15, 0, 75, 2, 100, 92, 0, 0}, {0, 64, 127, 80}, 0, TAG(BELL), FX(0, 0, 20, 55), PAT(7)},
    {"GAMELAN", {40, 20, 55, 7, 90, 80, 15, 0}, {0, 64, 127, 70}, 0, TAG(BELL), FX(0, 10, 25, 50), PAT(7)},
    {"FOLD BASS", {70, 30, 10, 0, 40, 60, 30, 0}, {0, 64, 127, 30}, 0, 1 | TAG(BASS), FX(0, 0, 0, 10), PAT(2)},
    {"RUBBER BASS", {35, 0, 45, 1, 30, 35, 70, 0}, {0, 64, 127, 25}, 0, 1 | TAG(BASS), FX(0, 0, 0, 5), PAT(2)},
    {"WEST LEAD", {50, 20, 15, 5, 70, 112, 25, 0}, {0, 64, 127, 40}, 0, 1 | TAG(LEAD), FX(0, 10, 30, 30), PAT(4),
     MODS(P_GLIDE, 12, MMOD(1, PS_MODW, PD_E(0), 40))},            /* MODW folds it */
    {"FOLD LEAD", {75, 45, 25, 1, 60, 118, 40, 0}, {0, 64, 127, 45}, 0, 1 | TAG(LEAD), FX(10, 0, 25, 25), PAT(4),
     MODS(MMOD(1, PS_AT, PD_E(0), 35), MLFO(LW_SIN, 50, 6, 0))},  /* pressure folds it, a vibrato */
    {"FOLD DRONE", {55, 10, 15, 5, 30, 126, 30, 0}, {60, 64, 127, 90}, 0, TAG(PAD), FX(0, 30, 30, 80), PAT(5),
     MODS(MLFO(LW_TRI, 8, 0, 0), P_LD_SHP, 40)},                   /* the folds breathing slowly */
    {"DUST DRONE", {30, 40, 20, 0, 20, 124, 50, 30}, {80, 64, 127, 100}, 0, TAG(PAD), FX(0, 20, 40, 90), PAT(5),
     MODS(P_LWAVE, LW_DRIFT, P_LRATE, 12, P_LD_SHP, 30)},
    {"INIT WEST", {30, 0, 0, 1, 64, 60, 0, 0}, {0, 64, 127, 40}, 0, TAG(KEYS), FX(0, 0, 0, 0)},
};
static const uint8_t WEST_ORDER[] = {8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,   /* the west coast, to play */
                                     5, 7, 4, 0, 3,                              /* NOISE's textures, remade */
                                     1, 2,                                       /* experiments */
                                     19, 6};                                     /* INIT */
_Static_assert(sizeof WEST_ORDER == NELEM(WEST_PRESETS), "WEST's browsing order: every preset once");
#define WEST_PC 8u                                      /* PROGRAM CHANGE reaches NOISE's eight (ui.c pc_shown) */

static const engine_t ENG_WEST = {
    .name = "WEST",
    .page_title = {"FOLD", "GATE"},
    .edit = {
        {"FOLD", F_PCT, 0, 127, 40, 0},
        {"SYMM", F_PCT, 0, 127, 0, 0},
        {"MOD", F_PCT, 0, 127, 0, 0},
        {"RATIO", F_ENUM, 0, 15, 1, N_WEST_RATIO},
        {"GATE", F_PCT, 0, 127, 64, 0},
        {"DECAY", F_PCT, 0, 127, 60, 0},
        {"RES", F_PCT, 0, 127, 20, 0},
        {"DUST", F_PCT, 0, 127, 0, 0},
    },
    .presets = WEST_PRESETS,
    .npresets = NELEM(WEST_PRESETS),
    .note_on = west_note_on,
    .render = west_render,
    .knob = {P_E0, P_E2, P_E5, P_E7},
    .keep = 0x73,                /* the filter, the DC blocker, the cell, the gain (a retrigger strikes the lamp anew) */
};
