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
    h0 = hit(DV_TOM, 0); h0b = hit(DV_TOM, 0); h5 = hit(DV_TOM, 5); h9 = hit(DV_TOM, 9);
    bad += check("DRUM-X MATH: no formula, the hit as before (twice the same)", h0 == h0b);
    bad += check("DRUM-X MATH: a formula changes the hit, each its own", h5 != h0 && h9 != h0 && h5 != h9);
    {
        memset(dx_fml, 0, sizeof dx_fml);
        dx_fml[DV_TOM] = 12; dx_fml[DV_KICK] = 48;
        ok = !project_save(1);
        memset(dx_fml, 0, sizeof dx_fml);
        project_load(1); frames(4);
        ok &= dx_fml[DV_TOM] == 12 && dx_fml[DV_KICK] == 48;
        memset(dx_fml, 0, sizeof dx_fml);
        ok &= !project_save(2);
        dx_fml[DV_TOM] = 7;
        project_load(2); frames(4);
        ok &= !dx_fml[DV_TOM];
        bad += check("DRUM-X MATH: kept with the project ('D' 'F'); one without it: none", ok);
    }
    printf(bad ? "dx-math: %d FAILED\n" : "dx-math: all ok\n", bad);
    return bad != 0;
}
