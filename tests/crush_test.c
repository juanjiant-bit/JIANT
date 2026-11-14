/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.33) CRUSH, the track's own (fx.c track_crush, crush_block; params.c cru_v; EDIT > VOICES 3), through the real
 * sources (ui_test.c's):
 * 1. CRUSH 0: bit for bit the input (not run);
 * 2. the sound of DIST TYPE CRUSH at that DRIVE: against the code before (old_dist_crush below, fx.c track_dist with
 *    dist_shape_x's CRUSH as V10.7.32 had them), DRIVE 1 .. 127 and TONE -64 .. 63, the same samples within 4 (of
 *    2^18: the blend's last bits once fully in) from the same random state;
 * 3. with DIST on (another TYPE) both run, DIST first; DIST's TYPE list without CRUSH (SOFT HARD FOLD RECT AMP);
 * 4. kept with a project ('X' 'C'); a project of before (b[65] 'V': DIST TYPE had CRUSH) loads its CRUSH tracks as
 *    CRUSH at their DRIVE with DIST off (SOFT), RECT and AMP one down; today's RECT stays RECT;
 * 5. a user preset of before (no UP_DT_MARK) migrates the same: DIST off, its DRIVE the track's CRUSH when loaded.
 * With a directory argument: crush_antes_dist_tipo_crush_70.wav and crush_despues_play3_crush_70.wav there (the same
 * chord, DIST TYPE CRUSH at DRIVE 70 as V10.7.32 had it, and CRUSH 70; renders/v33a) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static int bad;
static void ck(const char *w, int ok) { printf("crush: %-94s %s\n", w, ok ? "ok" : "FAIL"); bad += !ok; }

/* V10.7.32's DIST TYPE CRUSH (fx.c track_dist's loop, dist_shape_x's CRUSH half), on its own state */
typedef struct { int32_t hp, l1, l2, dc, hold, cl, cb, w; uint8_t hc; } od_t;
static void old_dist_crush(od_t *s, int32_t d, int32_t tone, int32_t *b, uint32_t n)
{
    int32_t i, g, k, mk, w0, w1, dw, full;
    w0 = s->w;
    w1 = d >= 8 ? 4096 : d * 512;
    if (w1 != w0)
        w1 = w1 - w0 > 64 || w0 - w1 > 64 ? w0 + (w1 - w0) / 2 : w1;
    s->w = w1;
    if (!w0 && !w1)
        return;
    if (!w0)
        s->l1 = s->l2 = 0, s->hp = 0;
    dw = (w1 - w0) >> CTL_LOG2;
    full = w0 >= 4096 && w1 >= 4096;
    g = 4096 + d * d * 2;
    k = clamp(32000 - d * 95 + tone * 300, 2500, 32767);
    mk = 30000 - d * 60;
    for (i = 0; i < (int32_t)n; i++) {
        int32_t x = b[i], y, st0 = (int32_t)(786432u / (uint32_t)(12 + d));
        s->hp += (x - s->hp + 64) >> 7;
        x = clamp(x - s->hp, -230000, 230000);
        if (s->hc != 3u + 16u) {
            s->hc = 3u + 16u;
            s->dc = s->hold = s->cl = 0;
            s->cb = st0;
        }
        if ((s->cl += s->cb) >= 65536) {
            uint32_t r = dist_rnd | 1u, sh = 4u + (uint32_t)d / 12u;
            s->cl &= 65535;
            s->dc = s->hold;
            s->hold = (int32_t)((uint32_t)((clamp(x, -131072, 131071) + (1 << (sh - 1u))) >> sh) << sh);
            r ^= r << 13; r ^= r >> 17; r ^= r << 5;
            dist_rnd = r;
            s->cb = clamp(st0 + (st0 >> 6) * ((int32_t)r >> 26), 1024, 65535);
        }
        y = s->dc + ((((s->hold - s->dc) >> 2) * (s->cl >> 6)) >> 8);
        y = clamp(((y >> 2) * g) >> 10, -26000, 26000);
        s->l1 += mulq15(y - s->l1, k);
        s->l2 += mulq15(s->l1 - s->l2, k);
        y = mulq15(s->l2, mk);
        b[i] = full ? y : b[i] + ((((y - b[i]) >> 2) * (w0 + dw * (i + 1))) >> 10);
    }
}
static int32_t sig(uint32_t i)                          /* a chord of three saws, ~-6 dBFS of the track's scale */
{
    static const uint32_t F[3] = {110, 138, 165};
    int32_t v = 0;
    uint32_t k;
    for (k = 0; k < 3u; k++) v += (int32_t)((i * F[k] * 65536u / FS) & 65535u) - 32768;
    return v;
}
static void wav_out(const char *dir, const char *name, const int32_t *y, uint32_t n)
{
    char p[512];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    if (!(f = fopen(p, "wb"))) return;
    wav_hdr(f, n);
    for (i = 0; i < n; i++) wav_put(f, y[i] >> 2, y[i] >> 2);
    fclose(f);
    printf("crush: wrote %s\n", p);
}
int main(int argc, char **argv)
{
    static int32_t a[FS * 3], c[FS * 3];
    uint32_t i, j, n = FS * 3u / CTL * CTL;              /* (whole blocks) */
    int ok;
    track_t *t;
    ui_power_on();
    t = &trk[0];
    {   /* 1 */
        int32_t b[CTL];
        uint32_t same = 1;
        cru_v[0] = 0;
        t->p[P_DIST] = 0;
        for (i = 0; i < 50u; i++) {
            for (j = 0; j < CTL; j++) b[j] = sig(i * CTL + j);
            track_dist(t, b, CTL);
            track_crush(t, b, CTL);
            for (j = 0; j < CTL; j++) same &= b[j] == sig(i * CTL + j);
        }
        ck("1. CRUSH 0 (DIST 0): bit for bit the input, not run", same && !cru_s[0].w);
    }
    {   /* 2 */
        static const int16_t D[] = {1, 3, 7, 8, 20, 45, 70, 100, 127}, T[] = {-64, 0, 40, 63};
        int32_t worst = 0;
        uint32_t di, ti, nz = 1;
        for (di = 0; di < NELEM(D); di++)
            for (ti = 0; ti < NELEM(T); ti++) {
                od_t s;
                memset(&s, 0, sizeof s);
                memset(&cru_s[0], 0, sizeof cru_s[0]);
                t->p[P_DIST] = 0;
                t->p[P_DTONE] = T[ti];
                cru_v[0] = D[di];
                dist_rnd = 0x2545F491u;
                for (i = 0; i < n; i += CTL) {
                    for (j = 0; j < CTL; j++) a[i + j] = sig(i + j);
                    old_dist_crush(&s, D[di], T[ti], &a[i], CTL);
                }
                dist_rnd = 0x2545F491u;
                for (i = 0; i < n; i += CTL) {
                    for (j = 0; j < CTL; j++) c[i + j] = sig(i + j);
                    track_dist(t, &c[i], CTL);
                    track_crush(t, &c[i], CTL);
                }
                for (i = 0; i < n; i++) {
                    int32_t e = a[i] - c[i];
                    e = e < 0 ? -e : e;
                    worst = e > worst ? e : worst;
                }
                for (i = n / 2u, j = 0; i < n; i++) j += c[i] != sig(i);
                nz &= j > n / 4u;
            }
        printf("crush:   DRIVE 1 .. 127, TONE -64 .. 63 against DIST TYPE CRUSH before: at most %d apart (of 2^18)\n", worst);
        ck("2. CRUSH d sounds as DIST TYPE CRUSH at DRIVE d did (the code before), its TONE too: within 4", worst <= 4 && nz);
    }
    {   /* 3 */
        int32_t b[CTL], b2[CTL];
        uint32_t diff = 0;
        const param_desc_t *d = track_desc(t, P_DTYPE);
        memset(&cru_s[0], 0, sizeof cru_s[0]);
        dist_w[0] = 0;
        t->p[P_DIST] = 60; t->p[P_DTYPE] = 1; t->p[P_DTONE] = 0;
        cru_v[0] = 50;
        for (i = 0; i < 40u; i++) {
            for (j = 0; j < CTL; j++) b[j] = b2[j] = sig(i * CTL + j);
            track_dist(t, b, CTL);
            track_crush(t, b, CTL);
        }
        cru_v[0] = 0;
        memset(&cru_s[0], 0, sizeof cru_s[0]);
        dist_w[0] = 0;
        t->dist_hp = t->dist_lp1 = t->dist_lp2 = 0;
        for (i = 0; i < 40u; i++) {
            for (j = 0; j < CTL; j++) b2[j] = sig(i * CTL + j);
            track_dist(t, b2, CTL);
        }
        for (j = 0; j < CTL; j++) diff += b[j] != b2[j];
        ok = diff > CTL / 2u && d->max == 4 && str_eq(d->names[0], "SOFT") && str_eq(d->names[2], "FOLD") &&
             str_eq(d->names[3], "RECT") && str_eq(d->names[4], "AMP");
        ck("3. DIST HARD and CRUSH together: both run; DIST TYPE SOFT HARD FOLD RECT AMP (no CRUSH)", ok);
        t->p[P_DIST] = 0; t->p[P_DTYPE] = 0;
    }
    {   /* 4 */
        static project_store_t st;
        uint32_t sum;
        ui_power_on();
        cru_v[1] = 90; cru_v[4] = 12;
        trk[2].p[P_DTYPE] = 3;                           /* (today's RECT) */
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch) && st.raw[65] == 'W';
        cru_v[1] = cru_v[4] = 0; trk[2].p[P_DTYPE] = 0;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("4. CRUSH kept with a project ('X' 'C': T2 90, T5 12); today's RECT stays RECT", ok && cru_v[1] == 90 &&
           cru_v[4] == 12 && !cru_v[0] && trk[2].p[P_DTYPE] == 3);
        ui_power_on();
        memset(cru_v, 0, sizeof cru_v);                  /* (the tests' power-on keeps it: the device's pool starts at 0) */
        trk[0].p[P_DIST] = 77; trk[0].p[P_DTYPE] = 3;   /* (as V10.7.32 stored them: CRUSH 3, RECT 4, AMP 5; today's */
        trk[1].p[P_DIST] = 40; trk[1].p[P_DTYPE] = 4;   /* packer takes 0 .. 4: AMP's 5 checked on proj_crush_mig below) */
        trk[4].p[P_DIST] = 30; trk[4].p[P_DTYPE] = 2;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        st.raw[65] = 'V';                                /* (a FUNC of V10.7.6 .. V10.7.32) */
        sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u); memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
        ui_power_on();
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   a project of before: DIST TYPE CRUSH at DRIVE 77 -> CRUSH 77, DIST 0 SOFT; RECT AMP one down, FOLD kept",
           ok && cru_v[0] == 77 && trk[0].p[P_DIST] == 0 && trk[0].p[P_DTYPE] == 0 && trk[1].p[P_DTYPE] == 3 &&
           trk[1].p[P_DIST] == 40 && trk[4].p[P_DTYPE] == 2 && !cru_v[1] && !cru_v[4]);
        proj_scratch.t[3].p[P_DTYPE] = 5;               /* (an AMP of before) */
        proj_crush_mig(&proj_scratch);
        ck("   .. AMP (5 before) is 4", proj_scratch.t[3].p[P_DTYPE] == 4);
    }
    {   /* 5 */
        up_rec_t r;
        int16_t v[P_COUNT];
        ui_power_on();
        memset(&r, 0, sizeof r);
        r.used = UP_USED; r.ver = UP_VER; r.engine = 0; r.np = P_COUNT;
        memcpy(r.name, "OLD CRUSH", 9);
        for (i = 0; i < P_COUNT; i++) up_set_value(&r, i, param_store_def(0, i));
        r.packed[UP_FM_AT] = UP_V04_MARK; r.packed[UP_VEL_AT] = UP_VEL_MARK;   /* (V10.7.6 .. V10.7.32: no UP_DT_MARK) */
        up_set_value(&r, P_DIST, 66); up_set_value(&r, P_DTYPE, 3);
        *up_rec(5) = r;
        up_migrate(up_rec(5));
        up_values(up_rec(5), v);
        ok = v[P_DIST] == 0 && v[P_DTYPE] == 0 && up_rec(5)->packed[UP_DT_AT] == UP_DT_MARK;
        song.sel = 2;
        ok &= up_load(5) == 0 && cru_v[2] == 66 && trk[2].p[P_DIST] == 0;
        up_set_value(&r, P_DTYPE, 5);                    /* an AMP of before */
        *up_rec(6) = r;
        up_migrate(up_rec(6));
        up_values(up_rec(6), v);
        ok &= v[P_DTYPE] == 4 && v[P_DIST] == 66;
        r.packed[UP_DT_AT] = UP_DT_MARK;                 /* today's RECT */
        up_set_value(&r, P_DTYPE, 3);
        *up_rec(7) = r;
        up_migrate(up_rec(7));
        up_values(up_rec(7), v);
        ok &= v[P_DTYPE] == 3 && v[P_DIST] == 66;
        ck("5. a user preset of before: DIST TYPE CRUSH -> CRUSH (the track's, when loaded), DIST off; AMP one down", ok);
        memset(up_rec(5), 0, sizeof r); memset(up_rec(6), 0, sizeof r); memset(up_rec(7), 0, sizeof r);
        song.sel = 0;
    }
    if (argc > 1) {                                      /* the renders: the same chord, before and now */
        od_t s;
        ui_power_on();
        memset(&s, 0, sizeof s);
        memset(&cru_s[0], 0, sizeof cru_s[0]);
        t->p[P_DIST] = 0; t->p[P_DTONE] = 0;
        cru_v[0] = 70;
        dist_rnd = 0x2545F491u;
        for (i = 0; i < n; i += CTL) {
            for (j = 0; j < CTL; j++) a[i + j] = sig(i + j);
            old_dist_crush(&s, 70, 0, &a[i], CTL);
        }
        dist_rnd = 0x2545F491u;
        for (i = 0; i < n; i += CTL) {
            for (j = 0; j < CTL; j++) c[i + j] = sig(i + j);
            track_dist(t, &c[i], CTL);
            track_crush(t, &c[i], CTL);
        }
        wav_out(argv[1], "crush_antes_dist_tipo_crush_70.wav", a, n);
        wav_out(argv[1], "crush_despues_play3_crush_70.wav", c, n);
        cru_v[0] = 0;
    }
    printf("crush: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
