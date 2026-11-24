/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* SWARM as a note source (MODE >Tn): simulated devices drive an ANALOG track (host only) */
#define main hostsim_main
#define HOST_OWN_TICKS 1
static unsigned host_clock;
#define fm1_ticks() (host_clock)
#include "hostsim.c"
#undef main
#include <math.h>
#include <time.h>
static void pkt(uint32_t dev, uint32_t lv, uint32_t ch)
{
    uint8_t p[30]; uint32_t i;
    p[0] = 0x00; p[1] = 28;
    for (i = 2; i < 30u; i++) p[i] = (uint8_t)(dev * 41u + i * (dev + 3u) * 17u);
    bt_air_tap(p, 30, lv | lv << 8, ch);
}
static double run(uint32_t blocks, int devs, uint32_t *ons, uint32_t *maxheld, double *ns)
{
    static int32_t o[2 * CTL]; double e = 0; uint32_t b, i, v; uint16_t last[SW_V]; struct timespec a, z;
    for (v = 0; v < SW_V; v++) last[v] = swn[v].nt ? swn[v].cnt : 0;
    clock_gettime(CLOCK_MONOTONIC, &a);
    for (b = 0; b < blocks; b++) {
        uint32_t held = 0;
        host_clock += CTL * 24000000u / 44100u;
        if (devs) { if (b % 37u == 0) pkt(1, 30, 37); if (b % 53u == 5) pkt(2, 48, 38); if (b % 71u == 9) pkt(3, 18, 39);
                    if (devs > 3) for (v = 4; v <= 8; v++) if (b % (11u + v) == v) pkt(v, 20 + v * 3, 37 + v % 3); }
        mix_block(o, CTL);
        for (i = 0; i < 2 * CTL; i++) e += (double)(o[i] >> 8) * (o[i] >> 8);
        for (v = 0; v < SW_V; v++) { uint16_t c = swn[v].nt ? swn[v].cnt : 0; if (c > last[v]) (*ons)++; last[v] = c; held += c != 0; }
        if (held > *maxheld) *maxheld = held;
    }
    clock_gettime(CLOCK_MONOTONIC, &z);
    *ns = ((z.tv_sec - a.tv_sec) * 1e9 + (z.tv_nsec - a.tv_nsec)) / blocks;
    return sqrt(e / (blocks * 2.0 * CTL));
}
int main(void)
{
    uint32_t ons, mh, v, fail = 0, eng, bl = 20 * 44100 / CTL; double r, ns; int mode, devs;
    static const uint8_t ENG[4] = {ENGI_ANALOG, ENGI_FM6, ENGI_WEST, ENGI_DRUM};
    static const char *EN[4] = {"ANALOG", "FM6", "WEST", "DRUM"};
    for (eng = 0; eng < 4; eng++)
      for (devs = 3; devs <= 8; devs += 5)
        for (mode = 0; mode <= 2; mode += 2) {
            host_tracks_init();
            host_preset(&trk[0], ENG[eng], 0);
            host_preset(&trk[1], ENGI_INPUT, 9u);
            memset(sw, 0, sizeof sw); memset(&sm, 0, sizeof sm); memset(swn, 0, sizeof swn);
            sw_lmin = 127; sw_lmax = 0; ain_tr = 1; ain_sw = 1; host_clock = 0; song.playing = 0; song.g[G_BPM] = 120; song.master_q12 = 4096;
            trk[1].p[P_E3] = (int16_t)mode;                 /* 0 THRU, 2 >T1 */
            ons = mh = 0; r = run(bl, devs, &ons, &mh, &ns);
            printf("%-6s %d devices MODE %s: out rms %7.1f  notes %4u (%.1f /s)  held at once max %u  swarm's own voices %s  %5.0f ns/block\n", EN[eng], devs,
                   mode ? ">T1 " : "THRU", r, ons, ons / 20.0, mh, sw_on ? "sound" : "silent", ns);
            if (mode) {
                if (!ons || r < 10) { printf("FAIL: no notes / no sound\n"); fail = 1; }
                if (sw_on) { printf("FAIL: the swarm's own voices still sound\n"); fail = 1; }
                trk[1].p[P_E3] = 0; ons = 0; run(4, 0, &ons, &mh, &ns);
                for (v = 0; v < SW_V; v++) if (swn[v].nt) { printf("FAIL: a note left hanging (voice %u)\n", v); fail = 1; }
            }
        }
    printf(fail ? "sw_notes: FAIL\n" : "sw_notes: all ok\n");
    return (int)fail;
}
