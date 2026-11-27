// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.66, JIANT BYTE 68b546c9's loopq) MATH's LOOP with WAVE SYNC: 1 .. 8 a division of the song (4 BAR .. 1/32),
 * the window aligned to the transport, the note's t from 0 at each window's start. LOOP 0 and every other WAVE as
 * before (the hashes taken from V10.8.65's code); at 125 BPM a bar is a whole number of blocks (1440 x 64), so a
 * 1 BAR loop repeats sample for sample, and two notes started at different times play the same loop on the same bars.
 * Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

/* WAVE w, LOOP lp, note n, BPM bpm: the note starts at block st; nb blocks rendered into out (or hashed if out == 0) */
static uint32_t run(int16_t w, int16_t lp, uint32_t n, int bpm, uint32_t st, uint32_t nb, int32_t *out)
{
    static track_t t;
    static voice_t v;
    static int32_t o[CTL];
    vmod_t m;
    tsvf_t flt;
    uint32_t k, i, h = 2166136261u;
    memset(&t, 0, sizeof t); memset(&v, 0, sizeof v); memset(&m, 0, sizeof m);
    t.p[P_E0] = 2; t.p[P_E1] = w; t.p[P_E2] = 4; t.p[P_E7] = lp; t.p[P_FCUT] = 127; t.p[P_FRES] = 50;
    m.amp0 = m.amp1 = 32767; m.shape = 64 << 8;
    tsvf_coef(&flt, 127 << 8, 50);
    song.g[G_BPM] = bpm;
    bar_n = 0; beat_n = 0; beat_pos = 0;
    for (k = 0; k < nb; k++) {
        memset(o, 0, sizeof o);
        if (k == st)
            lofi_note_on(&t, &v);
        if (k >= st)
            lofi_byte_render(&t, &v, o, CTL, &m, pitch_inc((int32_t)n << 4), 1, 8, &flt);
        for (i = 0; i < CTL; i++) {
            h = (h ^ (uint32_t)o[i]) * 16777619u;
            if (out) out[k * CTL + i] = o[i];
        }
        beat_pos += CTL;
        if (beat_pos >= beat_samples()) {
            beat_pos -= beat_samples();
            beat_n = (beat_n + 1u) & 3u;
            if (!beat_n) bar_n++;
        }
    }
    return h;
}
#ifndef LOOPQ_H0   /* (V10.8.65's own: run with an argument on that code to print them) */
#define LOOPQ_H0 0x0306bb47u
#define LOOPQ_H1 0x0306bb47u
#define LOOPQ_H2 0x39bbfc71u
#define LOOPQ_H3 0x57add740u
#define LOOPQ_H4 0x57add740u
#define LOOPQ_H5 0x57add740u
#endif
int main(int argc, char **argv)
{
    static const struct { int16_t w, lp; } C[] = {{7, 0}, {7, 40}, {7, 127}, {5, 0}, {5, 3}, {5, 60}};
    static const uint32_t H[6] = {LOOPQ_H0, LOOPQ_H1, LOOPQ_H2, LOOPQ_H3, LOOPQ_H4, LOOPQ_H5};
    uint32_t bar, k, nb, eq, eq2;
    static int32_t a[8000 * CTL], b[8000 * CTL];
    int bad = 0, i;
    for (i = 0; i < 6; i++) {
        uint32_t h = run(C[i].w, C[i].lp, 60, 120, 0, 1500, 0);
        if (argc > 1) { printf("#define LOOPQ_H%d 0x%08xu\n", i, h); continue; }
        char s[96];
        snprintf(s, sizeof s, "as before: WAVE %s LOOP %d", C[i].w == 7 ? "SYNC" : "BYTE", C[i].lp);
        bad += check(s, h == H[i]);
    }
    if (argc > 1)
        return 0;
    song.g[G_BPM] = 125;
    bar = 4u * beat_samples() / CTL;
    nb = bar * 4u;
    run(7, 3, 60, 125, 0, nb, a);                        /* (1 BAR) */
    eq = memcmp(&a[bar * CTL], &a[2u * bar * CTL], bar * CTL * sizeof a[0]) == 0
      && memcmp(&a[bar * CTL], &a[3u * bar * CTL], bar * CTL * sizeof a[0]) == 0;
    bad += check("SYNC LOOP 1 BAR at 125 BPM: bars 2, 3 and 4 the same, sample for sample", eq);
    run(7, 3, 60, 125, 300, nb, b);                      /* (the same note, started 300 blocks later) */
    eq2 = memcmp(&a[2u * bar * CTL], &b[2u * bar * CTL], 2u * bar * CTL * sizeof a[0]) == 0;   /* (bars 3 and 4: the filters' state settled) */
    bad += check("SYNC LOOP 1 BAR: a note started later plays the same loop on the same bars", eq2);
    run(7, 0, 60, 125, 0, nb, b);
    bad += check("SYNC LOOP OFF: bars 2 and 3 differ (no loop)", memcmp(&b[bar * CTL], &b[2u * bar * CTL], bar * CTL * sizeof b[0]) != 0);
#ifndef LOOPQ_OLD
    {
        int nm = 1;
        for (k = 0; k < 9; k++) { char v[8]; const char *u; param_format(&LOFI_LOOPQ, (int32_t)k, v, &u); nm &= !strcmp(v, N_LOOPQ[k]); }
        bad += check("the card names LOOP 0 .. 8: OFF 4 BAR .. 1/32", nm);
    }
#endif
    printf(bad ? "loopq: %d FAILED\n" : "loopq: all ok\n", bad);
    return bad != 0;
}
