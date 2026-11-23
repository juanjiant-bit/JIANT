/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The core's copies (jiant_copies.h; ports/core/build.sh test): linked beside the single library, a copy plays what
 * the single core plays, sample for sample; four copies on four threads at once play what each plays alone (nothing
 * shared between them), one copy's music changes no other's, and the claims run out after JIANT_COPIES. */
#include "jiant_copies.h"
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok    " __VA_ARGS__); else { printf("  FAIL  " __VA_ARGS__); fails++; } \
                           printf("\n"); } while (0)

#define FRAMES 44100u
#define NRUN 3
static const uint8_t note[NRUN] = {48, 60, 67};
static const uint32_t preset[NRUN] = {0, 3, 7};   /* PRESETS detents: different sounds */

typedef struct { const jiant_api *c; int k; float *l, *r; } run_t;

/* a copy's take: its preset, a note held half a second, in blocks of 256 */
static void take(const jiant_api *c, int k, float *l, float *r)
{
    uint32_t at;
    c->jiant_encoder(JIANT_E_PRESET, (int32_t)preset[k]);
    c->jiant_render(0, 0, 4410);
    c->jiant_midi(0x90, note[k], 100);
    for (at = 0; at < FRAMES; at += 256u) {
        uint32_t n = FRAMES - at < 256u ? FRAMES - at : 256u;
        if (at == FRAMES / 2u)
            c->jiant_midi(0x80, note[k], 0);
        c->jiant_render(l + at, r + at, n);
    }
}
static void *thread_take(void *p)
{
    run_t *t = (run_t *)p;
    take(t->c, t->k, t->l, t->r);
    return 0;
}
static double rms(const float *x, uint32_t n)
{
    double s = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        s += (double)x[i] * x[i];
    return sqrt(s / n);
}

int main(void)
{
    const jiant_api *c[JIANT_COPIES + 1];
    static float l[2][NRUN][FRAMES], r[2][NRUN][FRAMES], sl[FRAMES], sr[FRAMES];
    pthread_t th[NRUN];
    run_t t[NRUN];
    uint32_t k, idx = 99;
    int i, same = 1, differ = 1;

    printf("JIANT core copies: %d\n", JIANT_COPIES);
    for (k = 0; k < JIANT_COPIES; k++) {
        c[k] = jiant_copy_claim(&idx);
        if (!c[k] || idx != k)
            break;
    }
    CHECK(k == JIANT_COPIES && jiant_copies_free() == 0, "%u copies claimed, each its own", k);
    CHECK(jiant_copy_claim(&idx) == 0, "one more finds none");
    jiant_copy_release(c[2]);
    CHECK(jiant_copies_free() == 1 && jiant_copy_claim(&idx) == c[2] && idx == 2, "a copy given back is claimed again");
    CHECK(!c[0]->jiant_booted() && !c[1]->jiant_booted() && !jiant_booted(), "none booted yet, nor the single core");

    /* copy 0 against the single library: the same device */
    jiant_boot(7, 1);
    c[0]->jiant_boot(7, 1);
    CHECK(c[0]->jiant_booted() && !c[1]->jiant_booted(), "copy 0 boots alone");
    jiant_midi(0x90, 60, 100);
    c[0]->jiant_midi(0x90, 60, 100);
    jiant_render(sl, sr, FRAMES);
    c[0]->jiant_render(l[0][0], r[0][0], FRAMES);
    CHECK(!memcmp(sl, l[0][0], sizeof sl) && !memcmp(sr, r[0][0], sizeof sr) && rms(sl, FRAMES) > 0.005,
          "copy 0 plays what the single core plays, sample for sample (rms %.4f)", rms(sl, FRAMES));
    c[0]->jiant_midi(0x80, 60, 0);
    c[0]->jiant_render(0, 0, FRAMES);

    /* copies 1..3 each alone, one after another; copies 4..6, booted the same, all at once on their threads: the
     * same samples (nothing shared between copies) */
    for (k = 1; k <= 2u * NRUN; k++)
        c[k]->jiant_boot(7, 1);
    for (i = 0; i < NRUN; i++)
        take(c[1 + i], i, l[0][i], r[0][i]);
    for (i = 0; i < NRUN; i++)
        t[i] = (run_t){c[1 + NRUN + i], i, l[1][i], r[1][i]};
    for (i = 0; i < NRUN; i++)
        pthread_create(&th[i], 0, thread_take, &t[i]);
    for (i = 0; i < NRUN; i++)
        pthread_join(th[i], 0);
    for (i = 0; i < NRUN; i++) {
        double e = rms(l[0][i], FRAMES);
        printf("        note %u, preset +%u: rms %.4f\n", note[i], preset[i], e);
        same &= !memcmp(l[0][i], l[1][i], sizeof l[0][i]) && !memcmp(r[0][i], r[1][i], sizeof r[0][i]) && e > 0.003;
        if (i)
            differ &= memcmp(l[0][i], l[0][0], sizeof l[0][0]) != 0;
    }
    CHECK(same, "%d copies on %d threads at once play what each plays alone, sample for sample", NRUN, NRUN);
    CHECK(differ, "and each its own sound");

    {                                          /* one copy's music, another's untouched */
        size_t cap = c[1]->jiant_state_max(), n1, n2, m1, m2;
        uint8_t *a1 = (uint8_t *)malloc(cap), *b1 = (uint8_t *)malloc(cap), *a2 = (uint8_t *)malloc(cap),
                *b2 = (uint8_t *)malloc(cap);
        c[2]->jiant_render(0, 0, 4410);
        n1 = c[1]->jiant_state_save(a1, cap);
        n2 = c[2]->jiant_state_save(a2, cap);
        c[1]->jiant_generate(0, 555, 80, 0);
        c[1]->jiant_play(1);
        c[1]->jiant_render(sl, sr, FRAMES);
        m1 = c[1]->jiant_state_save(b1, cap);
        m2 = c[2]->jiant_state_save(b2, cap);
        CHECK(c[1]->jiant_playing() && !c[2]->jiant_playing() && !c[3]->jiant_playing() && rms(sl, FRAMES) > 0.003,
              "copy 1 plays a generated song, the others stopped");
        CHECK((m1 != n1 || memcmp(a1, b1, n1)) && m2 == n2 && !memcmp(a2, b2, n2),
              "copy 1's music changed (%zu -> %zu bytes), copy 2's the same (%zu bytes)", n1, m1, n2);
        c[1]->jiant_play(0);
        free(a1);
        free(b1);
        free(a2);
        free(b2);
    }
    {                                          /* a sample in copy 7's slot 0 only */
        static float s[22050];
        jiant_slot_info_t i3, i0;
        c[7]->jiant_boot(7, 1);
        for (k = 0; k < 22050u; k++)
            s[k] = 0.5f * (float)sin(k * 0.05);
        c[7]->jiant_slot_load(0, s, 0, 22050, 44100, "SINE");
        c[7]->jiant_slot_info(0, &i3);
        c[0]->jiant_slot_info(0, &i0);
        CHECK(i3.frames == 22050u && !strcmp(i3.name, "SINE") && i0.frames == 0u && !jiant_slot_object(0),
              "a sample in copy 7's slot 1, not in copy 0's nor the single core's");
    }
    printf(fails ? "core copies: %d FAILED\n" : "core copies: all passed\n", fails);
    return fails != 0;
}
