/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.44) VOICE's MODE TRACT (src/eng_tract.c: a Kelly-Lochbaum tube) on the host, through hostsim.c as regress.c.
 *   build/host/tract_test [DEMODIR]     (run_tests.sh: build/tract_demo; DEMODIR: each preset, a vowel sweep, a phrase)
 * 1. formants: AH EE OO whispered (BREATH 127: the noise draws the tube's response), F1 / F2 by FFT in their regions
 *    (AH ~700 / 1100 Hz, EE ~300 / 2300, OO ~300 / 800), and sung at C2 (its spectral envelope's F1 F2 the same);
 * 2. NASAL: the velum open adds an antiformant (a dip, >= 10 dB under the oral spectrum, between F1 and 3 kHz);
 * 3. bounded at every corner of the eight knobs (256 corners x two notes, velocity 127): the output under full scale,
 *    the waves inside the tube under 2^16 (the 32-bit products' bound: eng_tract.c), finite; stable at the maximum (all
 *    knobs at the top, 4 s: no growth); no DC (|mean| < 0.2 % of the RMS, at every corner sounding);
 * 4. on the step: a sequenced vowel heard within 0 .. +16 ms of its step (TUBE AH), a sung syllable's vowel within
 *    +-16 ms (TALK TUBE, WORDS); a glide between two vowels smooth (no step in the shape);
 * 5. VOWEL mode untouched: MODE 0 renders as before (the goldens hold it bit for bit), two voices at most in TRACT. */
#define main hostsim_main
#include <ctype.h>
#include "hostsim.c"
#undef main
#include "jiant_test_util.h"

static int bad;
static void check(const char *what, int ok) { bad += jt_verdict("tract: ", 100, what, ok, NULL); }
static int32_t preset_of(const char *name)
{
    uint32_t k;
    for (k = 0; k < ENG_FORMANT.npresets; k++)
        if (str_eq(ENG_FORMANT.presets[k].name, name))
            return (int32_t)k;
    printf("tract: no preset %s\n", name);
    bad++;
    return 0;
}
#define NFFT 8192u
static int32_t wmax;                                 /* the largest wave inside the tube seen */
static void wave_peak(void)
{
    uint32_t v, i;
    for (v = 0; v < TR_NV; v++) {
        const int32_t *w = tr_wav(0, v);
        for (i = 0; i < TR_W; i++)
            if (abs(w[i]) > wmax) wmax = abs(w[i]);
    }
}
/* track 0 VOICE preset pi, dry, the others silent */
static void tsetup(int32_t pi)
{
    uint32_t j;
    memset(trk, 0, sizeof trk);
    memset(sing, 0, sizeof sing);
    memset(sing_io, 0, sizeof sing_io);
    memset(tr_mode, 0, sizeof tr_mode);
    host_tracks_init();
    host_preset(&trk[0], ENGI_VOICE, (uint32_t)pi);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    for (j = 1; j < NPART; j++) trk[j].p[P_LEVEL] = 0;
}
static uint32_t frames;
static FILE *wav;
static void trender(double *y, uint32_t n)
{
    static int32_t o[2 * CTL];
    uint32_t i, j;
    for (i = 0; i < n; i += CTL) {
        mix_block(o, CTL);
        wave_peak();
        for (j = 0; j < CTL; j++) {
            if (y && i + j < n) y[i + j] = (o[2 * j] + o[2 * j + 1]) / 65536.0;
            if (wav) { int16_t m = (int16_t)clamp((o[2 * j] + o[2 * j + 1]) >> 1, -32768, 32767); fwrite(&m, 2, 1, wav); }
        }
        frames += CTL;
    }
}
static void spectrum(const double *y, uint32_t n, double *P)
{
    static double re[NFFT], im[NFFT];
    jt_hann_power(y, n, P, NFFT, re, im);
}
static double bin_hz(uint32_t k) { return (double)k * FS / NFFT; }
/* the strongest frequency in lo .. hi Hz of P smoothed over +-sm bins, the tilt taken out (P / f^tl: the radiation's
 * +6 dB / octave on a flat noise, tl 2; the source's -12 under it, sung, tl 0) */
static double peak_in(const double *P, double lo, double hi, uint32_t sm, double tl)
{
    uint32_t k, best = 0;
    double bv = -1;
    for (k = (uint32_t)(lo * NFFT / FS); k <= (uint32_t)(hi * NFFT / FS) && k + sm < NFFT / 2u; k++) {
        double s = 0;
        uint32_t j;
        for (j = k - sm; j <= k + sm; j++) s += P[j] / pow(bin_hz(j), tl);
        if (s > bv) { bv = s; best = k; }
    }
    return bin_hz(best);
}
/* the first two formants: the two strongest local maxima (each the highest within +-150 Hz) of P smoothed over +-sm
 * bins, the tilt out (as peak_in), between 180 and 2800 Hz, in order */
static void two_peaks(const double *P, uint32_t sm, double tl, double *f1, double *f2)
{
    static double S[NFFT / 2];
    uint32_t k, j, lo = (uint32_t)(180.0 * NFFT / FS), hi = (uint32_t)(2800.0 * NFFT / FS), W = (uint32_t)(150.0 * NFFT / FS);
    uint32_t b1 = 0, b2 = 0;
    for (k = sm + 1u; k < hi + W; k++) {
        double s = 0;
        for (j = k - sm; j <= k + sm; j++) s += P[j] / pow(bin_hz(j), tl);
        S[k] = s;
    }
    for (k = lo; k < hi; k++) {
        int top = 1;
        for (j = k > lo + W ? k - W : lo; j <= k + W && top; j++) top = j == k || S[j] < S[k];   /* (not below lo: the noise
                                                         * under it, tilt out, rises) */
        if (!top) continue;
        if (!b1 || S[k] > S[b1]) { b2 = b1; b1 = k; } else if (!b2 || S[k] > S[b2]) b2 = k;
    }
    if (b2 && b2 < b1) { k = b1; b1 = b2; b2 = k; }
    *f1 = bin_hz(b1);
    *f2 = bin_hz(b2);
}
/* the spectrum smoothed over +-sm bins, in dB, tilt removed (the radiation's +6 dB / octave) */
static void smooth_db(const double *P, double *D, uint32_t sm)
{
    uint32_t k, j;
    for (k = 0; k < NFFT / 2u; k++) {
        double s = 0;
        uint32_t a = k > sm ? k - sm : 0, b = k + sm < NFFT / 2u ? k + sm : NFFT / 2u - 1u;
        for (j = a; j <= b; j++) s += P[j];
        D[k] = 10 * log10(s / (b - a + 1) + 1e-30) - 20 * log10(bin_hz(k > 2 ? k : 2));
    }
}
/* a held vowel: knobs e[0..7] (or the preset's: e 0), note, n samples of its steady part */
static void held(int32_t pi, const int16_t *e, uint32_t note, double *y, uint32_t n)
{
    uint32_t k;
    tsetup(pi);
    if (e)
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = e[k];
    trk_note_on(&trk[0], note, 100);
    trender(0, FS / 4);
    trender(y, n);
}
static void check_formants(void)
{
    static double y[FS], P[NFFT / 2];
    static const struct { const char *p; double f1lo, f1hi, f2lo, f2hi; } V[] = {
        {"TUBE AH", 600, 900, 950, 1350}, {"TUBE EE", 200, 400, 1850, 2600}, {"TUBE OO", 200, 420, 650, 1050},
    };
    char what[240];
    uint32_t k, ok = 1, oks = 1;
    double f[3][4];
    for (k = 0; k < NELEM(V); k++) {
        int32_t pi = preset_of(V[k].p);
        int16_t e[8];
        uint32_t j;
        for (j = 0; j < 8u; j++) e[j] = ENG_FORMANT.presets[pi].e[j];
        e[4] = 127;                                      /* whispered */
        held(pi, e, 48, y, FS / 2);
        spectrum(y, FS / 2, P);
        two_peaks(P, 8, 2, &f[k][0], &f[k][1]);
        ok &= f[k][0] >= V[k].f1lo && f[k][0] <= V[k].f1hi && f[k][1] >= V[k].f2lo && f[k][1] <= V[k].f2hi;
        e[4] = 0;                                        /* sung, C2: the harmonics' envelope */
        held(pi, e, 36, y, FS / 2);
        spectrum(y, FS / 2, P);
        two_peaks(P, 12, -1, &f[k][2], &f[k][3]);
        oks &= fabs(f[k][2] - f[k][0]) < 130 && fabs(f[k][3] - f[k][1]) < 260;
        printf("tract:   %-8s whispered F1 %4.0f F2 %4.0f Hz, sung at C2 F1 %4.0f F2 %4.0f\n", V[k].p, f[k][0], f[k][1],
               f[k][2], f[k][3]);
    }
    snprintf(what, sizeof what, "AH EE OO: F1 / F2 in their regions (AH %.0f/%.0f, EE %.0f/%.0f, OO %.0f/%.0f Hz)", f[0][0],
             f[0][1], f[1][0], f[1][1], f[2][0], f[2][1]);
    check(what, ok);
    check("sung (C2) as whispered: the harmonics' envelope peaks within 130 / 260 Hz of the tube's", oks);
}
static int dcmp(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return x < y ? -1 : x > y; }
static void check_nasal(void)
{
    static double y[FS], P[NFFT / 2], A[NFFT / 2], B[NFFT / 2], D[NFFT / 2];
    char what[200];
    int32_t pi = preset_of("TUBE EE");
    int16_t e[8];
    uint32_t j, k, lo = (uint32_t)(300.0 * NFFT / FS), hi = (uint32_t)(3000.0 * NFFT / FS);
    double dip = 0, at = 0, med;
    for (j = 0; j < 8u; j++) e[j] = ENG_FORMANT.presets[pi].e[j];
    e[4] = 127;
    held(pi, e, 48, y, FS / 2);
    spectrum(y, FS / 2, P);
    smooth_db(P, A, 3);
    e[3] = 12;                                           /* the velum open */
    held(pi, e, 48, y, FS / 2);
    spectrum(y, FS / 2, P);
    smooth_db(P, B, 3);
    for (k = lo; k < hi; k++) D[k - lo] = A[k] - B[k];
    qsort(D, hi - lo, sizeof D[0], dcmp);
    med = D[(hi - lo) / 2];                              /* (the levels against each other: the nose's own share) */
    for (k = lo; k < hi; k++)
        if (A[k] - B[k] - med > dip) { dip = A[k] - B[k] - med; at = bin_hz(k); }
    printf("tract:   NASAL 12 on EE: a dip %.1f dB under the oral spectrum at %.0f Hz\n", dip, at);
    snprintf(what, sizeof what, "NASAL adds an antiformant: EE's spectrum %.1f dB down at %.0f Hz (>= 10 dB, 300 .. 3000 Hz)",
             dip, at);
    check(what, dip >= 10.0);
}
static void check_bounds(void)
{
    static double y[FS / 4];
    char what[240];
    uint32_t c, k, nt, ok = 1, dcok = 1;
    double peak = 0, worst_dc = 0;
    wmax = 0;
    for (c = 0; c < 256u; c++)
        for (nt = 0; nt < 2u; nt++) {
            tsetup(preset_of("TUBE AH"));
            for (k = 0; k < 8u; k++)
                trk[0].p[P_E0 + k] = (int16_t)(c >> k & 1u ? (k == 3u ? 12 : 127) : 0);
            trk[0].p[P_LEVEL] = 127;
            trk_note_on(&trk[0], nt ? 84 : 28, 127);
            trk_note_on(&trk[0], nt ? 91 : 35, 127);
            trender(y, FS / 4);
            for (k = 0; k < FS / 4; k++) {
                if (!isfinite(y[k])) ok = 0;
                if (fabs(y[k]) > peak) peak = fabs(y[k]);
            }
        }
    for (c = 0; c < 16u; c++) {                          /* DC: 2 s held at 16 corners, the power under 16 Hz */
        static double z[FS * 2], P[NFFT / 2];
        double lo = 0, all = 0;
        tsetup(preset_of("TUBE AH"));
        for (k = 0; k < 8u; k++)
            trk[0].p[P_E0 + k] = (int16_t)((c * 0x9Du) >> k & 1u ? (k == 3u ? 12 : 127) : 0);
        trk[0].p[P_E4] = (int16_t)(c & 1u ? 0 : 40);     /* (voiced) */
        trk_note_on(&trk[0], c & 2u ? 40 : 64, 100);
        trender(z, FS * 2);
        spectrum(z + FS / 2, FS * 3 / 2, P);
        for (k = 1; k < NFFT / 2u; k++) {
            all += P[k];
            if (bin_hz(k) < 16.0) lo += P[k];
        }
        lo += P[0];
        if (all > 0 && lo / all > worst_dc) worst_dc = lo / all;
    }
    dcok = worst_dc < 1e-4;
    snprintf(what, sizeof what, "bounded at every corner (256 x 2 notes, velocity 127, LEVEL 127): peak %.3f of full "
             "scale, the tube's waves at most %d (< 65536)", peak / 2, wmax);
    check(what, ok && peak / 2 < 1.0 && wmax < 65536);
    snprintf(what, sizeof what, "no DC at 16 corners sounding: the power under 16 Hz %.1f dB of the whole at worst (< -40)",
             10 * log10(worst_dc + 1e-30));
    check(what, dcok);
    {   /* stable at the maximum: 4 s held, no growth */
        static double z[FS * 4];
        double r0, r1;
        tsetup(preset_of("TUBE AH"));
        for (k = 0; k < 8u; k++) trk[0].p[P_E0 + k] = (int16_t)(k == 3u ? 12 : 127);
        trk_note_on(&trk[0], 40, 127);
        trender(z, FS * 4);
        r0 = jt_rms_d(z, FS / 2, FS);
        r1 = jt_rms_d(z, FS * 3, FS * 4);
        snprintf(what, sizeof what, "stable at the maximum (every knob at the top, 4 s): RMS %.4f then %.4f", r0, r1);
        check(what, r1 < r0 * 1.5 + 1e-4 && r1 > 0);
    }
}
/* the vowel heard (as voice_test.c's): y's 350 .. 2000 Hz band (4 poles each side), its energy over 4 ms each ms; in
 * [a, b): the vowel's level the most after mid, the least before it, then the first rise to -6 dB of the vowel's */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } bq_t;
static void bq_set(bq_t *q, int hp, double f)
{
    double w = 2 * M_PI * f / FS, c = cos(w), al = sin(w) / (2 * 0.7071), a0 = 1 + al;
    q->b0 = q->b2 = (hp ? (1 + c) : (1 - c)) / 2 / a0;
    q->b1 = (hp ? -(1 + c) : (1 - c)) / a0;
    q->a1 = -2 * c / a0; q->a2 = (1 - al) / a0; q->z1 = q->z2 = 0;
}
static double bq(bq_t *q, double x)
{
    double y = q->b0 * x + q->z1;
    q->z1 = q->b1 * x - q->a1 * y + q->z2;
    q->z2 = q->b2 * x - q->a2 * y;
    return y;
}
static double band_on(const double *y, uint32_t a, uint32_t mid, uint32_t b)
{
    static double f[FS * 3];
    bq_t q[4];
    double ref = 0, mn = 1e30;
    uint32_t i, j, am = a, W = FS / 250u, H = FS / 1000u;
    bq_set(&q[0], 1, 350); bq_set(&q[1], 1, 350); bq_set(&q[2], 0, 2000); bq_set(&q[3], 0, 2000);
    for (i = 0; i < b; i++)
        f[i] = bq(&q[3], bq(&q[2], bq(&q[1], bq(&q[0], y[i]))));
    for (i = a; i + W < b; i += H) {
        double e = 0;
        for (j = 0; j < W; j++) e += f[i + j] * f[i + j];
        f[i] = e;
    }
    for (i = mid; i + W < b; i += H)
        if (f[i] > ref) ref = f[i];
    for (i = a; i <= mid; i += H)
        if (f[i] < mn) { mn = f[i]; am = i; }
    for (i = am; i + W < b && f[i] < ref * 0.25; i += H)
        ;
    return (double)i + W / 2.0;
}
static void check_ontime(void)
{
    static double y[FS * 3];
    char what[240];
    uint32_t w, j, period, n;
    double lo[2] = {1e9, 1e9}, hi[2] = {-1e9, -1e9};
    for (w = 0; w < 2u; w++) {
        tsetup(preset_of(w ? (getenv("TREF") ? "CANTOR ES" : "TALK TUBE") : "TUBE AH"));
        if (w) {
            lyr_set(0, "pa ta ka ba sa fa cha va pa ta");    /* (Spanish, as voice_test.c's: its plosives unaspirated) */
            trk[0].p[P_VLANG] = LG_ES;
            trk[0].p[P_VLYR] = 0;
            trk[0].p[P_ATK] = ENG_FORMANT.presets[preset_of("TALK TUBE")].env[0];
        }
        trk[0].p[P_SLEN] = 8;
        trk[0].p[P_REL] = w ? trk[0].p[P_REL] : 0;      /* (the vowels: each note heard from a gap) */
        trk[0].p[P_SGATE] = w ? trk[0].p[P_SGATE] : 40;
        trk[0].p[P_SDIV] = 1;
        for (j = 0; j < 8u; j++) {
            trk[0].step[j].time = ST_NOTE; trk[0].step[j].n = 1; trk[0].step[j].note[0] = 52; trk[0].step[j].vel = 100;
        }
        song.g[G_BPM] = 130;
        period = div_samples(1);
        seq_start();
        for (n = 0; n + CTL <= period * 8u && n + CTL <= NELEM(y); n += CTL)
            trender(y + n, CTL);
        seq_stop();
        if (getenv("TDEBUG")) {
            uint32_t q;
            for (q = period; q < period * 3u; q += FS / 1000u) {
                double e = 0; uint32_t r;
                for (r = 0; r < FS / 1000u; r++) e += y[q + r] * y[q + r];
                printf("%u %.1f %.6f\n", w, (q - (double)period) * 1000.0 / FS, sqrt(e / (FS / 1000u)));
            }
        }
        for (j = 2; j < 7u; j++) {
            double g = (double)(j - 1u) * period,
                   t = (band_on(y, (uint32_t)g - period / 2u, (uint32_t)g + FS / 25u, (uint32_t)g + period - FS / 50u) - g) *
                       1000.0 / FS;
            if (getenv("TDEBUG")) printf("tract:   step %u: %.1f ms\n", j, t);
            if (t < lo[w]) lo[w] = t;
            if (t > hi[w]) hi[w] = t;
        }
    }
    printf("tract:   on the step: TUBE AH heard %.1f .. %.1f ms, TALK TUBE's vowels %.1f .. %.1f ms of their steps\n", lo[0],
           hi[0], lo[1], hi[1]);
    snprintf(what, sizeof what, "the vowel lands on the step: heard %.0f .. %.0f ms (0 .. 16), sung syllables %.0f .. %.0f ms "
             "(-16 .. 16)", lo[0], hi[0], lo[1], hi[1]);
    check(what, lo[0] >= -1 && hi[0] <= 16 && lo[1] >= -16 && hi[1] <= 16);
}
static void check_glide(void)
{
    /* FRONT 0 -> 127 at once: the shape glides there (each block's area within 25 % of the last's) */
    tr_ctl_t *c;
    int16_t last[TR_N];
    uint32_t b, i, ok = 1;
    double worst = 0;
    tsetup(preset_of("TUBE AH"));
    trk_note_on(&trk[0], 48, 100);
    trender(0, FS / 4);
    c = tr_ctl(0);
    memcpy(last, c->a, sizeof last);
    trk[0].p[P_E0] = 127;
    trk[0].p[P_E1] = 0;
    for (b = 0; b < 200u; b++) {
        trender(0, CTL);
        for (i = 0; i < TR_N; i++) {
            double r = fabs((double)c->a[i] - last[i]) / (last[i] > 128 ? last[i] : 128.0);   /* (of 0.5 cm^2 at least) */
            if (r > worst) worst = r;
        }
        memcpy(last, c->a, sizeof last);
    }
    ok = worst < 0.4 && c->a[16] < 100;
    {
        char what[160];
        snprintf(what, sizeof what, "AH -> EE glides: the area moves %.1f %% a block at most (< 40 %%: 1.45 ms), there after 0.3 s",
                 worst * 100);
        check(what, ok);
    }
    {   /* two voices at most */
        uint32_t k, act = 0;
        tsetup(preset_of("TUBE CHOIR"));
        for (k = 0; k < 4u; k++) trk_note_on(&trk[0], 48 + 4 * k, 100);
        trender(0, CTL * 4);
        for (k = 0; k < NVOICE; k++) act += trk[0].v[k].active;
        check("TRACT plays two voices at most (four keys held)", act == 2u);
    }
}
/* a mono 16-bit 44.1 kHz WAV's header (the demos: the mix of the two channels) */
static void mono_hdr(FILE *f, uint32_t frames)
{
    uint32_t v;
    uint16_t a = 1, ch = 1, ba = 2, bits = 16;
    uint32_t sr = FS, br = FS * 2;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 2; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 2; fwrite(&v, 4, 1, f);
}
static void demo(const char *dir, const char *name, int32_t pi, const char *text, uint32_t sweep)
{
    static const uint8_t MEL[] = {48, 50, 52, 55, 57, 55, 52, 50, 48, 52, 55, 60, 57, 55, 52, 48};
    char path[512];
    uint32_t k, j;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(wav = fopen(path, "wb")))
        return;
    mono_hdr(wav, 0);
    frames = 0;
    tsetup(pi);
    trk[0].p[P_ATK] = ENG_FORMANT.presets[pi].env[0];
    trk[0].p[P_REV] = 30;
    if (text) {
        lyr_set(0, text);
        trk[0].p[P_VLYR] = 0;
    }
    if (sweep) {                                         /* AH -> EE -> OO -> AH, 1.2 s each way, held 0.3 s at each */
        static const int16_t K[4][3] = {{10, 115, 0}, {127, 0, 0}, {38, 0, 127}, {10, 115, 0}};
        trk_note_on(&trk[0], 45, 100);
        for (k = 0; k < 4u * FS / CTL; k++) {
            uint32_t seg = k / (FS / CTL), q = k % (FS / CTL);
            double u = seg < 3u ? (q < FS / CTL / 4u ? 0.0 : (double)(q - FS / CTL / 4u) / (FS / CTL * 3u / 4u)) : 0.0;
            for (j = 0; j < 3u; j++)
                trk[0].p[P_E0 + j] = (int16_t)(K[seg < 3u ? seg : 3u][j] + (seg < 3u ? (K[seg + 1u][j] - K[seg][j]) * u : 0));
            trender(0, CTL);
        }
        trk_note_off(&trk[0], 45);
        trender(0, FS / 2);
    } else {
        uint32_t n = text ? 10u : 8u, len = FS * 60u / 100u / 2u;
        for (k = 0; k < n; k++) {
            uint32_t note = text ? MEL[k] : MEL[(k * 3u) % 16u] - (ENG_FORMANT.presets[pi].mono & 1u ? 12u : 0u);
            trk_note_on(&trk[0], note, 100);
            if (!text && !(ENG_FORMANT.presets[pi].mono & 1u) && k % 2u == 0u)
                trk_note_on(&trk[0], note + 7u, 90);
            for (j = 0; j < len; j += CTL) trender(0, CTL);
            trk_note_off(&trk[0], note);
            trk_note_off(&trk[0], note + 7u);
            for (j = 0; j < len / 4u; j += CTL) trender(0, CTL);
        }
        trender(0, FS / 2);
    }
    fseek(wav, 0, SEEK_SET);
    mono_hdr(wav, frames);
    fclose(wav);
    wav = 0;
}
int main(int argc, char **argv)
{
    check_formants();
    check_nasal();
    check_bounds();
    check_ontime();
    check_glide();
    if (argc > 1) {
        uint32_t k;
        for (k = 0; k < ENG_FORMANT.npresets; k++) {
            const preset_t *pr = &ENG_FORMANT.presets[k];
            char nm[40];
            uint32_t i;
            if (!pr->mod || pr->mod[0] != P_VTRACT)
                continue;
            for (i = 0; pr->name[i] && i < 30u; i++) nm[i] = pr->name[i] == ' ' ? '_' : (char)tolower(pr->name[i]);
            nm[i] = 0;
            demo(argv[1], nm, (int32_t)k, pr->xp ? "hello my friend the night is long and the sea is calm" : 0, 0);
        }
        demo(argv[1], "vowel_sweep", preset_of("TUBE AH"), 0, 1);
        demo(argv[1], "sung_phrase_en", preset_of("TALK TUBE"), "hello my friend the night is long and the sea is calm", 0);
        printf("tract:   demos in %s\n", argv[1]);
    }
    printf(bad ? "tract: FAIL (%d)\n" : "tract: ok\n", bad);
    return bad ? 1 : 0;
}
