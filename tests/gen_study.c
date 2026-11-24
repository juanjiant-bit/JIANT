/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The generator measured (firmware/src/gen.c), for docs/GENERATOR-STUDY.es.md: N seeds a genre, FORM PARTS, and what
 * a musician would count: how many different progressions, whether the sections differ (their chords, who plays, how
 * dense), the lead's shape (notes a bar, steps and leaps, its range, the motif's rhythm coming back, bars repeated
 * note for note), call and response (the lead resting while a second voice plays, and back), the bass on the kick.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_study tests/gen_study.c -lm
 *   build/host/gen_study [SEEDS [GENRE]]     (a line a genre, then the mean) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
typedef struct { double v[16]; } row_t;
enum { M_PROGS, M_SECDIFF, M_SECWHO, M_NPB, M_STEP, M_LEAP, M_RANGE, M_RHY, M_SAME, M_CALL, M_BK, M_LEADON, M_DENS, M_SECDENS, M_IDENT, M_N };
static const char *MN[M_N] = {"progs", "secChord", "secWho", "ld n/bar", "step%", "leap%", "range", "rhyBack%", "sameBar%", "call%", "bassKick%", "lead%", "voices", "secDens", "ident"};
static uint32_t chord_sig(void)
{
    uint32_t k, h = 2166136261u;
    for (k = 0; k < gs.nch; k++) { h = (h ^ gs.ch[k].deg) * 16777619u; h = (h ^ gs.ch[k].q) * 16777619u; h = (h ^ gs.ch[k].alt) * 16777619u; h = (h ^ gs.ch[k].len) * 16777619u; }
    return h;
}
static uint32_t bar_rhy(const track_t *t, uint32_t b) { uint32_t i, m = 0; for (i = 0; i < 16u; i++) m |= (uint32_t)(t->step[b * 16u + i].time == ST_NOTE && t->step[b * 16u + i].n) << i; return m; }
static uint32_t bar_sig(const track_t *t, uint32_t b) { uint32_t i, h = 2166136261u; for (i = 0; i < 16u; i++) { const step_t *s = &t->step[b * 16u + i]; h = (h ^ (s->time == ST_NOTE && s->n ? s->note[0] : s->time)) * 16777619u; } return h; }
static uint32_t onsets(const track_t *t, uint32_t len) { uint32_t i, n = 0; for (i = 0; i < len; i++) n += t->step[i].time == ST_NOTE && (t->step[i].n || t->step[i].hit); return n; }
static uint32_t sec_chords(const project_t *p, uint32_t pd)   /* a stored section's chords: the pad's (or any pitched track's) pitch classes a bar */
{
    uint32_t i, j, h = 2166136261u;
    for (i = 0; i < 64u; i++) { const step_t *s = &p->t[pd].step[i]; uint32_t m = 0; if (s->time != ST_NOTE) continue; for (j = 0; j < s->n; j++) m |= 1u << (s->note[j] % 12u); h += (m * 2654435761u) ^ (m << 7); }   /* (the chords as a set: the same ones turned are the same) */
    return h;
}
int main(int argc, char **argv)
{
    uint32_t seeds = argc > 1 ? (uint32_t)atoi(argv[1]) : 24, only = argc > 2 ? (uint32_t)atoi(argv[2]) : 0, g, s, k, m;
    row_t tot; uint32_t ng = 0;
    memset(&tot, 0, sizeof tot);
    ui_power_on(); go_home(); frames(4);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    printf("%-14s", "genre"); for (m = 0; m < M_N; m++) printf(" %9s", MN[m]); printf("\n");
    for (g = only ? only : 1; g <= (only ? only : NGENRE + 1u); g++) {
        row_t r; uint32_t sigs[64], nsig = 0, nl = 0, nb = 0, npart = 0; uint32_t ids[64], nid = 0;
        memset(&r, 0, sizeof r);
        for (s = 1; s <= seeds; s++) {
            static project_t p;
            uint32_t ld, bs, pd, ar, dr, len, bars, i, cs, steps = 0, leaps = 0, ivs = 0, lo = 127, hi = 0, back = 0, same = 0, cmp = 0, call = 0, cb = 0, bk = 0, bo = 0, act = 0, idh;
            gp.genre = (uint8_t)g; gp.seed = (uint16_t)(s * 37u); gp.energy = 50; gp.form = 1;
            for (k = 0; k < GP_N; k++) gp.amt[k] = getenv("GEN_ALL") ? 60 : GA_AUTO;   /* (GEN_ALL=1: every part on) */
            gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            ld = gs.trk_of[RL_LEAD]; bs = gs.trk_of[RL_BASS]; pd = gs.trk_of[RL_PAD]; ar = gs.trk_of[RL_ARP]; dr = gs.trk_of[RL_DRUM];
            cs = chord_sig();
            for (k = 0; k < nsig && sigs[k] != cs; k++) {}
            if (k == nsig && nsig < 64u) sigs[nsig++] = cs;
            for (k = 1; k < RL_N; k++) act += gs.act[k] != 0;
            r.v[M_DENS] += act;
            /* identity: the lead's first bar rhythm + the bass's first bar rhythm + the tempo's ten + the chord count */
            idh = (ld < NTRK && gs.act[RL_LEAD] ? bar_rhy(&trk[ld], 0) : 0) * 31u + (bs < NTRK ? bar_rhy(&trk[bs], 0) : 0) * 7u + gs.nch;
            for (k = 0; k < nid && ids[k] != idh; k++) {}
            if (k == nid && nid < 64u) ids[nid++] = idh;
            if (ld < NTRK && gs.act[RL_LEAD] && onsets(&trk[ld], dlen(&trk[ld]))) {
                const track_t *t = &trk[ld]; int32_t pv = -1; uint32_t r0;
                len = dlen(t); bars = len / 16u; if (!bars) bars = 1;
                r.v[M_NPB] += (double)onsets(t, len) / bars;
                for (i = 0; i < len; i++) { const step_t *x = &t->step[i]; if (x->time != ST_NOTE || !x->n) continue;
                    if (x->note[0] < lo) lo = x->note[0]; if (x->note[0] > hi) hi = x->note[0];
                    if (pv >= 0) { int32_t d = (int32_t)x->note[0] - pv; if (d < 0) d = -d; ivs++; steps += d <= 2; leaps += d > 7; } pv = x->note[0]; }
                r0 = bar_rhy(t, 0);
                for (i = 1; i < bars; i++) { cmp++; back += bar_rhy(t, i) == r0 && r0; same += bar_sig(t, i) == bar_sig(t, i - 1u) || (i >= 2u && bar_sig(t, i) == bar_sig(t, i - 2u)); }
                if (ivs) { r.v[M_STEP] += 100.0 * steps / ivs; r.v[M_LEAP] += 100.0 * leaps / ivs; }
                r.v[M_RANGE] += hi >= lo ? hi - lo : 0;
                if (cmp) { r.v[M_RHY] += 100.0 * back / cmp; r.v[M_SAME] += 100.0 * same / cmp; }
                /* call and response: half bars where the lead rests and the arp (or the pad's rhythm) sounds onsets, next to
                 * half bars where the lead plays */
                for (i = 0; i + 16u <= len; i += 8u) {
                    uint32_t j, lo1 = 0, lo2 = 0, o1 = 0, o2 = 0; const track_t *ot = ar < NTRK && gs.act[RL_ARP] ? &trk[ar] : pd < NTRK && gs.act[RL_PAD] ? &trk[pd] : t;
                    for (j = 0; j < 8u; j++) { lo1 += t->step[i + j].time == ST_NOTE; lo2 += t->step[i + 8u + j].time == ST_NOTE; o1 += ot->step[(i + j) % dlen(ot)].time == ST_NOTE; o2 += ot->step[(i + 8u + j) % dlen(ot)].time == ST_NOTE; }
                    cb++; call += (lo1 >= 2u && !o1 && o2 && lo2 <= 1u) || (lo2 >= 2u && !o2 && o1 && lo1 <= 1u);   /* (one speaks, the other is silent; then the other) */
                }
                if (cb) r.v[M_CALL] += 100.0 * call / cb;
                nl++;
            }
            if (bs < NTRK && gs.act[RL_BASS]) { const track_t *t = &trk[bs]; len = dlen(t);
                for (i = 0; i < len; i++) if (t->step[i].time == ST_NOTE && t->step[i].n) { bo++; bk += GKICK(i); }
                if (bo) { r.v[M_BK] += 100.0 * bk / bo; nb++; } }
            /* the sections: B's and C's chords against A's, who plays, how dense */
            { uint32_t a_ch = pd < NTRK ? 0 : 0, sd = 0, sw = 0, ns = 0, dA = 0, mA = 0; double dd = 0;
              if (proj_import(&p, &proj_slot[0], sizeof(project_store_t))) { a_ch = sec_chords(&p, pd < NTRK ? pd : bs); for (k = 0; k < NTRK; k++) { mA |= (uint32_t)(p.t[k].p[P_MUTE] != 0) << k; if (!p.t[k].p[P_MUTE]) { uint32_t i2; for (i2 = 0; i2 < 64u; i2++) dA += p.t[k].step[i2].time == ST_NOTE; } } }
              for (m = 1; m < 4u; m++) { uint32_t mm = 0, dS = 0;
                  if (!((dice_secs >> m) & 1u) || !proj_import(&p, &proj_slot[m], sizeof(project_store_t))) continue;
                  ns++; sd += sec_chords(&p, pd < NTRK ? pd : bs) != a_ch;
                  for (k = 0; k < NTRK; k++) { mm |= (uint32_t)(p.t[k].p[P_MUTE] != 0) << k; if (!p.t[k].p[P_MUTE]) { uint32_t i2; for (i2 = 0; i2 < 64u; i2++) dS += p.t[k].step[i2].time == ST_NOTE; } }
                  sw += mm != mA; dd += dA ? fabs((double)dS - dA) * 100.0 / dA : 0; }
              if (ns) { r.v[M_SECDIFF] += 100.0 * sd / ns; r.v[M_SECWHO] += 100.0 * sw / ns; r.v[M_SECDENS] += dd / ns; npart++; } }
        }
        r.v[M_PROGS] = nsig; r.v[M_IDENT] = nid;
        r.v[M_LEADON] = 100.0 * nl / seeds; r.v[M_DENS] /= seeds;
        for (m = M_NPB; m <= M_CALL; m++) if (nl) r.v[m] /= nl;
        if (nb) r.v[M_BK] /= nb;
        if (npart) { r.v[M_SECDIFF] /= npart; r.v[M_SECWHO] /= npart; r.v[M_SECDENS] /= npart; }
        printf("%-14s", g <= NGENRE ? GENRE[g - 1].name : "EXPLORE");
        for (m = 0; m < M_N; m++) { printf(" %9.1f", r.v[m]); tot.v[m] += r.v[m]; }
        printf("\n"); ng++;
    }
    printf("%-14s", "MEAN"); for (m = 0; m < M_N; m++) printf(" %9.1f", tot.v[m] / ng); printf("\n");
    return 0;
}
