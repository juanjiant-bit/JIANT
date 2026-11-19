// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.59) the repeats on the tempo: RATCH x2 .. x8 (a step component), STUTTER (a step component), the punch-in
 * STUT 1/8 / 1/16 and ARP: each note-on of track 1 against its ideal sample on the transport's grid (a block's
 * resolution: CTL), straight and swung, at a few tempos. A stumble: a note more than a block off its place, or one
 * missing / extra. Built and run: cc ... tests/rep_sync_test.c */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t rs_clock;
static void rs_setup(uint32_t bpm, uint32_t swing, uint32_t every)
{
    uint32_t i, k;
    ui_power_on();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults_steps(t);
        t->p[P_SLEN] = 16;
        t->p[P_SDIV] = 2;
        t->p[P_SSWING] = 0;
        t->p[P_SGATE] = 12;
        for (k = 0; k < 16u && !i; k++)
            if (!(k % every))
                t->step[k] = (step_t){{(uint8_t)(48 + k)}, 1, ST_NOTE, 0, 100};
    }
    song.g[G_BPM] = (int16_t)bpm;
    song.g[G_SWING] = (int16_t)swing;
    memset(fg_set, 0, sizeof fg_set);
    transport_req = 1;                              /* (mix_block starts the transport: seq_start) */
    rs_clock = 0;
}
static uint32_t rs_run(uint32_t samples, uint32_t *at, uint32_t max)
{
    uint32_t n = 0, end = rs_clock + samples;
    while (rs_clock < end) {
        uint32_t v0 = vage;
        static int32_t o[2 * CTL];
        mix_block(o, CTL);                          /* (the whole block: the punch-ins, the events, the parts) */
        for (; v0 < vage; v0++) {
            if (n < max) at[n] = rs_clock;
            n++;
        }
        rs_clock += CTL;
    }
    return n;
}
static void comp_put(step_t *s, uint32_t a, uint32_t len, uint32_t v)
{
    s->comp = CO_MAKE(a, CC_ALWAYS); s->carg = (uint8_t)v; s->clen[0] = (uint8_t)(len - 1u); s->cnm[0] = 0;
}
/* the onsets against the grid: want[] the ideal samples; returns the worst error in samples, *miss the count off */
static uint32_t rs_cmp(const uint32_t *at, uint32_t n, const uint32_t *want, uint32_t m, int32_t *miss)
{
    uint32_t i, worst = 0;
    *miss = (int32_t)n - (int32_t)m;
    for (i = 0; i < n && i < m; i++) {
        uint32_t d = at[i] > want[i] ? at[i] - want[i] : want[i] - at[i];
        worst = d > worst ? d : worst;
    }
    return worst;
}
int main(void)
{
    static const uint32_t BPM[3] = {93, 120, 137};
    static uint32_t at[512], want[512];
    int bad = 0;
    uint32_t b, h, sw, m, i, k, n, worst;
    int32_t miss;
    char what[200];
    for (b = 0; b < 3u; b++)
        for (sw = 0; sw < 2u; sw++)
            for (h = 2; h <= 8u; h++) {             /* RATCH xh on every 4th step (a note each beat), 2 bars */
                uint32_t P;
                rs_setup(BPM[b], sw ? 60u : 0u, 4u);
                for (k = 0; k < 16u; k += 4u)
                    comp_put(&trk[0].step[k], CA_RATCH, 1u, h - 2u);
                P = div_samples(2);
                n = rs_run(32u * P, at, 512);
                for (m = 0, i = 0; i < 8u; i++)       /* the beats (swing moves the odd 1/16s: a beat's start stays) */
                    for (k = 0; k < h; k++)
                        want[m++] = i * 4u * P + k * step_samples(&trk[0], P, 0) / h;
                worst = rs_cmp(at, n, want, m, &miss);
                snprintf(what, sizeof what, "RATCH x%u at %u BPM%s: %u notes for %u, worst %u samples off (a block %u)", h,
                         BPM[b], sw ? " swung" : "", n, m, worst, (unsigned)CTL);
                bad += check(what, !miss && worst <= CTL);
            }
    for (b = 0; b < 3u; b++) {                      /* STUTTER over 4 steps from step 0: a note each step, on the grid */
        uint32_t P;
        rs_setup(BPM[b], 0, 16u);
        comp_put(&trk[0].step[0], CA_STUT, 4u, 0);
        P = div_samples(2);
        n = rs_run(16u * P, at, 512);
        for (m = 0; m < 4u; m++) want[m] = m * P;
        worst = rs_cmp(at, n < 4u ? n : 4u, want, 4u, &miss);
        snprintf(what, sizeof what, "STUTTER x4 at %u BPM: the first four notes on the steps (worst %u samples), %u in all",
                 BPM[b], worst, n);
        bad += check(what, n >= 4u && worst <= CTL);
    }
    for (b = 0; b < 3u; b++)                        /* the punch-in STUT 1/16 and 1/8 held a bar on a note a beat */
        for (k = 0; k < 2u; k++) {
            uint32_t P, per;
            rs_setup(BPM[b], 0, 4u);
            P = div_samples(2);
            n = rs_run(4u * P, at, 512);              /* (a beat played: its note the one repeated) */
            perf_held = PF_BIT(k ? PF_S8 : PF_S16);
            n = rs_run(16u * P, at, 512);
            perf_held = 0;
            per = k ? 2u * P : P;
            for (m = 0; m * per <= 16u * P; m++) want[m] = 4u * P + m * per;   /* (the press's note, then each repeat on
                                                         * the grid: a 1/8 on the beat, V10.7.59) */
            worst = rs_cmp(at, n, want, m, &miss);
            if (getenv("RS_V")) { for (i = 0; i < n && i < 20u; i++) printf(" %d", (int)at[i] - (int)(4u * P)); printf("  (P %u)\n", P); }
            snprintf(what, sizeof what, "punch-in STUT %s at %u BPM: %u notes for %u, worst %u samples off", k ? "1/8" : "1/16",
                     BPM[b], n, m, worst);
            bad += check(what, !miss && worst <= CTL);
        }
    printf(bad ? "rep-sync: %d FAILED\n" : "rep-sync: all on the grid\n", bad);
    return bad != 0;
}
