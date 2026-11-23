// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.8, the owner) SEQ > STEP FX (COMP / COMP 2; V10.8.9: always, PRESETS picks the step): the white keys hold components live (seq.c hc_run):
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
static uint16_t spx(int32_t x, int32_t y) { return swap16(host_screen[y * 240 + x]); }
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
    {
        uint32_t p0 = TSEL->preset, c0 = ui.cursor;
        ok = cur_page()->graph == GR_COMP;
        turn(EN_PRESET, 1); frame();
        ok &= ui.cursor == c0 + 1u && TSEL->preset == p0 && cur_page()->graph == GR_COMP;
        fm1_in.notes |= 1u << wkey(3); host_notes |= 1u << wkey(3); frame();   /* a white key: no step picked */
        ok &= ui.cursor == c0 + 1u && sfx_key == 3u;
        fm1_in.notes = 0; frame();
        turn(EN_PRESET, -1); frame();
        ok &= ui.cursor == c0 && sfx_key == 16u;
        bad += check("STEP FX: PRESETS picks the step (no sound loaded), a white key holds a component (no step picked)", ok);
    }

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
    {   /* (V10.8.9) the banner: the component held large over the graph, its key, REC */
        uint32_t by = graph_y() + graph_h() / 2u - 24u;
        hold_key(1); frame();
        ok = str_eq(SFX_NM[sfx_key], "STUTTER /2") && sfx_k == wkey(1) && key_place(sfx_k) + 1u == 2u &&
             spx(120, (int32_t)by) == T_RAISE && rec_on(TSEL);
        let_go(1); frame(); frame();
        ok &= sfx_key == 16u && spx(120, (int32_t)by) == T_RAISE;   /* (V10.8.13: 0.6 s more) */
        frames(700);
        ok &= spx(120, (int32_t)by) != T_RAISE;
        bad += check("STEP FX banner: STUTTER /2, KEY 2, REC while held; 0.6 s after it is let go, then gone", ok);
    }
    song.rec = 0;
    {   /* (V10.8.9) the FX layer's punch-in held: its banner */
        uint32_t by = graph_y() + graph_h() / 2u - 24u;
        char nm[12];
        go_title("STEP"); frame();
        btn_down(B_FX); key_down(white(2)); frame(); frame();
        ok = ui.layer == LAYER_FX && fx_hk == white(2) + 1u;
        ly_keyname(LAYER_FX, 0, 2, nm);
        ok &= str_eq(nm, "RPT 1/32") && spx(120, (int32_t)by) == T_RAISE;
        key_up(white(2)); frame(); frame();
        ok &= !fx_hk && spx(120, (int32_t)by) == T_RAISE;            /* (V10.8.13: 0.6 s more) */
        frames(700);
        ok &= spx(120, (int32_t)by) != T_RAISE;
        key_down(white(2)); frame(); frame();                          /* latched: the key let go, the effect on */
        key_up(white(2)); perf_latched |= PF_BIT(PF_R32); frame(); frames(700);
        ok &= spx(120, (int32_t)by) != T_RAISE;
        perf_latched = 0;
        btn_up(B_FX); frame();
        bad += check("FX layer banner: the punch-in held (RPT 1/32) large with its key; 0.6 s after, then gone, latched too", ok);
    }
    {   /* (V10.8.9) the song mode's hints: one a state, each fits the line; HELP OFF: none */
        const char *h[6];
        uint8_t ho = ui_help_off;
        stop_transport(); frame();
        nav_put(SEC_HOME, "JAM"); frame();
        memset(live.jam, 0, sizeof live.jam); jam_lock = 0; jam_tr = 0; jam_undo = 0; live.srec = 0;
        h[0] = song_hint();
        live.srec = 1; h[1] = song_hint();
        live.srec = 2; h[2] = song_hint();
        live.srec = 0; jam_lock = 1; h[3] = song_hint();
        jam_undo = 1; h[4] = song_hint();
        jam_undo = 0; jam_lock = 0;
        nav_put(SEC_HOME, "TAKE"); frame();
        h[5] = song_hint();
        ok = h[0] && str_eq(h[0], "PLAY \xB7 KEYS A-D ALL \xB7 OCT+ GO \xB7 OCT- STOP") && str_eq(h[1], "REC: FROM THE NEXT BAR") &&
             str_eq(h[2], "RECORDING: CHANGE SEQS \xB7 REC ENDS") && str_eq(h[3], "PLAY OVER IT: YOU \xB7 REC: REDO A TRACK") &&
             str_eq(h[4], "OCT- UNDO THE ROUND") && str_eq(h[5], "NO TAKE: REC ONE ON JAM");
        for (i = 0; i < 6u; i++) ok &= h[i] && text_w(&AF_X, h[i]) <= 230;
        ui_help_off |= 1u;
        ok &= !song_hint();
        nav_put(SEC_HOME, "JAM"); frame();
        ok &= !song_hint();
        ui_help_off = ho;
        bad += check("song mode hints: JAM stopped / REC armed / recording / LOCK / UNDO, TAKE; they fit; HELP OFF none", ok);
        go_title("COMP"); frame();
        transport_req = 1; ui_input(); steps(1);
    }
    hold_key(4); steps(1);
    go_title("STEP"); frame();
    ok = hc_act == 0u && sfx_key == 16u;
    let_go(4);
    bad += check("STEP FX: off the page, nothing held", ok);
    {   /* (V10.8.10) the track's bar: TRACK N / ENGINE on its colour on the track's pages; not on HOME, GLO, SAVE */
        static const uint16_t C[6] = {0xFE87u, 0xFC45u, 0xF9C7u, 0xF9D6u, 0xA2FFu, 0x3C7Fu};
        char tt[40];
        uint32_t k;
        stop_transport(); song.rec = 0;
        ok = 1;
        for (k = 0; k < 6u; k++) {
            track_select(k); go_title("ENV"); frames(2);
            ui.msg_t = 0; ui.force = 1; ui_draw();
            ok &= head_trk() && head_title(tt) == C[k] && trk_col(k) == C[k] && !memcmp(tt, "TRACK ", 6) && !memcmp(tt + 7, " / ", 3) &&
                  tt[6] == (char)('1' + k) && str_eq(tt + 10, trk_engp(TSEL)->name) &&
                  swap16(host_screen[(Y_HEAD + HEAD_MID) * 240 + 192]) == C[k] &&
                  swap16(host_screen[(Y_HEAD + HEAD_MID) * 240 + HEAD_TK_X + 1]) == C[k];
        }
        track_select(0); go_title("STEP"); frames(2); ok &= head_trk();
        go_title("COMP"); frames(2); ok &= head_trk();
        go_title("MIXER"); frames(2); ok &= !head_trk();
        go_home(); frames(2); ok &= !head_trk();
        nav_put(SEC_HOME, "JAM"); frames(2); ok &= !head_trk();
        go_title("GLOBAL"); frames(2); ok &= !head_trk();
        bad += check("track bar: TRACK N / ENGINE on T1..T6's colours (header and the number's box); none on HOME, GLO, JAM", ok);
    }
    {   /* (V10.8.14) SCL > CHORD's names: the triads of D major, "-" minor, the degree sign diminished */
        static const uint8_t PC[7] = {0, 2, 4, 5, 7, 9, 11};
        static const char *const W[7] = {"D", "E-", "F#-", "G", "A", "B-", "C#\xB0"};
        char nm[4];
        for (ok = 1, i = 0; i < 7u; i++) { chord_nm(nm, PC, i, 2); ok &= str_eq(nm, W[i]) && text_w(&AF_M, nm) <= 28; }
        bad += check("SCL chords: D major's seven named (D E- F#- G A B- C#dim), each fits its card", ok);
    }
    if (bad) printf("stepfx: %d failed\n", bad);
    else printf("stepfx: all passed\n");
    return bad != 0;
}
