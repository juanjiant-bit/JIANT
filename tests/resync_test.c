/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.40, the owner: "after changes while it plays a track's start no longer lines up with the others'; stop and
 * play again") twin runs through events_block: one left alone, one with a live action on track 1 undone some steps
 * later (LEN, DIV, both, a sound's shorter pattern, EUCLID's steps, FIT on and off, a tempo change, SWING), at several
 * moments of the bar; a bar after the undo every track's step and its samples into it as the run left alone has them.
 * And RESYNC: polymetric tracks from their first step at the next bar, in time, the others untouched */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int rs_bad;
typedef struct { uint16_t idx[NTRK]; uint32_t pos[NTRK]; } snap_t;
static void setup(void)
{
    static const uint8_t LEN[NTRK] = {16, 16, 32, 12, 16, 8}, DIV[NTRK] = {2, 2, 3, 2, 1, 5};
    ui_power_on();
    song.g[G_BPM] = 133; song.g[G_SWING] = 0; song.g[G_CLOCK] = 0;
    for (uint32_t k = 0; k < NTRK; k++) {
        track_t *q = &trk[k];
        q->p[P_SLEN] = LEN[k]; q->p[P_SDIV] = DIV[k]; q->p[P_SSWING] = k == 4 ? 40 : 0; q->seq_fit = 0;
        for (uint32_t i = 0; i < NSTEP; i++) step_clear(&q->step[i]);
    }
    transport_req = 1;
    events_block(CTL);
}
static void run(uint32_t samples) { for (uint32_t el = 0; el < samples; el += CTL) events_block(CTL); }
static void snap(snap_t *s) { for (uint32_t k = 0; k < NTRK; k++) { s->idx[k] = trk[k].seq_idx; s->pos[k] = trk[k].seq_pos; } }
typedef void (*act_t)(int on);
static int16_t keep[4];
static void a_len12(int on)  { if (on) keep[0] = trk[1].p[P_SLEN], trk[1].p[P_SLEN] = 12; else trk[1].p[P_SLEN] = keep[0]; }
static void a_len5(int on)   { if (on) keep[0] = trk[1].p[P_SLEN], trk[1].p[P_SLEN] = 5; else trk[1].p[P_SLEN] = keep[0]; }
static void a_len32(int on)  { if (on) keep[0] = trk[1].p[P_SLEN], trk[1].p[P_SLEN] = 32; else trk[1].p[P_SLEN] = keep[0]; }
static void a_div8(int on)   { if (on) keep[1] = trk[1].p[P_SDIV], trk[1].p[P_SDIV] = 1; else trk[1].p[P_SDIV] = keep[1]; }
static void a_div32(int on)  { if (on) keep[1] = trk[1].p[P_SDIV], trk[1].p[P_SDIV] = 3; else trk[1].p[P_SDIV] = keep[1]; }
static void a_divT(int on)   { if (on) keep[1] = trk[1].p[P_SDIV], trk[1].p[P_SDIV] = 5; else trk[1].p[P_SDIV] = keep[1]; }
static void a_knob(int on)   { fit_ui_edit(&trk[1], &trk[1].p[P_SDIV], on ? 1 : 2); }   /* (the DIV knob's own path) */
static void a_knobl(int on)  { fit_ui_edit(&trk[1], &trk[1].p[P_SLEN], on ? 7 : 16); }
static void a_both(int on)   { a_len12(on); a_div8(on); }
static void a_fit(int on)    { fit_ui_set(&trk[1], on ? 128u | 12u : 16u); }
static void a_bpm(int on)    { song.g[G_BPM] = on ? 97 : 133; }
static void a_swing(int on)  { trk[1].p[P_SSWING] = on ? 70 : 0; }
static void a_gsw(int on)    { song.g[G_SWING] = on ? 55 : 0; }
static void a_mute(int on)   { trk[1].p[P_MUTE] = on ? 1 : 0; }

int main(void)
{
    static const struct { const char *name; act_t f; } A[] = {
        {"LEN 16 > 12 > 16", a_len12}, {"LEN 16 > 5 > 16", a_len5}, {"LEN 16 > 32 > 16", a_len32},
        {"DIV 1/16 > 1/8 > 1/16", a_div8}, {"DIV 1/16 > 1/32 > 1/16", a_div32}, {"DIV 1/16 > 1/16T > 1/16", a_divT},
        {"DIV knob (rescaled) and back", a_knob}, {"LEN knob 16 > 7 > 16", a_knobl}, {"LEN and DIV together", a_both},
        {"FIT 12 on and off", a_fit}, {"tempo 133 > 97 > 133", a_bpm}, {"track SWING 70 and back", a_swing},
        {"global SWING 55 and back", a_gsw}, {"mute and unmute", a_mute}};
    static const uint32_t at[] = {7, 3001, 5000, 9973, 23456, 40001}, hold[] = {4973u * 3u + 100u, 4973u * 7u + 2500u, 4973u * 29u + 11u};
    printf("%-32s %s\n", "action on track 2, undone", "tracks off the others afterwards (of 18 moments)");
    for (uint32_t a = 0; a < NELEM(A); a++) {
        uint32_t off = 0, worst = 0;
        for (uint32_t i = 0; i < NELEM(at); i++)
            for (uint32_t h = 0; h < NELEM(hold); h++) {
                snap_t ref, got;
                uint32_t tail = 240000u, d = 0;
                setup(); run(40000u + at[i]); run(hold[h]); run(tail); snap(&ref);
                setup(); run(40000u + at[i]); A[a].f(1); run(hold[h]); A[a].f(0); run(tail); snap(&got);
                if (A[a].f == a_bpm) {               /* (a tempo change moves every track together: against track 1) */
                    for (uint32_t k = 0; k < NTRK; k++) if (trk[k].p[P_SLEN] == 16 && trk[k].p[P_SDIV] == 2) d |= got.idx[k] != got.idx[0] || got.pos[k] != got.pos[0];
                } else
                    for (uint32_t k = 0; k < NTRK; k++) d |= ref.idx[k] != got.idx[k] || ref.pos[k] != got.pos[k];
                off += d;
                if (d) { uint32_t e = (got.idx[1] + 16u - ref.idx[1]) % 16u; worst = e > worst ? e : worst; }
            }
        printf("%-32s %2u   %s\n", A[a].name, off, off ? "DESYNC" : "in phase");
        rs_bad += off != 0;
    }
    {   /* EUCLID on track 2 while it plays (the real knobs: eu_put writes the pattern and its LEN), MUTATE: its place
         * the transport's after each (a 1/16 track: step = the 1/16s since PLAY mod LEN, the samples the click's) */
        static const struct { const char *name; uint8_t k; int16_t v; } E[] = {
            {"EUCLID STEPS 12", EK_STEPS, 12}, {"EUCLID ROTATE 5", EK_ROT, 5}, {"EUCLID HITS 7", EK_HITS, 7},
            {"EUCLID STEPS 7", EK_STEPS, 7}, {"EUCLID STEPS 16", EK_STEPS, 16}, {"EUCLID ROLE 2", EK_K4, 2},
            {"EUCLID STEPS 24", EK_STEPS, 24}, {"EUCLID STEPS 16 again", EK_STEPS, 16}, {"MUTATE", 255, 0}};
        uint32_t ok;
        setup(); song.sel = 1; run(40000u);
        for (uint32_t i = 0; i < NELEM(E); i++) {
            uint32_t len;
            fm1_ms += 3000u;
            if (E[i].k == 255) { eu_mutate(1, eu_of(1)); eu_gen(1, 0); } else eu_put(E[i].k, E[i].v);
            run(4973u * 11u + 777u * i);
            len = (uint32_t)trk[1].p[P_SLEN];
            ok = trk[1].p[P_SDIV] == 2 && trk[1].seq_idx == clk_n % len && trk[1].seq_pos == clk_pos &&
                 trk[0].seq_idx == clk_n % 16u && trk[0].seq_pos == clk_pos;
            printf("%-32s LEN %2u  %s\n", E[i].name, len, ok ? "in phase" : "DESYNC");
            rs_bad += !ok;
        }
    }
    {   /* RESYNC: asked mid-bar; at the next bar the 12-step and the 1/32 tracks at their step 0 with the 16-step ones */
        uint32_t s16, bar;
        setup(); s16 = div_samples(2); bar = 16u * s16;
        run(bar * 4u + 3000u);                          /* (bar 5: the 12-step track 80 steps in, its step 8 at the bar) */
        seq_rs = (clk_n / 16u + 1u) * 16u;
        while (clk_n < seq_rs || clk_pos < 2u * CTL) events_block(CTL);
        rs_bad += check("RESYNC: at the next bar every track on its first step, in time",
                        trk[3].seq_idx == 0 && trk[0].seq_idx == 0 && trk[2].seq_idx == 0 && trk[3].seq_pos == trk[0].seq_pos && trk[5].seq_idx == 0);
        run(bar * 6u);
        rs_bad += check("RESYNC: its request gone after it", !seq_rs);
    }
    printf(rs_bad ? "resync: FAIL\n" : "resync: ok\n");
    return rs_bad ? 1 : 0;
}
