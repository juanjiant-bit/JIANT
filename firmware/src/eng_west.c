/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* WEST (engine 11: NOISE's slot, its number, its first eight presets' places): west-coast synthesis, V10.7.37 remade
 * after the Buchla and Serge systems (their ideas: no code of anyone's). A complex oscillator, a principal core
 * (a sine, the Buchla 259's, or a triangle, Serge's) phase-modulated by a modulation oscillator (MOD, RATIO), into a
 * WAVEFOLDER (FOLD, SYMM), into a LOW-PASS GATE: a low-pass and a VCA driven together by a VACTROL (an LED lighting a
 * cell, the 292's), so a note is struck: a pluck, a bongo, a mallet, a bell.
 *   FOLD   the folder's gain, x1 .. x10: the core bent back on itself, more folds the further (a sine core through a
 *          sine fold: Bessel's sweep, the 259's timbre; a triangle core through a triangle fold: Serge's wave
 *          multiplier, glassier). The folds are integrated (first-order antiderivative anti-aliasing: each sample the
 *          fold's mean over the sample's span, not its point), so they sing instead of buzzing; the gain also stops
 *          short of a high note's Nyquist (the triangle's sooner: its corners' harmonics go on). The light folds it (louder brighter, as an LPG patch: the folder's gain
 *          0.4 .. 1 of FOLD with the cell's conductance), and so does a harder note.
 *   SYMM   an offset into the folder: asymmetric folds, even harmonics (a DC blocker after it).
 *   MOD    the modulation oscillator's depth on the core's phase (linear, through zero: FM), an index up to ~5;
 *          past 100 it also moves the core's amplitude (AM, up to half: the sidebands of a ring).
 *   RATIO  the modulator's ratio to the note: 0.5 1 1.41 1.5 1.73 2 2.5 2.76 3 3.5 4 5 5.40 6 7 8 (2.76 and 5.40 a
 *          free bar's modes, 1.41 and 1.73 roots: bells) with the sine core; then the same sixteen with a triangle
 *          core and fold (0.5T .. 8T: Serge). (V10.7.36's values 0..15 keep their meaning.)
 *   GATE   what the gate does: 0 a low-pass alone (the level the ADSR's), 64 both (the classic combo), 127 a VCA alone.
 *   DECAY  the vactrol's memory: its first halving 2 ms .. 2.6 s. Struck, the LED lights the cell for ~3 ms; the
 *          cell answers fast but not at once (~1 ms) and darkens slowly and not evenly: the darker, the slower (the
 *          light falls fast, then a long tail). Its conductance (the light to the 0.75) opens the filter (~8.6
 *          semitones an octave of light) and the VCA. Past 80 a held note keeps the LED lit (the ADSR's level, all
 *          of it at 127): it sustains.
 *   RES    the gate's filter resonance, bounded (Q up to ~3: a pinged gate rings down in pitch, the bongo's knock).
 *   DUST   (V10.8.28, the owner: the crackle gone) 1..95 white noise into the folder with the core, so it folds and
 *          gates with it (fused into the timbre, not beside it), and from 48 on some of it straight to the gate too
 *          (air); the core never fades. 96..127 METAL: instead of the noise, one of eight short register sequences, a
 *          period of it a period of the key (NOISE's META, Leo Kuroshita's, below), into the folder with the core
 *          at half: a metallic tone. 0: none at all.
 * The track's FLT moves the gate's ceiling (CUT, the matrix's CUT, LIFE's), SHP the fold, PIT the pitch; velocity
 * lights the cell brighter (the strike), folds harder and opens the ceiling. The ceiling follows the key by half.
 *
 * State (the voice's own): ph[0] the core's phase (UNISON spreads it), ph[1] the modulator's, ph[2] the noise's
 * generator (seeded by the note and the unison detune); s[0] s[1] the filter, s[2] the strike (the LED's pulse: its
 * level << 8 | its steps left), s[3] the dust's flashes waiting for the next step, s[4] the DC blocker, s[5] the
 * cell's darkness (-log2 of its light, Q20: the decay runs there, its rate the light's), s[6] the VCA's gain as last
 * ramped, s[7] the folder's last input (the antiderivative's other end). The vactrol and the filter's coefficients
 * step every 16 samples (WS_SUB), the gains ramp between them.
 *
 * NOISE (engine 11 until V10.7.35) is gone: a project or a user preset made with it opens as WEST in a DUST patch
 * (core.h west_from_noise, project.c proj_phys_gone, upreset.c up_migrate); its first eight presets are NOISE's names
 * remade (PROGRAM CHANGE, the generator's textures 4..7 and the stored preset numbers name what they named). */
static const char *const N_WEST_RATIO[] = {"0.5", "1", "1.41", "1.5", "1.73", "2", "2.5", "2.76",
                                           "3", "3.5", "4", "5", "5.40", "6", "7", "8",
                                           "0.5T", "1T", "1.41T", "1.5T", "1.73T", "2T", "2.5T", "2.76T",
                                           "3T", "3.5T", "4T", "5T", "5.40T", "6T", "7T", "8T"};
static const uint16_t WEST_RATIO[16] = {128, 256, 362, 384, 443, 512, 640, 706, 768, 896, 1024, 1280, 1383, 1536, 1792, 2048};
/* (V10.8.28) DUST's METAL, NOISE's META as Leo Kuroshita wrote it (eng_noise.c until V10.7.35): one period of a register's output bit (bit i of the words, from the state 1), its length,
 * and the levels of a 1 and a 0 (no DC, the larger at 16384). 7-bit taps 0x60 and 0x78, the 15-bit
 * register with its feedback from bits 0 and 6 (period 93), 6-bit 0x30 and 0x2D, 5-bit, 4-bit, 3-bit */
static const struct { uint32_t w[4]; uint8_t len; int16_t hi, lo; } NOISE_SEQ[8] = {
    {{0xD13C50C1u, 0x91C2F95Cu, 0xA58DED6Cu, 0x01FD533Bu}, 127, 16128, -16384},
    {{0xDBD165F1u, 0x9E46A1FDu, 0x84C15692u, 0x019B1CEBu}, 127, 16128, -16384},
    {{0x41008001u, 0x00492402u, 0x00824824u, 0x00000000u}, 93, 16384, -3404},
    {{0x27179461u, 0x03F566EDu, 0x00000000u, 0x00000000u}, 63, 15872, -16384},
    {{0x75A6C487u, 0x02FCA8CFu, 0x00000000u, 0x00000000u}, 63, 15872, -16384},
    {{0x05763E69u, 0x00000000u, 0x00000000u, 0x00000000u}, 31, 15360, -16384},
    {{0x00000F59u, 0x00000000u, 0x00000000u, 0x00000000u}, 15, 14336, -16384},
    {{0x0000001Du, 0x00000000u, 0x00000000u, 0x00000000u}, 7, 12288, -16384},
};
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
#define WS_SUB 16u                                      /* the vactrol's step (samples) */
#define WL_DARK (15 << 20)                              /* the cell's darkness at its darkest (Q20 octaves): shut */
/* 2^-x, x Q12 octaves (>= 0), Q15 (a quadratic between the octaves) */
static int32_t west_exp2n(int32_t x)
{
    uint32_t i = (uint32_t)x >> 12, f = ((uint32_t)x & 4095u) << 4;
    if (i >= 15u)
        return 0;
    return (int32_t)((65536u - ((f * 43024u) >> 16) + ((((f * f) >> 16) * 10256u) >> 16)) >> (i + 1u));
}
/* -log2(l / 32768), l Q15 (1 .. 32768), Q20 octaves */
static int32_t west_log2n(int32_t l)
{
    int32_t e = 1, m;
    if (l <= 0)
        return WL_DARK;
    while (l < 16384) {
        l <<= 1;
        e++;
    }
    m = l - 16384;                                      /* log2(1 + m) ~ m (1.3465 - 0.3465 m), m Q14 */
    return (e << 20) - ((m * (22061 - ((5677 * m) >> 14))) >> 8);
}
/* the folds: f(p) (Q15) and its antiderivative F(p) over the folder's input p (a quarter turn = full scale: the
 * core at 1, the gain at 1). tri 0: a sine fold, F the cosine (its scale is the divisor's, west_render); tri 1: a
 * triangle fold, F its parabolas (Q15: 0 .. 1) */
static int32_t west_f(uint32_t p, int tri)
{
    int32_t s = (int32_t)p;
    if (!tri)
        return sine_i(p);
    if (s > 0x40000000 || s < -0x40000000)
        s = (int32_t)(0x80000000u - p);
    return s >> 15;
}
static int32_t west_F(uint32_t p, int tri)
{
    int32_t s = (int32_t)p;
    if (!tri)
        return sine_i(p + 0x40000000u);
    if (s > 0x40000000 || s < -0x40000000) {
        s = (int32_t)(0x80000000u - p) >> 15;
        return 32768 - ((s * s) >> 16);
    }
    s >>= 15;
    return (s * s) >> 16;
}

static void west_note_on(track_t *t, voice_t *v)
{
    uint32_t seed = (v->note + 1u) * 0x9E3779B9u ^ (uint32_t)v->fine * 0x85EBCA6Bu;   /* (UNISON: each its own) */
    (void)t;
    v->ph[0] = v->ph[1] = 0;                            /* (from silence: the same strike every time; a sounding
                                                         * voice keeps its phases, its cell and its filter: voice.c) */
    v->ph[2] = seed ? seed : 1u;
    if (!v->env_out) {                                  /* (from silence; sounding, the fold's gain and the FM's
                                                         * depth go on from where they are: no step) */
        v->s[2] = v->s[3] = 0;
        v->s[0] = v->s[1] = v->s[4] = v->s[6] = v->s[7] = 0;
        v->s[5] = WL_DARK;
    }
    v->s[2] = (v->s[2] & ~15) | 12;                     /* the strike: the LED lit ~4 ms (render: harder brighter) */
}

static void west_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int32_t md = clamp(p[P_E2], 0, 127), gate = clamp(p[P_E4], 0, 127), rt = clamp(p[P_E3], 0, 31);
    int32_t dec = clamp(p[P_E5], 0, 127), dust = clamp(p[P_E7], 0, 127), vel = v->vel, tri = rt >> 4;
    int32_t ic1 = v->s[0], ic2 = v->s[1], strk = v->s[2] & 15, gq = (int32_t)((uint32_t)v->s[2] >> 4), dq = v->s[3];
    int32_t dc = v->s[4], dark = v->s[5], ga = v->s[6];
    int32_t xg0 = v->s[7], F0 = 0, fv = -1;
    uint32_t p0, cph = v->ph[0], mph = v->ph[1], inc = m->inc, off = (uint32_t)clamp(p[P_E1], 0, 127) << 22;
    uint32_t minc = (inc >> 8) * WEST_RATIO[rt & 15], dep = (uint32_t)(md * md * 13) >> 1;   /* (state in locals:
                                                         * out[] may alias v->s[]) */
    uint32_t r = 1753000000u / (inc | 1u), i = 0;      /* r: ~18 kHz / the note */
    int32_t rng = (int32_t)v->ph[2], kd = 8192 - clamp(p[P_E6], 0, 127) * 6800 / 127, ks, hold = 0, nmix = 0, air = 0, sel = -1;
    ks = kd < 5461 ? kd * 3 >> 1 : 8192;               /* (V10.7.41, the owner: RES 100 broke on the peaks) past RES ~43 the
                                                         * gate's input lowered with its resonance (x0.25 at 127): its ring kept
                                                         * inside the gate's product (it wrapped 32 bits) */
    int32_t fdep = (127 - gate) * 520, vdep = gate * 520, gcap, fg, ceil, flo, kf, k0;
    int32_t am = md > 100 ? (md - 100) * 600 : 0;
    {   /* FOLD (+ SHP, velocity), squared: 0 .. 2016 (Q8 over x1) */
        int32_t f = clamp((p[P_E0] << 8) + m->shape - (64 << 8) + (vel - 64) * 40, 0, 127 << 8);
        fg = ((f >> 4) * (f >> 4)) >> 11;
    }
    gcap = r >= 16u ? 4096 : r <= 3u ? 256 : (int32_t)(r - 2u) * 180;
    if (tri)                                            /* (the triangle's own harmonics go on: its folds a sixth as far,
                                                         * the folded corners' rate under ~3 kHz) */
        gcap = r >= 96u ? 4096 : r <= 6u ? 256 : (int32_t)r * 43;
    /* the gate's ceiling (+ FLT, velocity, the key by half) and its floor (the key, half an octave under it) */
    uint32_t vsh = VH_SH(), rep;                        /* (V10.7.56) at half the rate: the key's share of the ceiling as at
                                                         * the full rate (pitch16 is an octave up there), the vactrol's
                                                         * steps run as often a second, the DC block and DUST's rate kept */
    ceil = clamp((112 << 8) + m->cutoff + (vel - 64) * 48 + (m->pitch16 - 60 * 16) * 9 - 1728 * (int32_t)vsh, 0, 127 << 8);
    flo = clamp((m->pitch16 - 362) * 19 - (6 << 8), 0, ceil);
    fdep = fdep > 32767 ? 32767 : fdep;
    vdep = vdep > 32767 ? 32767 : vdep;
    /* the darkening a step (Q20 octaves) in the dark, k0, and 31 times that at full light (k0 + 30 k0 x the
     * conductance): an octave darker in a seventieth of the time to -40 dB, the ninth in a quarter of it; the cell
     * ~9 octaves dark (-40 dB) in 20 ms .. 10 s */
    k0 = (west_exp2n(dec * 290) * 1693) >> 10;
    kf = (k0 * 15) >> 5;
    if (v->gate && dec > 80)                            /* held: the LED lit by the ADSR */
        hold = mulq15((dec - 80) * 697, m->envq15);
    if (dust >= 96)                                     /* METAL: the register sequence (NOISE_SEQ) */
        sel = (dust - 96) >> 2;
    else if (dust) {                                    /* the noise into the folder, the air past 48 */
        nmix = dust * 172;
        air = dust > 48 ? (dust - 48) * 400 : 0;
    }
    p0 = ((uint32_t)xg0 << 7) + off;                    /* (F0 read when needed: fv -1) */
    while (i < n) {
        uint32_t len = n - i < WS_SUB ? n - i : WS_SUB, end = i + len;
        int32_t li, gc, lf, lv, g, dt, dga, dg, dd, drv;
        tsvf_t c;
        for (rep = 0; rep < (1u << vsh); rep++) {       /* (V10.7.56: a step a WS_SUB samples of the full rate) */
            drv = hold;
            if (strk) {                                 /* the strike's pulse: the LED, harder brighter */
                drv = 11000 + vel * 171 > drv ? 11000 + vel * 171 : drv;
                strk--;
            }
            li = west_exp2n(dark >> 8);                 /* the cell's light, Q15 */
            if (drv > li) {                             /* lit: quickly, not at once (~1.5 .. 3 ms) */
                li += ((drv - li) * (4096 + (drv >> 3))) >> 15;
                dark = west_log2n(li);
            } else {                                    /* darkening: slower the darker */
                dark += k0 + ((kf * west_exp2n((dark >> 8) * 3 >> 2)) >> 9);
                dark = dark > WL_DARK ? WL_DARK : dark;
            }
        }
        gc = dark >= WL_DARK ? 0 : west_exp2n((dark >> 8) * 3 >> 2);   /* the conductance: the light to the 0.75 */
        lf = mulq15(fdep, (dark >> 8) * 7 >> 4);        /* the filter's share of it (~6 st an octave), the VCA's */
        lv = 32767 - mulq15(vdep, 32767 - gc);
        tsvf_coef_k(&c, ceil - lf < flo ? flo : ceil - lf, kd);
        g = 256 + ((fg * (13107 + mulq15(19660, gc))) >> 15);   /* the fold: 0.4 .. 1 of FOLD with the light */
        g = (g > gcap ? gcap : g) << 8;
        dt = ((int32_t)(dep >> 5) * (9830 + mulq15(22937, gc))) >> 10;   /* the FM's index: 0.3 .. 1 of MOD's */
        dga = len == WS_SUB ? (lv - ga) >> 4 : (lv - ga) / (int32_t)len;
        dg = len == WS_SUB ? (g - gq) >> 4 : (g - gq) / (int32_t)len;
        dd = len == WS_SUB ? (dt - dq) >> 4 : (dt - dq) / (int32_t)len;
        for (; i < end; i++) {
            int32_t y = 0, v1, v2, v3;
            uint32_t q = 0;
            {
                int32_t x, ms = 0, xg, d, F1;
                uint32_t ph = cph, p1;
                if (dep) {
                    ms = sine_i(mph);
                    mph += minc;
                    dq += dd;
                    ph += (uint32_t)ms * (uint32_t)dq;
                }
                x = tri ? osc_tri(ph + 0x40000000u) : sine_i(ph);
                if (sel >= 0) {                         /* METAL: the sequence's step at the key's phase, the core half */
                    uint32_t j = ((cph >> 16) * NOISE_SEQ[sel].len) >> 16;
                    x = (x >> 1) + ((NOISE_SEQ[sel].w[j >> 5] >> (j & 31u)) & 1u ? NOISE_SEQ[sel].hi : NOISE_SEQ[sel].lo);
                } else if (nmix) {
                    q = noise32(&rng);
                    x += mulq15((int32_t)(q >> 16) - 32768, nmix);
                }
                gq += dg;
                xg = x * (gq >> 8);                     /* the folder's input: Q23, a quarter turn at 1 */
                p1 = ((uint32_t)xg << 7) + off;
                d = xg - xg0;
                if ((uint32_t)(d + (1 << 18)) >= 2u << 18) {   /* the fold's mean over the sample (F's slope) */
                    if (fv < 0)
                        F0 = west_F(p0, tri);
                    F1 = west_F(p1, tri);
                    y = tri ? (F1 - F0) * 32768 / (d >> 8) : (F0 - F1) * 20861 / (d >> 8);
                    F0 = F1;
                    fv = 0;
                } else {                                /* (a short span: the fold at its middle) */
                    y = west_f(p1 - ((uint32_t)d << 6), tri);
                    fv = -1;
                }
                p0 = p1;
                xg0 = xg;
                if (am)
                    y = mulq15(y, 32767 - am + mulq15(am, ms));
                dc += ((y << 8) - dc) >> (9u - vsh);    /* (~14 Hz: SYMM's DC gone within ~12 ms) */
                y -= dc >> 8;
            }
            cph += inc;
            if (air) {                                  /* the air: the same noise's other half, past the folder */
                y += mulq15((int32_t)(q & 0xFFFFu) - 32768, air);
                y = clamp(y, -65535, 65535);
            }
            /* the gate's low-pass (dsp.c tsvf_lp, rounded: no DC walk at low cutoffs), at half level (the sums of
             * its products inside 32 bits with the dust's peaks) */
            v3 = (((y >> 1) * ks) >> 13) - ic2;
            v1 = (c.a1 * ic1 + c.a2 * v3 + 4096) >> 13;
            v2 = ic2 + ((c.a2 * ic1 + c.a3 * v3 + 4096) >> 13);
#ifdef WEST_PROBE
            west_hits += (uint32_t)(2 * v1 - ic1 > 150000 || 2 * v1 - ic1 < -150000 || 2 * v2 - ic2 > 150000 || 2 * v2 - ic2 < -150000);
            west_wrap += (uint32_t)((v2 << 1) > 65535 || (v2 << 1) < -65535);
#endif
            ic1 = clamp(2 * v1 - ic1, -150000, 150000);
            ic2 = clamp(2 * v2 - ic2, -150000, 150000);
            ga += dga;
            out[i] += voice_amp(soft_knee(mulq15(v2 << 1, ga), 26000), m, i);
        }
        ga = lv;
        gq = g;
        dq = dt;
    }
    v->s[0] = ic1;
    v->s[1] = ic2;
    v->s[2] = (int32_t)((uint32_t)gq << 4) | strk;
    v->s[3] = dq;
    v->s[4] = dc;
    v->s[5] = dark;
    v->s[6] = ga;
    v->s[7] = xg0;
    v->ph[0] = cph;
    v->ph[1] = mph;
    v->ph[2] = (uint32_t)rng;
}

/* {FOLD, SYMM, MOD, RATIO, GATE, DECAY, RES, DUST}. 0..7: NOISE's names in their places, remade as DUST and noise
 * through the gate (the generator's textures are 4..7); then the west coast: plucks, bongos, mallets, bells, folded
 * basses, leads, drones (WEST_ORDER: those first). (V10.7.37) revoiced for the vactrol and the folds: the struck ones
 * leave their tails to the cell (REL long: the ADSR only ends them), the held ones (DECAY past 80) sustain */
static const preset_t WEST_PRESETS[] = {
    {"WIND", {0, 0, 0, 1, 0, 112, 100, 95}, {90, 90, 110, 90}, 0, TAG(FX), FX(0, 30, 20, 80), PAT(5),
     MODS(MMOD(1, PS_MODW, PD_E(6), 30), P_LWAVE, LW_DRIFT, P_LRATE, 20, P_LD_FLT, 30)},   /* MODW: it howls, gusts */
    /* the dust flashes the cell: drops of the note, each its own height and place */
    {"RAIN", {30, 0, 0, 1, 64, 26, 70, 70}, {0, 64, 127, 50}, 0, TAG(FX), FX(0, 0, 20, 70), PAT(5),
     MODS(MMOD(1, PS_RAND, PD_CUT, 30), MMOD(2, PS_RAND, PD_PAN, 40))},
    /* a folded zap: ENV 2 drops it an octave into the note, RAND its ratio */
    {"ARCADE", {100, 40, 50, 16, 80, 56, 30, 0}, {0, 80, 0, 50}, 0, TAG(FX), FX(0, 0, 10, 20), PAT(3),
     MODS(MENV2(0, 40, 0, 30), MMOD(1, PS_ENV2, PD_PITCH, 63), MMOD(2, PS_RAND, PD_E(3), 20))},
    /* clangorous: a triangle core at a bar's second mode, folded hard with DUST's METAL (V10.8.28: NOISE's META 93) */
    {"METAL", {85, 30, 75, 23, 96, 76, 30, 104}, {0, 75, 127, 70}, 10, TAG(FX), FX(0, 20, 30, 45), PAT(7),
     MODS(MMOD(1, PS_VEL, PD_CUT, 20))},
    {"SEA", {0, 0, 0, 1, 0, 104, 30, 95}, {100, 90, 120, 110}, 0, TAG(FX), FX(0, 20, 20, 80), PAT(5),
     MODS(P_LRATE, 10, P_LD_FLT, 20, P_LD_AMP, 30)},
    {"HISS PAD", {20, 0, 0, 1, 30, 120, 60, 90}, {80, 90, 105, 100}, 0, TAG(PAD), FX(0, 40, 30, 85), PAT(5),
     BREATHE(24, 0, 1), MODS(MMOD(1, PS_AT, PD_E(6), 30))},
    {"INIT NOISE", {0, 0, 0, 1, 127, 127, 0, 95}, {0, 64, 57, 20}, 0, TAG(FX), FX(0, 0, 0, 0)},
    {"TAPE AIR", {0, 0, 0, 1, 20, 100, 20, 80}, {90, 90, 115, 110}, 0, TAG(FX), FX(0, 30, 25, 70), PAT(5),
     MODS(P_LWAVE, LW_DRIFT, P_LRATE, -1, P_LD_FLT, 10)},
    /* (V10.7.36, revoiced V10.7.37) the west coast. A sine core folded, the gate pinged: the classic pluck */
    {"WEST PLUCK", {52, 12, 0, 1, 64, 68, 30, 0}, {0, 64, 127, 80}, 0, TAG(PLUCK), FX(0, 0, 25, 30), PAT(3)},
    /* the bongo: a gate pinged short and resonant (its knock falls in pitch with the light), the core barely
     * folded, a fifth above it in the FM */
    {"BONGO", {16, 0, 22, 3, 64, 50, 100, 0}, {0, 64, 127, 70}, 0, TAG(PLUCK), FX(0, 0, 10, 25), PAT(6)},
    /* a wooden bar: the 4th partial from the modulator, the gate mostly a low-pass, a soft mallet */
    {"MARIMBA", {10, 0, 34, 10, 56, 66, 18, 0}, {0, 64, 110, 85}, 0, TAG(KEYS), FX(0, 0, 15, 35), PAT(3)},
    /* a struck bell: 1.41 (inharmonic), the gate mostly a VCA (it stays bright as it rings), a long memory */
    {"WEST BELL", {12, 0, 70, 2, 104, 78, 0, 0}, {0, 64, 127, 100}, 0, TAG(BELL), FX(0, 0, 20, 55), PAT(7)},
    /* bronze: a free bar's 2.76 in the FM, folded asymmetric, the gate rings long */
    {"GAMELAN", {34, 24, 52, 7, 84, 74, 22, 0}, {0, 64, 127, 100}, 0, TAG(BELL), FX(0, 10, 25, 50), PAT(7)},
    /* a triangle core folded (Serge), the gate a low-pass mostly: it growls and closes */
    {"FOLD BASS", {70, 30, 8, 17, 36, 70, 34, 0}, {0, 64, 100, 60}, 0, 1 | TAG(BASS), FX(0, 0, 0, 10), PAT(2)},
    /* the octave below in the FM, a resonant low-pass gate: it bounces */
    {"RUBBER BASS", {26, 0, 48, 0, 16, 52, 84, 0}, {0, 64, 64, 50}, 0, 1 | TAG(BASS), FX(0, 0, 0, 5), PAT(2)},
    /* a held line: the triangle core folded, the cell kept lit while held (DECAY 112), MODW folds it */
    {"WEST LEAD", {48, 20, 14, 21, 64, 112, 26, 0}, {0, 64, 100, 60}, 0, 1 | TAG(LEAD), FX(0, 10, 30, 30), PAT(4),
     MODS(P_GLIDE, 12, MMOD(1, PS_MODW, PD_E(0), 40))},
    /* the sine folded to its limit and offset, pressure folds it, a vibrato */
    {"FOLD LEAD", {82, 46, 20, 1, 60, 120, 36, 0}, {0, 64, 92, 55}, 0, 1 | TAG(LEAD), FX(10, 0, 25, 25), PAT(4),
     MODS(MMOD(1, PS_AT, PD_E(0), 35), MLFO(LW_SIN, 50, 6, 0))},
    /* the folds breathing slowly (the LFO on SHP), a triangle core through a low-pass gate held open */
    {"FOLD DRONE", {56, 10, 12, 21, 28, 127, 30, 0}, {70, 64, 60, 90}, 0, TAG(PAD), FX(0, 30, 30, 80), PAT(5),
     MODS(MLFO(LW_TRI, 8, 0, 0), P_LD_SHP, 40)},
    /* a held tone the dust keeps flashing, drifting */
    {"DUST DRONE", {30, 24, 22, 5, 30, 104, 50, 30}, {80, 64, 44, 100}, 0, TAG(PAD), FX(0, 20, 40, 90), PAT(5),
     MODS(P_LWAVE, LW_DRIFT, P_LRATE, 12, P_LD_SHP, 30)},
    {"INIT WEST", {40, 0, 0, 1, 64, 66, 20, 0}, {0, 64, 127, 70}, 0, TAG(KEYS), FX(0, 0, 0, 0)},
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
        {"SKEW", F_PCT, 0, 127, 0, 0},
        {"MOD", F_PCT, 0, 127, 0, 0},
        {"RATIO", F_ENUM, 0, 31, 1, N_WEST_RATIO},
        {"GATE", F_PCT, 0, 127, 64, 0},
        {"DECAY", F_PCT, 0, 127, 60, 0},
        {"RES", F_PCT, 0, 127, 20, 0},
        {"DUST", F_PCT, 0, 127, 0, 0},
    },
    .presets = WEST_PRESETS,
    .npresets = NELEM(WEST_PRESETS),
    .note_on = west_note_on,
    .render = west_render,
    .half = 1,                   /* (V10.7.56) the generic half rate; the vactrol, the key's ceiling, DC, DUST follow VH_SH */
    .knob = {P_E0, P_E2, P_E5, P_E7},
    .keep = 0xF3,                /* the filter, the DC blocker, the cell, the gain, the folder's input (a retrigger strikes
                                  * the LED anew: the light from where it is) */
};
