// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.69, the owner) MATH FLOAT's MORPH between ALGO A and ALGO B (where CHIP was; MORPH where BEND was): MORPH 0
 * A alone, bit for bit as before; MORPH 127 B alone (as ALGO B on its own); between, neither. Built and run by CI
 * (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static uint32_t take(uint32_t a, uint32_t b, int16_t mo)   /* FLOAT, ALGO a, ALGO B b, MORPH mo, a fresh voice at C4's
                                                         * rate, the engine's render alone: its hash */
{
    static track_t t;
    static voice_t v;
    static int32_t o[CTL];
    vmod_t m;
    tsvf_t flt;
    uint32_t k, i, h = 2166136261u;
    memset(&t, 0, sizeof t); memset(&v, 0, sizeof v); memset(&m, 0, sizeof m);
    t.p[P_E1] = RW_FLOAT; t.p[P_E2] = (int16_t)(a * 4u); t.p[P_E0] = (int16_t)(b * 4u); t.p[P_E6] = mo;
    t.p[P_FCUT] = 127; t.p[P_FRES] = 50;
    m.amp0 = m.amp1 = 32767; m.shape = 64 << 8;
    tsvf_coef(&flt, 127 << 8, 50);
    lofi_note_on(&t, &v);
    for (k = 0; k < 100u; k++) {
        memset(o, 0, sizeof o);
        lofi_float_render(&t, &v, o, CTL, &m, pitch_inc(60 << 4), &flt);
        for (i = 0; i < CTL; i++) h = (h ^ (uint32_t)o[i]) * 16777619u;
    }
    return h;
}
int main(void)
{
    int bad = 0;
    uint32_t a0 = take(7, 28, 0), a1 = take(7, 0, 0), b0 = take(28, 7, 0), b1 = take(7, 28, 127), m = take(7, 28, 64);
    bad += check("MATH FLOAT MORPH 0: ALGO A alone (ALGO B no matter)", a0 == a1);
    bad += check("MATH FLOAT MORPH 127: ALGO B alone (as ALGO = B)", b1 == b0);
    bad += check("MATH FLOAT MORPH 64: between, neither A nor B", m != a0 && m != b0);
    printf(bad ? "math-morph: %d FAILED\n" : "math-morph: all ok\n", bad);
    return bad != 0;
}
