// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.59) the MACROS' own matrix (mod.c mx_r, LFO > MACROS): the defaults at power-on, a project keeps changed routes
 * ('M' 'X') and one with the defaults writes none, a macro at 0 changes nothing (bit for bit), M1 up closes the filters,
 * the FX layer's KNOB 1..4 turn them and FX let go puts them back. Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static double rms_song(uint32_t blocks)
{
    static int32_t o[2 * CTL];
    double e = 0;
    uint32_t b, i;
    for (b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++) e += (double)o[i] * o[i];
    }
    return e;
}
static void sine_song(void)                     /* T3 (PHASE INIT SAW at power-on) a held C3, the others silent */
{
    uint32_t k;
    ui_power_on();
    for (k = 0; k < NTRK; k++) if (k != 2u) trk[k].p[P_MUTE] = 1;
    song.master_q12 = 2048;
    trk_note_on(&trk[2], 48, 100);
}
int main(void)
{
    int bad = 0, ok;
    double e0, e1, e2;
    ui_power_on();
    ok = mx_r[0].m == 0 && mx_r[0].d == MD_CUT && mx_r[0].a < 0 && mx_r[1].d == MD_REV && mx_r[2].d == MD_DLY &&
         mx_r[3].d == MD_DIST && !mx_r[4].d;
    bad += check("MACROS: the defaults (M1 the filters, M2 REV, M3 DLY, M4 DIST), routes 5..8 free", ok);
    {   /* a project: the defaults write no block; a changed route is kept, back as it was after power-on and a load */
        uint32_t n0, n1;
        project_t p;
        project_capture(&p);
        n0 = proj_packed_size(&p);
        mx_r[5].m = 2; mx_r[5].d = MD_PAN; mx_r[5].a = 33;
        project_capture(&p);
        n1 = proj_packed_size(&p);
        ok = n1 == n0 + 2u + sizeof p.mx && !project_save(1);   /* (0: saved) */
        if (!ok) printf("  sizes %u %u\n", n0, n1);
        mx_defaults();
        project_load(1); frames(4);
        if (mx_r[5].d != MD_PAN) printf("  after load: route 6 %d %d %d\n", mx_r[5].m, mx_r[5].d, mx_r[5].a);
        ok &= mx_r[5].m == 2 && mx_r[5].d == MD_PAN && mx_r[5].a == 33 && mx_r[0].d == MD_CUT;
        bad += check("MACROS: a project with the defaults writes no block; a changed route saved and loaded back", ok);
    }
    sine_song();
    e0 = rms_song(400);
    e1 = 0;
    bad += check("MACROS: every macro at 0: no route acts (mod_begin and macro_master as without them)", !mx_on && !mx_trk);
    sine_song();
    macro_v[0] = 127;
    e2 = rms_song(400);
    macro_v[0] = 0;
    {
        char what[120];
        snprintf(what, sizeof what, "MACROS: M1 full closes the filters (energy %.2f of the open one)", e2 / e0);
        bad += check(what, e2 < e0 * 0.6);
    }
    {   /* (V10.7.65, the owner) the PUNCH-IN EDITOR: FX + OCT+ opens it (DICE gone, no octave), a key held + KNOB 1 its
         * EFFECT, KNOB 4 its MODE, OCT+ saves the slot and plays it, OCT- leaves; the slot's keys then play so (LATCH: a
         * press on, the next off; QUANT: from the next beat); untouched keys as the map always had them */
        int8_t oc;
        uint32_t k, b, s0, def = 1;
        static int32_t o[2 * CTL];
        sine_song();
        memset(&favorites.factory[0][5], 0, 27); memset(&favorites.factory[2][5], 0, 27);   /* (slot A as at the factory) */
        memset(&favorites.factory[7][5], 0, 27);
        favorites.factory[6][18] = 0;
        pk.magic = 0; frame();
        for (k = 0; k < PK_KEYS; k++) def &= pk.e[k] == 0u;
        ok = def && pk.slot == 0u;
        s0 = key_at(0, 0);                              /* (F3: slot 0, RPT 1/8 at the factory) */
        ok &= perf_key(s0) == PF_R8;
        oc = song.octave;
        go_title("PUNCH IN"); frames(32);              /* (the owner: OCT+ on FX > PUNCH IN, not FX + OCT+) */
        btn_down(B_OCTUP); frames(32); btn_up(B_OCTUP); frames(32);
        ok &= pk.edit && fx_lock && song.octave == oc;
        key_down(s0); frames(32);
        turn(EN_K1, 1); frames(32);                     /* EFFECT: RPT 1/16 */
        turn(EN_K2, 1); frames(32);                     /* HOW: LATCH (V10.7.66: KNOB 2) */
        key_up(s0); frames(32);
        ok &= pk.we[0] == PF_R16 + 1u && ((pk.wo[0] >> 4) & 3u) == 1u && !(perf_held | perf_latched);   /* (nothing played) */
        btn_down(B_OCTUP); frames(32); btn_up(B_OCTUP); frames(32);   /* save slot 1 */
        ok &= pk.e[0] == PF_R16 + 1u && favorites.factory[0][5] == PF_R16 + 1u && pk.edit;
        btn_down(B_OCTDN); frames(32); btn_up(B_OCTDN); frames(32);   /* leave: the PUNCH IN page again */
        ok &= !pk.edit && !fx_lock && perf_key(s0) == PF_R16 && cur_page()->graph == GR_PUNCH && song.octave == oc;
        btn_down(B_FX); frames(480);
        key_down(s0); frames(32); key_up(s0); frames(32);
        ok &= (perf_latched >> PF_R16) & 1u;            /* LATCH: on, and stays */
        key_down(s0); frames(32); key_up(s0); frames(32);
        ok &= !((perf_latched >> PF_R16) & 1u);         /* .. the next press off */
        pk.o[0] = 2u << 4;                              /* QUANT: from the next beat while held */
        key_down(s0);
        for (b = 0; b < 4u; b++) mix_block(o, CTL);
        ok &= (pk.qpend >> PF_R16) & 1u;
        for (b = 0; b < 600u; b++) mix_block(o, CTL);   /* (a beat at 120: ~345 blocks) */
        ok &= (perf_held >> PF_R16) & 1u && !pk.qpend;
        key_up(s0); frames(32);
        ok &= !((perf_held >> PF_R16) & 1u);
        btn_up(B_FX); frames(64);
        if (!ok) printf("  editor: def %u edit %u we %u wo %x e %u held %x latched %x qpend %x\n", def, pk.edit, pk.we[0], pk.wo[0], pk.e[0], perf_held, perf_latched, pk.qpend);
        bad += check("PUNCH-IN EDITOR: OCT+ on FX > PUNCH IN opens, a key + KNOB 1 / 2 its effect and how, OCT+ saves, OCT- leaves; LATCH, QUANT play so", ok);
        memset(&favorites.factory[0][5], 0, 27); memset(&favorites.factory[2][5], 0, 27); pk.magic = 0; frame();
    }
    {   /* (V10.7.65) MOD: a key moves its macro (TARGET M2, RAMP UP over a beat) while held, as it was when let go;
         * SELECT in the editor picks the slot, OCT+ saves and plays it */
        uint32_t s0 = key_at(0, 0), mv, mx = 0, k;
        pk.magic = 0; frame();
        pk.e[0] = PK_MOD + 1u; pk.o[0] = 1u | 1u << 4;  /* M2, RAMP UP, +100 %, 1 BEAT (V10.7.66) */
        pk.x[0] = 10u | 2u << 5;
        macro_v[1] = 20; mv = macro_v[1];
        btn_down(B_FX); frames(480);
        key_down(s0);
        for (k = 0; k < 30u; k++) { frames(16); if (macro_v[1] > mx) mx = macro_v[1]; }
        key_up(s0); frames(48);
        ok = mx > 60u && macro_v[1] == mv;
        btn_up(B_FX); frames(64);
        go_title("PUNCH IN"); frames(32);
        btn_down(B_OCTUP); frames(32); btn_up(B_OCTUP); frames(32);
        turn(EN_SELECT, 1); frames(32);
        ok &= pk.edit && pk.eslot == 1u;
        btn_down(B_OCTUP); frames(32); btn_up(B_OCTUP); frames(32);
        ok &= pk.slot == 1u && favorites.factory[6][18] == 1u;
        btn_down(B_OCTDN); frames(32); btn_up(B_OCTDN); frames(32);
        if (!ok) printf("  mod: max %u now %u was %u; edit %u eslot %u slot %u\n", mx, macro_v[1], mv, pk.edit, pk.eslot, pk.slot);
        bad += check("PUNCH-IN MOD: a key moves its macro while held, back when let go; SELECT a slot, OCT+ plays it", ok);
        favorites.factory[6][18] = 0; macro_v[1] = 0; pk.magic = 0; frame();
    }
    {   /* (V10.7.66, the owner) a PARAM key on a parameter: CUT -100 % HOLD closes every track's filter while held, back
         * when let go */
        uint32_t s0 = key_at(0, 0);
        double ea, eb, ec;
        sine_song();
        pk.magic = 0; frame();
        pk.e[0] = PK_MOD + 1u; pk.o[0] = 4u; pk.x[0] = (uint8_t)((-10 & 31) | 4 << 5);
        ea = rms_song(200);
        btn_down(B_FX); frames(480);
        key_down(s0); frames(64);
        ok = pkr_v == 127u && pkr_d == MD_CUT && pkr_a < 0;
        eb = rms_song(200);
        key_up(s0); frames(64);
        ok &= !pkr_v;
        ec = rms_song(200);
        btn_up(B_FX); frames(64);
        ok &= eb < ea * 0.6 && ec > ea * 0.8;
        if (!ok) printf("  param: v %u d %u a %d  e %.0f %.0f %.0f\n", pkr_v, pkr_d, pkr_a, ea, eb, ec);
        bad += check("PUNCH-IN PARAM: CUT -100 % while held closes the filters, open again when let go", ok);
        pk.magic = 0; frame();
    }
    {   /* (V10.7.66) FX > PUNCH IN: KNOB 1..4 the macros; the editor: KNOB 1 past the effects into the parameters
         * (VIBRATO the last), +100 % and 1 BAR at first, KNOB 3 the amount */
        uint32_t s0 = key_at(0, 0), m0;
        ui_power_on();
        memset(&favorites.factory[0][5], 0, 27); memset(&favorites.factory[2][5], 0, 27); memset(&favorites.factory[7][5], 0, 27);
        pk.magic = 0; frame();
        go_title("PUNCH IN"); frames(32);
        m0 = macro_v[2];
        turn(EN_K3, 3); frames(32);
        ok = macro_v[2] > m0;
        btn_down(B_OCTUP); frames(32); btn_up(B_OCTUP); frames(32);
        key_down(s0); frames(32);
        turn(EN_K1, -1); frames(32);
        ok &= pk.edit && pk.we[0] == PK_MOD + 1u && (pk.wo[0] & 15u) == 13u && pk_amt(pk.wx[0]) == 10 && (pk.wx[0] >> 5) == 4u;
        turn(EN_K3, -3); frames(32);
        ok &= pk_amt(pk.wx[0]) == 7;
        key_up(s0); frames(32);
        btn_down(B_OCTDN); frames(32); btn_up(B_OCTDN); frames(32);
        ok &= !pk.edit;
        bad += check("FX > PUNCH IN: its knobs the macros; the editor's KNOB 1 into the parameters, KNOB 3 the amount", ok);
        macro_v[2] = 0; pk.magic = 0; frame();
    }
    printf(bad ? "macro-mx: %d FAILED\n" : "macro-mx: all ok\n", bad);
    return bad != 0;
}
