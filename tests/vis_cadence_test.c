/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.72, the owner: HOME > VISUAL choppy under a heavy GEN song) VISUAL under the sound's load: every frame the UI
 * paces (ui.c UI_PACE: 15 / 50 / 100 ms) drawn, evenly (V10.8.67 left frames out by the animation's ticks: 3 to 7 a
 * second, gaps of 150 .. 400 ms), each frame lighter instead (ui_lzhome.c vis_fig: its points, copies, the second
 * picture); its work a second under load a small part of the idle one; the animation still (FREEZE): not drawn again;
 * off the page: nothing of it runs.
 *   cc -O2 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests tests/vis_cadence_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <time.h>
static double run(uint32_t q8, uint32_t avg_pct, uint32_t *fps, uint32_t *gap)
{
    uint32_t ms, last = fm1_ms, drawn = 0, maxgap = 0, lastd = fm1_ms, g;
    clock_t c0;
    double cpu = 0;
    song.playing = 1;
    for (ms = 0; ms < 2000u; ms++) {                    /* (2 s to settle the UI's pace) */
        fm1_ms++; host_ticks += 1000u; song.cpu_q8 = (uint16_t)q8; load_avg_q8 = avg_pct << 8;
        if (fm1_ms - last >= UI_PACE()) { last = fm1_ms; ui_hold_poll(); ui_input(); ui_draw(); }
    }
    lastd = fm1_ms;
    for (ms = 0; ms < 10000u; ms++) {
        fm1_ms++; host_ticks += 1000u; song.cpu_q8 = (uint16_t)q8; load_avg_q8 = avg_pct << 8;
        if (fm1_ms - last >= UI_PACE()) {
            uint64_t s0 = ui_sent_px;
            last = fm1_ms;
            ui_hold_poll();
            c0 = clock();
            ui_input(); ui_draw();
            cpu += (double)(clock() - c0) / CLOCKS_PER_SEC;
            if (ui_sent_px != s0) { g = fm1_ms - lastd; maxgap = g > maxgap ? g : maxgap; lastd = fm1_ms; drawn++; }
        }
    }
    song.playing = 0;
    *fps = drawn / 10u; *gap = maxgap;
    return cpu * 100.0;                                 /* (ms a second) */
}
int main(void)
{
    static const uint16_t Q8[] = {0, 179, 179, 210, 210, 230}, AVG[] = {0, 40, 66, 50, 79, 88}, PACE[] = {15, 15, 50, 50, 100, 0};
    uint32_t i, fps, gap;
    int bad = 0, ok;
    double idle = 0, c;
    ui_power_on(); go_home(); frames(64);
    turn(EN_SELECT, 4); frames(64);
    ok = home_vis();
    for (i = 0; i < sizeof Q8 / sizeof Q8[0]; i++) {
        c = run(Q8[i], AVG[i], &fps, &gap);
        if (!i) idle = c;
        printf("vis: load %3u %% now, %2u %% on average: %2u frames a second, the longest gap %3u ms, %.2f ms of work a second (host)\n",
               Q8[i] * 100u / 256u, AVG[i], fps, gap, c);
        if (PACE[i]) ok &= fps + 1u >= 1000u / PACE[i] && gap <= PACE[i] + 1u && (!i || c <= idle * 0.25);
        else ok &= fps == 0u;                           /* (FREEZE: the animation still, the picture kept) */
    }
    bad += check("V10.8.72 VISUAL under load: every paced frame drawn, evenly; under load a quarter of the idle work at most; still while frozen", ok);
    {   /* off the page: nothing of VISUAL runs */
        uint32_t fr;
        turn(EN_SELECT, -1); frames(64);
        fr = vq.fr;
        frames(500);
        bad += check("V10.8.72 VISUAL off its page: its frame clock untouched (no cost)", !home_vis() && vq.fr == fr);
    }
    printf(bad ? "vis-cadence: %d FAILED\n" : "vis-cadence: all ok\n", bad);
    return bad != 0;
}
