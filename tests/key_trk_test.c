/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.37) KEY TRK (EDIT > FILTER; eng_analog.c kt_cut, voice.c tf_run): OFF the filter stays, 100 % its resonance and
 * the COMB follow two notes an octave apart, 50 % half of it, NOTE puts a swept cutoff on the semitones. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
#define N 16384
static double buf[N];
static void setup(int ty, int kt, int note, int cut, int res)
{
    track_t *t = &trk[0];
    host_tracks_init();
    t->engine = ENGI_FM6;
    t->p[P_FTYPE] = (int16_t)ty; t->p[P_FCUT] = (int16_t)cut; t->p[P_FRES] = (int16_t)res;
    t->p[P_LD_FLT] = 0; t->p[P_ED_FLT] = 0;
    kt_v[0] = (uint8_t)kt;
    memset(t->v, 0, sizeof t->v);
    t->v[0].active = 1; t->v[0].age = 1; t->v[0].pitch_cur = (int16_t)(note * 16); t->v[0].pitch16 = (int16_t)(note * 16);
    tf_on[0] = 0; tf_cmb[0] = 0; tf_xb[0] = 0; tf_ft[0] = (uint8_t)ty; tf_ic1[0] = tf_ic2[0] = 0;
}
/* the part's filter of TYPE ty with one voice at note sounding: the frequency it rings at, Hz. The SVF: a sine at each
 * eighth of a semitone, the loudest out; the COMB: white noise, its period (the first lag as alike as the best) */
static double peak(int ty, int kt, int note, int cut, int res)
{
    track_t *t = &trk[0];
    uint32_t b, i, r = 12345u;
    int32_t blk[CTL];
    double best = 0, bf = 0, f;
    if (ty < FT_COMB) {
        for (f = 200; f < 6000; f *= 1.00724) {
            double e = 0;
            setup(ty, kt, note, cut, res);
            for (b = 0; b < 48u; b++) {
                for (i = 0; i < CTL; i++) blk[i] = (int32_t)(2000 * sin(2 * M_PI * f * (b * CTL + i) / 44100.0));
                track_filter(t, blk, CTL, 0, t->p[P_FCUT], t->p[P_FRES]);
                if (b >= 24u)
                    for (i = 0; i < CTL; i++) e += (double)blk[i] * blk[i];
            }
            if (e > best) { best = e; bf = f; }
        }
        kt_v[0] = 0;
        return bf;
    }
    setup(ty, kt, note, cut, res);
    for (b = 0; b < N / CTL + 64u; b++) {
        for (i = 0; i < CTL; i++) { r = r * 1664525u + 1013904223u; blk[i] = (int32_t)(r >> 18) - 8192; }
        track_filter(t, blk, CTL, 0, t->p[P_FCUT], t->p[P_FRES]);
        if (b >= 64u)
            for (i = 0; i < CTL; i++) buf[(b - 64u) * CTL + i] = blk[i];
    }
    kt_v[0] = 0;
    {
        double ac[700], mx = 0;
        uint32_t l;
        for (l = 30; l < 700u; l++) {
            double x = 0;
            for (i = 0; i + l < N; i++) x += buf[i] * buf[i + l];
            ac[l] = x;
            if (x > mx) mx = x;
        }
        for (l = 31; l < 699u; l++)
            if (ac[l] > 0.85 * mx && ac[l] >= ac[l - 1u] && ac[l] >= ac[l + 1u]) {
                double d = ac[l - 1u] - 2 * ac[l] + ac[l + 1u];   /* (the peak between the lags: a parabola) */
                return 44100.0 / (l + (d ? 0.5 * (ac[l - 1u] - ac[l + 1u]) / d : 0));
            }
    }
    return 0;
}
/* (V10.8.38) the peak to a 64th of a semitone: about the coarse one, the loudest of a finer sweep */
static double fine(int ty, int kt, int note, int cut, int res)
{
    track_t *t = &trk[0];
    double c = peak(ty, kt, note, cut, res), best = 0, bf = c, f;
    uint32_t b, i;
    int32_t blk[CTL];
    for (f = c / 1.03; f < c * 1.03; f *= 1.0009) {
        double e = 0;
        setup(ty, kt, note, cut, res);
        for (b = 0; b < 96u; b++) {
            for (i = 0; i < CTL; i++) blk[i] = (int32_t)(1000 * sin(2 * M_PI * f * (b * CTL + i) / 44100.0));
            track_filter(t, blk, CTL, 0, t->p[P_FCUT], t->p[P_FRES]);
            if (b >= 48u)
                for (i = 0; i < CTL; i++) e += (double)blk[i] * blk[i];
        }
        if (e > best) { best = e; bf = f; }
    }
    kt_v[0] = 0;
    return bf;
}
static int near(double a, double b, double semis) { return fabs(12 * log2(a / b)) <= semis; }
int main(void)
{
    int fails = 0, c, n = 0, last = -100000;
    double a, b;
    a = peak(FT_LP, 0, 48, 80, 118); b = peak(FT_LP, 0, 60, 80, 118);
    printf("key_trk: LP OFF     C3 %.0f Hz, C4 %.0f Hz\n", a, b);
    if (!near(a, b, 0.6)) { printf("key_trk: OFF moved the filter FAIL\n"); fails++; }
    a = peak(FT_LP, 2, 48, 80, 118); b = peak(FT_LP, 2, 60, 80, 118);
    printf("key_trk: LP 100 %%   C3 %.0f Hz, C4 %.0f Hz (%.2f semitones)\n", a, b, 12 * log2(b / a));
    if (!near(b, 2 * a, 0.8)) { printf("key_trk: 100 %% is not an octave an octave FAIL\n"); fails++; }
    a = peak(FT_LP, 1, 48, 80, 118); b = peak(FT_LP, 1, 60, 80, 118);
    printf("key_trk: LP 50 %%    C3 %.0f Hz, C4 %.0f Hz (%.2f semitones)\n", a, b, 12 * log2(b / a));
    if (!near(b, 1.4142 * a, 0.8)) { printf("key_trk: 50 %% is not half an octave an octave FAIL\n"); fails++; }
    a = peak(FT_COMB, 2, 60, 64, 110); b = peak(FT_COMB, 2, 72, 64, 110);
    printf("key_trk: COMB 100 %% C4 %.0f Hz, C5 %.0f Hz (C4 is 261.6)\n", a, b);
    if (!near(a, 261.63, 0.5) || !near(b, 523.25, 0.5)) { printf("key_trk: the COMB does not play the note FAIL\n"); fails++; }
    a = peak(FT_COMB, 3, 60, 67, 110); b = peak(FT_COMB, 3, 60, 68, 110);   /* (CUT half a semitone a step: 67 -> 68) */
    printf("key_trk: COMB NOTE  CUT 67 %.0f Hz, CUT 68 %.0f Hz (D4 is 293.7)\n", a, b);
    if (!near(a, b, 0.2) || !near(b, 293.66, 0.5)) { printf("key_trk: NOTE leaves the COMB between semitones FAIL\n"); fails++; }
    for (c = 40 << 8; c < (64 << 8); c += 16) {         /* NOTE: a swept cut lands on the semitones' cuts only */
        int32_t q = kt_cut(3, c, 60 * 16), k = (q - 13905) * 12;
        double st = k / 3589.0;
        if (fabs(st - floor(st + 0.5)) > 0.02) { printf("key_trk: NOTE cut %d off the grid FAIL\n", q); fails++; break; }
        if (q != last) { n++; last = q; }
    }
    printf("key_trk: NOTE, CUT 40 .. 64 swept: %d steps (20 or 21 semitones)\n", n);
    if (n < 20 || n > 22) { printf("key_trk: NOTE's steps are not semitones FAIL\n"); fails++; }
    a = fine(FT_LP, 3, 60, 77, 120);                    /* the resonance on a note of the chromatic scale */
    printf("key_trk: LP NOTE at C4, CUT 77: %.0f Hz (%.2f semitones from A4)\n", a, 12 * log2(a / 440.0));
    if (fabs(12 * log2(a / 440.0) - floor(12 * log2(a / 440.0) + 0.5)) > 0.1) { printf("key_trk: NOTE's resonance out of tune FAIL\n"); fails++; }
    {   /* (V10.8.38) NOTE: the peak of LP HP BP within a tenth of a semitone of the grid (RES 120 and 127, from 1 kHz up:
         * the LP's sat a quarter of a semitone under it at RES 120, the HP's over; below, tsvf_coef's g in whole Q12
         * units leaves up to a fifth of a semitone at 150 Hz, printed) */
        static const uint8_t RS[2] = {120, 127}, NT[2] = {60, 72}, CT[2] = {75, 90};
        static const char *const TN[3] = {"LP", "HP", "BP"};
        int ty, r, q, w;
        double worst = 0, low = 0, d;
        for (ty = FT_LP; ty <= FT_BP; ty++) {
            for (r = 0; r < 2; r++)
                for (q = 0; q < 2; q++)
                    for (w = 0; w < 2; w++) {
                        a = 12 * log2(fine(ty, 3, NT[q], CT[w], RS[r]) / 440.0);
                        d = fabs(a - floor(a + 0.5));
                        if (d > worst) worst = d;
                        if (d > 0.1) { printf("key_trk: %s NOTE RES %d note %d CUT %d: %.2f off the grid FAIL\n", TN[ty], RS[r], NT[q], CT[w], d); fails++; }
                    }
            a = 12 * log2(fine(ty, 3, 48, 60, 120) / 440.0);
            d = fabs(a - floor(a + 0.5));
            if (d > low) low = d;
        }
        printf("key_trk: NOTE's peak, LP HP BP: at most %.2f semitones off the grid (from 1 kHz up), %.2f at C3 CUT 60\n", worst, low);
    }
    printf(fails ? "key_trk: %d FAILED\n" : "key_trk: all ok\n", fails);
    return fails != 0;
}
