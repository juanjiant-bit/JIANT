/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z8) PROX / AIR calibration with a noisy RSSI (a Discord user: "near / far jump chaotically"). MENU > SYSTEM >
 * BLUETOOTH's PROX page takes FAR / NEAR as the median of ~2.5 s of the level (ui_menu.c bt_cal_tick, "HOLD STILL" and
 * its progress on the page), not one reading; AIR's level is smoothed longer and held within a level before PROX maps
 * it (bt.c bt_prox_step), so MS_PROX does not twitch with the advertisements' noise.
 *   cc -O1 -g -w -Wno-implicit-function-declaration -fsanitize=address,bounds -Ibuild/gen -Ifirmware/src \
 *      tests/prox_cal_test.c -lm -o build/song-check/prox-cal */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#ifndef AMP
#define AMP 16
#endif
#define LV(x) (uint16_t)(((x) / 2u) | ((x) - (x) / 2u) << 8)
static uint32_t nz = 12345u;
static int32_t noise(int32_t amp)                       /* a rough bell (four uniforms), +- amp */
{
    int32_t s = 0, i;
    for (i = 0; i < 4; i++) {
        nz = nz * 1103515245u + 12345u;
        s += (int32_t)(nz >> 16 & 1023u) - 512;
    }
    return s * amp / 2048;
}
/* one packet as AIR hears them: the phone's level `lv` with an advertisement's noise (+- amp; a flutter of +- amp / 2
 * over ~0.6 s, the hand's and the room's reflections; an outlier of +- 30 one time in eight) */
static void feed(int32_t lv, int32_t amp, uint32_t now)
{
    uint16_t w[1];
    int32_t x = lv + noise(amp), ph = (int32_t)(now % 600u);
    x += (ph < 300 ? ph - 150 : 450 - ph) * amp / 2 / 150;
    nz = nz * 1103515245u + 12345u;
    if ((nz >> 20) % 8u == 0u)
        x += (nz >> 8 & 1u) ? 30 : -30;
    x = x < 0 ? 0 : x > 126 ? 126 : x;
    w[0] = LV((uint32_t)x);
    bt_prox_step(w, 1, 1, now);
}
/* ms of a level: a packet every 10 ms, a UI frame every 16 (bt_cal_tick, as menu_input runs it) */
static void run_ms(int32_t lv, int32_t amp, uint32_t ms)
{
    uint32_t end = fm1_ms + ms, next = fm1_ms;
    while (fm1_ms < end) {
        fm1_ms += 2u;
        if (fm1_ms >= next) { feed(lv, amp, fm1_ms); next += 10u; }
        if (!(fm1_ms % 16u)) bt_cal_tick();
    }
}

int main(void)
{
    int bad = 0, ok;
    uint32_t i, take, n;
    int32_t lo, hi, one_lo = 999, one_hi = -999, med_lo = 999, med_hi = -999, v;
    ui_power_on();
    memset(&favorites.factory[6][19], 0, 13);
    memset(&bp, 0, sizeof bp);
    memset(&bpc, 0, sizeof bpc);
    bt_set(2);                                          /* AIR (the scanner): the noisiest */
    bt_up = BT_RUN;
    ui.menu = 3;
    run_ms(80, AMP, 1000);
    /* 1. a point taken: HOLD STILL, its progress on the page, then the median */
    bt_prox_input(0, 1);                                /* OCT-: FAR */
    ok = bpc.on == 1u && msg_is("HOLD STILL...") && favorites.factory[6][22] == 0u;
    run_ms(80, AMP, 1200);
    ui.force = 1; ui_draw();
    ok &= bpc.on == 1u && bpc.n >= 25u;                 /* (half way: still taking) */
    {
        uint32_t s0 = bt_prox_sig();
        run_ms(80, AMP, 300);
        ok &= bt_prox_sig() != s0;                      /* (the page redraws its progress) */
    }
    run_ms(80, AMP, 1200);
    ok &= !bpc.on && msg_is("PROX FAR SET") && abs((int32_t)favorites.factory[6][22] - 160) <= 2;
    bad += check("V10.6z8 PROX: OCT- takes FAR over ~2.5 s (HOLD STILL, the progress), the median of the noisy level", ok);
    /* 2. again and again, the same place: the median against one reading (the press's, as before V10.6z8); AIR and a
     * central's link (PROX: smoothed less, its single readings noisier) */
    for (n = 0; n < 2u; n++) {
        bt_set(n ? 1u : 2u);
        one_lo = med_lo = 999; one_hi = med_hi = -999;
        for (take = 0; take < 24u; take++) {
            run_ms(80, AMP, 700 + 37u * take);
            v = (bp.lvl + 64) >> 7;                     /* (what a press stored before) */
            one_lo = v < one_lo ? v : one_lo; one_hi = v > one_hi ? v : one_hi;
            bt_prox_input(0, 1);
            run_ms(80, AMP, BPC_MS + 100u);
            v = favorites.factory[6][22];
            med_lo = v < med_lo ? v : med_lo; med_hi = v > med_hi ? v : med_hi;
        }
        printf("prox: %s, 24 FAR takes at level 80 (x2 160), noise +-%d, flutter, outliers: one reading %d..%d, median %d..%d\n",
               n ? "PROX" : "AIR", AMP, (int)one_lo, (int)one_hi, (int)med_lo, (int)med_hi);
        bad += check(n ? "V10.6z8 PROX: 24 takes of one place within 2 levels (the median), tighter than one reading"
                       : "V10.6z8 AIR: 24 takes of one place within 2 levels (the median), no wider than one reading",
                     med_hi - med_lo <= 4 && (n ? med_hi - med_lo < one_hi - one_lo : med_hi - med_lo <= one_hi - one_lo) &&
                     abs(med_lo - 160) <= 4 && abs(med_hi - 160) <= 4);
    }
    bt_set(2);
    /* 3. NEAR, then the map; too close refused as before */
    run_ms(40, AMP, 5000);
    bt_prox_input(1, 0);
    run_ms(40, AMP, BPC_MS + 100u);
    ok = msg_is("PROX NEAR SET") && abs((int32_t)favorites.factory[6][23] - 80) <= 4;
    run_ms(42, AMP / 2, 5000);
    bt_prox_input(0, 1);                                /* FAR taken at the NEAR place: too close */
    run_ms(42, AMP / 2, BPC_MS + 100u);
    ok &= msg_is("FAR AND NEAR TOO CLOSE");
    favorites.factory[6][22] = 160;
    bad += check("V10.6z8 PROX: NEAR taken the same way; FAR at NEAR's place still refused as too close", ok);
    /* 4. no packets while it takes: NOT CONNECTED, nothing stored; away from the page long: dropped */
    bt_prox_input(1, 0);
    bp.conn = 0;
    for (i = 0; i < (BPC_MS + 100u) / 16u; i++) { fm1_ms += 16u; bt_cal_tick(); }
    ok = msg_is("NOT CONNECTED") && !bpc.on && abs((int32_t)favorites.factory[6][23] - 80) <= 2;
    bp.conn = 1;
    bt_prox_input(1, 0);
    fm1_ms += BPC_MS + 2000u;
    ui_message("");
    bt_cal_tick();
    ok &= !bpc.on && !msg_is("PROX NEAR SET");
    bad += check("V10.6z8 PROX: no level while taking: NOT CONNECTED, nothing stored; left for long: dropped", ok);
    /* 5. AIR's level as MS_PROX reads it: half way between the points, the noise */
    favorites.factory[6][22] = 160; favorites.factory[6][23] = 80;
    for (n = 0; n < 2u; n++) {
        bt_set(n ? 1u : 2u);
        run_ms(60, AMP, 4000);
        lo = 99999; hi = -1;
        for (i = 0; i < 600u; i++) {                    /* 6 s */
            run_ms(60, AMP, 10);
            lo = mod_prox < lo ? mod_prox : lo; hi = mod_prox > hi ? mod_prox : hi;
        }
        printf("prox: %s at level 60 (half way), noise, flutter, outliers: MS_PROX %d..%d (%d %% of the range)\n",
               n ? "a central's link (PROX)" : "AIR", (int)lo, (int)hi, (int)((hi - lo) * 100 / 32767));
        if (!n) { one_lo = lo; one_hi = hi; }
        else { med_lo = lo; med_hi = hi; }
    }
    bad += check("V10.6z8 AIR: MS_PROX steady near half way (within 10 % of its range), steadier than a central's PROX",
                 one_hi - one_lo < 32767 / 10 && one_hi - one_lo < med_hi - med_lo && one_lo > 13000 && one_hi < 21000);
    bt_set(2);
    run_ms(40, AMP, 4000);                                /* a hand moved in: it follows (NEAR) */
    ok = mod_prox > 30000;
    run_ms(100, AMP, 4000);                               /* .. and away (past FAR) */
    ok &= mod_prox < 2000;
    bad += check("V10.6z8 AIR: the smoothed, held level still follows a move within ~4 s, both ways", ok);
    {   /* 5b. (V10.7.37, a user: PROX too slow) SLEW on a central's link: a clean step from FAR to NEAR, a packet every 10
         * ms; the time to 90 % per SLEW (0: a packet or two), always bounded; a SPEED of before reads as its SLEW; SENS
         * widens PROX about the middle (0: the map as before) */
        static const uint8_t SL[4] = {0, 30, 60, 100};
        uint32_t ms[4], k, t;
        int32_t lo2 = 0, hi2 = 0, m0, m7;
        uint16_t w1[1];
        bt_set(1);
        for (k = 0; k < 4u; k++) {
            favorites.factory[6][21] = (uint8_t)(0x80u | SL[k]);
            w1[0] = LV(80u);                                /* (FAR: rest there) */
            for (t = 0; t < 300u; t++) bt_prox_step(w1, 1, 1, fm1_ms += 10u);
            w1[0] = LV(40u);                                /* (NEAR, at once) */
            for (t = 0; t < 400u && mod_prox < 29490; t++) {
                bt_prox_step(w1, 1, 1, fm1_ms += 10u);
                lo2 = mod_prox < lo2 ? mod_prox : lo2; hi2 = mod_prox > hi2 ? mod_prox : hi2;
            }
            ms[k] = (t + 1u) * 10u;
        }
        printf("prox: SLEW 0 30 60 100 %%: a step to 90 %% in %u %u %u %u ms\n", ms[0], ms[1], ms[2], ms[3]);
        ok = ms[0] <= 30u && ms[0] < ms[1] && ms[1] < ms[2] && ms[2] < ms[3] && ms[3] >= 300u && lo2 >= 0 && hi2 <= 32767;
        favorites.factory[6][21] = 2;                       /* (SPEED 3 of before: 1/8) */
        ok &= bt_slew() == 60;
        favorites.factory[6][21] = 0;
        ok &= bt_slew() == 100;
        bad += check("V10.7.37 PROX SLEW: 0 follows in a packet or two, slower up to 100 (as before), bounded; SPEED read as SLEW", ok);
        m0 = bt_prox_map(55 << 8);                          /* (a little nearer than the middle, 60) */
        favorites.factory[6][19] = 7u << 5;
        m7 = bt_prox_map(55 << 8);
        ok = m7 > m0 && m7 - 16384 >= (m0 - 16384) * 4 && bt_prox_map(40 << 8) == 32767 && bt_prox_map(80 << 8) == 0 && BT_MIN() == 0;
        favorites.factory[6][19] = 0;
        printf("prox: SENS 0 %d, SENS 7 %d (a level of 55, FAR 80 NEAR 40)\n", (int)m0, (int)m7);
        bad += check("V10.7.37 PROX SENS 7: x4.5 about the middle (a small move sweeps it), the ends held; MIN kept", ok);
    }
    bt_up = 0; ui.menu = 0;
    /* 6. (V10.6z8, a Discord user) GLO > SYSTEM 2 shows USB, TRS and BT; its knobs say where BT is edited, change nothing */
    {
        int16_t g[G_COUNT];
        ui_power_on();
        ok = nav_put(SEC_GLO, "PORTS");
        frame();
        memcpy(g, song.g, sizeof g);
        for (i = 0; i < 4u; i++) {
            ui_message("");
            turn(EN_K1 + i, 1);
            ok &= msg_is("EDIT: MENU > BLUETOOTH") && !memcmp(g, song.g, sizeof g) && !ui.menu;
        }
        bad += check("V10.6z8 GLO > PORTS (was SYSTEM 2): any knob says EDIT: MENU > BLUETOOTH and changes nothing", ok);
    }
    printf("prox cal: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
