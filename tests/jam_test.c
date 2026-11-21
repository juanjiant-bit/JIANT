// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8, the owner's song mode) JAM: each track its sequence A..D. SEQ > STEP's KNOB 1 switches the selected track's;
 * the next one goes in on the step CAMBIO allows (AT ONCE: the next step; 1/4: a quarter's line), the playhead kept
 * where it was; the other tracks untouched; what the track played is put back into its part first (its edits kept);
 * an empty part an empty sequence; stopped, at once. Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void notes(uint32_t k, uint32_t base)            /* track k: every step a note, base + step */
{
    uint32_t i;
    track_defaults_steps(&trk[k]);
    trk[k].p[P_SLEN] = 16;
    for (i = 0; i < 16u; i++) trk[k].step[i] = (step_t){{(uint8_t)(base + i), 0, 0, 0}, 1, ST_NOTE, 0, 96, 0, 0};
}
static void blocks_until(int (*cond)(void), uint32_t most)
{
    int32_t o[2 * CTL];
    uint32_t i;
    for (i = 0; i < most && !cond(); i++) mix_block(o, CTL);
}
static int in_b(void) { return trk[1].step[0].note[0] == 70; }
int main(void)
{
    int bad = 0, ok;
    int32_t o[2 * CTL];
    uint32_t i, at, k;
    ui_power_on();
    song.master_q12 = 4096;
    for (k = 0; k < NTRK; k++) notes(k, 40);
    notes(1, 70); section_store(1);                     /* part B: T2 70.. */
    notes(1, 50); section_store(0);                     /* part A: T2 50.. (the music now) */
    memset(clip_now, 0, sizeof clip_now);
    song.sel = 1;
    go_title("STEP"); frame();
    ok = jam_cur(1) == 0u;
    turn(EN_K1, 1); frame();                             /* stopped: B at once */
    ok &= trk[1].step[0].note[0] == 70 && jam_cur(1) == 1u && trk[0].step[0].note[0] == 40;
    bad += check("JAM stopped: KNOB 1 on STEP puts T2's sequence B in at once, the other tracks as they were", ok);
    turn(EN_K1, -1); frame();                            /* back to A */
    ok = trk[1].step[0].note[0] == 50 && jam_cur(1) == 0u;
    bad += check("  .. and back to A", ok);

    favorites.factory[15][31] = (uint8_t)((3u ^ 1u) << 2);   /* CAMBIO 1/4 (MENU's byte) */
    transport_req = 1; ui_input();
    for (i = 0; i < 4000u && trk[1].seq_idx != 1u; i++) mix_block(o, CTL);   /* the second step */
    trk[1].step[5].note[0] = 99;                         /* an edit of A, playing */
    turn(EN_K1, 1); frame();
    ok = trk[1].step[0].note[0] == 50 && clip_go[1] == 2u;   /* staged, not in yet */
    blocks_until(in_b, 8000u);
    at = trk[1].seq_idx;
    ok &= in_b() && at % 4u == 0u && at != 0u;           /* in on a quarter's line, the playhead not back to 0 */
    bad += check("JAM playing, CAMBIO 1/4: the next sequence in on a quarter, the playhead kept (not from step 1)", ok);
    turn(EN_K1, -1); frame();                            /* to A: was B put back? A's edit kept? */
    blocks_until(in_b, 0u);
    for (i = 0; i < 8000u && trk[1].step[0].note[0] != 50; i++) mix_block(o, CTL);
    ok = trk[1].step[5].note[0] == 99;
    bad += check("  .. A's edit made while it played kept: it comes back with A", ok);

    favorites.factory[15][31] = (uint8_t)((0u ^ 1u) << 2);   /* AT ONCE */
    turn(EN_K1, 3); frame();                             /* D: empty */
    for (i = 0; i < 200u && clip_go[1]; i++) mix_block(o, CTL);
    for (ok = 1, k = 0; k < 16u; k++) ok &= !(trk[1].step[k].time == ST_NOTE && trk[1].step[k].n);
    ok &= jam_cur(1) == 3u && trk[0].step[0].note[0] == 40;
    bad += check("JAM AT ONCE: an empty part, an empty sequence, at the next step; T1 untouched", ok);
    transport_req = 2; ui_input();
    printf(bad ? "jam: %d FAILED\n" : "jam: all ok\n", bad);
    return bad != 0;
}
