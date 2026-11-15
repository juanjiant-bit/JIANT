/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.37) screenshots for the notes (not a test: run_tests.sh does not run it), through ui_render.c's scenes:
 *   sed 's/^int main(/int ui_render_main(/' tests/ui_render.c > build/host/ui_render_lib.c
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -Ibuild/host -o build/host/west_frames tests/west_frames.c -lm
 *   build/host/west_frames DIR        (PPMs: DIR/ppm/JIANT_*.ppm; tests/ui_render.py-style conversion by the caller)
 * WEST's circuit on EDIT's whole panel, struck and ringing down (a frame at 20, 150, 400, 900 ms after the note and in
 * silence), with FOLD and MOD up, and DUST; each page of the DRUM-X chain on the factory kit. */
#include "ui_render_lib.c"                               /* (ui_render.c, its main renamed: the command below) */
static const char *out_dir;
static void west_at(const int16_t *k, uint32_t age, int32_t peak, const char *name)
{
    uint32_t i;
    setup(S_V6_WEST_FULL);
    for (i = 0; i < 8u; i++)
        TSEL->p[P_E0 + i] = k[i];
    vxs.nms[6] = fm1_ms - age;
    TSEL->peak = peak;
    if (!peak) {                                         /* (silence: every voice off, the level at rest) */
        for (i = 0; i < NVOICE; i++) TSEL->v[i].active = 0;
        vxs.en[6] = 0;
    } else
        vxs.en[6] = 0;
    memset(vxs.fr, 0xFF, sizeof vxs.fr);
    vxs.mt[6] += age * 8u;
    pal(UI_JIANT_INDEX);
    draw(S_V6_WEST_FULL);
    write_ppm(out_dir, "JIANT", name);
}
int main(int argc, char **argv)
{
    static const int16_t K0[8] = {60, 20, 30, 5, 64, 80, 40, 20}, K1[8] = {110, 60, 90, 9, 64, 50, 40, 0},
                         K2[8] = {30, 0, 10, 1, 64, 110, 20, 100};
    static const char *const DP[] = {"EDIT 1", "EDIT 2", "KIT 3", "SOUND", "SOUND 2", "SOUND 3", "LEVELS", "LEVELS 2",
                                     "KIT LFO", "EROSION", "FILTER", "OUT"};
    char nm[64];
    uint32_t i;
    out_dir = argc > 1 ? argv[1] : "build/west_frames";
    rep = fopen("/dev/null", "w");
    west_at(K0, 20, 7000, "west_1_strike");
    west_at(K0, 150, 5000, "west_2_150ms");
    west_at(K0, 400, 2500, "west_3_400ms");
    west_at(K0, 900, 900, "west_4_900ms");
    west_at(K0, 4000, 0, "west_5_silence");
    west_at(K1, 60, 7000, "west_6_fold_mod");
    west_at(K2, 200, 4000, "west_7_dust");
    for (i = 0; i < NELEM(DP); i++) {
        setup(S_DXSEND);
        dx_ui_lane = 1;
        nav_scene(SEC_EDIT, DP[i]);
        pal(UI_JIANT_INDEX);
        draw(S_DXSEND);
        snprintf(nm, sizeof nm, "drumx_%02u_%s", i + 1u, DP[i]);
        for (char *c = nm; *c; c++) if (*c == ' ') *c = '_';
        write_ppm(out_dir, "JIANT", nm);
    }
    return 0;
}
