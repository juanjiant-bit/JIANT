// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.62, a user: "I love the MATH BYTEBEAT's sound and its kinda arp, but the bpm is not the same from a note to
 * another") MATH's WAVE SYNC (the time locked): BYTE's slow bits (its rhythm, the arp's steps) from the tempo, the fast
 * ones (the pitch) the note's. Two notes an octave apart at 120 BPM: the same rhythm (the slow time and the windows the
 * pattern steps in, block by block), different pitches; WAVE BYTE as before: the rhythm the note's (an octave up, twice
 * as fast). Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

#define TL_BLOCKS 6000u   /* (~8.7 s: ~17 beats at 120) */
/* BYTEBEAT's formula (ALGO 4: B01), WAVE w, at note n for TL_BLOCKS blocks, the bar's clock run by the test at 120 BPM:
 * each block's slow time (v.ph[1]) into ts[], the steps of the rhythm (the slow bits' changes) counted; the output's hash */
static uint32_t take(int16_t w, uint32_t n, uint32_t *ts, uint32_t *steps)
{
    static track_t t;
    static voice_t v;
    static int32_t o[CTL];
    vmod_t m;
    tsvf_t flt;
    uint32_t k, i, h = 2166136261u, last = 0xFFFFFFFFu;
    memset(&t, 0, sizeof t); memset(&v, 0, sizeof v); memset(&m, 0, sizeof m);
    t.p[P_E0] = 2; t.p[P_E1] = w; t.p[P_E2] = 4; t.p[P_FCUT] = 127; t.p[P_FRES] = 50;
    m.amp0 = m.amp1 = 32767; m.shape = 64 << 8;
    tsvf_coef(&flt, 127 << 8, 50);
    song.g[G_BPM] = 120;
    bar_n = 0; beat_n = 0; beat_pos = 0;
    lofi_note_on(&t, &v);
    *steps = 0;
    for (k = 0; k < TL_BLOCKS; k++) {
        memset(o, 0, sizeof o);
        lofi_byte_render(&t, &v, o, CTL, &m, pitch_inc((int32_t)n << 4), 1, 8, &flt);
        for (i = 0; i < CTL; i++) h = (h ^ (uint32_t)o[i]) * 16777619u;
        ts[k] = v.ph[1];
        if ((ts[k] >> 16) != last) { last = ts[k] >> 16; (*steps)++; }
        beat_pos += CTL;                                 /* (the bar's clock, as seq.c runs it) */
        if (beat_pos >= beat_samples()) {
            beat_pos -= beat_samples();
            beat_n = (beat_n + 1u) & 3u;
            if (!beat_n) bar_n++;
        }
    }
    return h;
}
int main(void)
{
    static uint32_t a[TL_BLOCKS], b[TL_BLOCKS], c[TL_BLOCKS], d[TL_BLOCKS];
    uint32_t sa, sb, sc, sd, k, same = 1, differ = 0;
    uint32_t ha = take(RW_BLOCK, 60, a, &sa), hb = take(RW_BLOCK, 72, b, &sb);
    uint32_t hc = take(RW_BYTE, 60, c, &sc), hd = take(RW_BYTE, 72, d, &sd);
    int bad = 0;
    for (k = 0; k < TL_BLOCKS; k++) {
        same &= a[k] == b[k];
        differ |= c[k] != d[k];
    }
    bad += check("SYNC at 120 BPM: C4 and C5 the same slow time, block by block (the same rhythm)", same);
    bad += check("SYNC: the rhythm steps as many times for both notes", sa == sb && sa > 4u);
    bad += check("SYNC: the notes still two pitches (different output)", ha != hb);
    bad += check("BYTE (no lock): the octave runs the rhythm faster (as before)", differ && sd > sc);
    bad += check("SYNC and BYTE: different time bases at C4", ha != hc);
    printf("math-timelock: SYNC steps %u / %u, BYTE %u / %u\n", sa, sb, sc, sd);
    printf(bad ? "math-timelock: %d FAILED\n" : "math-timelock: all ok\n", bad);
    return bad != 0;
}
