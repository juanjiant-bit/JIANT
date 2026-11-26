/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.51, JIANT Studio's analysis: an EXPLORE piece's DRUM stem at +1.5 dBFS, the mix at +3.1, thousands of samples
 * clipped) the generator's pieces measured where the emulator's stems are (fx.c stem_buf: each part mono, before the
 * master; the mix: the parts and the FX return summed, before the master): every genre and EXPLORE, MOOD -50 / 0 / +50,
 * SEEDS seeds each, SECS seconds. Its peak a part and the mix (dBFS) and the samples past full scale; c: the check, no
 * stem or mix over -1 dBFS.
 *   cc -O2 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_peak tests/gen_peak.c -lm
 *   build/host/gen_peak [SEEDS [SECS [c]]] */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#define PK_LIM (-1.0)
static int32_t gpk[NPART + 1];
static uint32_t clipn[NPART + 1];
static void take(int32_t v, uint32_t k)
{
    int32_t a = v < 0 ? -v : v;
    if (a > gpk[k]) gpk[k] = a;
    if (a > 32767) clipn[k]++;
}
static void render(double secs)
{
    uint32_t nsamp = 0, want = (uint32_t)(secs * FS), f = 0, i, k;
    memset(gpk, 0, sizeof gpk); memset(clipn, 0, sizeof clipn);
    fx_stems = 1;
    transport_req = 1;
    while (nsamp < want) {
        uint32_t upto = (uint32_t)((uint64_t)(++f) * FS / 30u);
        ui_input(); song_poll(); fx_stems = 1;   /* (the USB setting rewrites it each pass) */
        while (nsamp < upto && nsamp < want) {
            int32_t o[2 * CTL];
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) {
                int32_t l = stem_fx[0][i], r = stem_fx[1][i];
                for (k = 0; k < NPART; k++) { take(stem_buf[k][i], k); l += stem_buf[k][i]; r += stem_buf[k][i]; }
                take(l, NPART); take(r, NPART);
            }
            nsamp += CTL;
        }
        host_ticks += 1000000u / 30u; fm1_ms += 1000u / 30u;
    }
    transport_req = 2; ui_input();
    for (i = 0; i < 40u; i++) { int32_t o[2 * CTL]; mix_block(o, CTL); }
    fx_stems = 0;
}
static double db(int32_t p) { return 20.0 * log10((p ? p : 1) / 32768.0); }
int main(int argc, char **argv)
{
    static const int8_t MOOD[3] = {-50, 0, 50};
    uint32_t seeds = argc > 1 ? (uint32_t)atoi(argv[1]) : 2, g, s, k, m, over = 0, n = 0;
    double secs = argc > 2 ? atof(argv[2]) : 8.0, worst = -99, wstem = -99;
    int chk = argc > 3;
    ui_power_on(); go_home(); frames(4);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    song.master_q12 = 4096;
    for (g = getenv("GEN_PEAK_G") ? (uint32_t)atoi(getenv("GEN_PEAK_G")) : 1u; g <= (getenv("GEN_PEAK_G") ? (uint32_t)atoi(getenv("GEN_PEAK_G")) : GEN_EXPLORE); g++)
        for (m = 0; m < 3u; m++)
            for (s = 1; s <= seeds; s++) {
                double pm, ps = -99;
                uint32_t ks = 0, cl = 0;
                gp.genre = (uint8_t)g; gp.seed = (uint16_t)(s * 37u + m); gp.energy = 50; gp.form = 0;
                for (k = 0; k < GP_N; k++) gp.amt[k] = 60;
                gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
                grd.mood = MOOD[m];
                gen_go();
                render(secs);
                n++;
                for (k = 0; k < NPART; k++) {
                    if (db(gpk[k]) > ps) ps = db(gpk[k]), ks = k;
                    cl += clipn[k];
                }
                pm = db(gpk[NPART]);
                if (pm > worst) worst = pm;
                if (ps > wstem) wstem = ps;
                if (getenv("GEN_PEAK_ALL")) {
                    printf("%-15s %+3d %2u:", g <= NGENRE ? GENRE[g - 1].name : "EXPLORE", MOOD[m], s);
                    for (k = 0; k < NPART; k++) printf(" %c%+5.1f", "DBPLAT"[gs.role[k] % 6u], db(gpk[k]));
                    printf(" | mix %+5.1f", pm);
                    printf("  loudest T%u %s lvl %d dist %d pre %u\n", ks + 1, ENGINES[trk[ks].engine]->name, trk[ks].p[P_LEVEL], trk[ks].p[P_DIST], trk[ks].preset);
                }
                if (ps > PK_LIM || pm > PK_LIM) {
                    over++;
                    printf("%-15s mood %+3d seed %2u: stem T%u %+5.1f dBFS (%u clipped), mix %+5.1f dBFS (%u clipped)%s\n",
                           g <= NGENRE ? GENRE[g - 1].name : "EXPLORE", MOOD[m], s, ks + 1, ps, cl, pm, clipn[NPART],
                           chk ? "  OVER" : "");
                }
            }
    printf("gen-peak: %u pieces, %u over %.0f dBFS; the loudest stem %+.1f dBFS, the loudest mix %+.1f dBFS\n", n, over, PK_LIM,
           wstem, worst);
    if (chk) printf(over ? "gen-peak: %u FAILED\n" : "gen-peak: all ok\n", over);
    return chk && over;
}
