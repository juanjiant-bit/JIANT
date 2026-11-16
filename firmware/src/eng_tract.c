/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.43) TRACT: VOICE's second MODE (EDIT > WORDS KNOB 4: VOWEL / TRACT), a vocal tract simulated as a tube.
 * An original implementation, written from the textbook descriptions only (no other synthesizer's code read):
 *   J. L. Kelly, C. Lochbaum, "Vocal tract modeling", Proc. 4th Int. Congr. Acoustics (1962): the tube as cylindrical
 *     sections, a scattering junction between two, k = (A1 - A2) / (A1 + A2) of their areas;
 *   J. O. Smith III, "Physical Audio Signal Processing" (CCRMA, 2010): digital waveguides, the one-multiply junction,
 *     the N-port junction of a branch; G. Fant, "Acoustic Theory of Speech Production" (1960), B. H. Story, "A
 *     parametric model of the vocal tract area function" (JASA 117, 2005; the idea of a neutral tube moved by a few
 *     shapes) and J. Mullen's thesis (York, 2006) for the tube's geometry and losses; A. E. Rosenberg, "Effect of
 *     glottal pulse shape on the quality of natural vowels" (JASA 49, 1971) for the source.
 *
 * The tube: 22 sections glottis -> lips (0.78 cm each: 17.1 cm, an adult male's), the waves in both directions. A
 * section is half a sample of the tract's rate (22.05 kHz) long: the lattice of one-sample sections at 44.1 kHz falls
 * in two halves that never meet, so only one is computed (each tract sample: the even junctions, then the odd ones):
 * the audio band to 11 kHz, half the work, the output interpolated to 44.1 kHz (as voice.c voice_half). Each junction
 * one multiply (w = k (f - b): f + w on, b + w back); the glottis reflects 0.78 .. 0.88 (TENSE) and adds the flow; the
 * lips reflect -0.9 at DC, less as the frequency rises (radiation: the highs leave), the nostrils the same. The nose:
 * 12 sections (9.4 cm) from a three-port junction at the velum (section 10, the pressure p = sum 2 A_i b_i / sum A_i,
 * each port's wave out p - its wave in), its port the velum's opening (NASAL; m n ng): a zero in the mouth's spectrum
 * where the nose resonates against it (an antiformant). Output: the volume velocity out of the lips and the nostrils,
 * differenced (radiation: +6 dB / octave and no DC, a DC block exact).
 *
 * The area function (cm^2 per section, the EDIT screen draws it): a neutral tube moved by the tongue, a raised cosine
 * constriction of ~7.7 sections at a place between pharynx and palate (FRONT, OPEN: high vowels constrict the mouth,
 * low ones the pharynx), the cavities behind and before it widened or narrowed with it, the jaw (OPEN) widening the
 * mouth, the lips' opening (OPEN, ROUND); consonants (WORDS) a second, narrow constriction at their place (closed: p t
 * k b d g m n ng; narrow: f s sh ..). The shape's twelve numbers were fitted (least squares on a chain-matrix model,
 * then checked on this lattice) to male vowel formants (Peterson & Barney 1952, Hillenbrand et al. 1995): AH 770 1100
 * Hz, EE 250 2050, OO 280 840 at C3 (tests/tract_test.c). The source: a Rosenberg flow pulse (rising half cosine,
 * falling quarter cosine, closed), its open quotient and closing speed by TENSE, with aspiration noise louder while the
 * glottis is open (BREATH; at the top only the noise: a whisper); frication noise enters the tube at the consonant's
 * constriction (WORDS: s sh f th x and the bursts).
 *
 * Organic: the shape glides to its targets (~23 ms, the closures ~4 ms), each period its own length and level (jitter,
 * shimmer), a vibrato (VIB, RATE) coming in over ~0.4 s, the tongue wandering a little on its own. WORDS (LANG on): the
 * articulation of sing.c (its formants, gains and phonemes, the vowel on the step as VOICE's) moves the tube: its F1 F2
 * and the rounding give FRONT OPEN ROUND (the fitted inverse), a nasal opens the velum, the consonants close at their
 * place. Two voices a track (engine poly: voice.c trk_nvoice); the shape is the track's (a choir sings one vowel).
 *
 * Memory: none of its own. The waves and the shape live in the COMB filter's lines (comb_buf, as STRING's): a TRACT
 * part has no COMB (tract_block turns a COMB TYPE into BP). Fixed point: waves int32 (under 2^16), k Q14 (|k| < 0.97),
 * the velum's gains Q13: every product in 32 bits, no division in the sample loop (the coefficients once a block).
 * EDIT 1 FRONT OPEN ROUND NASAL, EDIT 2 BREATH VIB RATE TENSE (WORDS: POS THEME ARTIC NASAL | BREATH VIB RATE LOOP);
 * ENV / LFO -> FILTER moves FRONT, -> SHAPE TENSE. */
#define TR_N 22                  /* oral sections, glottis (0) .. lips (21) */
#define TR_M 12                  /* nasal sections, velum (0) .. nostrils (11) */
#define TR_J 10                  /* the velum's junction: between oral sections 9 and 10 (even: the lips' half) */
#define TR_NV 2                  /* voices a track */
#define TR_GAIN 18               /* the radiated sample's gain (an open AH at C3 as loud as VOICE's CHOIR AAH) */
#define TR_W (2 * TR_N + 2 * TR_M)   /* a voice's waves (int32) */
typedef struct {
    int32_t k[TR_N];             /* junction j (1..21) between sections j - 1 and j, Q14; k[0]: the source's gain (Q8:
                                  * a volume velocity into the glottis's section, 1 / its area) */
    int32_t kn[TR_M];            /* the nose's junctions (Q14: 1 the velum's port's, 2 .. 11 TR_KN's) */
    int32_t g1, g2, g3;          /* the velum's three-port (Q13) */
    int32_t gl[6];               /* as glided (Q16 of 0..256): FRONT OPEN ROUND, the velum (Q8 cm^2 << 8), the
                                  * consonant's place (Q8 sections << 8) and its closure */
    int32_t g[3][2];             /* voicing, aspiration, frication (Q15): the block's start, its end */
    int32_t cut;                 /* ENV / LFO -> FILTER (voice 0's, the next block's shape) */
    int16_t a[TR_N + 1];         /* the area function (Q8 cm^2; [TR_N] the velum's port), for the screen */
    uint8_t fpl, nas, on, rsv;   /* the consonant's section (odd: frication, the closure), the nose computed, the shape set */
    int32_t vg;                  /* the closure's valve: the waves on through it (Q8; 256 open) */
    uint32_t clk;                /* blocks (the tongue's wander) */
} tr_ctl_t;
static int16_t comb_buf[NTRK][1024] __attribute__((section(".pool"), aligned(4)));   /* (eng_string.c, voice.c) */
_Static_assert(TR_NV * TR_W * 4 + sizeof(tr_ctl_t) <= sizeof comb_buf[0], "TRACT in a part's COMB lines");
static uint8_t tr_mode[NTRK] __attribute__((section(".pool")));   /* the MODE the part played last (tract_mode) */
static tr_ctl_t *tr_ctl(uint32_t ti) { return (tr_ctl_t *)(void *)(comb_buf[ti % NTRK] + TR_NV * TR_W * 2); }
static int32_t *tr_wav(uint32_t ti, uint32_t vi) { return (int32_t *)(void *)comb_buf[ti % NTRK] + vi * TR_W; }
/* the nose's junctions 2..11, Q14 (its areas 1.0 1.4 2.0 2.6 3.0 3.0 2.8 2.6 2.4 2.0 1.6 1.2 cm^2: Fant's, rounded) */
static const int16_t TR_KN[TR_M - 2] = {-2891, -2137, -1170, 0, 565, 607, 655, 1489, 1820, 2341};
static const param_desc_t TR_DESC[8] = {
    {"FRONT", F_PCT, 0, 127, 0, 0}, {"OPEN", F_PCT, 0, 127, 64, 0}, {"ROUND", F_PCT, 0, 127, 0, 0},
    {"NASAL", F_INT, 0, 12, 0, 0},   /* (SHIFT's slot, -12 .. 12: its upper half) */
    {"BREATH", F_PCT, 0, 127, 64, 0}, {"VIB", F_PCT, 0, 127, 10, 0}, {"RATE", F_PCT, 0, 127, 64, 0},
    {"TENSE", F_PCT, 0, 127, 0, 0},
};
static const char *const N_VMODE[] = {"VOWEL", "TRACT"};
static const param_desc_t VMODE_DESC = {"MODE", F_ENUM, 0, 1, 0, N_VMODE};   /* (P_VTRACT, EDIT > WORDS KNOB 4) */

/* a half step of the lattice: the junctions j, j + 2, .. under e, each one multiply (f the waves towards the lips or
 * the nostrils, b back) */
static __attribute__((noinline)) void tr_sc(int32_t *f, int32_t *b, const int32_t *k, int32_t j, int32_t e)
{
    for (; j < e; j += 2) {
        int32_t x = f[j - 1], y = b[j], w = (k[j] * (x - y)) >> 14;
        f[j] = x + w;
        b[j - 1] = y + w;
    }
}

/* the area function of FRONT OPEN ROUND (Q8 of 0..256), a consonant's closure ccl (Q8) at cpl (Q8 sections) */
static __attribute__((noinline)) void tr_shape(int16_t *a, int32_t fr, int32_t op, int32_t rd, int32_t cpl, int32_t ccl)
{
    int32_t c = ((2541 * 256 + 975 * fr) * (256 - op) + (1788 * 256 + 2553 * fr) * op) >> 16;   /* the tongue's place */
    int32_t ac = 38 + ((op * (55 * 256 + 167 * fr)) >> 16);                 /* its constriction */
    int32_t ab = 205 + (((((120 * fr) >> 8) + 293) * (256 - op)) >> 8);      /* behind it */
    int32_t af = 287 + ((1637 * op) >> 8) + ((375 * (256 - fr) * (256 - op)) >> 16);   /* before it */
    int32_t lip = ((319 + ((763 * op) >> 8)) * (256 - ((217 * rd) >> 8))) >> 8, i;
    for (i = 0; i < TR_N; i++) {
        int32_t d = (((i << 8) - c) * 33) >> 8, x = (i << 8) < c ? ab : af, y;   /* (d: Q8 of hw) */
        y = d > -256 && d < 256 ? (32768 + sine_i((uint32_t)d * 0x800000u + 0x40000000u)) >> 1 : 0;   /* (1 + cos) / 2 */
        y = ac + (((x - ac) * (32768 - y)) >> 15);
        d = (((i << 8) - cpl) * 171) >> 8;                                    /* (the consonant: 1.5 sections) */
        if (ccl && d > -256 && d < 256)
            y -= (y * ((ccl * ((32768 + sine_i((uint32_t)d * 0x800000u + 0x40000000u)) >> 1)) >> 15)) >> 9;   /* (half) */
        a[i] = (int16_t)y;
    }
    if (a[0] > 384) a[0] = 384;                          /* the larynx's tube */
    if (a[1] > 563) a[1] = 563;
    a[TR_N - 2] = (int16_t)((a[TR_N - 2] + lip) >> 1);
    a[TR_N - 1] = (int16_t)(ccl && a[TR_N - 1] < lip ? a[TR_N - 1] : lip);   /* (a lip closure stays narrow) */
    for (i = 0; i < TR_N; i++)
        a[i] = (int16_t)clamp(a[i], 13, 2048);           /* 0.05 .. 8 cm^2 */
}
static int32_t tr_k(int32_t a1, int32_t a2) { return clamp(((a1 - a2) << 14) / (a1 + a2), -15892, 15892); }

/* the place of articulation (sing.c PL_*) as a section: lips .. uvula; glottis / none: 0. The closure of each class
 * (PC_*, Q8): stops, nasals, affricates closed, fricatives narrow, the approximants half */
static const uint8_t TR_PL[PL_GLO + 1] = {0, 21, 21, 20, 19, 17, 16, 14, 12, 0};
static const uint8_t TR_CL[PC_AFF + 1] = {0, 0, 128, 128, 128, 128, 255, 255, 216, 0, 255};
/* the glides' speed (a shift a block): FRONT OPEN ROUND ~23 ms, the velum ~12, the consonant's place and closure ~6 */
static const uint8_t TR_GLIDE[6] = {4, 4, 4, 3, 2, 2};

/* once a block: the targets (the knobs, or the words), glided; the shape; its coefficients */
static __attribute__((noinline)) void tract_block(track_t *t)
{
    const int16_t *p = t->p;
    uint32_t ti = (uint32_t)(t - trk) % NTRK, j;
    tr_ctl_t *c = tr_ctl(ti);
    int32_t tg[6], g[3] = {32767, 0, 0}, br = clamp(p[P_E4], 0, 127), k;
    if (FT_IS_COMB(ft_of(p)))                            /* (its lines are the tube) */
        t->p[P_FTYPE] = FT_BP;
    tg[3] = clamp(p[P_E3], 0, 12) * 26;                  /* NASAL: the velum, up to 1.2 cm^2 */
    tg[4] = 0;
    tg[5] = 0;
    if (p[P_VLANG]) {                                    /* WORDS: sing.c's articulation */
        const sing_t *s = sing_view(t);
        uint32_t ph = sing_phon(s), cl = PHON[ph].cls, pl = PHON[ph].place & 0x7Fu;
        int32_t f1 = s->f[0], f2 = s->f[1], rd = PHON[ph].place & PL_RND ? 256 : 0;
        tg[0] = -454 + ((f1 * 57 + f2 * 83) >> 8) + rd - ((f1 * rd * 63) >> 16);   /* (the shape's fitted inverse) */
        tg[1] = -380 + ((f1 * 187 + f2 * 24) >> 8) + ((rd * 44) >> 8) + ((f1 * rd * 31) >> 16);
        tg[2] = rd;
        if (cl == PC_NAS)
            tg[3] += 256;
        if (pl <= PL_GLO && TR_PL[pl]) {
            tg[4] = TR_PL[pl] << 8;
            tg[5] = cl == PC_PLO && s->st ? 96 : TR_CL[cl];   /* (a plosive's burst: open a little) */
            c->fpl = (uint8_t)(TR_PL[pl] | 1u);
        }
        g[0] = s->gv;
        g[1] = s->ga;
        g[2] = s->gf;
    } else {                                             /* the knobs, FILTER's modulation on FRONT, the tongue wandering */
        int32_t w = sy_noise(ti + 7u, c->clk++, 9);      /* (a slow, small drift: a point every ~0.75 s) */
        tg[0] = (p[P_E0] << 1) + (c->cut >> 7) + (w >> 12);
        tg[1] = (p[P_E1] << 1) + (w >> 13);
        tg[2] = p[P_E2] << 1;
    }
    g[0] = (g[0] * (32767 - br * br * 2)) >> 15;         /* BREATH: the voicing out at the top, the noise in */
    g[1] += br * 258;
    for (j = 0; j < 6u; j++) {
        int32_t x = (j < 3u ? clamp(tg[j], 0, 256) : tg[j]) << 8;
        if (!c->on)
            c->gl[j] = x;
        k = j == 5u && x < c->gl[5] ? 1 : j < 3u && p[P_VLANG] ? 1 : TR_GLIDE[j];   /* (a closure's release ~3 ms; the
                                                         * words' vowels ~3 after sing.c's own glide) */
        c->gl[j] += (x - c->gl[j]) >> k;
        if (j < 3u) {                                    /* the gains: the block's start, its end */
            c->g[j][0] = c->on ? c->g[j][1] : g[j];
            c->g[j][1] = g[j];
        }
    }
    c->on = 1;
    tr_shape(c->a, c->gl[0] >> 8, c->gl[1] >> 8, c->gl[2] >> 8, c->gl[4] >> 8, c->gl[5] >> 8);
    c->vg = 256 - (c->gl[5] >> 8);                       /* (the area halved at most, the valve the rest: no energy from
                                                         * a squeezed section, Kelly-Lochbaum's pressure waves'; a closed
                                                         * tube's ringing kept behind it) */
    if (!c->fpl)
        c->fpl = TR_N - 1;
    for (j = 1; j < TR_N; j++)
        c->k[j] = tr_k(c->a[j - 1], c->a[j]);
    c->k[0] = 65536 / (c->a[0] > 128 ? c->a[0] : 128);   /* (its area 0.5 cm^2 at least: 2 at the most) */
    k = c->gl[3] >> 8;                                   /* the velum */
    c->a[TR_N] = (int16_t)k;
    c->nas = k > 2;
    if (c->nas) {
        int32_t s = c->a[TR_J - 1] + c->a[TR_J] + k;
        c->g1 = (c->a[TR_J - 1] << 14) / s;
        c->g2 = (c->a[TR_J] << 14) / s;
        c->g3 = 16384 - c->g1 - c->g2;
        c->kn[1] = tr_k(k, 358);                         /* (the nose's first section: 1.4 cm^2) */
        for (j = 2; j < TR_M; j++)
            c->kn[j] = TR_KN[j - 2];
    }
}

static __attribute__((noinline)) void tract_note_on(track_t *t, voice_t *v)
{
    uint32_t vi = (uint32_t)(v - t->v);
    if (t->p[P_VLANG])
        sing_note(t);
    if (vi < TR_NV && !v->env && !v->env_out) {          /* a fresh voice (not a retrigger): from rest */
        int32_t *w = tr_wav((uint32_t)(t - trk), vi), i;
        for (i = 0; i < TR_W; i++)
            w[i] = 0;
        v->ph[0] = v->ph[1] = 0;
        v->ph[2] = 0x2545F491u ^ (vi * 0x9E3779B9u) ^ v->note;
        for (i = 0; i < 7; i++)
            v->s[i] = 0;
    }
}

/* a voice: the source into the tube, CTL samples of 44.1 kHz from CTL / 2 of the tract's. Voice state: ph[0] the
 * glottal phase, ph[1] the vibrato's, ph[2] the noise; s[0] s[1] the lips' and the nostrils' last wave out, s[2] the
 * last volume velocity, s[3] the last sample (the interpolation), s[4] the blocks since the note (the vibrato's
 * onset), s[5] s[6] the DC block's; s[7] VOICE's (formant_amp, WORDS) */
static __attribute__((noinline)) void tract_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t ti = (uint32_t)(t - trk) % NTRK, vi = (uint32_t)(v - t->v), i, nt = (n + 1u) >> 1, inc, ph, te, tp, rp, rn;
    tr_ctl_t *c = tr_ctl(ti);
    int32_t *R, *L, *NR, *NL, gv, dgv, ga, dga, gf, dgf, rg, lp, np, up, yp, dp, dc, nst, nzp = 0, tn, vib, jr, e, o;
    if (vi >= TR_NV)
        return;
    R = tr_wav(ti, vi);
    L = R + TR_N;
    NR = L + TR_N;
    NL = NR + TR_M;
    if (!vi)
        c->cut = m->cutoff;
    inc = m->inc;
    if (p[P_VLANG]) {                                    /* WORDS: the singer's pitch (eng_formant.c: scoops, landings) */
        int32_t p16;
        inc = fmt_life(t, v, m, &sing_x[ti], 64, &p16, vi);
    } else if (v->pitch_cur != v->pitch16) {
        int32_t p16;
        inc = fmt_sung(v, m, &p16);
    }
    /* the vibrato: VIB up to +-60 ct, RATE 3 .. 8 Hz, in over ~0.4 s */
    e = v->s[4] < 280 ? v->s[4] : 280;
    v->s[4]++;
    vib = (clamp(p[P_E5], 0, 127) * e) >> 8;
    v->ph[1] += (uint32_t)(292100 + clamp(p[P_E6], 0, 127) * 3834) * CTL;   /* (2^32 x Hz / FS) */
    vib = (sine_i(v->ph[1]) * vib) >> 15;                /* (1/4096 of the increment: ~0.42 ct) */
    jr = (int32_t)(noise32((int32_t *)&v->ph[2]) >> 16) - 32768;   /* (jitter: each block its period, ~0.3 %) */
    inc += (uint32_t)((int32_t)(inc >> 12) * (vib + ((jr * 13) >> 15)));
    inc <<= 1;                                           /* (the tract's rate) */
    /* the source: TENSE (+ SHAPE) the open quotient 0.85 .. 0.4 and the closing's share, the glottis's reflection */
    tn = clamp((p[P_E7] << 8) + m->shape - (64 << 8) + (v->vel - 96) * 40, 0, 127 << 8);
    te = (uint32_t)(55705 - ((tn * 232) >> 8)) << 16;
    tp = (te >> 16) * (uint32_t)(40632 + ((tn * 24) >> 8));   /* (opening 62 .. 72 % of it) */
    rp = (1u << 30) / ((tp >> 16) | 1u);
    rn = (1u << 30) / (((te - tp) >> 16) | 1u);
    rg = 12780 + ((tn * 13) >> 8);
    gv = c->g[0][0];
    dgv = (c->g[0][1] - gv) >> 5;
    ga = c->g[1][0];
    dga = (c->g[1][1] - ga) >> 5;
    gf = c->g[2][0];
    dgf = (c->g[2][1] - gf) >> 5;
    ph = v->ph[0];
    nst = (int32_t)v->ph[2];
    lp = v->s[0];
    np = v->s[1];
    up = v->s[2];
    yp = v->s[3];
    dp = v->s[5];
    dc = v->s[6];
    for (i = 0; i < nt; i++) {
        int32_t u, nz, a, b, w, y;
        /* the flow (Q15): a rising half cosine, a falling quarter cosine, shut */
        u = 0;
        if (ph < te) {                                   /* (one cosine: the opening's, or the closing's) */
            u = sine_i((ph < tp ? ((ph >> 16) * rp) << 1 : ((ph - tp) >> 16) * rn) + 0x40000000u);
            if (ph < tp)
                u = (32768 - u) >> 1;
        }
        nz = (int32_t)(noise32(&nst) >> 17) - 16384;
        u = ((u * gv) >> 18) + (((nz * (ph < te ? ga : ga >> 2)) >> 17));   /* (~4096 at the most) */
        ph += inc;
        /* even junctions: the glottis, 2 .. 20 (the velum's at 10), the lips; the nose's 2 .. 10, the nostrils */
        R[0] = ((rg * L[0]) >> 14) + ((u * c->k[0]) >> 8);
        if (c->nas) {
            int32_t q = NL[0], pj;
            tr_sc(R, L, c->k, 2, TR_J);
            a = R[TR_J - 1];
            b = L[TR_J];
            pj = (c->g1 * a + c->g2 * b + c->g3 * q) >> 13;
            R[TR_J] = pj - b;
            L[TR_J - 1] = pj - a;
            NR[0] = pj - q;
            tr_sc(R, L, c->k, TR_J + 2, TR_N);
            tr_sc(NR, NL, c->kn, 2, TR_M);
            a = NR[TR_M - 1];
            b = -((a * 8192 + np * 4915) >> 14);
            np = a;
            NL[TR_M - 1] = b;
            y = ((a - b) * 307) >> 8;                    /* (the nostrils: 1.2 cm^2) */
            tr_sc(NR, NL, c->kn, 1, TR_M);               /* (the nose's odd junctions now: its waves wait for nothing) */
        } else {
            tr_sc(R, L, c->k, 2, TR_N);
            y = 0;
        }
        a = R[TR_N - 1];                                 /* the lips: -0.9 at DC, -0.2 at 11 kHz */
        b = -((a * 9011 + lp * 5734) >> 14);
        lp = a;
        L[TR_N - 1] = b;
        y += ((a - b) * c->a[TR_N - 1]) >> 8;           /* (the volume velocity: the lips' area x the wave) */
        tr_sc(R, L, c->k, 1, TR_N);                      /* odd junctions 1 .. 21 */
        R[c->fpl] = (R[c->fpl] * c->vg) >> 8;            /* a closure: what passes it on to the lips held back */
        if (gf) {                                        /* frication: turbulence at the constriction, on its way out */
            w = ((nz - nzp) * gf) >> 20;                 /* (differenced: its highs, as a jet's noise; none at DC) */
            R[c->fpl] += w + (w >> 1);
        }
        nzp = nz;
        gv += dgv;
        ga += dga;
        gf += dgf;
        /* the radiation: the volume velocity differenced */
        o = soft_knee(clamp((y - up) * TR_GAIN, -32767, 32767), 24000);
        up = y;
        dc += o - dp - (dc >> 9);                        /* (a DC block after the knee: -3 dB at ~14 Hz) */
        dp = o;
        y = dc;
        a = amp_at(m, 2u * i);                           /* (the level's ramp: one point a pair) */
        out[2u * i] += mulq15(mulq15((yp + y) >> 1, a), VOICE_FS);
        if (2u * i + 1u < n)
            out[2u * i + 1u] += mulq15(mulq15(y, a), VOICE_FS);
        yp = y;
    }
    v->ph[0] = ph;
    v->ph[2] = (uint32_t)nst;
    v->s[0] = lp;
    v->s[1] = np;
    v->s[2] = up;
    v->s[3] = yp;
    v->s[5] = dp;
    v->s[6] = dc;
}

/* VOICE's hooks (eng_formant.c): once a block, the MODE (a change: what sounds goes on from rest, the other mode's state
 * not this one's) and TRACT's shape; EDIT's labels */
static __attribute__((noinline)) void tract_mode(track_t *t)
{
    uint32_t ti = (uint32_t)(t - trk) % NTRK, on = t->p[P_VTRACT] != 0, i, j;
    if (on != tr_mode[ti]) {
        tr_mode[ti] = (uint8_t)on;
        tr_ctl(ti)->on = 0;
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &t->v[i];
            v->ph[0] = v->ph[1] = v->ph[2] = 0;
            for (j = 0; j < 7u; j++)
                v->s[j] = 0;
            if (on && i < TR_NV)
                memset(tr_wav(ti, i), 0, TR_W * 4u);
        }
    }
    if (on)
        tract_block(t);
}
static const param_desc_t *tract_desc(const track_t *t, uint32_t k, const param_desc_t *d)
{
    return t->p[P_VTRACT] && !d ? &TR_DESC[k & 7u] : d;   /* (WORDS: POS THEME ARTIC, LOOP theirs) */
}

/* (V10.7.43) the tube's presets, appended to FORMANT_PRESETS (15 ..: every number before kept). FRONT OPEN ROUND NASAL |
 * BREATH VIB RATE TENSE (WORDS: POS THEME ARTIC NASAL | BREATH VIB RATE LOOP); MODE TRACT in their movement */
#define TRACT_PRESETS                                                                                                 \
    {"TUBE AH", {10, 115, 0, 0, 12, 34, 64, 70}, {20, 70, 115, 70}, 0, TAG(VOX), FX(0, 20, 20, 45), PAT(5),          \
     MODS(P_VTRACT, 1, MMOD(1, PS_MODW, PD_E(5), 50))},                                                               \
    {"TUBE EE", {127, 0, 0, 0, 12, 34, 64, 70}, {20, 70, 115, 70}, 0, TAG(VOX), FX(0, 20, 20, 45), PAT(5),           \
     MODS(P_VTRACT, 1, MMOD(1, PS_MODW, PD_E(5), 50))},                                                               \
    {"TUBE OO", {38, 0, 127, 0, 12, 34, 64, 70}, {20, 70, 115, 70}, 0, TAG(VOX), FX(0, 20, 20, 45), PAT(5),          \
     MODS(P_VTRACT, 1, MMOD(1, PS_MODW, PD_E(5), 50))},                                                               \
    {"TUBE CHOIR", {45, 80, 40, 2, 34, 46, 50, 40}, {70, 90, 115, 95}, 0, TAG(PAD), FX(0, 70, 15, 90), PAT(5),       \
     MODS(P_VTRACT, 1, MLFO(LW_DRIFT, 30, 0, 14), MMOD(1, PS_RAND, PD_PAN, 40))},   /* the tongue drifts */         \
    {"BASS SINGR", {20, 70, 70, 1, 6, 40, 44, 96}, {10, 70, 110, 60}, 0, 1 | TAG(VOX), FX(0, 10, 15, 40), PAT(8),    \
     MODS(P_VTRACT, 1, P_GLIDE, 14, MMOD(1, PS_VEL, PD_SHP, 30))},                                                    \
    {"WHISPER TB", {70, 50, 10, 0, 127, 0, 64, 20}, {30, 80, 110, 80}, 0, TAG(VOX), FX(0, 30, 25, 60), PAT(5),       \
     MODS(P_VTRACT, 1, MMOD(1, PS_MODW, PD_CUT, 40))},                                                                \
    {"TALK TUBE", {0, 64, 0, 0, 8, 28, 64, 0}, {24, 70, 112, 72}, 0, 1 | TAG(VOX), FX(0, 10, 30, 35), PAT(4),         \
     WORDS(LG_EN, 3), MODS(P_VTRACT, 1, MMOD(1, PS_AT, PD_E(5), 40))},
#define TRACT_ORDER 15, 16, 17, 18, 19, 20, 21,
