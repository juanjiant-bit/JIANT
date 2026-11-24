// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.27) JIANT BYTE's tests/clay_engines_test.c (d7f32b11), on JIANT FM's engines.
 * DRONE CLAY on every engine (the owner: "it must work with ANY engine"): ANALOG PHASE MATH VOICE WHEEL STRING WEST FM6
 * ADD ANIMAL and DRUM. For each: the drone sounds and keeps sounding (a decaying envelope too), follows DRONE / PITCH,
 * a modulator of each family (TABLE, GESTURE, NOTE, FM, HIT) moves the sound on each kind of DEST (PITCH, CUTOFF, SHAPE,
 * VOLUME, the engine's own knobs E1..E8), a KICK is a hit; a table of it */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int ce_bad;
#define NS (FS * 4u)
static double ya[NS], yb[NS];
/* (FM) zc: an oscillator engine, its OCT and PITCH told by its zero crossings (a formant, noise or resonator voice keeps
 * its own: VOICE ANIMAL WEST ADD STRING, a kit's pads their own tuning); sus: the drone held at the level asked (STRING
 * plucks: its string rings out, a known gap, only shown) */
typedef struct { const char *name; uint32_t eng; int mode, zc, sus; } ce_eng_t;
static const ce_eng_t CE[] = {{"ANALOG", ENGI_ANALOG, -1, 1, 1}, {"PHASE", ENGI_PHASE, -1, 1, 1}, {"MATH", ENGI_LOFI, -1, 1, 1},
                              {"VOICE", ENGI_VOICE, -1, 0, 1}, {"WHEEL", ENGI_WHEEL, -1, 1, 1}, {"STRING", ENGI_STRING, -1, 0, 0},
                              {"WEST", ENGI_WEST, -1, 0, 1}, {"FM6", ENGI_TRIO, -1, 1, 1}, {"ADD", ENGI_ADD, -1, 0, 1},
                              {"ANIMAL", ENGI_ANIMAL, -1, 0, 1}, {"DRUM", ENGI_DRUM, -1, 0, 1}};
static void ce_setup(const ce_eng_t *e, int pluck)
{
    ui_power_on(); song.sel = 0; set_engine(e->eng); frame();
    if (e->mode >= 0) trk[0].p[P_E1] = (int16_t)e->mode;
    for (unsigned k = 1; k < NTRK; k++) trk[k].p[P_MUTE] = 1;
    if (pluck) { trk[0].p[P_SUS] = 0; trk[0].p[P_DEC] = 30; }
    clay_defaults(clay[0]); song.master_q12 = 4096; song.g[G_BPM] = 120; frames(8);
    for (unsigned m = 0; m < 4u; m++) { clay[0][CL_MF(m, 3u)] = 0; clay[0][CL_MF(m, 2u)] = 0; }
    rng_state = 0x1234567u; clay_rng = 1u; dx_mot_rng = 0x9E3779B9u; mod_seed = 0x2545F491u;               /* (the same render twice the same) */
}
static void ce_play(double *y)
{
    int32_t b[CTL * 2]; unsigned n = 0;
    transport_req = 1;
    unsigned k = 0;                                     /* (the main loop's frame each 16 ms: MATH USER's text built) */
    while (n < NS) { mix_block(b, CTL); for (unsigned i = 0; i < CTL && n < NS; i++) y[n++] = (b[2 * i] + b[2 * i + 1]) / 65536.0; if (++k % (FS / 60u / CTL) == 0u) frame(); }
    transport_req = 2; for (unsigned j = 0; j < FS / 4u / CTL; j++) mix_block(b, CTL);
}
static double rms_r(const double *y, unsigned a, unsigned b) { double e = 0; for (unsigned i = a; i < b; i++) e += y[i] * y[i]; return sqrt(e / (b - a)); }
/* how far two renders sound apart: their level and brightness (zero crossings) overall and in each 20 ms of the beat
 * (the beats folded: a kit's or a formula's own chance averaged out), not the waveform (a formula's phase is free) */
static void feat(const double *y, double *f)
{
    unsigned w = FS / 50u, nb = FS / 2u / w, i, k;
    for (k = 0; k < 2u * nb + 2u; k++) f[k] = 0;
    for (i = 0; i + w <= NS; i += w) {
        unsigned z = 0, b = (i / w) % nb; double r = rms_r(y, i, i + w);
        for (unsigned j = i + 1; j < i + w; j++) z += (y[j] >= 0) != (y[j - 1] >= 0);
        f[b] += r; f[nb + b] += z; f[2u * nb] += r; f[2u * nb + 1u] += z;
    }
}
static double dlt(const double *a, const double *b)
{
    double fa[64], fb[64], d = 0, s = 0, dz = 0, sz = 0; unsigned nb = FS / 2u / (FS / 50u);
    feat(a, fa); feat(b, fb);
    for (unsigned k = 0; k < nb; k++) { d += fabs(fa[k] - fb[k]); s += fa[k] + fb[k]; dz += fabs(fa[nb + k] - fb[nb + k]); sz += fa[nb + k] + fb[nb + k]; }
    return 2 * d / (s + 1e-12) + 2 * dz / (sz + 1);
}
static double zc(const double *y) { unsigned z = 0; for (unsigned i = NS / 4; i < NS; i++) z += (y[i] >= 0) != (y[i - 1] >= 0); return z; }
/* a modulator: MOD 1 SHAPE (the knob's rank), DEST, on 1/1 at 1/8, AMOUNT 127 */
static void ce_mod(int rank, int dest) { clay[0][CL_MF(0, 0u)] = (int8_t)dest; clay[0][CL_MF(0, 1u)] = (int8_t)rank; clay[0][CL_MF(0, 2u)] = 2; clay[0][CL_MF(0, 3u)] = 127; clay[0][CL_MF(0, 4u)] = 40; clay[0][CL_MF(0, 5u)] = 3; }
int main(int argc, char **argv)
{
    (void)argv; if (argc > 1) return 0;
    static const int RANK[5] = {12, 24, 36, 51, 0};      /* SINE, RAMP UP, ARP UP, FM x2, KICK */
    static const char *const FAM[5] = {"TABLE", "GESTURE", "NOTE", "FM", "HIT"};
    static const int DEST[12] = {1, 2, 3, 4, 12, 13, 14, 15, 16, 17, 18, 19};   /* PITCH CUTOFF SHAPE VOLUME E1 .. E8 */
    static const char *const DN[12] = {"PITCH", "CUT", "SHAPE", "VOL", "E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8"};
    printf("clay_engines: %-10s %6s %6s %5s %5s %5s", "ENGINE", "DRONE", "PLUCK", "OCT", "PITCH", "NOISE");
    for (unsigned d = 0; d < 12u; d++) printf(" %5s", DN[d]);
    for (unsigned f = 0; f < 5u; f++) printf(" %6s", FAM[f]);
    printf(" %5s\n", "KICK");
    for (unsigned e = 0; e < sizeof CE / sizeof CE[0]; e++) {
        const ce_eng_t *E = &CE[e]; int drum = E->eng == ENGI_DRUM, func = 0, ok = 1;
        double dr, pl, oc = 0, pt = 0, dd[12], ff[5], kick = 0, fl;
        ce_setup(E, 0); ce_play(ya);                     /* (once to warm it: a formula's first run) */
        ce_setup(E, 0); ce_play(ya); dr = rms_r(ya, NS / 2, NS);
        ce_setup(E, 0); ce_play(yb); fl = dlt(ya, yb);       /* (the same twice: how far a render is from itself) */
        ce_setup(E, 1); ce_play(yb); pl = rms_r(yb, NS / 2, NS);
        {
            double z0 = zc(ya);
            ce_setup(E, 0); clay[0][CL_OCT] = 2; ce_play(yb); oc = zc(yb) / (z0 + 1);
            ce_setup(E, 0); clay[0][CL_PIT] = 12; ce_play(yb); pt = zc(yb) / (z0 + 1);
        }
        for (unsigned d = 0; d < 12u; d++) {               /* a SINE on each DEST against the drone alone */
            ce_setup(E, 0); ce_mod(12, DEST[d]); ce_play(yb); dd[d] = dlt(ya, yb);
        }
        for (unsigned f = 0; f < 5u; f++) {               /* each family on the CUTOFF */
            ce_setup(E, 0); ce_mod(RANK[f], 2); ce_play(yb); ff[f] = dlt(ya, yb);
        }
        {   /* a KICK on the beat: louder on it than between (DRUM-W: its pad) */
            ce_setup(E, 0); ce_mod(0, 0); clay[0][CL_MF(0, 2u)] = 3; clay[0][CL_MF(0, 5u)] = 2; ce_play(yb);
            double on = 0, off = 0; unsigned beat = FS / 2u;
            for (unsigned i = beat; i + beat <= NS; i += beat) { on += rms_r(yb, i, i + FS / 40u); off += rms_r(yb, i + beat - FS / 20u, i + beat - FS / 40u); }
            kick = on / (off + 1e-6);
        }
        if (E->sus) ok &= dr > 0.005 && (drum || pl > 0.005);   /* (FM: WEST quiet at its preset, ~0.03) */
        if (E->zc) ok &= oc > 1.5 && pt > 1.5;
        {   /* CUTOFF VOLUME always, PITCH but on a kit (its pads' own) and FUNC (its recipe's time, not the voice's
             * phase: DRONE and PITCH reach it as its note); SHAPE shown (WAVE: its POS, MATH: VAR, USER
             * and a kit none); 3 of the 8 knobs at least (a knob may be a choice that sounds alike in this sound: a
             * table's POS, WARP's AMT with WARP off, B with MIX 0) */
            unsigned kn = 0; double th = 0.08 + fl;      /* (FM: 1x the render's own spread, WHEEL's noise) */
            for (unsigned d = 4; d < 12u; d++) kn += dd[d] > th;
            ok &= (drum || func || !E->zc || dd[0] > th) && dd[1] > th && dd[3] > th && kn >= 3u;
            for (unsigned f = 0; f < 5u; f++) ok &= ff[f] > th;
        }
        ok &= kick > (drum ? 1.3 : 3.0);
        printf("clay_engines: %-10s %6.3f %6.3f %5.2f %5.2f %5.2f", E->name, dr, pl, oc, pt, fl);
        for (unsigned d = 0; d < 12u; d++) printf(" %5.2f", dd[d]);
        for (unsigned f = 0; f < 5u; f++) printf(" %6.2f", ff[f]);
        printf(" %5.1f    %s\n", kick, ok ? "ok" : "FAIL");
        ce_bad += !ok;
    }
    printf("clay_engines: %s\n", ce_bad ? "FAIL" : "all ok");
    return ce_bad != 0;
}
