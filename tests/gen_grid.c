/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The generator's pieces as text (a drum grid, the lead's and the arp's lines) and as sound (a WAV), to read and hear a
 * change of the generator before and after:
 *   cc -O2 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_grid tests/gen_grid.c -lm
 *   build/host/gen_grid GENRE SEEDS [WAVDIR [SECONDS]] */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static void wav(const char *path, double secs)
{
    uint32_t nsamp = 0, want = (uint32_t)(secs * FS), f = 0, i; FILE *o = fopen(path, "wb"); uint32_t h[11];
    if (!o) return;
    h[0] = 0x46464952; h[1] = 36 + want * 4; h[2] = 0x45564157; h[3] = 0x20746d66; h[4] = 16; h[5] = 0x00020001; h[6] = FS; h[7] = FS * 4; h[8] = 0x00100004; h[9] = 0x61746164; h[10] = want * 4;
    fwrite(h, 4, 11, o);
    transport_req = 1;
    while (nsamp < want) {
        uint32_t upto = (uint32_t)((uint64_t)(++f) * FS / 30u);
        ui_input(); song_poll();
        while (nsamp < upto && nsamp < want) {
            int32_t b[2 * CTL]; int16_t w[2 * CTL];
            mix_block(b, CTL);
            for (i = 0; i < 2 * CTL; i++) w[i] = (int16_t)clamp(b[i], -32768, 32767);
            fwrite(w, 2, 2 * CTL, o); nsamp += CTL;
        }
        host_ticks += 1000000u / 30u; fm1_ms += 1000u / 30u;
    }
    transport_req = 2; ui_input();
    for (i = 0; i < 40u; i++) { int32_t b[2 * CTL]; mix_block(b, CTL); }
    fclose(o);
}
static void line(const track_t *t, const char *nm)
{
    static const char *NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    uint32_t i, len = dlen(t);
    printf("  %-4s len %2u |", nm, len);
    for (i = 0; i < len; i++) {
        const step_t *s = &t->step[i];
        if (!(i % 16u) && i) printf("|");
        if (s->time == ST_NOTE && s->n) printf("%-2s%d", NN[s->note[0] % 12], s->note[0] / 12 - 1);
        else if (s->time == ST_TIE) printf(" _ ");
        else printf(" . ");
        if (s->time == ST_NOTE && step_ratchet(s) > 1u) printf("\b%u", step_ratchet(s));
    }
    printf("|\n");
}
int main(int argc, char **argv)
{
    uint32_t g = argc > 1 ? (uint32_t)atoi(argv[1]) : 4, seeds = argc > 2 ? (uint32_t)atoi(argv[2]) : 4, s, k, l, i;
    double secs = argc > 4 ? atof(argv[4]) : 16.0;
    static const char LC[8] = {'K', 'S', 'C', 'h', 'O', 'T', 'p', 'q'};
    ui_power_on(); go_home(); frames(4);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    song.master_q12 = 4096;
    if (!g) {                                           /* every genre: its distinct lines of SEEDS pieces */
        uint32_t bad = 0;
        for (g = 1; g <= NGENRE; g++) {
            uint32_t h[4][64], nd[4] = {0, 0, 0, 0}, na[4] = {0, 0, 0, 0}, r;
            for (s = 1; s <= seeds && s <= 64u; s++) {
                gp.genre = (uint8_t)g; gp.seed = (uint16_t)(s * 37u); gp.energy = 50; gp.form = 0;
                for (k = 0; k < GP_N; k++) gp.amt[k] = 60;
                gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
                gen_go();
                for (r = 0; r < 4u; r++) {              /* drums, bass, lead, arp: pitch classes from its first note */
                    static const uint8_t RR[4] = {RL_DRUM, RL_BASS, RL_LEAD, RL_ARP};
                    uint32_t kk = gs.trk_of[RR[r]], x = 2166136261u, j, first = 255; const track_t *t;
                    if (kk >= NTRK || !gs.act[RR[r]]) continue;
                    t = &trk[kk];
                    for (i = 0, j = 0; i < dlen(t); i++) j |= t->step[i].time == ST_NOTE;
                    if (!j) continue;                   /* (a part with nothing in it: not a line) */
                    na[r]++;
                    for (i = 0; i < (r == 2u ? 16u : dlen(t)); i++) {   /* (the lead: its first bar, the motif) */
                        const step_t *q = &t->step[i]; uint32_t v = q->time;
                        if (q->time == ST_NOTE) { if (r) { if (first == 255) first = q->note[0]; v = 16u + (uint32_t)(q->note[0] - first + 96); } else v = 16u + q->hit; }
                        x = (x ^ v) * 16777619u;
                    }
                    for (j = 0; j < nd[r]; j++) if (h[r][j] == x) break;
                    if (j == nd[r]) h[r][nd[r]++] = x;
                }
            }
            printf("%-15s distinct of pieces: drums %2u/%2u bass %2u/%2u lead %2u/%2u arp %2u/%2u%s\n", GENRE[g - 1].name, nd[0], na[0], nd[1], na[1], nd[2], na[2], nd[3], na[3],
                   na[2] >= 8u && nd[2] * 6u < na[2] ? "  <-- LEAD REPEATS" : "");
            bad += na[2] >= 8u && nd[2] * 6u < na[2];   /* (a genre's leads: a sixth of them distinct first bars at least) */
        }
        return (int)bad;
    }
    for (s = 1; s <= seeds; s++) {
        gp.genre = (uint8_t)g; gp.seed = (uint16_t)(s * 37u); gp.energy = 50; gp.form = 0;
        for (k = 0; k < GP_N; k++) gp.amt[k] = 60;
        gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
        gen_go();
        printf("== %s seed %u  bpm %d\n", GENRE[g - 1].name, s * 37u, (int)song.g[G_BPM]);
        for (k = 0; k < NTRK; k++) {
            const track_t *t = &trk[k]; uint32_t r = gs.role[k], len = dlen(t);
            if (!gs.act[r] || (r == RL_TEX && k != gs.trk_of[RL_TEX])) continue;
            if (r == RL_DRUM) {
                for (l = 0; l < 8u; l++) {
                    uint32_t any = 0;
                    for (i = 0; i < len; i++) any |= t->step[i].time == ST_NOTE && ((t->step[i].hit >> l) & 1u);
                    if (!any) continue;
                    printf("  %c    len %2u |", LC[l], len);
                    for (i = 0; i < len; i++) {
                        const step_t *x = &t->step[i]; int on = x->time == ST_NOTE && ((x->hit >> l) & 1u);
                        if (!(i % 16u) && i) printf("|");
                        putchar(!on ? (i % 4u ? '.' : ',') : (x->acc >> l) & 1u ? 'X' : x->vel < 60 ? 'o' : 'x');
                    }
                    printf("|\n");
                }
                {   /* rolls a bar (lane hits x2 x4 x8), the hits with a way of their own (SHORT GHOST FLAM, a chance, a
                     * lock on their step), the steps that differ between a bar and the one before */
                    uint32_t rolls = 0, hits = 0, own = 0, diff = 0, tot = 0, e, lk;
                    for (i = 0; i < len; i++) {
                        const step_t *x = &t->step[i];
                        for (lk = 0, e = 0; e < motion.count; e++) lk |= (motion.event[e].place & 255u) == ((k << 6 | i) & 255u);
                        if (i >= 16u) { tot++; diff += x->hit != t->step[i - 16u].hit || (x->time == ST_NOTE) != (t->step[i - 16u].time == ST_NOTE); }
                        if (x->time != ST_NOTE) continue;
                        for (l = 0; l < 8u; l++) {
                            uint32_t m = step_lmode(x, l);
                            if (!((x->hit >> l) & 1u)) continue;
                            hits++; rolls += lm_hits(m) > 1u; own += m >= LM_SHORT || lk || x->probability;
                        }
                    }
                    printf("  drums: %.1f rolls a bar, %u %% of %u hits their own way, %u %% of the steps differ from the bar before\n",
                           rolls * 16.0 / len, hits ? own * 100u / hits : 0u, hits, tot ? diff * 100u / tot : 0u);
                }
            } else line(t, r == RL_BASS ? "BASS" : r == RL_LEAD ? "LEAD" : r == RL_ARP ? "ARP" : r == RL_PAD ? "PAD" : "TEX");
        }
        if (argc > 3) { char p[256]; snprintf(p, sizeof p, "%s/g%02u-s%u.wav", argv[3], g, s); wav(p, secs); }
    }
    return 0;
}
