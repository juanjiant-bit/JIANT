/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.34; V10.7.43 rebuilt: one value, the tracks' cross-FM alone) LIFE (firmware/src/life.c): build/host/life_test
 * 1. still: LIFE 0 renders bit for bit as without LIFE (tests/golden_life0.txt, run_tests.sh's LIFE=0 regress, and here: a
 *    project's old LIFE values and matrix routes on the old LIFE / BEING / LIFE ids change nothing).
 * 2. the curve: 0 at 0, rising, ~1 % at 25, ~6 % at 50, ~25 % at 75, all at 100 (logarithmic: fine at the bottom).
 * 3. alive and bounded: the default moves a mix of six; everything at 100 bounded, silence after the notes end.
 * 4. the ring: two tracks a duet (each moves the other), one pair up to LIFE 50, two past it, a kit never the one moved.
 * 5. the cross-FM modulates: a drone's spectrum further from LIFE 0's at 50 and again at 100, sidebands between its
 *    partials; every engine but the kit moved by it.
 * 6. HOME > LIFE: its one card (KNOB 1), its lines the pairs; kept in a project, the old bytes ignored. */
#define HOST_LIFE LIFE_DEF              /* (the firmware's power-on: LIFE on) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <sys/wait.h>
#include <unistd.h>
static int bad;
static void ck(const char *w, int ok) { printf("life: %-100s %s\n", w, ok ? "ok" : "FAIL"); bad += !ok; }
static int32_t ob[2 * CTL];
static uint64_t hsh;
static int32_t pk, nclip;
static FILE *wav;
static void blk(uint32_t n)
{
    uint32_t i;
    while (n--) {
        mix_block(ob, CTL);
        for (i = 0; i < 2u * CTL; i++) {
            int32_t a = ob[i] < 0 ? -ob[i] : ob[i];
            hsh = (hsh ^ (uint32_t)ob[i]) * 0x100000001B3ull;
            if (a > pk) pk = a;
            nclip += a >= 32767;
        }
        if (wav)
            for (i = 0; i < CTL; i++) wav_put(wav, ob[2u * i], ob[2u * i + 1u]);
    }
}
static uint32_t eng(const char *n)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (str_eq(ENGINES[e]->name, n)) return e;
    return 0;
}
/* the boot state, n tracks: T1 a kit, T2 a pad, T3 FM6, T4 WEST (V10.7.36: NOISE's slot and its first preset's name), T5 a
 * lead, T6 a bass (the rest asleep) */
static void scene(uint32_t n, int32_t life)
{
    static const char *const E[6] = {"DRUM", "ANALOG", "FM6", "WEST", "PHASE", "ANALOG"};
    static const uint8_t PR[6] = {0, 1, 0, 0, 0, 2};
    uint32_t k;
    memset(dly_buf, 0, sizeof dly_buf); memset(cho_buf, 0, sizeof cho_buf); memset(rev_comb, 0, sizeof rev_comb);
    memset(&rev_u, 0, sizeof rev_u); memset(&fx, 0, sizeof fx);
    memset(fxq_c, 0, sizeof fxq_c); memset(fxq_d, 0, sizeof fxq_d); memset(fxq_r, 0, sizeof fxq_r);
    memset(mp_seen, 0, sizeof mp_seen); memset(trk, 0, sizeof trk); memset(mod_c, 0, sizeof mod_c);
    memset(drift_clk, 0, sizeof drift_clk); memset(track_env, 0, sizeof track_env); mns_clk = 0;
    host_tracks_init();
    trk_live = (uint8_t)n;
    for (k = 0; k < NPART; k++) {
        host_preset(&trk[k], k < n ? eng(E[k]) : 0u, k < n ? PR[k] : 0u);
        memset(&trk[k].p[P_M1SRC], 0, 12u * sizeof trk[k].p[0]);
    }
    life_defaults();
    life_reset();
    life_v[LIFE_AMT] = (int16_t)life;
    hsh = 0xCBF29CE484222325ull;
    pk = nclip = 0;
}
/* the phrase: a kick every beat on T1, a chord on T2, notes on the rest; secs of it, then the releases and a tail */
static void play(uint32_t n, uint32_t secs, uint32_t tail)
{
    uint32_t b, nb = secs * FS / CTL, beat = FS / 2u / CTL;
    static const uint8_t NT[6] = {36, 60, 67, 72, 79, 36};
    for (b = 0; b < n; b++)
        if (b) trk_note_on(&trk[b], NT[b], 100);
    if (n > 1u) { trk_note_on(&trk[1], 64, 90); trk_note_on(&trk[1], 67, 80); }
    for (b = 0; b < nb; b++) {
        if (!(b % beat) && n) trk_note_on(&trk[0], 36, 120);
        if (b % beat == 8u && n) trk_note_off(&trk[0], 36);
        blk(1);
    }
    for (b = 0; b < NTRK; b++) trk_all_off(&trk[b]);
    blk(tail * FS / CTL);
}
static uint64_t child(void (*f)(void))                  /* f's hash in a fork()ed child: every seed as it was */
{
    int fd[2];
    uint64_t h = 0;
    if (pipe(fd)) return 0;
    if (!fork()) { f(); if (write(fd[1], &hsh, sizeof hsh) < 0) _exit(1); _exit(0); }
    wait(0);
    if (read(fd[0], &h, sizeof h) != sizeof h) h = 0;
    close(fd[0]); close(fd[1]);
    return h;
}
static void r_still(void) { scene(6, 0); play(6, 3, 1); }
static void r_def(void) { scene(6, LIFE_DEF); play(6, 3, 1); }
static void r_old(void)                                 /* (LIFE 0, a project of before: its other values, its routes) */
{
    scene(6, 0);
    life_v[LIFE_RCH] = 100; life_v[LIFE_XFM] = 0; life_v[LIFE_SWM] = 100; life_v[LIFE_N + 1] = 0;
    trk[1].p[P_M1SRC] = MS_LIFE; trk[1].p[P_M1DST] = MD_CUT; trk[1].p[P_M1AMT] = 60;
    trk[2].p[P_M1SRC] = MS_LFO; trk[2].p[P_M1DST] = MD_LIFE; trk[2].p[P_M1AMT] = 60;
    play(6, 3, 1);
}
#define XN 8192u
static float xbuf[XN];
static double xspec[XN / 2u];
static void xrender(uint32_t e2, int32_t amt)
{
    uint32_t b, i, k = 0;
    scene(2, amt);
    host_preset(&trk[0], eng("ANALOG"), 2); host_preset(&trk[1], e2, 0);
    memset(&trk[0].p[P_M1SRC], 0, 12u * sizeof trk[0].p[0]); memset(&trk[1].p[P_M1SRC], 0, 12u * sizeof trk[1].p[0]);
    for (i = 0; i < 2u; i++) trk[i].p[P_REV] = trk[i].p[P_DLY] = trk[i].p[P_CHOR] = 0;
    trk_note_on(&trk[0], 45, 110); trk_note_on(&trk[1], 70, 100);   /* (110 Hz, 466 Hz: its sidebands off the bass's
                                                         * partials) */
    for (b = 0; b < (FS + XN) / CTL + 1u; b++) {
        mix_block(ob, CTL);
        if (b >= FS / CTL)
            for (i = 0; i < CTL && k < XN; i++) xbuf[k++] = (float)(ob[2u * i] + ob[2u * i + 1u]);
    }
}
static void xdft(void)
{
    uint32_t f, i;
    for (f = 1; f < XN / 2u && f * FS / XN < 12000u; f++) {
        double re = 0, im = 0, w = 2 * M_PI * f / XN;
        for (i = 0; i < XN; i++) {
            double h = 0.5 - 0.5 * cos(2 * M_PI * i / XN), x = xbuf[i] * h;
            re += x * cos(w * i); im -= x * sin(w * i);
        }
        xspec[f] = re * re + im * im + 1.0;
    }
}
/* the share of the energy outside the 24 strongest partials (+-3 bins): what sits between them, the sidebands */
static double xside(void)
{
    static uint8_t keep[XN / 2u];
    double all = 0, out = 0;
    uint32_t f, n, nb = 12000u * XN / FS;
    memset(keep, 0, sizeof keep);
    for (n = 0; n < 24u; n++) {
        uint32_t m = 0;
        for (f = 1; f < nb; f++) if (!keep[f] && (!m || xspec[f] > xspec[m])) m = f;
        for (f = m > 3u ? m - 3u : 1u; f <= m + 3u && f < nb; f++) keep[f] = 1;
    }
    for (f = 1; f < nb; f++) { all += xspec[f]; if (!keep[f]) out += xspec[f]; }
    return out / all;
}


int main(void)
{
    uint64_t a, b;
    uint32_t i, k;
    setvbuf(stdout, 0, _IONBF, 0);
    /* 1 */
    a = child(r_still); b = child(r_old);
    ck("1. LIFE 0: an old project's LIFE values and its routes on LIFE / BEING / LIFE change nothing, bit for bit", a && a == b);
    ck("   the old LIFE / BEING / LIFE ids past the matrix's (inert, hidden)", MS_LIFE >= MS_N && MS_BEING >= MS_N && MD_LIFE >= MD_N);
    /* 2 */
    {
        int32_t d[5], up = 1;
        for (i = 0; i < 5u; i++) d[i] = life_depth((int32_t)i * 25);
        for (i = 1; i <= 100u; i++) up &= life_depth((int32_t)i) >= life_depth((int32_t)i - 1);
        printf("life:   the curve (Q10): %d %d %d %d %d at 0 25 50 75 100\n", d[0], d[1], d[2], d[3], d[4]);
        ck("2. the curve: 0 at 0, rising, ~1 % at 25, ~6 % at 50, ~25 % at 75, all at 100", !d[0] && up && d[1] > 4 && d[1] < 30 &&
           d[2] > 40 && d[2] < 90 && d[3] > 200 && d[3] < 300 && d[4] == 1024);
    }
    /* 3 */
    ck("3. the default LIFE moves the sound (a mix of six against LIFE 0)", child(r_def) != child(r_still));
    {
        int32_t pk0, nc0;
        scene(6, 0); play(6, 3, 0); pk0 = pk; nc0 = nclip;
        scene(6, 100); play(6, 3, 3);
        ck("   LIFE 100, six tracks: bounded (no more samples at full scale than LIFE 0's mix)", nclip <= nc0 + 64 && pk <= 32767 && pk0 > 0);
        pk = 0;
        blk(FS / CTL);
        ck("   .. and silence after the notes end (no runaway)", pk < 64);
        for (i = 0; i < LPM_N && life.pm[i].dc <= LPM_D && life.pm[i].b <= LPM_D; i++) {}
        ck("   its depth inside its line", i == LPM_N && 2 * LPM_D < (int32_t)(LPM_L - 2u) << 8);
    }
    /* 4 */
    {
        uint32_t np = 0, kit = 0;
        scene(3, 100);                                  /* (T1 the kit, T2 the pad, T3 FM6) */
        trk[0].p[P_MUTE] = 1;
        play(3, 2, 0);
        for (k = 0; k < LPM_N; k++) np += life.pm[k].car < NTRK && life.pm[k].dc > 0;
        ck("4. two tracks: a duet, each moving the other", np == 2 && (life.pmc & 6u) == 6u && (life.tapm & 6u) == 6u);
        scene(6, 50);
        play(6, 2, 0);
        for (k = 0, np = 0; k < LPM_N; k++) np += life.pm[k].car < NTRK && life.pm[k].dc > 0;
        ck("   LIFE 50: one pair", np == 1 && life.tapm && life.pmc);
        scene(6, 100);
        play(6, 2, 0);
        for (k = 0, np = 0; k < LPM_N; k++) {
            np += life.pm[k].car < NTRK && life.pm[k].dc > 0;
            kit |= life.pm[k].car == 0u;
        }
        ck("   LIFE 100: two pairs, the kit (T1) never the one moved", np == 2 && !kit && !(life.pmc & 1u));
    }
    /* 5 */
    {
        static double ref[XN / 2u];
        double sd[3] = {0, 0, 0}, sb[3];
        int32_t A[3] = {0, 50, 100};
        uint32_t f, e, nb = 12000u * XN / FS, moved = 0, tried = 0;
        char miss[160] = "";
        for (k = 0; k < 3u; k++) {
            xrender(eng("FM6"), A[k]);
            xdft();
            if (!k) memcpy(ref, xspec, sizeof ref);
            for (f = 1; f < nb; f++) sd[k] += fabs(10 * log10(xspec[f] / ref[f]));
            sd[k] /= nb;
            sb[k] = xside();
        }
        printf("life:   cross-FM, FM6 moved by ANALOG's bass: the spectrum from LIFE 0's %.2f / %.2f dB (LIFE 50 / 100), between the partials %.1f %.1f %.1f dB of the whole (LIFE 0 / 50 / 100)\n",
               sd[1], sd[2], 10 * log10(sb[0]), 10 * log10(sb[1]), 10 * log10(sb[2]));
        ck("5. the cross-FM modulates: the spectrum further from LIFE 0's at 50, further again at 100", sd[1] > 0.3 && sd[2] > sd[1]);
        ck("   .. its sidebands: more energy between the partials as LIFE goes up", sb[1] > sb[0] && sb[2] > sb[1] * 1.5);
        for (e = 0; e < NENGINES; e++) {                /* every engine moved by it (a kit moves, it is never moved) */
            uint64_t h0 = 0;
            uint32_t on = 0, b2;
            if (e == ENGI_DRUM || e == ENGI_INPUT || (!FELUCCA_FM4 && e == ENGI_DIGITAL) || !ENGINES[e]->name[0] || ENGINES[e]->name[0] == '-') continue;
            tried++;
            for (k = 0; k < 2u; k++) {
                scene(2, k ? 100 : 0);
                host_preset(&trk[0], eng("ANALOG"), 2); host_preset(&trk[1], e, 0);
                memset(&trk[0].p[P_M1SRC], 0, 12u * sizeof trk[0].p[0]); memset(&trk[1].p[P_M1SRC], 0, 12u * sizeof trk[1].p[0]);
                trk_note_on(&trk[0], 45, 110); trk_note_on(&trk[1], 60, 110);
                for (b2 = 0; b2 < FS / CTL; b2++) {
                    blk(1);
                    for (f = 0; f < LPM_N; f++) on |= k && life.pm[f].car == 1u && life.pm[f].dc > 0;
                }
                if (!k) h0 = hsh;
                else if (on && hsh != h0) moved++;
                else if (str_len(miss) < 140u) { str_cpy(miss + str_len(miss), " ", 2); str_cpy(miss + str_len(miss), ENGINES[e]->name, 12); }
            }
        }
        printf("life:   cross-FM on every engine: %u of %u moved%s%s\n", moved, tried, miss[0] ? ", not:" : "", miss);
        ck("   .. on every engine (but the kit): each one moved by the other track's samples", moved == tried && tried >= 8u);
    }
    /* 6 */
    {   /* HOME > LIFE drawn: one card, the pairs' lines */
        static uint16_t s0[240 * 240];
        ui_power_on();
        frame();
        nav_go(SEC_HOME, HE_LIFE, 0);
        scene(4, 100);
        play(4, 2, 0);
        for (i = 0; i < 4u; i++) trk_note_on(&trk[i], 48 + 7 * i, 110);
        blk(FS / 4u / CTL);
        ui.force = 1; frames(64);
        ck("6. HOME > LIFE: its page, LIFE alone on KNOB 1", home_life() && !strncmp(ui.col[0], "LIFE|", 5));
        memcpy(s0, host_screen, sizeof s0);
        life_v[LIFE_AMT] = 0;
        blk(FS / 8u / CTL);
        ui.force = 1; frames(64);
        ck("   .. drawn: its lines go with LIFE 0 (the beings stay)", memcmp(s0, host_screen, sizeof s0) != 0);
        for (i = 0; i < 4u; i++) trk_all_off(&trk[i]);
    }
    {   /* kept with a project */
        static project_store_t st;
        int ok;
        ui_power_on();
        ck("   power-on: LIFE at its default (25 %)", life_v[LIFE_AMT] == LIFE_DEF);
        life_v[LIFE_AMT] = 70; life_v[LIFE_XFM] = 5; life_v[LIFE_N + 2] = 0;   /* (the old values: as a project of before) */
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        life_defaults();
        life_v[LIFE_AMT] = 3;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   a project keeps LIFE 70; its other old LIFE bytes read and ignored", ok && life_v[LIFE_AMT] == 70 &&
           life_v[LIFE_XFM] == 70 && life_v[LIFE_N + 2] == 100);
        life_defaults();
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        life_v[LIFE_AMT] = 0;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   one without 'X' 'L' (every project before V10.7.34) opens with LIFE on, at its default", ok && life_v[LIFE_AMT] == LIFE_DEF);
    }
    {   /* the track's LIFE gone from EDIT (TONE, VOICES 3, OUT) */
        int16_t *vp = 0;
        uint32_t n = 0, j;
        for (i = 0; i < NPAGES; i++)
            for (j = 0; j < 4u; j++) n += PAGES[i].id[j] == ID_LIF;
        (void)vp;
        ck("   EDIT: the track's own LIFE gone from every page", n == 0u);
    }
    printf("life: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
