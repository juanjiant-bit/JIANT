// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.8, the owner) SEQ > STEP FX's PLAY: OCT+ on COMP turns the white keys into components held live (seq.c hc_run):
 * held, the selected track plays it from its next step; let go, the pattern again; REC armed and playing, the step it
 * began on gets it (VALUE, LENGTH the steps held), in C1, or C2 when C1 holds another. Built and run by CI
 * (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t wkey(uint32_t p)                       /* the key of white place p */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) == p)
            return k;
    return 0;
}
static void steps(uint32_t n)                           /* n steps of track 1 played, the UI's frames between */
{
    int32_t o[2 * CTL];
    uint32_t i, at = trk[0].seq_idx;
    for (i = 0; i < 200000u && n; i++) {
        mix_block(o, CTL);
        if (!(i & 15u)) frame();
        if (trk[0].seq_idx != at) { at = trk[0].seq_idx; n--; }
    }
    frame();
}
static void hold_key(uint32_t p) { fm1_in.notes |= 1u << wkey(p); frame(); }
static void let_go(uint32_t p) { fm1_in.notes &= ~(1u << wkey(p)); frame(); }
int main(void)
{
    int bad = 0, ok;
    uint32_t i;
    ui_power_on();
    song.master_q12 = 4096;
    song.sel = 0;
    track_defaults_steps(&trk[0]);
    trk[0].p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++) trk[0].step[i] = (step_t){{(uint8_t)(48 + i), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
    go_title("COMP"); frame();
    ok = cur_page()->graph == GR_COMP && !sfx_play;
    press(B_OCTUP); frame();
    ok &= sfx_play == 1u && song.octave == 0;
    bad += check("STEP FX: OCT+ turns PLAY on (no octave)", ok);

    transport_req = 1; ui_input();
    steps(2);
    ok = trk[0].cx_oct == 0;
    hold_key(10);                                       /* OCT+ held */
    ok &= hc_act == CA_OCTU && sfx_key == 10u;
    steps(2);
    ok &= trk[0].cx_oct == 12;
    let_go(10);
    steps(2);
    ok &= trk[0].cx_oct == 0 && hc_act == 0u && !comp_any(&trk[0].step[trk[0].seq_idx % 16u]);
    bad += check("PLAY: a key held plays its component (OCT+) from the next step, let go the pattern again, nothing written (no REC)", ok);

    song.rec = 1;                                       /* REC armed, playing: recorded */
    hold_key(4);                                        /* RATCH x3 */
    steps(1);
    {
        uint32_t i0 = hc_i0;
        steps(2);
        ok = trk[0].cx_rat == 3u;
        let_go(4);
        steps(2);
        ok &= CO_ACT(trk[0].step[i0].comp) == CA_RATCH && trk[0].step[i0].carg == 1u && trk[0].step[i0].clen[0] >= 1u &&
              trk[0].step[i0].clen[0] <= 4u && !CO_ACT(trk[0].step[i0].comp2);
        bad += check("REC: the component held written into the step it began on, its VALUE and its LENGTH (the steps held)", ok);

        for (i = 0; i < 16u; i++)                       /* every step's C1 another component (SOUND) */
            if (i != i0) trk[0].step[i].comp = CO_MAKE(CA_SOUND, CC_ALWAYS), trk[0].step[i].carg = 0, trk[0].step[i].clen[0] = 0;
        hold_key(11);                                   /* OCT- held: C1 busy, so into C2 */
        steps(1);
        i0 = hc_i0;
        steps(1);
        let_go(11);
        steps(2);
        ok = (CO_ACT(trk[0].step[i0].comp) == CA_SOUND || CO_ACT(trk[0].step[i0].comp) == CA_RATCH) &&   /* C1 kept */
             CO_ACT(trk[0].step[i0].comp2) == CA_OCTD && trk[0].step[i0].clen[1] >= 1u;
        bad += check("REC: C1 holding another component, the new one goes into C2 (they add up)", ok);
    }
    song.rec = 0;
    press(B_OCTUP); frame();
    ok = !sfx_play && hc_act == 0u;
    bad += check("STEP FX: OCT+ again, the keys pick the step", ok);
    if (bad) printf("stepfx: %d failed\n", bad);
    else printf("stepfx: all passed\n");
    return bad != 0;
}
