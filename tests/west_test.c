/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* WEST engine test (src/eng_west.c, V10.7.36: engine 11, NOISE's slot; V10.7.37 remade: the vactrol, the integrated
 * folds, the triangle core) on the host, through hostsim.c as regress.c.
 *   build/host/west_test [DEMODIR]           (run_tests.sh: build/west_demo)
 * 1. bounded: every parameter at 0 and at 127 (the others at a preset's), and the 256 corners of all eight at 0 / 127,
 *    8 notes at velocity 127 over the keyboard, LEVEL 127: no voice sum past 2 x VOICE_FS a voice, no mix sample at
 *    full scale; and no DC (|mean| < 1 % of a voice's full scale) on any preset.
 * 2. the fold is heard: the share of the energy above the fundamental grows with FOLD (0 -> 64 -> 127), SYMM brings
 *    even harmonics (the 2nd), MOD and RATIO move the spectrum.
 * 3. the gate is heard: GATE 64, DECAY 20: the strike's first 30 ms far louder than 0.4 s on; GATE 0 (a low-pass):
 *    the level stays but the highs go (the brightness falls); GATE 127 (a VCA): the brightness stays, the level falls;
 *    DECAY 127 held: the level holds.
 * 3b. (V10.7.37) the vactrol: struck, the cell lights in a few ms but darkens over far longer (attack faster than decay);
 *    each octave darker takes longer than the one before (the tail longer the darker); a strike released at once
 *    still rings (a pluck: the cell's memory), darker as it fades; a longer DECAY rings longer; the triangle core
 *    (RATIO's second bank) at FOLD 0 a triangle (odd harmonics, the 3rd ~1/9), folded brighter; the folds integrated:
 *    a high note folded hard leaves little energy where its harmonics would alias.
 * 4. velocity: 127 is louder and brighter than 40 (the fold and the ceiling).
 * 5. DUST 0 adds no noise: the same note with two noise seeds renders bit for bit the same; DUST 30 differs; DUST 127
 *    with the core gone still sounds (a noise patch), broadband.
 * 6. determinism: a note from silence renders the same samples again; FLT (the track's CUT) moves the ceiling, SHP the
 *    fold, as LIFE and the matrix move them.
 * 7. NOISE projects: a project of before WEST (phys 2) with a NOISE track (each of NOISE's eight presets' values) opens
 *    with west_from_noise's values, its motion on the EDIT values gone, phys 3; a second pass changes nothing; a project
 *    of today (phys 3) is never touched. Each migrated sound is bounded and sounds.
 * 8. cost: host instructions a sample and voice where the host counts them (macOS), else ns, against ANALOG's heaviest
 *    preset measured the same way: WEST's heaviest corner at most 1.25 x (8 voices fit as other engines' do).
 * 9. demos into DEMODIR: every preset playing its suggested pattern; a NOISE project before (as V10.7.35 played it)
 *    is renders/v36-west's, not here. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#define PROJ_HOST 1
#include "../firmware/src/project.c"
#ifdef __APPLE__
#include <libproc.h>
#include <sys/resource.h>
#endif

#define NFFT 8192

static int bad;
static void check(const char *what, int ok)
{
    printf("west: %-92s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static const int16_t *pe(uint32_t pi)
{
    static int16_t e[8];
    uint32_t i;
    for (i = 0; i < 8u; i++)
        e[i] = ENG_WEST.presets[pi].e[i];
    return e;
}

/* track 0 WEST with e[0..7], a flat envelope (ATK 0, SUS full), no sends, POLY */
static void setup(const int16_t *e)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ENGI_WEST, 0);
    for (i = 0; i < 8u; i++)
        trk[0].p[P_E0 + i] = e[i];
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_REL] = 10;
    trk[0].p[P_ED_FLT] = 0;
    trk[0].p[P_LD_FLT] = trk[0].p[P_LD_SHP] = trk[0].p[P_LD_PIT] = trk[0].p[P_LD_AMP] = 0;
    trk[0].p[P_M1SRC] = trk[0].p[P_M2SRC] = trk[0].p[P_M3SRC] = trk[0].p[P_M4SRC] = 0;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_DIST] = 0;
}

/* the voices' sum (track_render, before the mix) of one note for n samples from its start (skip blocks first) */
static void note_render(const int16_t *e, uint32_t note, uint32_t vel, double *y, uint32_t n, uint32_t skip)
{
    static int32_t b[CTL];
    uint32_t i, k;
    setup(e);
    trk_note_on(&trk[0], note, vel);
    for (i = 0; i < skip; i++)
        track_render(&trk[0], b, CTL);
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k] / 32768.0;
    }
}

/* ------------------------------------------------------------ spectra --- */
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, len;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (j = 0; j < len / 2; j++) {
                double wr = cos(a * j), wi = sin(a * j);
                double xr = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
                double xi = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
                re[i + j + len / 2] = re[i + j] - xr, im[i + j + len / 2] = im[i + j] - xi;
                re[i + j] += xr, im[i + j] += xi;
            }
    }
}
static void spectrum(const double *y, double *ps)            /* one Hann frame of NFFT */
{
    static double re[NFFT], im[NFFT];
    uint32_t i;
    for (i = 0; i < NFFT; i++) {
        re[i] = y[i] * (0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
        im[i] = 0;
    }
    fft(re, im, NFFT);
    for (i = 0; i < NFFT / 2; i++)
        ps[i] = re[i] * re[i] + im[i] * im[i];
}
static double hz_of_note(uint32_t note) { return 440.0 * pow(2.0, (note - 69.0) / 12.0); }
/* the share of the energy above 1.5 f0 (the harmonics over the fundamental) */
static double bright(const double *ps, double f0)
{
    double lo = 0, hi = 0;
    uint32_t i, c = (uint32_t)(1.5 * f0 * NFFT / FS);
    for (i = 2; i < NFFT / 2; i++)
        *(i < c ? &lo : &hi) += ps[i];
    return hi / (lo + hi + 1e-30);
}
/* the spectral centroid in harmonics of f0 */
static double centroid(const double *ps, double f0)
{
    double a = 0, w = 0;
    uint32_t i;
    for (i = 2; i < NFFT / 2; i++) {
        a += ps[i];
        w += ps[i] * i;
    }
    return w / (a + 1e-30) * FS / NFFT / f0;
}
/* the energy near harmonic h of f0 (+-1/4 of f0) */
static double harm(const double *ps, double f0, uint32_t h)
{
    double s = 0;
    uint32_t i, a = (uint32_t)((h - 0.25) * f0 * NFFT / FS), b = (uint32_t)((h + 0.25) * f0 * NFFT / FS);
    for (i = a; i <= b && i < NFFT / 2; i++)
        s += ps[i];
    return s;
}
static double rms(const double *y, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += y[i] * y[i];
    return sqrt(s / (n ? n : 1));
}

/* ------------------------------------------------------------ 1. bounded --- */
static int corner_ok(const int16_t *e, double *dc)
{
    static int32_t o[2 * CTL], b[CTL];
    uint32_t i, k, j, nb = FS / CTL;
    int32_t peak = 0, vpk = 0;
    double sum = 0;
    setup(e);
    trk[0].p[P_LEVEL] = 127;
    for (k = 0; k < 8u; k++)
        trk_note_on(&trk[0], 24 + k * 12u > 127 ? 127 : 24 + k * 12u, 127);
    for (i = 0; i < nb; i++) {
        mix_block(o, CTL);
        for (j = 0; j < 2u * CTL; j++)
            peak = abs(o[j]) > peak ? abs(o[j]) : peak;
    }
    /* the voices' sum alone, every note at once (a second pass, the part rendered by itself) */
    setup(e);
    for (k = 0; k < 8u; k++)
        trk_note_on(&trk[0], 24 + k * 12u > 127 ? 127 : 24 + k * 12u, 127);
    for (i = 0; i < nb; i++) {
        track_render(&trk[0], b, CTL);
        for (j = 0; j < CTL; j++) {
            vpk = abs(b[j]) > vpk ? abs(b[j]) : vpk;
            sum += b[j];
        }
    }
    *dc = sum / (nb * CTL) / VOICE_FS;
    return vpk <= 8 * 2 * VOICE_FS && peak < 32767;
}
static void bounded(void)
{
    static const int16_t LO[8] = {0}, HI[8] = {127, 127, 127, 15, 127, 127, 127, 127};
    uint32_t k, c, pi, fails = 0, dcbad = 0;
    double dc, worst = 0;
    int16_t e[8];
    for (k = 0; k < 8u; k++)
        for (c = 0; c < 2u; c++) {
            memcpy(e, pe(8), sizeof e);                  /* (WEST PLUCK, one value at an end) */
            e[k] = c ? HI[k] : LO[k];
            fails += !corner_ok(e, &dc);
        }
    for (c = 0; c < 256u; c++) {
        for (k = 0; k < 8u; k++)
            e[k] = (c >> k) & 1u ? HI[k] : LO[k];
        fails += !corner_ok(e, &dc);
    }
    check("bounded: each value at 0 and 127, all 256 corners; 8 notes vel 127: voices <= 2 x VOICE_FS, no full scale",
          !fails);
    for (pi = 0; pi < ENG_WEST.npresets; pi++) {
        corner_ok(pe(pi), &dc);
        worst = fabs(dc) > worst ? fabs(dc) : worst;
        dcbad += fabs(dc) > 0.01;
    }
    {
        char what[160];
        snprintf(what, sizeof what, "no DC: every preset, 8 notes: |mean| %.4f of a voice's full scale (< 0.01)", worst);
        if (getenv("VERBOSE"))
            for (pi = 0; pi < ENG_WEST.npresets; pi++) {
                corner_ok(pe(pi), &dc);
                printf("west:   %-12s DC %+.4f\n", ENG_WEST.presets[pi].name, dc);
            }
        check(what, !dcbad);
    }
}

/* ------------------------------------------------------------- 2. fold --- */
static double y[FS * 2], ps[NFFT / 2];
static double bright_of(const int16_t *e, uint32_t note, uint32_t vel)
{
    note_render(e, note, vel, y, NFFT, 2);
    spectrum(y, ps);
    return bright(ps, hz_of_note(note));
}
static double centroid_of(const int16_t *e, uint32_t note, uint32_t vel)
{
    note_render(e, note, vel, y, NFFT, 2);
    spectrum(y, ps);
    return centroid(ps, hz_of_note(note));
}
static void fold(void)
{
    int16_t e[8] = {0, 0, 0, 1, 127, 127, 0, 0};       /* (a VCA, held open: the folder's own) */
    double b0, b1, b2, h1, h2, h2s, m0, m1;
    char what[200];
    b0 = centroid_of(e, 48, 64);
    e[0] = 64; b1 = centroid_of(e, 48, 64);
    e[0] = 127; b2 = centroid_of(e, 48, 64);
    snprintf(what, sizeof what, "FOLD 0 / 64 / 127: the spectrum's centroid at harmonic %.2f %.2f %.2f (rising; 0 a sine)", b0, b1, b2);
    check(what, b0 < 1.2 && b1 > b0 + 1 && b2 > b1 + 1);
    e[0] = 64;
    note_render(e, 48, 64, y, NFFT, 2);
    spectrum(y, ps);
    h1 = harm(ps, hz_of_note(48), 1);
    h2 = harm(ps, hz_of_note(48), 2);
    e[1] = 100;
    note_render(e, 48, 64, y, NFFT, 2);
    spectrum(y, ps);
    h2s = harm(ps, hz_of_note(48), 2);
    snprintf(what, sizeof what, "SYMM 0 -> 100: the 2nd harmonic %.1f dB -> %.1f dB of the 1st (even harmonics in)",
             10 * log10(h2 / h1 + 1e-30), 10 * log10(h2s / h1 + 1e-30));
    check(what, h2s > h2 * 100 && h2s / h1 > 0.01);
    e[0] = 0; e[1] = 0;
    m0 = bright_of(e, 48, 64);
    e[2] = 100; e[3] = 7;
    m1 = bright_of(e, 48, 64);
    snprintf(what, sizeof what, "MOD 0 -> 100 (RATIO 2.76): the energy above the fundamental %.3f -> %.3f", m0, m1);
    check(what, m1 > m0 + 0.2);
}

/* ------------------------------------------------------------- 3. gate --- */
static void gate(void)
{
    int16_t e[8] = {60, 0, 0, 1, 64, 20, 20, 0};
    double a, b, br0, br1;
    uint32_t n30 = FS * 30u / 1000u, at = FS * 4u / 10u;
    char what[200];
    note_render(e, 48, 100, y, FS, 0);
    a = rms(y, n30);
    b = rms(y + at, n30);
    snprintf(what, sizeof what, "GATE 64 DECAY 20: the strike's first 30 ms %.1f dB over 0.4 s on (> 30)", 20 * log10(a / (b + 1e-12)));
    check(what, a > 30 * b);
    e[4] = 0;                                            /* a low-pass alone: the level stays, the highs go */
    e[5] = 80;
    note_render(e, 48, 100, y, FS, 0);
    spectrum(y, ps);
    br0 = bright(ps, hz_of_note(48));
    spectrum(y + FS - NFFT, ps);
    br1 = bright(ps, hz_of_note(48));
    a = rms(y + 2000, 4000);
    b = rms(y + FS - 4000, 4000);
    snprintf(what, sizeof what, "GATE 0 (low-pass): brightness %.3f -> %.3f, level %.1f dB over 1 s (the highs go, the level stays)",
             br0, br1, 20 * log10(b / (a + 1e-12)));
    check(what, br1 < br0 * 0.5 && b > a * 0.3);
    e[4] = 127;                                          /* a VCA alone: the brightness stays, the level goes */
    note_render(e, 48, 100, y, FS, 0);
    spectrum(y, ps);
    br0 = bright(ps, hz_of_note(48));
    spectrum(y + FS / 4, ps);
    br1 = bright(ps, hz_of_note(48));
    a = rms(y + 2000, 4000);
    b = rms(y + FS - 4000, 4000);
    snprintf(what, sizeof what, "GATE 127 (VCA): brightness %.3f -> %.3f (the fold follows the strike, the highs stay), level %.1f dB at 1 s",
             br0, br1, 20 * log10(b / (a + 1e-12)));
    check(what, br1 > 0.5 * br0 && b < a * 0.1);
    e[4] = 64; e[5] = 127;                               /* held: a drone */
    note_render(e, 48, 100, y, FS * 2u, 0);
    a = rms(y + FS / 2, 4000);
    b = rms(y + FS * 2u - 4000, 4000);
    snprintf(what, sizeof what, "DECAY 127 held: the level at 2 s %.1f dB of 0.5 s's (a drone)", 20 * log10(b / (a + 1e-12)));
    check(what, b > a * 0.8 && a > 0.01);
}


/* ------------------------------------------------------------ 3b. vactrol --- */
static double light_of(void)                             /* the struck voice's light (eng_west.c s[5]: -log2 of it, Q20) */
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (trk[0].v[k].active)
            return pow(2.0, -trk[0].v[k].s[5] / 1048576.0);
    return 0;
}
/* a note struck (released after rel samples), the light every block into lt[], the voice into y */
static void strike(const int16_t *e, uint32_t note, uint32_t rel, double *lt, uint32_t nb)
{
    static int32_t b[CTL];
    uint32_t i, k;
    setup(e);
    trk[0].p[P_REL] = 100;                               /* (the ADSR's release long: the cell's own decay heard) */
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < nb; i++) {
        if (i * CTL >= rel && i * CTL < rel + CTL)
            trk_note_off(&trk[0], note);
        track_render(&trk[0], b, CTL);
        lt[i] = light_of();
        for (k = 0; k < CTL; k++)
            y[i * CTL + k] = b[k] / 32768.0;
    }
}
static double tri_h3(uint32_t fold)                      /* the triangle core, held open: its 3rd harmonic over the 1st */
{
    int16_t e[8] = {0, 0, 0, 17, 127, 127, 0, 0};
    e[0] = (int16_t)fold;
    note_render(e, 48, 64, y, NFFT, 4);
    spectrum(y, ps);
    return harm(ps, hz_of_note(48), 3) / (harm(ps, hz_of_note(48), 1) + 1e-30);
}
static void vactrol(void)
{
    static double lt[FS * 2 / CTL];
    int16_t e[8] = {70, 0, 0, 1, 64, 60, 20, 0};
    uint32_t nb = FS * 2 / CTL, i, pkb = 0, hb = 0, oct[9] = {0}, o;
    double pk = 0, a, b, c, b0, b1, t20[3];
    char what[240];
    strike(e, 48, CTL, lt, nb);                          /* (a ping: released after one block) */
    for (i = 0; i < nb; i++)
        if (lt[i] > pk)
            pk = lt[i], pkb = i;
    for (o = 1, i = pkb; i < nb && o < 9u; i++)          /* the block each octave darker than the peak is reached */
        while (o < 9u && lt[i] < pk / (double)(1u << o))
            oct[o++] = i;
    while (hb < pkb && lt[hb] < pk / 2)                  /* (lit to half its peak) */
        hb++;
    snprintf(what, sizeof what, "vactrol: lit to half in %.1f ms (its peak %.2f at %.1f ms), half again %.1f ms after the peak "
             "(attack faster than decay)", (hb + 1) * 1000.0 * CTL / FS, pk, (pkb + 1) * 1000.0 * CTL / FS,
             (oct[1] - pkb) * 1000.0 * CTL / FS);
    check(what, pkb * CTL < FS * 8u / 1000u && pk > 0.5 && oct[1] > pkb && (oct[1] - pkb) > 2 * (hb + 1));
    snprintf(what, sizeof what, "  the tail longer the darker: octaves 1-2 %.1f ms, 4-5 %.1f ms, 7-8 %.1f ms",
             (oct[2] - oct[1]) * 1000.0 * CTL / FS, (oct[5] - oct[4]) * 1000.0 * CTL / FS, (oct[8] - oct[7]) * 1000.0 * CTL / FS);
    check(what, oct[8] && oct[2] - oct[1] < oct[5] - oct[4] && oct[5] - oct[4] < oct[8] - oct[7]);
    a = rms(y + FS / 100, FS / 100);                      /* 10 .. 20 ms */
    b = rms(y + FS / 10, FS / 100);                       /* 100 .. 110 ms */
    c = rms(y + FS / 2, FS / 100);                        /* 500 ms */
    spectrum(y, ps);
    b0 = centroid(ps, hz_of_note(48));
    spectrum(y + FS / 4, ps);
    b1 = centroid(ps, hz_of_note(48));
    snprintf(what, sizeof what, "a ping (released at once) plucks: %.1f dB at 0.1 s, %.1f dB at 0.5 s of its start; the centroid %.2f -> %.2f",
             20 * log10(b / (a + 1e-12)), 20 * log10(c / (a + 1e-12)), b0, b1);
    check(what, b < a * 0.5 && b > a * 0.02 && c < b * 0.5 && b1 < b0 * 0.85);
    for (i = 0; i < 3u; i++) {                           /* DECAY 20, 50, 80: the time to -20 dB of the light's peak */
        uint32_t k;
        e[5] = (int16_t)(20 + 30 * i);
        strike(e, 48, CTL, lt, nb);
        for (k = 4; k < nb && lt[k] > 0.01 * pk; k++)
            ;
        t20[i] = k * 1000.0 * CTL / FS;
    }
    snprintf(what, sizeof what, "  DECAY 20 / 50 / 80: the light 40 dB down in %.0f / %.0f / %.0f ms (longer, a memory)", t20[0], t20[1], t20[2]);
    check(what, t20[0] < t20[1] && t20[1] < t20[2] && t20[0] < 150 && t20[2] > 500);
    a = tri_h3(0);
    b = tri_h3(80);
    snprintf(what, sizeof what, "the triangle core (RATIO 1T): FOLD 0 a triangle (3rd harmonic %.1f dB, -19 a triangle's), FOLD 80 %.1f dB",
             10 * log10(a + 1e-30), 10 * log10(b + 1e-30));
    check(what, a > 0.008 && a < 0.02 && b > a * 4);
    {   /* the folds integrated: C6 on the triangle core folded hard (FOLD 127) leaves little energy off its harmonics
         * (folded back over Nyquist: aliases; -22 dB unintegrated and uncapped, V10.7.37's first try) */
        int16_t h[8] = {127, 0, 0, 17, 127, 127, 0, 0};
        double lo = 0, all = 0, f0 = hz_of_note(84);
        note_render(h, 84, 127, y, NFFT, 4);
        spectrum(y, ps);
        for (i = 2; i < NFFT / 2; i++) {
            double f = (double)i * FS / NFFT, r = f / f0;
            all += ps[i];
            if (fabs(r - floor(r + 0.5)) > 0.1)          /* (off the harmonics: aliases) */
                lo += ps[i];
        }
        snprintf(what, sizeof what, "  C6, the triangle core folded hard: %.1f dB of its energy off its harmonics (aliases)", 10 * log10(lo / all + 1e-30));
        check(what, lo < all * 0.003);
    }
}

/* --------------------------------------------------------- 4. velocity --- */
static void velocity(void)
{
    const int16_t *e = pe(8);                            /* WEST PLUCK */
    double l40, l127, b40, b127;
    char what[200];
    note_render(e, 55, 40, y, NFFT, 0);
    l40 = rms(y, NFFT);
    spectrum(y, ps);
    b40 = bright(ps, hz_of_note(55));
    note_render(e, 55, 127, y, NFFT, 0);
    l127 = rms(y, NFFT);
    spectrum(y, ps);
    b127 = bright(ps, hz_of_note(55));
    snprintf(what, sizeof what, "velocity 40 -> 127: level +%.1f dB, energy above the fundamental %.3f -> %.3f",
             20 * log10(l127 / (l40 + 1e-12)), b40, b127);
    check(what, l127 > l40 * 2 && b127 > b40 * 1.3);
}

/* ------------------------------------------------------------- 5. dust --- */
static uint32_t render_seeded(const int16_t *e, uint32_t seed, int32_t *buf, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k, nz = 0;
    setup(e);
    trk_note_on(&trk[0], 60, 100);
    for (k = 0; k < NVOICE; k++)
        if (trk[0].v[k].active)
            trk[0].v[k].ph[2] = seed;                    /* (another noise for the same note) */
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL; k++) {
            buf[i + k] = b[k];
            nz += b[k] != 0;
        }
    }
    return nz;
}
static void dust(void)
{
    static int32_t a[FS / 2 + CTL], b[FS / 2 + CTL];
    int16_t e[8] = {40, 0, 30, 5, 64, 80, 40, 0};
    uint32_t n = FS / 2, nz;
    int same0, same30;
    double lo, hi;
    uint32_t i;
    char what[200];
    render_seeded(e, 12345u, a, n);
    render_seeded(e, 987654321u, b, n);
    same0 = !memcmp(a, b, n * sizeof *a);
    e[7] = 30;
    render_seeded(e, 12345u, a, n);
    render_seeded(e, 987654321u, b, n);
    same30 = !memcmp(a, b, n * sizeof *a);
    check("DUST 0: two noise seeds, the same samples (no noise at all); DUST 30: they differ", same0 && !same30);
    e[7] = 127;                                          /* the core gone: hiss and impulses through the open gate */
    e[5] = 127;
    nz = render_seeded(e, 777u, a, n);
    for (i = 0; i < NFFT; i++)
        y[i] = a[i + 4096] / 32768.0;
    spectrum(y, ps);
    for (i = 2, lo = hi = 0; i < NFFT / 2; i++)
        *(i < NFFT * 2000u / FS ? &lo : &hi) += ps[i];
    snprintf(what, sizeof what, "DUST 127: a noise patch (%u of %u samples sound), broadband (%.2f of it above 2 kHz)", nz, n,
             hi / (lo + hi + 1e-30));
    check(what, nz > n * 9 / 10 && hi / (lo + hi + 1e-30) > 0.2);
}

/* ---------------------------------------------------- 6. determinism, macros --- */
/* the brightness (left channel, through the mix: the matrix runs there) of a note, MODW -> dst at amt (0: none) */
static double mix_bright(const int16_t *e, uint32_t note, uint32_t vel, int32_t dst, int32_t amt)
{
    static int32_t bb[2 * CTL];
    uint32_t i, k;
    setup(e);
    if (dst) {
        trk[0].p[P_M1SRC] = PS_MODW; trk[0].p[P_M1DST] = (int16_t)dst; trk[0].p[P_M1AMT] = (int16_t)amt;
        trk[0].mw = 127;
    }
    trk_note_on(&trk[0], note, vel);
    for (i = 0; i < 2u; i++) mix_block(bb, CTL);
    for (i = 0; i < NFFT; i += CTL) {
        mix_block(bb, CTL);
        for (k = 0; k < CTL; k++) y[i + k] = bb[2 * k] / 32768.0;
    }
    spectrum(y, ps);
    return centroid(ps, hz_of_note(note));
}
static void determinism(void)
{
    static double y2[NFFT];
    int16_t e[8] = {70, 20, 40, 7, 64, 60, 50, 20};
    double b0, b1, f0, f1;
    char what[200];
    note_render(e, 60, 100, y, NFFT, 0);
    note_render(e, 60, 100, y2, NFFT, 0);
    check("a note from silence: the same samples twice (DUST 20 too: seeded by the note)", !memcmp(y, y2, sizeof y2));
    e[2] = 0; e[7] = 0; e[4] = 0; e[5] = 110;            /* (a low-pass held partly open: CUT moves its ceiling) */
    b0 = mix_bright(e, 48, 100, 0, 0);
    b1 = mix_bright(e, 48, 100, PD_CUT, -63);
    snprintf(what, sizeof what, "the track's CUT down (MODW -> CUT -63): the gate's ceiling lower, centroid %.2f -> %.2f", b0, b1);
    check(what, b1 < b0 * 0.8);
    e[0] = 20; e[4] = 127; e[5] = 127;
    f0 = mix_bright(e, 48, 64, 0, 0);
    f1 = mix_bright(e, 48, 64, PD_SHP, 63);
    snprintf(what, sizeof what, "SHP up (MODW -> SHP +63): it folds further, centroid %.2f -> %.2f", f0, f1);
    check(what, f1 > f0 + 1);
}

/* ------------------------------------------------------- 7. NOISE projects --- */
/* NOISE's eight presets' values as V10.7.35 had them {MODE COLOR FREQ PEAK TRK DENSE DRIFT CRUSH} */
static const int16_t NOISE_E[8][8] = {
    {0, 64, 70, 100, 127, 0, 70, 0}, {1, 30, 88, 80, 64, 105, 40, 0}, {2, 0, 127, 0, 127, 112, 0, 84},
    {3, 0, 100, 40, 100, 32, 0, 0}, {0, 40, 50, 60, 40, 0, 90, 0}, {0, 90, 95, 110, 127, 0, 20, 0},
    {0, 0, 127, 0, 0, 0, 0, 0}, {0, 75, 60, 20, 64, 0, 50, 10}};
static void projects(void)
{
    static project_t q;
    uint32_t k, i, ok = 1, sounds = 1;
    int16_t want[8];
    for (k = 0; k < 8u; k++) {
        memset(&q, 0, sizeof q);
        q.magic = PROJ_MAGIC;
        q.size = sizeof q;
        q.phys = 2;                                      /* (V10.7.35's) */
        for (i = 0; i < NTRK; i++) {
            uint32_t j;
            for (j = 0; j < P_COUNT; j++)
                q.t[i].p[j] = param_store_def(i == 2u ? ENGI_WEST : 0u, j);
            q.t[i].engine = (uint8_t)(i == 2u ? ENGI_WEST : 0u);
        }
        memcpy(&q.t[2].p[P_E0], NOISE_E[k], sizeof NOISE_E[k]);
        memcpy(&q.t[0].p[P_E0], NOISE_E[k], sizeof NOISE_E[k]);   /* (an ANALOG track with the same numbers: kept) */
        q.t[2].preset = (uint8_t)k;
        q.motion.count = 3;
        q.motion.event[0].place = 2u << 6; q.motion.event[0].param = P_E0 + 2; q.motion.event[0].value = 30;   /* T3 FREQ */
        q.motion.event[1].place = 2u << 6; q.motion.event[1].param = P_LEVEL; q.motion.event[1].value = 90;   /* T3 LEVEL */
        q.motion.event[2].place = 0u << 6; q.motion.event[2].param = P_E0 + 2; q.motion.event[2].value = 30;  /* T1's E */
        memcpy(want, NOISE_E[k], sizeof want);
        west_from_noise(want);
        proj_phys_gone(&q);
        ok &= !memcmp(&q.t[2].p[P_E0], want, sizeof want) && q.phys == PROJ_PHYS && q.t[2].engine == ENGI_WEST &&
              q.t[2].preset == k && !memcmp(&q.t[0].p[P_E0], NOISE_E[k], sizeof NOISE_E[k]) && q.motion.count == 2 &&
              q.motion.event[0].param == P_LEVEL && q.motion.event[1].param == P_E0 + 2 && MOTION_TRK(&q.motion.event[1]) == 0;
        ok &= want[7] >= 96 || k == 2u || k == 3u;       /* (the noise modes a DUST patch: the core silent) */
        proj_phys_gone(&q);                              /* (again: nothing) */
        ok &= !memcmp(&q.t[2].p[P_E0], want, sizeof want);
        {
            double dc;
            sounds &= corner_ok(want, &dc);
            note_render(want, 60, 100, y, FS / 4, 0);
            sounds &= rms(y, FS / 4) > 0.01;
        }
    }
    check("NOISE projects (phys 2): engine 11's values west_from_noise's, its EDIT motion gone, the rest kept, phys 3; twice the same",
          ok);
    check("  each of NOISE's eight sounds so migrated: bounded and sounding", sounds);
    memset(&q, 0, sizeof q);
    q.phys = PROJ_PHYS;
    q.t[2].engine = ENGI_WEST;
    memcpy(&q.t[2].p[P_E0], NOISE_E[0], sizeof NOISE_E[0]);
    proj_phys_gone(&q);
    check("a project of today (phys 3): its WEST tracks as they are", !memcmp(&q.t[2].p[P_E0], NOISE_E[0], sizeof NOISE_E[0]));
}

/* -------------------------------------------------------------- 8. cost --- */
#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
static int perf_fd = -2;
#endif
static int cost_instr;                                   /* 1: instructions counted, 0: ns */
static uint64_t instr_now(void)
{
#ifdef __APPLE__
    struct rusage_info_v4 ri;
    cost_instr = 1;
    if (!proc_pid_rusage(getpid(), RUSAGE_INFO_V4, (rusage_info_t *)&ri))
        return ri.ri_instructions;
    return 0;
#else
    struct timespec ts;
#ifdef __linux__
    if (perf_fd == -2) {                                 /* (user-space instructions of this process, where allowed) */
        struct perf_event_attr a;
        memset(&a, 0, sizeof a);
        a.type = PERF_TYPE_HARDWARE;
        a.size = sizeof a;
        a.config = PERF_COUNT_HW_INSTRUCTIONS;
        a.exclude_kernel = 1;
        a.exclude_hv = 1;
        perf_fd = (int)syscall(__NR_perf_event_open, &a, 0, -1, -1, 0);
    }
    if (perf_fd >= 0) {
        uint64_t v;
        if (read(perf_fd, &v, sizeof v) == (ssize_t)sizeof v) {
            cost_instr = 1;
            return v;
        }
    }
#endif
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
#endif
}
static double part_cost(uint32_t eng, uint32_t pi, const int16_t *e)
{
    static int32_t o[2 * CTL];
    uint32_t i, nb = FS * 2u / CTL, r;
    double best = 1e30;
    for (r = 0; r < 5u; r++) {                           /* (the best of five: the host's noise) */
        uint64_t i0;
        memset(trk, 0, sizeof trk);
        host_tracks_init();
        host_preset(&trk[0], eng, pi);
        if (e)
            for (i = 0; i < 8u; i++) trk[0].p[P_E0 + i] = e[i];
        trk[0].p[P_SUS] = 127;
        trk[0].p[P_VOICE] = V_POLY;
        for (i = 0; i < 8u; i++)
            trk_note_on(&trk[0], 48 + 3 * i, 100);
        for (i = 0; i < 16u; i++)
            mix_block(o, CTL);
        i0 = instr_now();
        for (i = 0; i < nb; i++)
            mix_block(o, CTL);
        best = fmin(best, (double)(instr_now() - i0) / (nb * CTL));
    }
    return best;
}
static void cost(void)
{
    static const int16_t HEAVY[8] = {127, 127, 127, 15, 64, 127, 127, 72};   /* every stage on, the core whole, FM AM, impulses */
    static const int16_t HEAVY2[8] = {127, 127, 127, 31, 64, 127, 127, 60};   /* .. the triangle core */
    const engine_t *an = ENGINES[ENGI_ANALOG];
    double worst_an = 0, worst = 0, c, idle;
    uint32_t pi;
    char what[200];
    idle = part_cost(ENGI_WEST, 0, 0);
    {   /* the idle mix: no notes (as part_cost, none held) */
        static int32_t o[2 * CTL];
        uint32_t i, nb = FS * 2u / CTL, r;
        idle = 1e30;
        for (r = 0; r < 5u; r++) {
            uint64_t i0;
            memset(trk, 0, sizeof trk);
            host_tracks_init();
            i0 = instr_now();
            for (i = 0; i < nb; i++) mix_block(o, CTL);
            idle = fmin(idle, (double)(instr_now() - i0) / (nb * CTL));
        }
    }
    for (pi = 0; pi < an->npresets; pi++) {
        c = part_cost(ENGI_ANALOG, pi, 0) - idle;
        if (getenv("VERBOSE")) printf("west:   ANALOG %-12s %6.1f\n", an->presets[pi].name, c);
        worst_an = c > worst_an ? c : worst_an;
    }
    for (pi = 0; pi < ENG_WEST.npresets; pi++) {
        c = part_cost(ENGI_WEST, pi, 0) - idle;
        if (getenv("VERBOSE")) printf("west:   %-12s %6.1f\n", ENG_WEST.presets[pi].name, c);
        worst = c > worst ? c : worst;
    }
    c = part_cost(ENGI_WEST, 0, HEAVY) - idle;
    if (getenv("VERBOSE")) printf("west:   HEAVY        %6.1f\n", c);
    worst = c > worst ? c : worst;
    c = part_cost(ENGI_WEST, 0, HEAVY2) - idle;
    if (getenv("VERBOSE")) printf("west:   HEAVY2       %6.1f\n", c);
    worst = c > worst ? c : worst;
    snprintf(what, sizeof what, "cost, 8 voices (%s a sample over the idle mix): WEST's heaviest %.1f, ANALOG's %.1f (x%.2f, at most 1.25)",
             cost_instr ? "instructions" : "ns", worst, worst_an, worst / worst_an);
    if (cost_instr)
        check(what, worst <= worst_an * 1.25);
    else                                                 /* (a shared host's clock: for information; tests/cpu_vg.c counts
                                                          * WEST's corners exactly under valgrind) */
        printf("west: %s (ns: not checked)\n", what);
}

/* --------------------------------------------------------------- demos --- */
static void demo(const char *dir, uint32_t pi)
{
    static int32_t o[2 * CTL];
    const preset_t *pr = &ENG_WEST.presets[pi];
    uint32_t pat = pr->pat ? pr->pat - 1u : 2u, step = FS / 8u, s, i, held = 0, frames = 0;
    char path[512], nm[32];
    FILE *w;
    for (i = 0; pr->name[i] && i < 31u; i++)
        nm[i] = pr->name[i] == ' ' ? '_' : pr->name[i];
    nm[i] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi, nm);
    if (!(w = fopen(path, "wb")))
        return;
    wav_hdr(w, 0);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ENGI_WEST, pi);
    for (s = 0; s < 32u; s++) {                          /* twice through the pattern's 16 steps, 120 BPM 1/16 */
        uint32_t k = s % 16u, note = PATTERNS[pat].note[k], fl = PATTERNS[pat].flags[k];
        if (!(fl & 4u) && held) {
            trk_note_off(&trk[0], held);
            held = 0;
        }
        if (note && !(fl & 4u)) {
            trk_note_on(&trk[0], note, fl & 1u ? 120 : 90);
            held = note;
        }
        for (i = 0; i < step; i += CTL) {
            uint32_t j;
            mix_block(o, CTL);
            for (j = 0; j < CTL; j++)
                wav_put(w, o[2 * j], o[2 * j + 1]);
            frames += CTL;
        }
    }
    if (held)
        trk_note_off(&trk[0], held);
    for (i = 0; i < FS * 2u; i += CTL) {
        uint32_t j;
        mix_block(o, CTL);
        for (j = 0; j < CTL; j++)
            wav_put(w, o[2 * j], o[2 * j + 1]);
        frames += CTL;
    }
    fseek(w, 0, SEEK_SET);
    wav_hdr(w, frames);
    fclose(w);
}

int main(int argc, char **argv)
{
    if (getenv("COSTONLY")) { cost(); return bad != 0; }
    bounded();
    fold();
    gate();
    vactrol();
    velocity();
    dust();
    determinism();
    projects();
    if (!getenv("NOCOST"))
        cost();
    if (argc > 1) {
        uint32_t pi;
        for (pi = 0; pi < ENG_WEST.npresets; pi++)
            demo(argv[1], pi);
        printf("west: demos in %s: the %u presets, each playing its suggested pattern\n", argv[1], (uint32_t)ENG_WEST.npresets);
    }
    printf("west_test: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
