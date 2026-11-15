/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.37) MATH's EDIT > RETRIG TIME (core.h P_LTIME, eng_lofi.c lofi_tq), through hostsim.c:
 *   build/host/math_time_test
 * 1. NOTE (127 stored, every MATH track before it): each note's slow time from its own start (a new note: from 0).
 * 2. a rate (1/16): the slow time the transport's, the same in every voice (a second note goes on where the first is,
 *    no restart: their slow bits one), one formula step (2048 t) a division, within a block of the transport's; MOTION
 *    not used.
 * 3. the stored default is NOTE, and a rate's names span the knob (NOTE 1/1 .. 1/32). */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "jiant_test_util.h"

static int fails;
#define check(what, ok, ...) (fails += jt_verdict("math_time: ", 66, (what), (ok), __VA_ARGS__))

static void blocks(uint32_t n)
{
    int32_t o[2 * CTL];
    while (n--)
        mix_block(o, CTL);
}
static track_t *math_track(int16_t time)
{
    track_t *t = &trk[0];
    uint32_t k;
    for (k = 0; k < NTRK * NVOICE; k++)
        trk[k / NVOICE].v[k % NVOICE].active = 0;
    host_tracks_init();
    host_preset(t, ENGI_LOFI, 0);
    t->p[P_E1] = RW_BYTE;
    t->p[P_E4] = 64;                                    /* (MOTION x1) */
    t->p[P_LTIME] = time;
    t->p[P_VOICE] = V_POLY;
    return t;
}
static voice_t *newest(track_t *t)
{
    voice_t *v = 0;
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && (!v || t->v[i].age > v->age))
            v = &t->v[i];
    return v;
}
int main(void)
{
    track_t *t;
    voice_t *a, *b;
    uint32_t sr, q, s;
    t = math_track(127);
    check("TIME's default is NOTE (127 stored: MOTION as it is)", lofi_mo(t->p) == t->p[P_E4] && track_desc(t, P_LTIME)->def == 127 &&
          str_eq(track_desc(t, P_LTIME)->label, "TIME"), 0);
    transport_req = 1;
    blocks(40);
    trk_note_on(t, 60, 100);
    blocks(30);
    a = newest(t);
    trk_note_on(t, 64, 100);
    blocks(1);
    b = newest(t);
    check("NOTE: the second note's slow time from its own start (a restart)", a && b && a != b && b->ph[1] < a->ph[1] / 8u,
          "first %u, second %u", a ? a->ph[1] : 0u, b ? b->ph[1] : 0u);
    transport_req = 2;
    blocks(2);

    t = math_track(32);                                 /* (32 * 10 >> 7 = 2: 1/16) */
    q = (uint32_t)lofi_mo(t->p);
    transport_req = 1;
    blocks(40);
    trk_note_on(t, 60, 100);
    blocks(30);
    a = newest(t);
    trk_note_on(t, 64, 100);
    blocks(1);
    b = newest(t);
    s = lofi_sync(&sr);
    check("1/16: SYNC at MOTION 64's speed (x1: 2048 t a 1/16)", q == 128u + 64u && lofi_motion((int32_t)q) == 256u, "MOTION %u", q);
    check("1/16: the second note goes on where the first is (one time, no restart)", a && b && a != b && (a->ph[1] >> 8) == (b->ph[1] >> 8) &&
          b->ph[1] > 1024u * 256u, "first %u, second %u", a ? a->ph[1] : 0u, b ? b->ph[1] : 0u);
    check("1/16: that time the transport's (2048 t a 1/16, MOTION not used)",
          b && (int32_t)((b->ph[1] >> 8) - s) < 64 && (int32_t)((b->ph[1] >> 8) - s) > -64, "voice %u, transport %u", b ? b->ph[1] >> 8 : 0u, s);
    t->p[P_E4] = 120;                                   /* (MOTION up: the same time) */
    blocks(1);
    s = b->ph[1];
    t->p[P_E4] = 0;
    blocks(1);
    check("1/16: MOTION does not move it (the step a block the same at 0 and 120)", b->ph[1] - s < 2u * CTL * 256u * 2u, 0);
    transport_req = 2;
    blocks(2);
    {
        static const int16_t V[3] = {127, 110, 0};
        static const char *const N[3] = {"NOTE", "1/1", "1/32"};
        const param_desc_t *d = track_desc(t, P_LTIME);
        char val[12];
        const char *u;
        int ok = 1, i;
        for (i = 0; i < 3; i++) {
            param_format(d, V[i], val, &u);
            ok &= str_eq(val, N[i]);
        }
        check("TIME's names over the stored value: 127 NOTE, 110 1/1, 0 1/32", ok, 0);
    }
    printf("math_time: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
