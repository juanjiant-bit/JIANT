/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.36) DIST's NOISE (fx.c track_dist, dnz_v): it modulates the drive (sidebands), adds nothing to silence and
 * keeps the level; at 0 the drive is bit for bit what it was. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
static int32_t buf[2][4096];
static double run(int nz, int type, int amp, double *side)
{
    track_t *t = &trk[0];
    uint32_t b, i, n = 4096 / CTL;
    double e = 0, re = 0, im = 0, sre = 0, sim = 0;
    host_tracks_init();
    t->p[P_DIST] = 60; t->p[P_DTYPE] = (int16_t)type; t->p[P_DTONE] = 0;
    dnz_v[0] = (int16_t)nz;
    dist_w[0] = 0; dnz_lp[0] = 0; dnz_r[0] = 0;
    for (b = 0; b < n * 4u; b++) {                       /* four passes: the blend in, the filters settled */
        int32_t *o = buf[nz ? 1 : 0] + (b % n) * CTL;
        for (i = 0; i < CTL; i++)
            o[i] = (int32_t)(amp * sin(2 * M_PI * 220.0 * (b * CTL + i) / 44100.0));
        track_dist(t, o, CTL);
    }
    for (i = 0; i < 4096; i++) {
        double x = buf[nz ? 1 : 0][i], w = 2 * M_PI * i / 44100.0, hw = 0.5 - 0.5 * cos(2 * M_PI * i / 4096.0);
        e += x * x;
        x *= hw;                                         /* (a Hann window: the partial's skirt out of the way) */
        re += x * cos(220 * w); im += x * sin(220 * w);
        sre += x * cos(300 * w); sim += x * sin(300 * w);   /* between the partials: only the noise's sidebands */
    }
    (void)re; (void)im;
    *side = 10 * log10((sre * sre + sim * sim + 1e-9) / (e * 4096 + 1e-9));   /* against the whole signal */
    return sqrt(e / 4096);
}
int main(void)
{
    int fails = 0, type;
    for (type = 0; type < 5; type++) {
        double s0, s1, sz, l0 = run(0, type, 12000, &s0), l1 = run(100, type, 12000, &s1), z0 = run(0, type, 0, &sz), z = run(100, type, 0, &sz);
        double d = 20 * log10(l1 / l0);
        printf("type %d: level %.0f -> %.0f (%+.2f dB), between partials %.1f -> %.1f dB, silence %.1f\n", type, l0, l1, d, s0, s1, z);
        if (z > z0) { printf("FAIL NOISE adds to silence (%.2f, without %.2f)\n", z, z0); fails++; }
        if (d < -3.0 || d > 3.0) { printf("FAIL level moved\n"); fails++; }
        if (s1 < s0 + 6) { printf("FAIL no sidebands\n"); fails++; }
    }
    {   /* NOISE 0: as before (the same samples twice) */
        double s; static int32_t keep[4096];
        run(0, 0, 12000, &s); memcpy(keep, buf[0], sizeof keep);
        run(0, 0, 12000, &s);
        if (memcmp(keep, buf[0], sizeof keep)) { printf("FAIL NOISE 0 not deterministic\n"); fails++; }
    }
    printf(fails ? "dist noise test FAILED\n" : "dist noise test passed\n");
    return fails ? 1 : 0;
}
