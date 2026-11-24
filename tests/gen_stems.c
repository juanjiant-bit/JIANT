/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The generator's mix measured (docs/GENERATOR-STUDY.es.md): a genre's pieces with every part on, each role alone
 * (the others muted), 6 s each: its RMS against the drums' (dB) and its brightness (zero crossings a second / 2: a
 * centroid's stand-in).
 *   cc -O2 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_stems tests/gen_stems.c -lm
 *   build/host/gen_stems [SEEDS [GENRE]] */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static const char *RLN[6] = {"DRUM", "BASS", "PAD", "LEAD", "ARP", "TEX"};
static void render(double secs, double *rms, double *zc)
{
    uint32_t nsamp = 0, want = (uint32_t)(secs * FS), f = 0, i; double e = 0; uint32_t z = 0; int32_t pv = 0;
    transport_req = 1;
    while (nsamp < want) {
        uint32_t upto = (uint32_t)((uint64_t)(++f) * FS / 30u);
        ui_input(); song_poll();
        while (nsamp < upto && nsamp < want) {
            int32_t o[2 * CTL];
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) { int32_t m = (o[2 * i] + o[2 * i + 1]) / 2; e += (double)m * m; z += (m >= 0) != (pv >= 0); pv = m; }
            nsamp += CTL;
        }
        host_ticks += 1000000u / 30u; fm1_ms += 1000u / 30u;
    }
    transport_req = 2; ui_input();
    for (i = 0; i < 40u; i++) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    *rms = 10.0 * log10(e / nsamp / (32768.0 * 32768.0) + 1e-12); *zc = z * FS / (2.0 * nsamp);
}
int main(int argc, char **argv)
{
    uint32_t seeds = argc > 1 ? (uint32_t)atoi(argv[1]) : 3, only = argc > 2 ? (uint32_t)atoi(argv[2]) : 0, g, s, k, r;
    double tot[6] = {0}; uint32_t tn[6] = {0};
    ui_power_on(); go_home(); frames(4);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    song.master_q12 = 4096;
    printf("%-15s %6s | rel dB to the drums (brightness Hz):  BASS        PAD         LEAD        ARP         TEX     | mix dB\n", "genre", "DRUM");
    for (g = only ? only : 1; g <= (only ? only : NGENRE); g++) {
        double sum[6] = {0}, zs[6] = {0}, mixs = 0; uint32_t n[6] = {0};
        for (s = 1; s <= seeds; s++) {
            int16_t mu[NTRK]; double rm, zc, d0 = 0;
            gp.genre = (uint8_t)g; gp.seed = (uint16_t)(s * 37u); gp.energy = 50; gp.form = 0;
            for (k = 0; k < GP_N; k++) gp.amt[k] = 60;
            gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            for (k = 0; k < NTRK; k++) mu[k] = trk[k].p[P_MUTE];
            render(6.0, &rm, &zc); mixs += rm;
            for (r = 0; r < 6u; r++) {
                uint32_t any = 0;
                for (k = 0; k < NTRK; k++) { int on = gs.role[k] == r && gs.act[r] && !(r == RL_TEX && k != gs.trk_of[RL_TEX]); trk[k].p[P_MUTE] = (int16_t)!on; any |= on; }
                if (!any) continue;
                render(6.0, &rm, &zc);
                if (r == 0) d0 = rm;
                sum[r] += r ? rm - d0 : rm; zs[r] += zc; n[r]++;
            }
            for (k = 0; k < NTRK; k++) trk[k].p[P_MUTE] = mu[k];
        }
        printf("%-15s %6.1f |", GENRE[g - 1].name, n[0] ? sum[0] / n[0] : 0);
        for (r = 1; r < 6u; r++) { if (n[r]) { printf(" %5.1f (%4.0f)", sum[r] / n[r], zs[r] / n[r]); tot[r] += sum[r] / n[r]; tn[r]++; } else printf("     -      "); }
        printf(" | %5.1f\n", mixs / seeds);
    }
    printf("%-15s        |", "MEAN"); for (r = 1; r < 6u; r++) printf(" %5.1f       ", tn[r] ? tot[r] / tn[r] : 0); printf("\n");
    return 0;
}
