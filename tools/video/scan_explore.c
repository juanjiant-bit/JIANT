/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.7.81) the film's pieces picked by ear's proxies: GEN thrown with EXPLORE (or a genre) over many seeds, each
 * played 12 s by the real engines, measured: its loudness, how many tracks sound, how harsh it is (the energy of the
 * sample-to-sample difference over the signal's), the silent share. One line a seed, sorted by the caller.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/scan tools/video/scan_explore.c -lm
 *   build/scan EXPLORE 1 60      (genre, first seed, how many) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t genre_of(const char *name)
{
    uint32_t i;
    if (str_eq(name, "EXPLORE")) return GEN_EXPLORE;
    for (i = 0; i < NGENRE; i++) if (str_eq(GENRE[i].name, name)) return i + 1u;
    return GEN_EXPLORE;
}
int main(int argc, char **argv)
{
    uint32_t g = genre_of(argc > 1 ? argv[1] : "EXPLORE"), s0 = argc > 2 ? (uint32_t)atoi(argv[2]) : 1u,
             n = argc > 3 ? (uint32_t)atoi(argv[3]) : 40u, s, k;
    for (s = s0; s < s0 + n; s++) {
        double e = 0, d = 0, prev = 0;
        uint32_t nb = (uint32_t)(FS * 12u / CTL), b, i, quiet = 0, act = 0, peak[NTRK] = {0};
        ui_power_on(); go_home(); frames(4);
        gp.genre = (uint8_t)g; gp.seed = (uint16_t)s; gp.energy = 70; gp.form = 0;
        gen_go();
        song.master_q12 = 4096;
        transport_req = 1;
        for (b = 0; b < nb; b++) {
            int32_t o[2 * CTL];
            double be = 0;
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                double x = (o[2u * i] + o[2u * i + 1u]) * 0.5 / 32768.0;
                be += x * x; d += (x - prev) * (x - prev); prev = x;
            }
            e += be;
            if (be / CTL < 1e-5) quiet++;
            for (k = 0; k < NTRK; k++) { if (trk[k].peak > peak[k]) peak[k] = trk[k].peak; trk[k].peak = 0; }
            if ((b & 31u) == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
        }
        for (k = 0; k < NTRK; k++) act += peak[k] > 600u;
        {
            uint32_t mb = gs.trk_of[RL_BASS] < NTRK && trk_eng(&trk[gs.trk_of[RL_BASS]]) == ENGI_LOFI, nf = 0;
            for (k = 0; k < 8u; k++) nf += dx_fml[k] != 0;
            printf("%u\t%s\t%d\t%.1f\t%.3f\t%u\t%.2f\t%u\t%u\n", s, gp.last, song.g[G_BPM],
                   10.0 * log10(e / (nb * CTL) + 1e-12), d / (e + 1e-9), act, (double)quiet / nb, mb, nf);
        }
        fflush(stdout);
    }
    return 0;
}
