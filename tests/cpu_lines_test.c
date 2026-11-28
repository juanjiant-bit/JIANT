/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.60, technique from JIANT BYTE byte-cpu-light) the scenes of the lines model (tests/cpu_lines_test.py): the
 * FM-1's time follows the 32-byte lines of the flash the sound reads each block (code and constants), core by core, more
 * than its instructions. Run under valgrind's lackey: each block of the scene between two stores to cl_mark, core 1's
 * work (mc_job) between two to cl_c1, each part (fx.c mix_part_t) between two to cl_part[k].
 *   cpu_lines_test SCENE NBLK      (SCENE hex: 101 GEN EXPLORE s123 ENERGY 85 with CLOUD, the delay and the chorus;
 *                                   102 .. 108 other GEN songs; 0c SAVE > TOOLS 6 CPU TEST's song (TRACKS 4))
 * CL_COST="c0,..,c5,fx" (us): each part's cost as the FM-1 measures it (mc_tk, mc_fx_tk), for the device's split */
#include <stdint.h>
#define UI_TEST_NO_MAIN 1
#define MC_HOST_ASYNC
static int cl_mc_run(void (*f)(uint32_t), uint32_t a);
#define mc_run(f, a) cl_mc_run(f, a)
#define mc_wait() 0
#define HOST_LIFE 0
volatile int cl_mark, cl_c1, cl_ex, cl_part[8];
#define CPU_LINES_PART(i, v) (cl_part[(i) & 7u] = (v))
#include "ui_test.c"
static __attribute__((noinline)) void core1_run(void (*f)(uint32_t), uint32_t a) { f(a); __asm__ volatile(""); }
static int cl_mc_run(void (*f)(uint32_t), uint32_t a)
{
    if (!mc_host) return 0;
    mc_host_cpu = 1;
    cl_c1 = 1;
    core1_run(f, a);
    cl_c1 = 2;
    mc_host_cpu = 0;
    return 1;
}
static int32_t vo[2 * CTL];
static uint32_t blk;
static void move(void)                                   /* the matrix's sources moving: the macros, AT, MODW */
{
    uint32_t k, ph = blk++;
    for (k = 0; k < NTRK; k++) { trk[k].at = (uint8_t)((ph * 5u + k * 17u) % 128u); trk[k].mw = (uint8_t)((ph * 2u) % 128u); }
}
static uint32_t cl_cost[NTRK + 1u], cl_cost_on;
static void cost_in(void)                                /* (the device's split: what each part took, as it measures it) */
{
    uint32_t k;
    if (!cl_cost_on) return;
    for (k = 0; k < NTRK; k++) mc_tk[k] = cl_cost[k];
    mc_fx_tk = cl_cost[NTRK];
}
static uint32_t scene(uint32_t mask, uint32_t nblk)
{
    uint32_t b, q, f = 0, h = 2166136261u;
    int dty;
    blk = 0;
    ui_power_on(); go_home(); frames(4);
    if (mask >= 0x100u) {                                /* GEN songs, TRACKS 6 */
        static const struct { uint8_t g; uint16_t s; uint8_t e; int8_t m; } GS[9] = {
            {0, 123, 85, 0}, {0, 123, 85, 0}, {0, 7, 90, 50}, {7, 42, 85, 0}, {12, 2024, 85, 50}, {0, 999, 100, 0},
            {0, 321, 80, -50}, {3, 77, 95, 60}, {19, 5, 60, -40}};   /* (V10.8.68) 106 EXPLORE CALM, 107 DANCE, 108 NEO CLASSIC CALM */
        uint32_t k, gi = (mask & 0xFFu) < 9u ? mask & 0xFFu : 0u;
        ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
        gp.genre = GS[gi].g ? GS[gi].g : GEN_EXPLORE; gp.seed = GS[gi].s; gp.energy = GS[gi].e; gp.form = 1;
        grd.mood = GS[gi].m;
        gp.complex = 70; gp.swing = GA_AUTO; gp.human = 30;
        for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
        gen_go();
        if (mask == 0x101u) { song.g[G_RTYPE] = 4; song.g[G_DFDBK] = 70;
            for (k = 0; k < NTRK; k++) { trk[k].p[P_DLY] = 70; trk[k].p[P_REV] = 80; trk[k].p[P_CHOR] = 70; } }
    } else {                                             /* SAVE > TOOLS 6 CPU TEST's song, TRACKS 4 (the device's measure) */
        ui_rec_prefs &= (uint8_t)~(PREF_SIX >> 8); rp_apply();
        cpu_test_song();
    }
    song.master_q12 = 4096;
    if (getenv("CL_OTT")) {                              /* (V10.8.71, as JIANT BYTE's) every track's OTT at this DEPTH (the
                                                         * worst case: 100) */
        uint32_t k;
        for (k = 0; k < NTRK; k++) ott_v[k][OT_DEPTH] = (int16_t)atoi(getenv("CL_OTT"));
    }
    if (live.mode) chain_play_ui(); else transport_req = 1;
    ui_input();
    dty = getenv("CL_DTYPE") ? atoi(getenv("CL_DTYPE")) : -1;
    for (b = 0; b < 400u + nblk; b++) {                  /* (its first 400 blocks to warm: the song under way) */
        if (b % 23u == 0u) { f++; ui_input(); song_poll(); host_ticks += 1000000u / 30u; fm1_ms += 1000u / 30u; }
        if (dty >= 0) {                                  /* (V10.8.72) every track's DIST at 100 on this TYPE (BYTE's formulas:
                                                         * 6 .. 11; a type's worst case), each block (the sections load theirs) */
            uint32_t k;
            for (k = 0; k < NTRK; k++) { trk[k].p[P_DIST] = 127; trk[k].p[P_DTYPE] = (int16_t)dty; }
        }
        move();
        cost_in();
        if (b >= 400u) cl_mark = 1;
        mix_block(vo, CTL);
        if (b >= 400u) cl_mark = 2;
        for (q = 0; q < 2u * CTL; q++) h = (h ^ (uint32_t)vo[q]) * 16777619u;
    }
    (void)f;
    return h;
}
int main(int argc, char **argv)
{
    uint32_t m = argc > 1 ? strtoul(argv[1], 0, 16) : 0x101u, nblk = argc > 2 ? strtoul(argv[2], 0, 10) : 16u, h, k;
    mc_host = 1;                                         /* (two cores: core 1's job at once, between cl_c1's stores) */
    if (getenv("CL_COST")) {
        char *p = getenv("CL_COST");
        cl_cost_on = 1;
        for (k = 0; k <= NTRK && *p; k++) { cl_cost[k] = strtoul(p, &p, 10); if (*p == ',') p++; }
    }
    h = scene(m, nblk);
    printf("cpu_lines: scene %03x hash %08x voices", m, h);
    for (k = 0; k < NTRK; k++) { uint32_t i, n = 0; for (i = 0; i < NVOICE; i++) n += trk[k].v[i].active != 0; printf(" %u", n); }
    printf("\n");
    return 0;
}
