/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.71, JIANT BYTE 43923fd4's tests/ott_test.c, GPL, on FM) FX > OTT, a track's two-band upward / downward
 * compressor (fx.c ott_block; params.c ott_v): a quiet band comes up, a loud one goes down, each band on its own; DEPTH 0
 * the track's sound bit for bit (not run), and back to it after DEPTH goes down; silence stays silence; no value out of
 * range on any input; the overload's guard eases it (never cuts the sound); kept with a project ('O' 'T', its TIME
 * UP/DN X-OVER at DEPTH 0 too) and a user preset, an older one loading with it off; the page (FX > TRACK's fourth).
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/ott_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <math.h>
static int bad;
static void ck(const char *what, int ok) { printf("ott: %-100s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static double run_sine(uint32_t ti, double amp, double hz, uint32_t nblk, double *out_rms, int32_t *mx)
{
    static int32_t b[CTL];
    double ein = 0, eout = 0;
    uint32_t k, i;
    *mx = 0;
    for (k = 0; k < nblk; k++) {
        for (i = 0; i < CTL; i++) b[i] = (int32_t)lrint(amp * sin(2 * M_PI * hz * (k * CTL + i) / 44100.0));
        if (k >= nblk / 2) for (i = 0; i < CTL; i++) ein += (double)b[i] * b[i];
        if (ott_on(ti)) ott_block(ti, b, CTL);
        if (k >= nblk / 2) for (i = 0; i < CTL; i++) { eout += (double)b[i] * b[i]; if (abs(b[i]) > *mx) *mx = abs(b[i]); }
    }
    *out_rms = sqrt(eout / (nblk / 2 * CTL));
    return 10 * log10((eout + 1) / (ein + 1));          /* (the gain, dB) */
}
static void ott_set(uint32_t ti, int d, int tm, int ud, int xo)
{
    ott_v[ti][OT_DEPTH] = (int16_t)d; ott_v[ti][OT_TIME] = (int16_t)tm; ott_v[ti][OT_UPDN] = (int16_t)ud; ott_v[ti][OT_XO] = (int16_t)xo;
}
static uint32_t part_hash(uint32_t nblk, int depth_until)
{
    track_t *t = &trk[0];
    uint32_t h = 2166136261u, b, i;
    ui_power_on();
    host_preset(t, 0, 0);
    memset(ott_s, 0, sizeof ott_s);
    ott_defaults();
    rng_state = 99;
    for (b = 0; b < nblk; b++) {
        if (b == 2) trk_note_on(t, 48, 110);
        ott_v[0][OT_DEPTH] = (int16_t)((int)b < depth_until ? 100 : 0);
        memset(mix_l, 0, sizeof mix_l); memset(mix_r, 0, sizeof mix_r);
        memset(send_c, 0, sizeof send_c); memset(send_d, 0, sizeof send_d); memset(send_r, 0, sizeof send_r);
        mix_part_m(t, CTL);
        if (b >= 60) for (i = 0; i < CTL; i++) h = (h ^ (uint32_t)mix_l[i]) * 16777619u;
    }
    return h;
}
int main(void)
{
    double r, g;
    int32_t mx;
    uint32_t i, k;
    ui_power_on();
    ck("power-on: every track's OTT off at its defaults (DEPTH 0, TIME 72, UP/DN 0, X-OVER 37), not run",
       ott_v[0][OT_DEPTH] == 0 && ott_v[5][OT_TIME] == 72 && ott_v[3][OT_UPDN] == 0 && ott_v[2][OT_XO] == 37 && !ott_on(0) && !ott_on(5));
    {   /* 1. a quiet band up, a loud one down (DEPTH 100), each band its own; UP/DN takes from one side */
        double q_lo, q_hi, l_lo, l_hi, q_none, l_none, q_mix;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 0, 37);
        q_lo = run_sine(0, 1300, 80, 400, &r, &mx);          /* -40 dB of full scale (2^17), under X-OVER */
        memset(ott_s, 0, sizeof ott_s);
        q_hi = run_sine(0, 1300, 3000, 400, &r, &mx);        /* .. over it */
        memset(ott_s, 0, sizeof ott_s);
        l_lo = run_sine(0, 120000, 80, 400, &r, &mx);        /* -0.8 dB */
        memset(ott_s, 0, sizeof ott_s);
        l_hi = run_sine(0, 120000, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 63, 37);                          /* UP/DN +: no downward */
        l_none = run_sine(0, 120000, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, -64, 37);                         /* UP/DN -: no upward */
        q_none = run_sine(0, 1300, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 50, 50, 0, 37);                            /* half the DEPTH: about half the gain */
        q_mix = run_sine(0, 1300, 3000, 400, &r, &mx);
        printf("ott: quiet low %+.1f dB, quiet high %+.1f, loud low %+.1f, loud high %+.1f; UP/DN +63 loud %+.1f, -64 quiet %+.1f; DEPTH 50 quiet %+.1f\n",
               q_lo, q_hi, l_lo, l_hi, l_none, q_none, q_mix);
        ck("1. DEPTH 100 (V10.8.71, BYTE v1.1b4's law): a quiet band up (+18 .. +28 dB), a loud one down (-6 .. -24 dB)",
           q_lo > 18 && q_lo < 28.5 && q_hi > 18 && q_hi < 28.5 && l_lo < -6 && l_lo > -24.5 && l_hi < -6 && l_hi > -24.5);
        ck("   UP/DN +63: no downward (a loud band within 1 dB: the limiter); -64: no upward (within 0.3 dB); DEPTH 50 a quarter",
           fabs(l_none) < 1.0 && fabs(q_none) < 0.3 && q_mix > q_hi * 0.15 && q_mix < q_hi * 0.4);
        {   /* the level kept even: a -40 dBFS sine and a -6 dBFS one, within 6 dB of each other (full scale 2^17) */
            double r40, r6;
            memset(ott_s, 0, sizeof ott_s);
            ott_set(0, 100, 72, 0, 37);
            (void)run_sine(0, 1311, 1000, 400, &r40, &mx);
            memset(ott_s, 0, sizeof ott_s);
            (void)run_sine(0, 65700, 1000, 400, &r6, &mx);
            printf("ott: -40 dBFS in: %.1f dBFS out; -6 dBFS in: %.1f dBFS out (rms, peak %d)\n",
                   20 * log10(r40 * 1.4142 / 131072), 20 * log10(r6 * 1.4142 / 131072), mx);
            ck("   DEPTH 100: -40 dBFS and -6 dBFS come out within 6 dB of each other, no sample past full scale",
               fabs(20 * log10(r40 / r6)) < 6 && mx < 131072);
        }
    }
    {   /* 2. a quiet low under a loud high: the low comes up, the high goes down (the bands apart) */
        static int32_t b[CTL];
        double elo = 0, elo0 = 0;
        uint32_t n;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 0, 37);
        for (n = 0; n < 400; n++) {
            for (i = 0; i < CTL; i++) {
                double tt = (n * CTL + i) / 44100.0;
                b[i] = (int32_t)lrint(1300 * sin(2 * M_PI * 60 * tt) + 100000 * sin(2 * M_PI * 4000 * tt));
            }
            ott_block(0, b, CTL);
        }
        (void)elo; (void)elo0;
        ck("2. a quiet low under a loud high: LOW's gain up, HIGH's down (each band its own level)",
           ott_s[0].g[0] > 0 && ott_s[0].g[1] < -50);
    }
    {   /* 3. silence stays silence; anything in, nothing out of range (the integers' path: no NaN, no denormal) */
        static int32_t b[CTL];
        int ok = 1;
        uint32_t n, rs = 12345u;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(1, 100, 30, 0, 15);
        for (n = 0; n < 300; n++) {
            memset(b, 0, sizeof b);
            ott_block(1, b, CTL);
            for (i = 0; i < CTL; i++) ok &= b[i] == 0;
        }
        ck("3. DEPTH 100, silence in: silence out (no hiss brought up), its gain within the bounds", ok && ott_s[1].g[0] <= OT_GUP && ott_s[1].g[0] >= 0);
        for (k = 0; k < 4u; k++) {
            ott_set(1, 100, k ? 30 + (int)k * 23 : 30, k & 1 ? 63 : -64, k ? 100 : 15);
            for (n = 0; n < 500; n++) {
                for (i = 0; i < CTL; i++) {
                    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
                    b[i] = n % 50 < 25 ? (int32_t)rs >> (n % 7 == 0 ? 8 : 14) : (int32_t)(rs & 15u) - 8;   /* (bursts of noise past
                                                         * full scale, near-silence between) */
                }
                ott_block(1, b, CTL);
                for (i = 0; i < CTL; i++) ok &= b[i] > -(1 << 25) && b[i] < (1 << 25);
                ok &= ott_s[1].g[0] >= -OT_GDN && ott_s[1].g[0] <= OT_GUP && ott_s[1].g[1] >= -OT_GDN && ott_s[1].g[1] <= OT_GUP;
                ok &= ott_s[1].lv[0] >= 0 && ott_s[1].lv[1] >= 0;
            }
        }
        ck("   noise bursts past full scale and near-silence, every setting: every sample and gain within range", ok);
    }
    {   /* 4. DEPTH 0: not run, bit for bit; after DEPTH 100 then 0: back to the same sound within a block, then not run */
        uint32_t h0 = part_hash(160, 0), h1 = part_hash(160, 40), h2 = part_hash(160, 1000), h3 = part_hash(160, 0);
        int quiet;
        quiet = !ott_on(0);
        ck("4. DEPTH 0 bit-exact (not run); DEPTH 100 for 40 blocks then 0: back to the dry part bit for bit", h0 == h3 && h0 == h1 && quiet);
        ck("   DEPTH 100 throughout: the part changed", h2 != h0);
        {   /* the fade: gains back at 1 after one block, the state cleared */
            static int32_t b[CTL];
            memset(ott_s, 0, sizeof ott_s);
            ott_set(2, 100, 50, 0, 37);
            for (k = 0; k < 100u; k++) {
                for (i = 0; i < CTL; i++) b[i] = (int32_t)lrint(1300 * sin(2 * M_PI * 200 * (k * CTL + i) / 44100.0));
                ott_block(2, b, CTL);
            }
            g = ott_s[2].g[0];
            ott_v[2][OT_DEPTH] = 0;
            ott_block(2, b, CTL);
            ck("   DEPTH to 0: a block's ramp to unity, then out (ott_on 0, the state cleared)",
               g > 0 && !ott_on(2) && !ott_s[2].lv[0] && !ott_s[2].lp);
        }
    }
    {   /* 5. the overload's guard (voice.c): half the effect tripped, none while the load stays past OT_DEEP (the sound
         * itself as it is) */
        double full, half, none;
        memset(ott_s, 0, sizeof ott_s);
        ott_set(0, 100, 50, 0, 37);
        load_tight = 0; load_avg_q8 = 0;
        full = run_sine(0, 1300, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        load_tight = 1; load_avg_q8 = 80u << 8;
        half = run_sine(0, 1300, 3000, 400, &r, &mx);
        memset(ott_s, 0, sizeof ott_s);
        load_avg_q8 = (OT_DEEP + 5u) << 8;
        none = run_sine(0, 1300, 3000, 400, &r, &mx);
        load_tight = 0; load_avg_q8 = 0;
        printf("ott: guard: off %+.1f dB, tripped %+.1f, past OT_DEEP %+.2f (the level the sound had: %.0f rms)\n", full, half, none, r);
        ck("5. the guard: tripped half the DEPTH (a quarter of the gain); past OT_DEEP none (the sound as it was, never cut)",
           half > full * 0.15 && half < full * 0.4 && fabs(none) < 0.01 && r > 800);
    }
    {   /* 6. kept with a project: 'O' 'T' when one is on; without it (every one of before): off */
        static project_store_t st;
        int ok;
        uint32_t j, found = 0;
        ui_power_on();
        ott_set(2, 70, 40, -20, 60);
        ott_set(4, 0, 90, 10, 20);
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        for (j = 68; j + 1 < PROJ_STORE_SIZE; j++) found |= st.raw[j] == 'O' && st.raw[j + 1] == 'T' && st.raw[j + 2 + 2 * OT_N] == 70;
        ui_power_on();
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("6. a project keeps each track's OTT ('O' 'T'): T3 70 40 -20 60, T5's knobs at DEPTH 0, the others off",
           ok && found && ott_v[2][OT_DEPTH] == 70 && ott_v[2][OT_TIME] == 40 && ott_v[2][OT_UPDN] == -20 && ott_v[2][OT_XO] == 60 &&
           ott_v[4][OT_DEPTH] == 0 && ott_v[4][OT_TIME] == 90 && ott_v[4][OT_UPDN] == 10 && ott_v[4][OT_XO] == 20 &&
           ott_v[0][OT_DEPTH] == 0 && ott_v[0][OT_TIME] == 72 && ott_v[0][OT_XO] == 37);
        ui_power_on();                                    /* (V10.8.71, BYTE's pending fixed) DEPTH 0 everywhere, a knob moved */
        ott_set(1, 0, 80, -30, 50);
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        ui_power_on();
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   DEPTH 0 on every track: TIME UP/DN X-OVER kept all the same (80 -30 50)",
           ok && ott_v[1][OT_DEPTH] == 0 && ott_v[1][OT_TIME] == 80 && ott_v[1][OT_UPDN] == -30 && ott_v[1][OT_XO] == 50);
        ui_power_on();                                    /* a project of before: no 'O' 'T' */
        project_capture(&proj_scratch);
        ok &= proj_pack(&st, &proj_scratch);
        for (j = 68, found = 0; j + 1 < PROJ_STORE_SIZE; j++) found |= st.raw[j] == 'O' && st.raw[j + 1] == 'T';
        ott_set(1, 100, 40, 0, 50);
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   a project with no OTT on writes no block; loading one (as every project of before): OTT off, the defaults",
           ok && !found && ott_v[1][OT_DEPTH] == 0 && ott_v[1][OT_TIME] == 72 && ott_v[1][OT_XO] == 37);
    }
    {   /* 7. a user preset keeps the track's OTT; one of before loads it off */
        int ok;
        ui_power_on();
        song.sel = 3;
        ott_set(3, 55, 70, 30, 80);
        ok = (up_store(9, "OTT TEST") | 3) == 3;
        ott_set(3, 0, 72, 0, 37);
        ok &= up_load(9) == 0 && ott_v[3][OT_DEPTH] == 55 && ott_v[3][OT_TIME] == 70 && ott_v[3][OT_UPDN] == 30 && ott_v[3][OT_XO] == 80;
        {
            up_rec_t rr = *up_rec(9);
            rr.packed[UP_OT_AT] = 0;                    /* (a record of before) */
            *up_rec(10) = rr;
        }
        ok &= up_load(10) == 0 && ott_v[3][OT_DEPTH] == 0 && ott_v[3][OT_TIME] == 72;
        ck("7. a user preset keeps the track's OTT (55 70 +30 80); one of before loads with it off", ok);
        memset(up_rec(9), 0, sizeof(up_rec_t)); memset(up_rec(10), 0, sizeof(up_rec_t));
        song.sel = 0;
    }
    {   /* 8. the page: FX > TRACK's fourth, DEPTH TIME UP/DN X-OVER (their formats), drawn; HOME's tag */
        int16_t *vp;
        const param_desc_t *d;
        char v[12];
        const char *u;
        int ok;
        ui_power_on(); go_home(); frame(); press(B_FX); frame();
        nav_put(SEC_FX, "OTT"); frame();
        ok = str_eq(cur_page()->title, "OTT") && fx_level() == FXL_TRACK && fx_trk_k(ui.page) == 3;
        d = page_desc(cur_page(), 0, &vp);
        ok &= d && str_eq(d->label, "DEPTH") && vp == &ott_v[song.sel % NTRK][OT_DEPTH];
        turn(EN_K1, 5); frame();
        ok &= ott_v[song.sel % NTRK][OT_DEPTH] > 0 && ott_on(song.sel % NTRK);
        d = page_desc(cur_page(), 3, &vp);
        param_format(d, *vp, v, &u);
        ok &= str_eq(d->label, "X-OVER") && str_eq(v, "187") && str_eq(u, "Hz");
        d = page_desc(cur_page(), 1, &vp);
        param_format(d, *vp, v, &u);
        ok &= str_eq(d->label, "TIME") && str_eq(u, "ms");
        ok &= str_eq(page_desc(cur_page(), 2, &vp)->label, "UP/DN");
        ck("8. FX > TRACK's fourth page OTT: DEPTH (KNOB 1 turns it on) TIME (ms) UP/DN X-OVER (187 Hz)", ok);
        ott_v[song.sel % NTRK][OT_DEPTH] = 0;
    }
    printf("ott: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
