// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.59) the FILL step component (LOCK's place): a kit's fill (its hits and rolls), a synth's (the step's note
 * rolled, up ..), the banks RANDOM and STAIR. Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t fl_notes[64], fl_n;
static void fl_setup(uint32_t tr, uint32_t eng, uint32_t v, uint32_t len)
{
    uint32_t i, k;
    ui_power_on();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults_steps(t);
        t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_SSWING] = 0; t->p[P_SGATE] = 12;
        if (i != tr) t->p[P_MUTE] = 1;
    }
    set_engine_of(&trk[tr], eng); trk[tr].engine = trk[tr].eng_req;
    for (k = 0; k < 16u; k++) trk[tr].step[k] = (step_t){{0}, 0, ST_REST, 0, 100};
    trk[tr].step[0] = (step_t){{eng == ENGI_DRUM ? 36u : 60u}, 1, ST_NOTE, 0, 100};
    trk[tr].step[0].comp = CO_MAKE(CA_LOCK, CC_ALWAYS);
    trk[tr].step[0].carg = (uint8_t)v;
    trk[tr].step[0].clen[0] = (uint8_t)(len - 1u);
    song.g[G_BPM] = 120; song.g[G_SWING] = 0;
    memset(fg_set, 0, sizeof fg_set);
    seq_start();
    fl_n = 0;
}
static uint32_t fl_run(uint32_t tr, uint32_t samples)
{
    uint32_t c = 0, v0, i;
    while (c < samples) {
        v0 = vage;
        events_block(CTL);
        for (; v0 < vage; v0++) {
            uint32_t best = 0;
            for (i = 1; i < NVOICE; i++) if (trk[tr].v[i].age > trk[tr].v[best].age) best = i;
            if (fl_n < 64u) fl_notes[fl_n++] = trk[tr].v[best].note;
        }
        c += CTL;
    }
    return fl_n;
}
int main(void)
{
    int bad = 0, ok;
    uint32_t P, n, i;
    char what[200];
    fl_setup(0, ENGI_DRUM, 0, 8);                   /* a kit: SNARE over 8 steps, its rolls rising: 19 hits */
    P = div_samples(2);
    n = fl_run(0, 8u * P);
    if (getenv("FL_V")) { for (i = 0; i < n; i++) printf(" %u", fl_notes[i]); printf("\n"); }
    snprintf(what, sizeof what, "FILL SNARE on a kit, 8 steps: %u hits (the rolls rising: 19)", n);
    bad += check(what, n == 19u);
    fl_setup(0, ENGI_DRUM, 1, 8);                   /* TOMS: down, a hit a step, the last ones doubled */
    n = fl_run(0, 8u * P);
    if (getenv("FL_V")) { for (i = 0; i < n; i++) printf(" %u", fl_notes[i]); printf("\n"); }
    snprintf(what, sizeof what, "FILL TOMS on a kit: %u hits (12)", n);
    bad += check(what, n == 12u);
    fl_setup(1, ENGI_ANALOG, 1, 8);                 /* a synth: UP the scale from the step's note, a note a step */
    n = fl_run(1, 8u * P);
    for (ok = n == 8u, i = 1; i < n && i < 8u; i++) ok &= fl_notes[i] > fl_notes[i - 1u];
    snprintf(what, sizeof what, "FILL UP on a synth: %u notes, each higher (%u .. %u)", n, fl_notes[0], fl_notes[n ? n - 1u : 0]);
    bad += check(what, ok && fl_notes[0] == 60u);
    fl_setup(1, ENGI_ANALOG, 0, 8);                 /* ROLL: x1 .. x4 */
    n = fl_run(1, 8u * P);
    snprintf(what, sizeof what, "FILL ROLL on a synth: %u notes (1 1 2 2 3 3 4 4 = 20)", n);
    bad += check(what, n == 20u);
    {   /* STAIR x1: each launch the next bank (fill_cnt) */
        uint32_t b0, b1;
        fill_cnt[0] = 0;
        b0 = fill_bank(0, 9); b1 = fill_bank(0, 9);
        ok = b0 == 0u && b1 == 1u;
        fill_cnt[0] = 0;
        b0 = fill_bank(0, 10); b1 = fill_bank(0, 10);
        ok &= b0 == 0u && b1 == 0u && fill_bank(0, 10) == 1u;
        bad += check("FILL STAIR: x1 a bank each launch, x2 every second", ok);
    }
    printf(bad ? "fill: %d FAILED\n" : "fill: all ok\n", bad);
    return bad != 0;
}
