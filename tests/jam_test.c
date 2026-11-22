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
    transport_req = 2; ui_input(); mix_block(o, CTL);

    {   /* (V10.8.1) the JAM take: REC from the next bar, T2 to B a 16th exact, REC again: its END on the bar line; LOCK
         * plays it round: T2 back to A at the start, to B on that 16th, every round */
        uint32_t at = 0, g = 0, rnd = 0, len;
        favorites.factory[15][31] = (uint8_t)((0u ^ 1u) << 2);   /* AT ONCE */
        turn(EN_K1, -3); frame();                        /* T2: A (stopped: at once) */
        nav_put(SEC_HOME, "JAM"); frame();
        transport_req = 1; ui_input();
        press(B_REC); frame();                           /* REC: armed */
        ok = live.srec == 1u;
        for (i = 0; i < 40000u && live.srec != 2u; i++) { mix_block(o, CTL); if (!(i & 63u)) frame(); }
        ok &= live.srec == 2u && live.nrec == NTRK;      /* (the take's start: the six tracks) */
        for (i = 0; i < 40000u && gl_step_at(0) - jam_t0 < 5u; i++) mix_block(o, CTL);
        jam_switch(1, 1);                                /* T2 to B at once (the next 16th) */
        for (i = 0; i < 4000u && clip_go[1]; i++) mix_block(o, CTL);
        at = JE_POS(live.jam[NTRK]);
        ok &= live.nrec == NTRK + 1u && JE_K(live.jam[NTRK]) == 1u && JE_S(live.jam[NTRK]) == 1u && at >= 5u && at <= 7u;
        for (i = 0; i < 40000u && gl_step_at(0) - jam_t0 < 20u; i++) mix_block(o, CTL);
        press(B_REC); frame();                           /* the take ends: 32 (two bars) */
        transport_req = 2; ui_input(); mix_block(o, CTL); frames(32);
        len = jam_len();
        ok &= !live.srec && len == 32u;
        bad += check("JAM REC: from the bar line, the tracks at the start, T2's change on its 16th, the END on the bar (32)", ok);
        turn(EN_K4, 1); frame();                         /* LOCK */
        song_poll(); frames(32);                         /* (song_poll: the main loop's, main.c) */
        ok = jam_lock && jam_cur(1) == 0u;               /* (stopped: the take's start, T2 on A) */
        transport_req = 1; ui_input();
        for (i = 0; i < 200000u && rnd < 3u; i++) {
            mix_block(o, CTL);
            if (!(i & 15u)) { frame(); song_poll(); }
            if (jam_cur(1) == 1u && !g) { g = (gl_step_at(0) - jam_base) % len; rnd++; ok &= g >= at && g <= at + 1u; }
            if (jam_cur(1) == 0u) g = 0;
        }
        ok &= rnd == 3u;
        bad += check("JAM LOCK: the take round and round, T2 to B on its 16th every round (and back to A)", ok);
        {   /* (V10.8.2) LOCK's REC: T3 again for a round (its change to C kept, the others as they were); the
             * player's over the take marked (VOS) */
            uint32_t n3 = 0, c3 = 0, sorted = 1, t2 = 0;
            track_select(2); frame();
            press(B_REC); frame();
            ok = jam_tr == 3u;
            for (i = 0; i < 200000u && !(jam_tr & 8u); i++) { mix_block(o, CTL); if (!(i & 15u)) { frame(); song_poll(); } }
            ok &= (jam_tr & 8u) != 0u;
            for (i = 0; i < 40000u && (gl_step_at(0) - jam_base) % len != (jam_r0 + 6u) % len; i++) mix_block(o, CTL);
            jam_switch(2, 2);                            /* T3 to C, recorded */
            for (i = 0; i < 400000u && jam_tr; i++) { mix_block(o, CTL); if (!(i & 15u)) { frame(); song_poll(); } }
            ok &= !jam_tr && jam_len() == 32u && live.rec_done;
            for (i = 0; i < JAM_N && live.jam[i] && JE_K(live.jam[i]) != 7u; i++) {
                if (i && JE_POS(live.jam[i]) < JE_POS(live.jam[i - 1u])) sorted = 0;
                if (JE_K(live.jam[i]) == 2u) { n3++; c3 |= JE_S(live.jam[i]) == 2u; ok &= JE_POS(live.jam[i]) < 32u; }
                if (JE_K(live.jam[i]) == 1u && JE_POS(live.jam[i]) == at) t2 = 1;
            }
            ok &= n3 >= 2u && c3 && sorted && t2;
            bad += check("JAM LOCK REC: T3 again for a round, its change to C in the take, T2's kept, in time order", ok);
            jam_switch(0, 3);                            /* the player's T1 over the take */
            ok = jam_vos == 1u;                         /* (only the player's: the take's start is not) */
            bad += check("JAM LOCK: the player's sequence over the take marked (VOS)", ok);
        }
        transport_req = 2; ui_input(); mix_block(o, CTL); frames(8);
        {   /* (V10.8.2) TAKE: T2's change a 16th later, to C, then out; the take kept when the page is left */
            uint32_t e = jam_end(), j;
            jam_lock = 0;
            nav_put(SEC_HOME, "TAKE"); frame();
            ok = SONG_TAKE(cur_page());
            for (j = 0; j < e && !(JE_K(live.jam[j]) == 1u && JE_POS(live.jam[j])); j++) {}
            take_i = (uint8_t)j;
            turn(EN_K2, 1); frame();
            ok &= JE_K(live.jam[take_i]) == 1u && JE_POS(live.jam[take_i]) == at + 1u;
            turn(EN_K3, 1); frame();
            ok &= JE_S(live.jam[take_i]) == 2u;
            press(B_OCTDN); frame();
            for (j = 0; j < jam_end(); j++) ok &= !(JE_K(live.jam[j]) == 1u && JE_POS(live.jam[j]));
            ok &= jam_end() == e - 1u && jam_len() == 32u;
            nav_put(SEC_HOME, "JAM"); frame(); song_poll();
            ok &= !live.rec_done;
            bad += check("TAKE: a change moved a 16th, its sequence changed, then out; kept when the page is left", ok);
            nav_put(SEC_HOME, "TAKE"); frame();
            e = jam_end(); take_i = 0;                   /* (T1's start) */
            press(B_OCTUP); frame();                     /* OCT+: a new change a beat later, the next sequence */
            ok = jam_end() == e + 1u && JE_K(live.jam[take_i]) == JE_K(live.jam[0]) && JE_POS(live.jam[take_i]) == 4u;
            turn(EN_K4, -1); frame();                    /* the take a bar shorter: 16 */
            ok &= jam_len() == 16u;
            for (j = 0; j < jam_end(); j++) ok &= JE_POS(live.jam[j]) < 16u;
            bad += check("TAKE: OCT+ a new change a beat on, KNOB 4 the take's bars (what falls past it goes)", ok);
        }
        jam_lock = 0;
    }
    printf(bad ? "jam: %d FAILED\n" : "jam: all ok\n", bad);
    return bad != 0;
}
