/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.72) DIST TYPE 6 .. 11, JIANT BYTE's formula effects (RING STEPS XOR WOB SFOLD SAT; its DIST_EQ, run there by the formula machine) as FM's
 * fixed code (fx.c dist_shape, deq_k): each against the formula in floating point (BYTE's numbers: I the part, A DIST, B TONE,
 * p the beats (fx.c deq_beat); its TONE stage and DIST's blend after), within 1.5 % of the signal; every output bounded; SOFT .. AMP's
 * numbers as they were (a stored 0 .. 5 the same type); the cost of the heaviest one on six tracks.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src tests/dist_eq_test.c -lm */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
#include <time.h>
static const char *const NM[6] = {"RING", "STEPS", "XOR", "WOB", "SFOLD", "SAT"};
static double tri(double x) { double f = x - floor(x); return f < 0.5 ? 4 * f - 1 : 3 - 4 * f; }
static double formula(int f, double I, double A, double p, double *ph)
{
    double N;
    switch (f) {
    case 0: *ph += (110 + floor(880 * A)) / 44100.0; return I * sin(2 * M_PI * *ph);
    case 1: N = 2 + (1 - A) * 30; return floor(I * N) / N;
    case 2: return (double)(((int)floor(I * 128)) ^ (int)floor(A * 127)) / 128;
    case 3: return I * (1 - A * (tri(p * 2) / 2 + .5));
    case 4: return sin(2 * M_PI * I * (1 + A * 12) / 4);
    default: return tanh(I * (1 + A * 20) > 2 ? 2 : I * (1 + A * 20) < -2 ? -2 : I * (1 + A * 20)) * (1 - A / 2);
    }
}
int main(void)
{
    track_t *t = &trk[0];
    int bad = 0, f, ok;
    uint32_t b, i;
    static int32_t o[CTL];
    host_tracks_init();
    for (f = 0; f < 6; f++) {
        double ph = 0, l1 = 0, l2 = 0, e = 0, er = 0, A;
        int32_t mx = 0;
        t->p[P_DIST] = 90; t->p[P_DTYPE] = (int16_t)(DT_EQ0 + f); t->p[P_DTONE] = 20;
        A = t->p[P_DIST] * 516 / 65536.0;
        dnz_v[0] = 0; dist_w[0] = 4096; t->dist_dc = 0; deq_beat = 0; t->dist_lp1 = t->dist_lp2 = t->dist_hp = 0;
        song.playing = 0;
        for (b = 0; b < 600u; b++) {
            double p, k = clamp(32000 - t->p[P_DIST] * 95 + t->p[P_DTONE] * 300, 2500, 32767) / 32768.0;
            for (i = 0; i < CTL; i++)
                o[i] = (int32_t)(50000 * sin(2 * M_PI * 180.0 * (b * CTL + i) / 44100.0) + 20000 * sin(2 * M_PI * 1250.0 * (b * CTL + i) / 44100.0));
            {
                int32_t in[CTL];
                memcpy(in, o, sizeof in);
                deq_beat += (CTL << 16) / beat_samples();   /* (the block's beats, as DRONE CLAY's clock gives them) */
                track_dist(t, o, CTL);
                p = deq_beat / 65536.0;
                for (i = 0; i < CTL; i++) {
                    double I = in[i] * 2.0 / 65536.0, y;
                    I = I > 2 ? 2 : I < -2 ? -2 : I;
                    y = formula(f, I, A, p, &ph) * 32768.0;
                    y = y > 98304 ? 98304 : y < -98304 ? -98304 : y;
                    l1 += (y - l1) * k; l2 += (l1 - l2) * k;
                    if (b >= 100u) { e += l2 * l2; er += (o[i] - l2) * (o[i] - l2); }
                    if (abs(o[i]) > mx) mx = abs(o[i]);
                }
            }
        }
        ok = er <= e * 0.015 * 0.015 && mx <= 98304 && e > 0;
        printf("dist-eq: %-6s against the formula %6.3f %% of the signal, peak %6d  %s\n", NM[f], 100 * sqrt(er / (e + 1)), mx, ok ? "ok" : "FAIL");
        bad += !ok;
    }
    {   /* SOFT .. AMP: the stored numbers mean what they did (the names' order), the formulas after them */
        ok = DT_SOFT == 0 && DT_HARD == 1 && DT_FOLD == 2 && DT_CRUSH == 3 && DT_RECT == 4 && DT_AMP == 5 && DT_EQ0 == 6 && DT_N == 12 &&
             !strcmp(PN_AT(N_DTYPE, 3), "CRUSH") && !strcmp(PN_AT(N_DTYPE, 5), "AMP") && !strcmp(PN_AT(N_DTYPE, 6), "RING") &&
             !strcmp(PN_AT(N_DTYPE, 10), "SFOLD") && !strcmp(PN_AT(N_DTYPE, 11), "SAT") && TP[P_DTYPE].max == 11;
        printf("dist-eq: TYPE's list SOFT .. AMP at 0 .. 5 as stored, RING STEPS XOR WOB SFOLD SAT 6 .. 11  %s\n", ok ? "ok" : "FAIL");
        bad += !ok;
    }
    {   /* the cost: six tracks on each, a second of blocks (host time, against SOFT's) */
        double ts = 0, tw = 0;
        int fw = 0;
        for (f = -1; f < 6; f++) {
            clock_t c0;
            double sec;
            uint32_t k;
            for (k = 0; k < NTRK; k++) { trk[k].p[P_DIST] = 100; trk[k].p[P_DTYPE] = (int16_t)(f < 0 ? DT_SOFT : DT_EQ0 + f); dist_w[k] = 4096; dnz_v[k] = 50; }
            c0 = clock();
            for (b = 0; b < 20u * 44100u / CTL; b++)
                for (k = 0; k < NTRK; k++) {
                    for (i = 0; i < CTL; i++) o[i] = (int32_t)((b * 7919u + i * 104729u) & 0xFFFFu) - 32768;
                    track_dist(&trk[k], o, CTL);
                }
            sec = (double)(clock() - c0) / CLOCKS_PER_SEC / 20.0;
            if (f < 0) ts = sec;
            else if (sec > tw) tw = sec, fw = f;
        }
        printf("dist-eq: six tracks a second of audio, host: SOFT %.2f ms, the heaviest formula (%s) %.2f ms (x%.2f)\n", ts * 1000, NM[fw], tw * 1000, tw / ts);
    }
    printf(bad ? "dist-eq: %d FAILED\n" : "dist-eq: all ok\n", bad);
    return bad != 0;
}
