/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.40, a user: "the sequencer creeps ahead of the project's tempo") the sequencer against the ideal grid: ten
 * minutes of blocks through seq_time and events_block (mix_block's two) at each tempo, every track on its own DIV / SWING / FIT, the metronome's
 * 1/16 clock too; where a step starts (the samples rendered less seq_pos) against k x FS x 60 / (BPM x its division).
 * (FIT's place is reckoned from its steps' own lengths: a sample or two of the reckoning's.)
 * Passes with at most DRIFT_MAX samples off at the end of the ten minutes, on every track, and no track off another */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define DRIFT_MAX 3
static int dr_bad;
/* a track's division of the beat as a ratio (N_DIV order: 1/4 1/8 1/16 1/32 1/8T 1/16T) */
static const uint8_t DEN[6] = {1, 2, 4, 8, 3, 6};

int main(int argc, char **argv)
{
    static const uint32_t tempos[] = {60, 90, 120, 121, 133, 140, 174, 239};
    static const struct { uint8_t div, swing, fit, len; const char *name; } T[NTRK] = {
        {2, 0, 0, 16, "1/16"}, {1, 0, 0, 16, "1/8"}, {3, 0, 0, 32, "1/32"}, {5, 0, 0, 12, "1/16T"},
        {2, 60, 0, 16, "1/16 sw60"}, {2, 0, 128u | 16u, 18, "FIT 18:16"}};
    uint32_t minutes = argc > 1 ? (uint32_t)atoi(argv[1]) : 10u, worst = 0;
    printf("drift after %u min, samples (ms a minute); + is early\n%-5s", minutes, "BPM");
    for (uint32_t k = 0; k < NTRK; k++) printf(" %-18s", T[k].name);
    printf(" %-18s\n", "click 1/16");
    for (uint32_t b = 0; b < NELEM(tempos); b++) {
        uint64_t steps[NTRK] = {0}, total = (uint64_t)FS * 60u * minutes, el = 0;
        uint32_t last[NTRK], bpm = tempos[b];
        ui_power_on();
        song.g[G_BPM] = (int16_t)bpm; song.g[G_SWING] = 0; song.g[G_CLOCK] = 0;
        for (uint32_t k = 0; k < NTRK; k++) {
            track_t *q = &trk[k];
            q->p[P_SLEN] = T[k].len; q->p[P_SDIV] = T[k].div; q->p[P_SSWING] = T[k].swing; q->seq_fit = T[k].fit;
            for (uint32_t i = 0; i < NSTEP; i++) step_clear(&q->step[i]);
        }
        transport_req = 1;
        events_block(CTL);
        for (uint32_t k = 0; k < NTRK; k++) last[k] = trk[k].seq_idx;
        while (el < total) {
            events_block(seq_time(CTL));                /* (as fx.c mix_block) */
            el += CTL;
            for (uint32_t k = 0; k < NTRK; k++) {
                uint32_t len = T[k].len, d = (trk[k].seq_idx + len - last[k]) % len;
                steps[k] += d; last[k] = trk[k].seq_idx;
            }
        }
        printf("%-5u", bpm);
        for (uint32_t k = 0; k <= NTRK; k++) {
            /* where the playhead is, in samples of the ideal grid: whole steps (pairs with swing, loops with FIT) */
            uint32_t den = k < NTRK ? DEN[T[k].div] : 4u;
            double step = (double)FS * 60.0 / ((double)bpm * den), at, d;
            if (k == NTRK) at = (double)clk_n * step + clk_pos;
            else {
                uint64_t s = steps[k]; uint32_t in = trk[k].seq_pos, i;
                if (T[k].fit) {
                    double loop = step * 16.0;          /* SPAN 16 units a loop of LEN steps */
                    uint32_t per = div_samples(T[k].div);
                    at = (double)(s / T[k].len) * loop;
                    for (i = 0; i < s % T[k].len; i++) at += (double)step_samples(&trk[k], per, i) * loop / (16.0 * per);
                    at += in;
                } else {
                    uint32_t per = div_samples(T[k].div);
                    at = (double)(s / 2u) * 2.0 * step;
                    if (s & 1u) at += (double)step_samples(&trk[k], per, 0) * step / per;
                    at += in;
                }
            }
            d = at - (double)el;                        /* + : further on than the samples played: early */
            printf(" %+8.1f (%+6.2f)  ", d, d * 1000.0 / FS / minutes);
            if (fabs(d) > worst) worst = (uint32_t)fabs(d);
        }
        printf("\n");
    }
    dr_bad += check("the sequencer on the tempo's grid for ten minutes: every DIV, SWING, FIT and the click", worst <= DRIFT_MAX);
    return dr_bad ? 1 : 0;
}
