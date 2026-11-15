/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.36) WAV renders of the FX layer's punch-in keys over perform_test.c's loop (an acid bass, a chord, drums at
 * 120 BPM): 4 bars each, the key held over bars 2 and 3, let go for bar 4.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/punch_render tests/punch_render.c -lm
 *   build/host/punch_render renders/v36-punch          (-DPUNCH_OLD: GLITCH alone, for an older tree's sources) */
#define PERFORM_NO_MAIN
#include "perform_test.c"

static int pr_e;
static uint32_t pr_on, pr_off;
static void pr_at(uint32_t t)
{
    if (pr_e >= 0 && t == pr_on)
        perf_press((uint32_t)pr_e, 1);
    if (pr_e >= 0 && t == pr_off)
        perf_press((uint32_t)pr_e, 0);
}
static void pr_render(const char *dir, const char *name, int e, int rtype)
{
    char path[512];
    FILE *f;
    uint32_t t, bar = 4u * 22050u / CTL * CTL, n = 4u * bar;
    song_setup();
    if (rtype >= 0)
        song.g[G_RTYPE] = (int16_t)rtype;          /* (FREEZE on CLOUD too: its line held) */
    pr_e = e;
    pr_on = bar;
    pr_off = 3u * bar;
    song_render(n, pr_at);
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, n);
    for (t = 0; t < n; t++)
        wav_put(f, song_l[t], song_r[t]);
    fclose(f);
    printf("punch_render: %s\n", path);
}
int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : ".";
#ifdef PUNCH_OLD
    pr_render(dir, "glitch_antes", PF_GLT, -1);
#else
    static const struct { const char *n; int e, rt; } E[] = {
        {"00_loop_sin_efecto", -1, -1}, {"reverse", PF_REV, -1}, {"tape_stop", PF_TAPE, -1}, {"pitch_up", PF_PUP, -1},
        {"pitch_down", PF_PDN, -1}, {"freeze", PF_FRZ, -1}, {"freeze_cloud", PF_FRZ, 4}, {"00_loop_cloud", -1, 4},
        {"rev_all", PF_RVA, -1}, {"dly_all", PF_DLA, -1}, {"ring", PF_RING, -1}, {"life_max", PF_LMAX, -1},
        {"glitch_despues", PF_GLT, -1}, {"wobble", PF_WOB, -1}, {"dly_1_16", PF_D16, -1}, {"comb", PF_COMB, -1}};
    uint32_t i;
    for (i = 0; i < sizeof E / sizeof E[0]; i++) {   /* (each from the power-on's state: its own process) */
        pid_t pid = fork();
        if (!pid) {
            pr_render(dir, E[i].n, E[i].e, E[i].rt);
            _exit(0);
        }
        waitpid(pid, 0, 0);
    }
#endif
    return 0;
}
