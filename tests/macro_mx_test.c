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
    printf(bad ? "macro-mx: %d FAILED\n" : "macro-mx: all ok\n", bad);
    return bad != 0;
}
