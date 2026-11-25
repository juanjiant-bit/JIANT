/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.44, the owner: "a gate reverb and a gate delay") FX > REVERB's and FX > DLY's GATE (fx.c fx_gt): 0 the buses
 * as they were; on, a return open while its own input sounds and GATE / 64 of a beat after, then shut: 40 dB down
 * 30 ms later, no click; open, the return bit for bit the ungated one. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <math.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#define NB 2400u                                        /* blocks a run: ~1.7 s */
static int32_t (*out)[NB * CTL];                         /* (shared: each run a process of its own, from power-on) */
static uint32_t rnd = 12345u;
static void run(int bus, int gate, int cont, int32_t *o)
{
    uint32_t b, i;
    pid_t pid = fork();
    if (pid) { int st; waitpid(pid, &st, 0); return; }
    host_tracks_init();
    song.g[G_BPM] = 120;
    song.g[G_RTYPE] = 0; song.g[G_RSIZE] = 110; song.g[G_RWIDE] = 40;
    song.g[G_DFDBK] = 90; song.g[G_DMIX] = 127; song.g[G_DTIME] = 2; song.g[G_WIDTH] = 30;
    fx_gt[0] = fx_gt[1] = 0; fx_gt[bus] = (int16_t)gate;
    rnd = 12345u;
    for (b = 0; b < NB; b++) {
        for (i = 0; i < CTL; i++) {
            int32_t x = 0;
            rnd = rnd * 1664525u + 1013904223u;
            if (cont || b < 60u) x = (int32_t)(rnd >> 18) - 8192;   /* a burst of noise: 43 ms (or for ever) */
            fxq_c[i] = 0; fxq_d[i] = bus ? x : 0; fxq_r[i] = bus ? 0 : x;
        }
        memset(fx_sl, 0, sizeof fx_sl); memset(fx_sr, 0, sizeof fx_sr);
        fx_run(CTL);
        for (i = 0; i < CTL; i++) o[b * CTL + i] = fx_wet[i] + fx_sl[i];
    }
    _exit(0);
}
static double rms(const int32_t *a, uint32_t from, uint32_t to) { double e = 0; for (uint32_t i = from; i < to; i++) e += (double)a[i] * a[i]; return sqrt(e / (to - from) + 1e-9); }
int main(void)
{
    int fails = 0, bus;
    out = mmap(0, 4u * sizeof out[0], PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    for (bus = 0; bus < 2; bus++) {
        const char *nm = bus ? "DELAY" : "REVERB";
        uint32_t end = 60u * CTL, hold = 44100u / 2u * 32u / 64u, i;   /* GATE 32 at 120 BPM: half a beat, 250 ms */
        double h0, a0, h1, a1, st = 0, sh = 0;
        run(bus, 0, 1, out[0]); run(bus, 32, 1, out[1]);
        if (memcmp(out[0], out[1], sizeof out[0]) || rms(out[0], 0, NB * CTL) < 20) { printf("FAIL %s: the gate open is not the ungated return\n", nm); fails++; }
        run(bus, 0, 0, out[2]); run(bus, 32, 0, out[3]);
        h0 = rms(out[2], end + 2000u, end + hold - 1000u); a0 = rms(out[2], end + hold + 1323u, end + hold + 9000u);
        h1 = rms(out[3], end + 2000u, end + hold - 1000u); a1 = rms(out[3], end + hold + 1323u, end + hold + 9000u);
        printf("%s: ungated hold %.0f after %.0f (%.1f dB); gated hold %.0f after %.2f (%.1f dB)\n", nm, h0, a0,
               20 * log10(a0 / h0), h1, a1, 20 * log10(a1 / h1));
        if (h0 < 20 || a0 < h0 * 0.03) { printf("FAIL %s: no tail to gate\n", nm); fails++; }
        if (fabs(h1 - h0) > h0 * 0.02) { printf("FAIL %s: the held part is not the ungated one\n", nm); fails++; }
        if (a1 > h1 * 0.01) { printf("FAIL %s: 30 ms after the hold not 40 dB down\n", nm); fails++; }
        for (i = end + 2000u; i < end + hold - 1000u; i++) { double d = fabs((double)out[3][i] - out[3][i - 1]); if (d > sh) sh = d; }
        for (i = end + hold - 1000u; i < end + hold + 4000u; i++) { double d = fabs((double)out[3][i] - out[3][i - 1]); if (d > st) st = d; }
        printf("%s: the largest step while held %.0f, at the close %.0f\n", nm, sh, st);
        if (st > sh * 1.25 + 8) { printf("FAIL %s: a click at the close\n", nm); fails++; }
    }
    printf(fails ? "gate fx test FAILED\n" : "gate fx test passed\n");
    return fails ? 1 : 0;
}
