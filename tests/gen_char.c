/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * How far apart the generator's genres are (docs/GENERATOR-STUDY.es.md): N seeds a genre, every part on, FORM LOOP; a
 * vector of what a listener hears first (tempo, the drums' lanes a bar, rolls, the kick off the beat, the bass's notes
 * and its syncopation, the chords, the lead's and the arp's density and range, components, loops of other lengths,
 * automation). Each feature in standard deviations across the genres; a genre's distance to its nearest neighbour
 * (the larger, the more its own), and its seeds' spread around its mean.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_char tests/gen_char.c -lm
 *   build/host/gen_char [SEEDS] [v]     (v: the features' means too) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
enum { X_BPM, X_KICK, X_SNARE, X_HAT, X_PERC, X_ROLL, X_KOFF, X_BASS, X_BPCS, X_BOFF, X_PAD, X_NCH, X_LEAD, X_LRANGE, X_ARP, X_COMP,
       X_LENS, X_MOT, X_SWING, X_N };
static const char *FN[X_N] = {"bpm", "kick", "snare", "hat", "perc", "roll", "kOff%", "bass", "bPcs", "bOff%", "pad", "nch", "lead", "lRng",
                              "arp", "comp", "lens", "mot", "swing"};
#define MAXG 40
#define MAXS 32
static double fv[MAXG][MAXS][X_N], mean[MAXG][X_N];
static void feat(double *f)
{
    uint32_t k, i, l, lens[NTRK], nl = 0;
    const track_t *t;
    memset(f, 0, sizeof(double) * X_N);
    f[X_BPM] = song.g[G_BPM]; f[X_SWING] = song.g[G_SWING]; f[X_NCH] = gs.nch; f[X_MOT] = motion.count;
    for (k = 0; k < NTRK; k++) {
        uint32_t r = gs.role[k], len, bars, on = 0, lo = 127, hi = 0, off = 0, pcs = 0, comp = 0;
        t = &trk[k]; len = dlen(t); bars = len / 16u ? len / 16u : 1u;
        if (trk[k].p[P_MUTE]) continue;
        for (i = 0; i < len; i++) {
            const step_t *s = &t->step[i];
            if (s->time != ST_NOTE || !(s->n || s->hit)) continue;
            on++;
            comp += comp_any(s) + (step_ratchet(s) > 1u) + (step_chance(s) < 100u);
            if (r == RL_DRUM) {
                f[X_KICK] += (s->hit & 1u) != 0; f[X_SNARE] += (s->hit & 2u) != 0; f[X_HAT] += ((s->hit >> 3) & 1u) + ((s->hit >> 4) & 1u);
                f[X_PERC] += __builtin_popcount(s->hit & 0xE4u);
                if ((s->hit & 1u) && i % 4u) f[X_KOFF] += 1;
                for (l = 0; l < NLANE; l++) f[X_ROLL] += lm_hits(step_lmode(s, l)) > 1u;
                f[X_ROLL] += step_ratchet(s) > 1u;
            } else if (s->n) {
                if (s->note[0] < lo) lo = s->note[0];
                if (s->note[0] > hi) hi = s->note[0];
                pcs |= 1u << (s->note[0] % 12u);
                off += i % 4u != 0;
            }
        }
        if (!on) continue;
        for (i = 0; i < nl && lens[i] != len; i++) {}
        if (i == nl) lens[nl++] = len;
        if (r == RL_DRUM) { double kk = f[X_KICK]; f[X_KICK] /= bars; f[X_SNARE] /= bars; f[X_HAT] /= bars; f[X_PERC] /= bars; f[X_ROLL] /= bars; f[X_KOFF] = kk ? 100.0 * f[X_KOFF] / kk : 0; }
        else {
            f[X_COMP] += (double)comp / bars;
            if (r == RL_BASS) { f[X_BASS] = (double)on / bars; f[X_BPCS] = __builtin_popcount(pcs); f[X_BOFF] = 100.0 * off / on; }
            else if (r == RL_PAD) f[X_PAD] = (double)on / bars;
            else if (r == RL_LEAD) { f[X_LEAD] = (double)on / bars; f[X_LRANGE] = hi >= lo ? hi - lo : 0; }
            else if (r == RL_ARP) f[X_ARP] = (double)on / bars;
        }
    }
    f[X_LENS] = nl;
}
int main(int argc, char **argv)
{
    uint32_t seeds = argc > 1 ? (uint32_t)atoi(argv[1]) : 12, g, s, k, m, ng = NGENRE;
    double sd[X_N], gm[X_N], tot = 0, tin = 0;
    if (seeds > MAXS) seeds = MAXS;
    ui_power_on(); go_home(); frames(4);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    for (g = 0; g < ng; g++)
        for (s = 0; s < seeds; s++) {
            gp.genre = (uint8_t)(g + 1u); gp.seed = (uint16_t)((s + 1u) * 37u); gp.energy = 50; gp.form = 0;
            for (k = 0; k < GP_N; k++) gp.amt[k] = 60;
            gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            feat(fv[g][s]);
            for (m = 0; m < X_N; m++) mean[g][m] += fv[g][s][m] / seeds;
        }
    for (m = 0; m < X_N; m++) {
        gm[m] = 0; sd[m] = 0;
        for (g = 0; g < ng; g++) gm[m] += mean[g][m] / ng;
        for (g = 0; g < ng; g++) sd[m] += (mean[g][m] - gm[m]) * (mean[g][m] - gm[m]) / ng;
        sd[m] = sqrt(sd[m]); if (sd[m] < 1e-9) sd[m] = 1;
    }
    if (getenv("GEN_CHAR_SD")) {                        /* (the scale of a run before: the same ruler for both) */
        FILE *fp = fopen(getenv("GEN_CHAR_SD"), "r");
        if (fp) { for (m = 0; m < X_N; m++) fscanf(fp, "%lf", &sd[m]); fclose(fp); }
        else if ((fp = fopen(getenv("GEN_CHAR_SD"), "w"))) { for (m = 0; m < X_N; m++) fprintf(fp, "%.9g\n", sd[m]); fclose(fp); }
    }
    if (argc > 2) {
        printf("%-15s", "genre"); for (m = 0; m < X_N; m++) printf(" %6s", FN[m]); printf("\n");
        for (g = 0; g < ng; g++) { printf("%-15s", GENRE[g].name); for (m = 0; m < X_N; m++) printf(" %6.1f", mean[g][m]); printf("\n"); }
    }
    printf("%-15s %8s %-15s %8s\n", "genre", "nearest", "(which)", "spread");
    for (g = 0; g < ng; g++) {
        double best = 1e9, in = 0; uint32_t bi = 0, h;
        for (h = 0; h < ng; h++) {
            double d = 0;
            if (h == g) continue;
            for (m = 0; m < X_N; m++) d += (mean[g][m] - mean[h][m]) * (mean[g][m] - mean[h][m]) / (sd[m] * sd[m]);
            d = sqrt(d);
            if (d < best) { best = d; bi = h; }
        }
        for (s = 0; s < seeds; s++) { double d = 0; for (m = 0; m < X_N; m++) d += (fv[g][s][m] - mean[g][m]) * (fv[g][s][m] - mean[g][m]) / (sd[m] * sd[m]); in += sqrt(d) / seeds; }
        printf("%-15s %8.2f %-15s %8.2f\n", GENRE[g].name, best, GENRE[bi].name, in);
        tot += best / ng; tin += in / ng;
    }
    printf("%-15s %8.2f %-15s %8.2f\n", "MEAN", tot, "", tin);
    return 0;
}
