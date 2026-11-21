// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.66, the owner: "EUCLID makes one or two notes") a melodic track's EUCLID: CONTOUR UP climbs over RANGE, MOTIF
 * repeats its first notes as a sequence, CHORD 100 only the triad's notes, RANGE wider more notes. Built and run by CI
 * (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t notes(const track_t *t, uint8_t *o)     /* the first note of each step struck, in order */
{
    uint32_t i, n = 0;
    for (i = 0; i < (uint32_t)t->p[P_SLEN] && i < NSTEP; i++)
        if (t->step[i].time == ST_NOTE && t->step[i].n) o[n++] = t->step[i].note[0];
    return n;
}
static uint32_t distinct(const uint8_t *v, uint32_t n)
{
    uint32_t i, j, d = 0;
    for (i = 0; i < n; i++) { for (j = 0; j < i && v[j] != v[i]; j++) {} d += j == i; }
    return d;
}
int main(void)
{
    int bad = 0, ok;
    uint8_t v[64];
    uint32_t n, i, up;
    track_t *m = &trk[1];
    ui_power_on();
    set_engine_of(m, ENGI_ANALOG);
    memset(eu, 0, sizeof eu);
    for (i = 0; i < NSTEP; i++) step_clear(&m->step[i]);
    m->p[P_SLEN] = 16; m->p[P_SCALE] = 1; m->p[P_ROOT] = 0;
    song.sel = 1; frame();
    go_title("EUCLID"); frame();
    eu_put(EK_HITS, 8); frame();
    eu_put(EK_M0, 1); eu_put(EK_M0 + 1u, 8); eu_put(EK_M0 + 2u, 0); eu_put(EK_M0 + 3u, 0); frame();   /* UP, 8, no motif */
    n = notes(m, v);
    for (i = 1, up = 0; i < n; i++) up += v[i] >= v[i - 1];
    ok = n == 8u && distinct(v, n) >= 6u && up == n - 1u && v[n - 1] > v[0];
    if (!ok) { printf("  up:"); for (i = 0; i < n; i++) printf(" %u", v[i]); printf("\n"); }
    bad += check("EUCLID melody: CONTOUR UP over RANGE 8, eight hits, six notes or more, climbing", ok);
    eu_put(EK_M0 + 2u, 3); frame();                    /* MOTIF 4: the first four answered a step up */
    n = notes(m, v);
    ok = n == 8u && v[4] != v[0] && (v[5] > v[4]) == (v[1] > v[0]) && distinct(v, n) >= 4u;
    if (!ok) { printf("  motif:"); for (i = 0; i < n; i++) printf(" %u", v[i]); printf("\n"); }
    bad += check("EUCLID melody: MOTIF 4 repeats its shape moved (a sequence)", ok);
    eu_put(EK_M0, 6); eu_put(EK_M0 + 2u, 0); eu_put(EK_M0 + 3u, 100); frame();   /* RANDOM, CHORD 100: C E G only */
    n = notes(m, v);
    for (i = 0, ok = n == 8u; i < n; i++) ok &= v[i] % 12u == 0u || v[i] % 12u == 4u || v[i] % 12u == 7u;
    if (!ok) { printf("  chord:"); for (i = 0; i < n; i++) printf(" %u", v[i]); printf("\n"); }
    bad += check("EUCLID melody: CHORD 100 only the triad's notes (C E G in C major)", ok);
    printf(bad ? "euclid-mel: %d FAILED\n" : "euclid-mel: all ok\n", bad);
    return bad != 0;
}
