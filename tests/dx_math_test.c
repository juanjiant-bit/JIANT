// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.74, the owner) DRUM-X's MATH modulator: a sound's WAVE past BELL a formula (F01..F48) as its FM's modulator
 * (COLOR its index): the hit not the sine's FM, each formula its own, 0 as before bit for bit; the project keeps it
 * ('D' 'F'), one without it none; SOUND 3's WAVE knob walks SINE FM METAL BELL F01 .. F48. Built and run by CI
 * (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t hit(uint32_t lane, uint8_t f)       /* lane's hit rendered alone, its hash */
{
    static dx_voice_t v;
    static int32_t y[CTL];
    uint32_t b, i, h = 2166136261u;
    uint8_t f0 = dx_fml[lane];
    memset(&v, 0, sizeof v);
    v.trig = 1;
    dx_fml[lane] = f;
    for (b = 0; b < 40u; b++) {
        dx_run_fx(&dx_kit[lane], &v, 0, 0, 0, 0, 0, 0, 0, y, CTL, 0);
        for (i = 0; i < CTL; i++) h = (h ^ (uint32_t)y[i]) * 16777619u;
    }
    dx_fml[lane] = f0;
    return h;
}
int main(void)
{
    int bad = 0, ok;
    uint32_t h0, h0b, h5, h9;
    ui_power_on();
    dx_kit[DV_TOM].a[DXP_COLOR] = dx_kit[DV_TOM].b[DXP_COLOR] = 80;
    dx_kit[DV_TOM].mode = (uint8_t)((dx_kit[DV_TOM].mode & ~3u) | DXW_FM);   /* (a formula: WAVE FM, as SOUND 3 sets it) */
    h0 = hit(DV_TOM, 0); h0b = hit(DV_TOM, 0); h5 = hit(DV_TOM, 5); h9 = hit(DV_TOM, 9);
    bad += check("DRUM-X MATH: no formula, the hit as before (twice the same)", h0 == h0b);
    bad += check("DRUM-X MATH: a formula changes the hit, each its own", h5 != h0 && h9 != h0 && h5 != h9);
    dx_frt[DV_TOM] = 6;                                 /* (V10.7.75) RATIO 3 */
    bad += check("DRUM-X MATH: its RATIO changes the hit", hit(DV_TOM, 5) != h5);
    dx_frt[DV_TOM] = 0;
    {
        memset(dx_fml, 0, sizeof dx_fml);
        dx_fml[DV_TOM] = 12; dx_fml[DV_KICK] = 48; dx_frt[DV_TOM] = 9;
        ok = !project_save(1);
        memset(dx_fml, 0, sizeof dx_fml); memset(dx_frt, 0, sizeof dx_frt);
        project_load(1); frames(4);
        ok &= dx_fml[DV_TOM] == 12 && dx_fml[DV_KICK] == 48 && dx_frt[DV_TOM] == 9;
        memset(dx_fml, 0, sizeof dx_fml);
        ok &= !project_save(2);
        dx_fml[DV_TOM] = 7;
        project_load(2); frames(4);
        ok &= !dx_fml[DV_TOM];
        bad += check("DRUM-X MATH: kept with the project ('D' 'F'); one without it: none", ok);
    }
    printf(bad ? "dx-math: %d FAILED\n" : "dx-math: all ok\n", bad);
    {   /* (V10.7.79, the owner) the MATH kits load their formulas (MATH LAB DRUM-X's sounds, GLITCH FM its own), any
         * other kit none; the DRUM-X DICE gives formulas to some sounds */
        uint32_t l, nf = 0, n0 = 0, nd = 0, r;
        dx_kit_load(12); for (l = 0; l < 8u; l++) nf += dx_fml[l] != 0 && (dx_kit[l].mode & 3u) == DXW_FM;
        dx_kit_load(11); for (l = 0; l < 8u; l++) nf += dx_fml[l] != 0;
        dx_kit_load(0); for (l = 0; l < 8u; l++) n0 += dx_fml[l] != 0;
        for (r = 0; r < 8u; r++) { sound_dice_of(&trk[3], 1); for (l = 0; l < 8u; l++) nd += dx_fml[l] != 0; }
        bad += check("DRUM-X MATH: MATH LAB and GLITCH FM their formulas, other kits none, DICE some", nf >= 15u && !n0 && nd >= 16u && nd < 56u);
    }
    return bad != 0;
}
