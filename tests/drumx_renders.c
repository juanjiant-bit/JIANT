/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* DRUM-X renders for listening (not a test: run_tests.sh does not run it), through hostsim.c as drumx_test:
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/drumx_renders tests/drumx_renders.c -lm
 *   build/host/drumx_renders DIR [ero]
 * DIR/sound_<KIT>_<LANE>.wav  each sound of five factory kits struck alone at velocity 100 (1.2 s)
 * DIR/kit_<KIT>.wav            two bars of a beat at 120 BPM on the kit (16ths; the open hat cut by a closed one)
 * DIR/vel_<LANE>.wav           the kick, the snare, the tom and the closed hat at velocities 30 60 90 127 (0.5 s each)
 * DIR/noise_<LANE>.wav         the kick's and the tom's NOISE (the kick's CLICK) at 0 40 70 100 127 (0.6 s each)
 * DIR/decay_<LANE>.wav         the kick, the snare and the tom at the kit's DECAY 16 40 64 88 112 (0.8 s each)
 * ero: DIR/erosion_<MODE>.wav  (V10.7.37) the beat on DRUM-X with EROSION NOISE 90 in each MODE (DEST MIX), the
 *                              filter LP at 90, and NOISE 0 (erosion_OFF.wav) */
#define HOST_BAL 0
#define main hostsim_main
#include "hostsim.c"
#undef main

#define MAXN (FS * 8u)
static int32_t bl[MAXN], br[MAXN];
static const uint8_t NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};
static const char *const LN[8] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL"};
static const uint8_t KITS[5] = {0, 1, 2, 5, 7};          /* DRUM-X 909 CLUB 808 BOOM ACOUSTIC TECHNO RAW */
static const char *const KN[5] = {"DRUMX", "909", "808", "ACOUSTIC", "TECHNO"};

static track_t *kit(uint32_t k)
{
    track_t *t = &trk[0];
    uint32_t i;
    for (i = 0; i < NTRK * NVOICE; i++)
        trk[i / NVOICE].v[i % NVOICE].active = 0;
    host_tracks_init();
    lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; lim_env = LIM_T;
    memset(mp_seen, 0, sizeof mp_seen); memset(dist_w, 0, sizeof dist_w);
    for (i = 0; i < DV_NLANE; i++)
        drum_kit[0][i].x.live = drum_kit[0][i].x.trig = 0, drum_kit[0][i].x.rng = 0;
    host_preset(t, ENGI_DRUM, k);
    dx_kit_load(k);
    t->p[P_DIST] = 0;                                    /* (dry: the sound itself) */
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;
    return t;
}
static void run(uint32_t a, uint32_t b)
{
    uint32_t k, i;
    for (k = a; k < b; k += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL && k + i < b; i++) {
            bl[k + i] = o[2 * i];
            br[k + i] = o[2 * i + 1];
        }
    }
}
static void hit(track_t *t, uint32_t note, uint32_t vel)
{
    trk_note_on(t, note, vel);
    trk_note_off(t, note);
}
static void save(const char *dir, const char *name, uint32_t n)
{
    char path[512];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, n);
    for (i = 0; i < n; i++)
        wav_put(f, bl[i], br[i]);
    fclose(f);
}
/* two bars of 16ths at 120 BPM (5512 samples a step) */
static void beat(track_t *t)
{
    static const char *const P[8] = {"x...x.....x.x...", "....x.......x...", "............x..x", "x.x.x.x.x.x.x...",
                                     "..............x.", ".........x....x.", "...x......x.....", "......x........."};
    uint32_t s, l, at = 0, st = FS * 60u / 120u / 4u;
    for (s = 0; s < 32u; s++, at += st) {
        for (l = 0; l < 8u; l++)
            if (P[l][s & 15u] == 'x' && !(l == 3u && s == 30u))
                hit(t, NOTE[l], l == 0u || ((s & 3u) == 0u) ? 110 : 84);
        if (s == 31u) hit(t, NOTE[3], 90);              /* (the open hat at 14 cut by a closed one at 15, bar 2) */
        run(at, at + st);
    }
    run(at, at + FS);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
    uint32_t k, l, j;
    char nm[96];
    track_t *t;
    if (argc > 2 && !strcmp(argv[2], "ero")) {
        static const char *const MN[6] = {"OFF", "SOFT", "STEP", "METAL", "BREAK", "DUST"};
        for (j = 0; j < 6u; j++) {
            t = kit(0);
            t->p[P_FTYPE] = 0;
            t->p[P_FCUT] = 90;
            t->p[P_FRES] = 40;
#ifdef P_DNZ
            t->p[P_DNZ] = (int16_t)(j ? 90 : 0);
            t->p[P_DNMD] = (int16_t)(j ? j - 1u : 0u);
            t->p[P_DNRT] = 70;
            t->p[P_DDST] = 2;
#endif
            beat(t);
            snprintf(nm, sizeof nm, "erosion_%s.wav", MN[j]);
            save(dir, nm, FS * 5u);
        }
        return 0;
    }
    for (k = 0; k < 5u; k++)
        for (l = 0; l < 8u; l++) {
            t = kit(KITS[k]);
            hit(t, NOTE[l], 100);
            run(0, FS * 6u / 5u);
            snprintf(nm, sizeof nm, "sound_%s_%s.wav", KN[k], LN[l]);
            save(dir, nm, FS * 6u / 5u);
        }
    for (k = 0; k < 5u; k++) {
        t = kit(KITS[k]);
        beat(t);
        snprintf(nm, sizeof nm, "kit_%s.wav", KN[k]);
        save(dir, nm, FS * 5u);
    }
    {
        static const uint8_t VL[4] = {0, 1, 5, 3}, VV[4] = {30, 60, 90, 127};
        for (l = 0; l < 4u; l++) {
            t = kit(0);
            for (j = 0; j < 4u; j++) {
                hit(t, NOTE[VL[l]], VV[j]);
                run(j * FS / 2u, (j + 1u) * FS / 2u);
            }
            snprintf(nm, sizeof nm, "vel_%s.wav", LN[VL[l]]);
            save(dir, nm, FS * 2u);
        }
    }
    {   /* the kick's and the tom's NOISE (V10.7.37: the kick's CLICK) at 0 40 70 100 127, the DRUM-X kit, 0.6 s each */
        static const uint8_t CL[2] = {0, 5}, CV[5] = {0, 40, 70, 100, 127};
        for (l = 0; l < 2u; l++) {
            t = kit(0);
            for (j = 0; j < 5u; j++) {
                dx_kit[CL[l]].a[DXP_NOISE] = dx_kit[CL[l]].b[DXP_NOISE] = CV[j];
                hit(t, NOTE[CL[l]], 100);
                run(j * FS * 3u / 5u, (j + 1u) * FS * 3u / 5u);
            }
            snprintf(nm, sizeof nm, "noise_%s.wav", LN[CL[l]]);
            save(dir, nm, FS * 3u);
        }
    }
    {
        static const uint8_t DL[3] = {0, 1, 5}, DV[5] = {16, 40, 64, 88, 112};
        for (l = 0; l < 3u; l++) {
            t = kit(0);
            for (j = 0; j < 5u; j++) {
                t->p[P_E3] = DV[j];
                hit(t, NOTE[DL[l]], 100);
                run(j * FS * 4u / 5u, (j + 1u) * FS * 4u / 5u);
            }
            snprintf(nm, sizeof nm, "decay_%s.wav", LN[DL[l]]);
            save(dir, nm, FS * 4u);
        }
    }
    return 0;
}
