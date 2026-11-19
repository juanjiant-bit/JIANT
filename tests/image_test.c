// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.60) IMAGE on a part not in POLY: a spread (fx.c mix_spread), wide in stereo (L and R apart) and gone in a
 * mono sum (L + R as with IMAGE 0, within rounding). Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int32_t L0[400 * CTL], R0[400 * CTL], L1[400 * CTL], R1[400 * CTL];
static void run_img(int32_t simg, int32_t *L, int32_t *R)
{
    static int32_t o[2 * CTL];
    uint32_t k, b, i;
    ui_power_on();
    song.master_q12 = 2048;
    for (k = 0; k < NTRK; k++) if (k != 2u) trk[k].p[P_MUTE] = 1;
    trk[2].p[P_VOICE] = V_MONO;
    trk[2].p[P_SIMG] = (int16_t)simg;
    trk[2].p[P_REV] = trk[2].p[P_DLY] = trk[2].p[P_CHOR] = 0;
    trk_note_on(&trk[2], 48, 100);
    for (b = 0; b < 400u; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) { L[b * CTL + i] = o[2 * i]; R[b * CTL + i] = o[2 * i + 1]; }
    }
}
int main(void)
{
    int bad = 0;
    double side = 0, mid = 0, err = 0;
    uint32_t i;
    char what[160];
    run_img(127, L0, R0);                       /* (stored inverted: 127 = 0 %) */
    run_img(0, L1, R1);                         /* IMAGE 100 % */
    for (i = 8000; i < 400u * CTL; i++) {
        double d = (double)L1[i] - R1[i], m = (double)L0[i] + R0[i], e = (double)L1[i] + R1[i] - m;
        side += d * d; mid += m * m; err += e * e;
    }
    snprintf(what, sizeof what, "IMAGE on a MONO part: L and R apart (side %.3f of the mid), the mono sum kept (error %.5f)",
             side / mid, err / mid);
    bad += check(what, side / mid > 0.02 && err / mid < 0.02);
    printf(bad ? "image: %d FAILED\n" : "image: all ok\n", bad);
    return bad != 0;
}
