// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.66, the owner: "EUCLID makes one or two notes") a melodic track's EUCLID: CONTOUR UP climbs over RANGE, MOTIF
 * repeats its first notes as a sequence; (V10.7.67) ROLE CHORDS BASS, the neighbours' harmony followed. Built and run by CI
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
    eu_put(EK_M0, 1); eu_put(EK_M0 + 1u, 8); eu_put(EK_M0 + 2u, 0); eu_put(EK_K4, 0); frame();   /* UP, 8, no motif */
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
    {   /* (V10.7.67, the owner) ROLE: CHORDS the key's triad (nothing else plays: C E G, a seventh or ninth over it),
         * BASS an octave down on the root's beats; a neighbour in A minor: the beats on A C E, the chords A's; four bars
         * of C F G C next door: four bars, the third G B D */
        track_t *nb = &trk[2];
        uint32_t k, c, ok2 = 1;
        eu_put(EK_M0, 6); eu_put(EK_M0 + 2u, 0); eu_put(EK_K4, 1); frame();   /* RANDOM, CHORDS */
        for (i = 0, n = 0; i < 16u; i++) {
            const step_t *s = &m->step[i];
            if (s->time != ST_NOTE) continue;
            n++;
            ok2 &= s->n >= 3u;
            for (c = 0; c < 3u; c++) ok2 &= s->note[c] % 12u == 0u || s->note[c] % 12u == 4u || s->note[c] % 12u == 7u;
        }
        bad += check("EUCLID ROLE CHORDS: a triad on every hit (C E G in C major, nothing else playing)", ok2 && n == 8u);
        eu_put(EK_K4, 2); frame();                 /* BASS */
        for (i = 0, ok2 = 1, n = 0; i < 16u; i++)
            if (m->step[i].time == ST_NOTE) { n++; ok2 &= m->step[i].note[0] <= 60u; if (!(i & 3u)) ok2 &= m->step[i].note[0] % 12u == 0u; }
        if (!ok2) { printf("  bass:"); for (i = 0; i < 16u; i++) if (m->step[i].time == ST_NOTE) printf(" %u:%u", i, m->step[i].note[0]); printf("\n"); }
        bad += check("EUCLID ROLE BASS: below the middle, the root on the beats", ok2 && n == 8u);
        set_engine_of(nb, ENGI_ANALOG); nb->p[P_MUTE] = 0;
        for (i = 0; i < NSTEP; i++) step_clear(&nb->step[i]);
        nb->p[P_SLEN] = 16;
        for (i = 0; i < 16u; i += 4u) { nb->step[i].time = ST_NOTE; nb->step[i].n = 3; nb->step[i].note[0] = 57; nb->step[i].note[1] = 60; nb->step[i].note[2] = 64; }
        eu_put(EK_K4, 0); frame();                 /* MELODY over A minor */
        for (i = 0, ok2 = 1; i < 16u; i++)
            if (m->step[i].time == ST_NOTE && !(i & 3u)) { k = m->step[i].note[0] % 12u; ok2 &= k == 9u || k == 0u || k == 4u; }
        bad += check("EUCLID follows the neighbours: a track in A minor, the melody's beats on A C E", ok2);
        eu_put(EK_K4, 1); frame();
        for (i = 0, ok2 = 1; i < 16u; i++)
            if (m->step[i].time == ST_NOTE) for (c = 0; c < 3u; c++) { k = m->step[i].note[c] % 12u; ok2 &= k == 9u || k == 0u || k == 4u; }
        bad += check("EUCLID CHORDS over the neighbour: A minor's (A C E)", ok2);
        {   static const uint8_t PR[4][3] = {{48, 52, 55}, {53, 57, 60}, {55, 59, 62}, {48, 52, 55}};   /* C F G C */
            nb->p[P_SLEN] = 64;
            for (i = 0; i < 64u; i++) step_clear(&nb->step[i]);
            for (i = 0; i < 64u; i += 4u) { nb->step[i].time = ST_NOTE; nb->step[i].n = 3; for (c = 0; c < 3u; c++) nb->step[i].note[c] = PR[i / 16u][c]; }
            eu_put(EK_HITS, 8); frame();
            ok2 = m->p[P_SLEN] == 64;
            for (i = 32; i < 48u; i++)
                if (m->step[i].time == ST_NOTE) for (c = 0; c < 3u; c++) { k = m->step[i].note[c] % 12u; ok2 &= k == 7u || k == 11u || k == 2u; }
            if (!ok2) { printf("  prog: len %d", m->p[P_SLEN]); for (i = 32; i < 48u; i++) if (m->step[i].time == ST_NOTE) printf(" %u:%u,%u,%u", i, m->step[i].note[0], m->step[i].note[1], m->step[i].note[2]); printf("\n"); }
            bad += check("EUCLID CHORDS over C F G C next door: four bars, the third G B D", ok2);
        }
    }
    printf(bad ? "euclid-mel: %d FAILED\n" : "euclid-mel: all ok\n", bad);
    return bad != 0;
}
