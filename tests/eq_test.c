/* (p9) the master's EQ (fx.c eq_block, FX > EQ: params.c eq_v) through the real sources (ui_test.c's):
 * 1. flat (all 0): bit for bit the input (the four bands sum back to it);
 * 2. each band +6 / -6 dB: a tone in it moves at least half of it, more than any other tone (broad one-pole bands,
 *    a Baxandall's gentleness: the neighbours follow a little);
 * 3. kept with a project ('E' 'Q'), one without it opens flat.
 * (V10.7.30) BALANCE (fx.c bal_ctl, params.c bal_v) and the DIST NOISE (dnz_v):
 * 4. a mix heavy in the low mids: after a few seconds their tone down 2 .. 8 dB more than the others move; never a boost
 *    of them (a mix light in them: their gain stays 0); AMOUNT 0: bit for bit the input again; silence holds the gains;
 * 5. kept with a project ('X' 'M'), one without it opens with BALANCE at BAL_DEF and no NOISE. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
static void ck(const char *w, int ok) { printf("eq: %-96s %s\n", w, ok ? "ok" : "FAIL"); bad += !ok; }
static double level(double f, int band, int db)
{
    uint32_t b, i; double ph = 0, acc = 0; int k;
    memset(&eqs, 0, sizeof eqs);
    bal_v = 0;                                          /* (the EQ alone) */
    for (k = 0; k < 4; k++) eq_v[k] = 0;
    if (band >= 0) eq_v[band] = (int16_t)db;
    for (b = 0; b < 300u; b++) {
        for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = (int32_t)(40000 * sin(ph)); wet[i] = 0; ph += 2 * M_PI * f / FS; }
        eq_block(CTL);
        if (b >= 100u) for (i = 0; i < CTL; i++) acc += (double)mix_l[i] * mix_l[i];
    }
    return 10 * log10(acc / (200.0 * CTL) / (40000.0 * 40000.0 / 2));
}
int main(void)
{
    static const double F[4] = {60, 450, 2000, 9000};
    int band, ok, k;
    double l;
    {   uint32_t i, same = 1;
        memset(&eqs, 0, sizeof eqs); for (k = 0; k < 4; k++) eq_v[k] = 0;
        bal_v = 0;
        for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = (int32_t)(i * 997u % 60000u) - 30000; wet[i] = 0; }
        { int32_t ref[CTL]; for (i = 0; i < CTL; i++) ref[i] = mix_l[i]; eq_block(CTL); for (i = 0; i < CTL; i++) same &= mix_l[i] == ref[i]; }
        ck("1. flat: bit for bit the input", same);
    }
    for (band = 0; band < 4; band++) {
        int db;
        for (db = -6; db <= 6; db += 12) {
            double own = 0, other = 0;
            printf("eq:   band %d %+d dB:", band, db);
            for (k = 0; k < 4; k++) {
                l = level(F[k], band, db);
                printf(" %5.0f Hz %+5.1f", F[k], l);
                if (k == band) own = l; else if (fabs(l) > fabs(other)) other = l;
            }
            ok = fabs(own) >= fabs(db) / 2.0 && (own > 0) == (db > 0) && fabs(own) > fabs(other);
            printf("\n");
            ck(db > 0 ? "2. the band +6 dB: its tone up at least 3 dB, more than any other" : "   the band -6 dB the same", ok);
        }
    }
    {
        static project_store_t st;
        ui_power_on();
        eq_v[0] = 5; eq_v[1] = -3; eq_v[2] = 0; eq_v[3] = 12;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        eq_v[0] = eq_v[1] = eq_v[2] = eq_v[3] = 0;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;   /* (V10.58: set when the project is opened, not by the read) */
        ck("3. a project keeps the EQ (5 -3 0 12)", ok && eq_v[0] == 5 && eq_v[1] == -3 && eq_v[2] == 0 && eq_v[3] == 12);
        eq_v[0] = eq_v[1] = eq_v[2] = eq_v[3] = 0;
        project_capture(&proj_scratch); ok = proj_pack(&st, &proj_scratch);
        eq_v[1] = 7;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;   /* (V10.58: set when the project is opened, not by the read) */
        ck("   one without it opens flat", ok && !eq_v[0] && !eq_v[1] && !eq_v[2] && !eq_v[3]);
    }
    {   /* 4. BALANCE: tones at 60, 450, 2000, 9000 Hz (a balanced mix's tilt, roughly), the 450 Hz one `mud` dB off it */
        static const double AMP[4] = {9000, 4000, 1600, 700};
        double ph[4] = {0, 0, 0, 0};
        uint32_t b, i, r;
        for (r = 0; r < 2u; r++) {
            double mud = r ? -8 : 10;
            int32_t c1 = 0;
            memset(&eqs, 0, sizeof eqs); for (k = 0; k < 4; k++) eq_v[k] = 0;
            bal_v = BAL_DEF;
            for (b = 0; b < 4000u; b++) {               /* (~5.8 s) */
                for (i = 0; i < CTL; i++) {
                    double v = 0;
                    for (k = 0; k < 4; k++) {
                        v += AMP[k] * (k == 1 ? pow(10, mud / 20) : 1) * sin(ph[k]);
                        ph[k] += 2 * M_PI * F[k] / FS;
                    }
                    mix_l[i] = mix_r[i] = (int32_t)v; wet[i] = 0;
                }
                eq_block(CTL);
                if (eqs.c[1] > c1) c1 = eqs.c[1];
            }
            printf("eq:   BALANCE, the low mids %+.0f dB: its bands' gains %+.1f %+.1f %+.1f %+.1f dB\n", mud, eqs.c[0] / 256.0 / 42.52,
                   eqs.c[1] / 256.0 / 42.52, eqs.c[2] / 256.0 / 42.52, eqs.c[3] / 256.0 / 42.52);
            if (!r)                                     /* (log2 Q16: 1 dB 10885) */
                ck("4. BALANCE: low mids piled up: their band down 4 .. 12 dB, the lows within +-1.5 dB",
                   eqs.c[1] <= -4 * 10885 && eqs.c[1] >= -510 * 256 && eqs.c[0] >= -64 * 256 && eqs.c[0] <= 64 * 256);
            else
                ck("   low mids lacking: never boosted (their gain stays 0)", c1 == 0 && eqs.c[1] == 0);
        }
        {   /* AMOUNT 0: back to bit for bit */
            int32_t ref[CTL], same = 1;
            bal_v = 0;
            for (b = 0; b < 3000u; b++) {
                for (i = 0; i < CTL; i++) { mix_l[i] = mix_r[i] = (int32_t)(i * 997u % 60000u) - 30000; wet[i] = 0; ref[i] = mix_l[i]; }
                eq_block(CTL);
            }
            for (i = 0; i < CTL; i++) same &= mix_l[i] == ref[i];
            ck("   AMOUNT 0: settles back to the input bit for bit (the EQ flat)", same && !eqs.g[0] && !eqs.g[1] && !eqs.g[2] && !eqs.g[3]);
        }
        {   /* silence holds what BALANCE had */
            int32_t keep[4];
            bal_v = BAL_DEF;
            for (b = 0; b < 3000u; b++) {
                for (i = 0; i < CTL; i++) { double v = 4000 * sin(ph[1]) + 9000 * sin(ph[0]); ph[0] += 2 * M_PI * 60 / FS; ph[1] += 2 * M_PI * 450 / FS; mix_l[i] = mix_r[i] = (int32_t)v; wet[i] = 0; }
                eq_block(CTL);
            }
            for (b = 0; b < 3100u; b++) {               /* (after 100 blocks the band filters' own tails are out) */
                if (b == 100u) for (k = 0; k < 4; k++) keep[k] = eqs.e[k];
                for (i = 0; i < CTL; i++) mix_l[i] = mix_r[i] = wet[i] = 0;
                eq_block(CTL);
            }
            ck("   silence: the followers held (the balance kept for the next sound)", keep[0] == eqs.e[0] && keep[1] == eqs.e[1] && keep[2] == eqs.e[2] && keep[3] == eqs.e[3]);
        }
    }
    {   /* 5. kept with a project */
        static project_store_t st;
        ui_power_on();
        ck("5. power-on: BALANCE at BAL_DEF, no NOISE", bal_v == BAL_DEF && !dnz_v[0] && !dnz_v[NTRK - 1]);
        bal_v = 20; dnz_v[2] = 70;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        bal_v = 99; dnz_v[2] = 0;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;
        ck("   a project keeps BALANCE 20 and T3's NOISE 70", ok && bal_v == 20 && dnz_v[2] == 70 && !dnz_v[0]);
        bal_v = BAL_DEF; dnz_v[2] = 0;
        project_capture(&proj_scratch); ok = proj_pack(&st, &proj_scratch);
        bal_v = 0; dnz_v[1] = 40;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st);
        ok = ok && project_restore_runtime(&proj_scratch) == 0;
        ck("   one without it ('X' 'M' not written: every project before) opens with BALANCE on (BAL_DEF), NOISE 0",
           ok && bal_v == BAL_DEF && !dnz_v[1]);
    }
    printf("eq: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
