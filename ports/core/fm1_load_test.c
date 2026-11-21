/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The FM-1's side of a project made on a big CPU (ports/core/build.sh test): the core built as the FM-1 is
 * (FELUCCA_HOST_POWER 0: no SAMPLE, no GRAIN, DRUM-X without its sample layer) loads the state sample_test saved with a
 * SAMPLE, a GRAIN and a DRUM-X track whose sample layer is on: SAMPLE and GRAIN play as ANALOG (the retired engines'
 * way, project.c proj_phys_gone), DRUM-X as its synth kit (the layer's values sit in slots the FM-1 never reads), and
 * everything sounds, finite.
 *   fm1_load_test STATE */
#define FELUCCA_HOST_POWER 0
#include "jiant_core.c"
#include <stdio.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok    " __VA_ARGS__); else { printf("  FAIL  " __VA_ARGS__); fails++; } \
                           printf("\n"); } while (0)
static float ol[44100], or_[44100];

int main(int argc, char **argv)
{
    static uint8_t st[4u << 20];
    size_t n;
    uint32_t i, k, bad = 0;
    double e[3] = {0, 0, 0};
    FILE *f = argc > 1 ? fopen(argv[1], "rb") : 0;
    printf("the FM-1 (no host power) loading a big CPU's project\n");
    if (!f) {
        printf("  FAIL  no state file\n");
        return 1;
    }
    n = fread(st, 1, sizeof st, f);
    fclose(f);
    jiant_boot(3, 1);
    CHECK(jiant_state_load(st, n) == 0, "the state loads (%zu bytes)", n);
    jiant_render(ol, or_, 4096);
    CHECK(trk[0].engine == ENGI_DRUM && trk[1].engine == ENGI_PHYS_TO && trk[2].engine == ENGI_PHYS_TO &&
              trk[1].eng_req == ENGI_PHYS_TO && trk[2].eng_req == ENGI_PHYS_TO,
          "T1 DRUM-X; T2 SAMPLE and T3 GRAIN as ANALOG (engines %u %u %u)", trk[0].engine, trk[1].engine, trk[2].engine);
    for (k = 0; k < 3u; k++) {
        for (i = 0; i < 6u; i++)
            trk[i].p[P_MUTE] = i != k;
        jiant_midi((uint8_t)(0x90 | k), k ? 60 : 36, 100);
        jiant_render(ol, or_, 22050);
        jiant_midi((uint8_t)(0x80 | k), k ? 60 : 36, 0);
        for (i = 0; i < 22050u; i++) {
            bad += !(ol[i] == ol[i]) || fabsf(ol[i]) > 2.0f;
            e[k] += (double)ol[i] * ol[i];
        }
        jiant_render(ol, or_, 22050);
    }
    CHECK(e[0] > 1.0 && e[1] > 1.0 && e[2] > 1.0 && !bad, "each plays (energy %.0f %.0f %.0f), finite", e[0], e[1], e[2]);
    printf(fails ? "fm1 load: %d FAILED\n" : "fm1 load: all passed\n", fails);
    return fails != 0;
}
