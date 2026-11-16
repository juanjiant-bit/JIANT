/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.34; V10.7.43 rebuilt) LIFE (firmware/src/life.c): build/host/life_test [DEMO_DIR]
 * 1. still: LIFE 0 renders bit for bit as every track's LIFE 0 with the global one and its three knobs at 100 (and so as
 *    before LIFE: tests/golden_life0.txt, run_tests.sh's LIFE=0 regress); a matrix's T1 ENV the same with LIFE running.
 * 2. alive: the default LIFE moves the sound; FOLLOW: a kit ducks a pad's cutoff, a track's own sound on its SHAPE.
 * 3. bounded: everything at 100 on six tracks (drums, a pad, FM, WEST, a lead, a bass): no sample past full scale more
 *    than the same mix at LIFE 0 has, no runaway, the cross-FM's depth inside its line, silence after the notes end.
 * 4. who moves whom: two tracks a duet (each the other's), three a ring (the one before most), a muted track leaves it;
 *    the cross-FM's pairs: one up to LIFE 50, two past it, a kit never the one moved, X-FM 0 none.
 * 5. the cross-FM really modulates: a drone's spectrum against LIFE 0 further at LIFE 50 and again at 100 (FOLLOW 0:
 *    nothing but the cross-FM), sidebands between its partials; every engine but the kit moved by it.
 * 6. the matrix: LIFE and BEING as sources, LIFE as a destination; HOME > LIFE drawn (the beings, their lines), its
 *    knobs LIFE FOLLOW X-FM SWIM; kept in a project ('X' 'L'), one without it at the defaults.
 * DEMO_DIR: WAVs (LIFE 0 / default / high; a cross-FM drone duet; six tracks at 100) and the LIFE page's PNG data */
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
static void scene(uint32_t n, int32_t life, int32_t knobs, int32_t tl)
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
    if (knobs >= 0) life_v[LIFE_FOL] = life_v[LIFE_XFM] = life_v[LIFE_SWM] = (int16_t)knobs;
    for (k = 0; k < NTRK; k++) life_v[LIFE_N + k] = (int16_t)tl;
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
static void r_still(void) { scene(6, 0, -1, 100); play(6, 3, 1); }
static void r_tracks0(void) { scene(6, 100, 100, 0); play(6, 3, 1); }
static void r_def(void) { scene(6, LIFE_DEF, -1, 100); play(6, 3, 1); }
static void r_mx0(void) { scene(6, 0, -1, 100); trk[1].p[P_M1SRC] = MS_T1; trk[1].p[P_M1DST] = MD_CUT; trk[1].p[P_M1AMT] = -40; play(6, 2, 1); }
static void r_mx1(void) { scene(6, 50, 100, 0); trk[1].p[P_M1SRC] = MS_T1; trk[1].p[P_M1DST] = MD_CUT; trk[1].p[P_M1AMT] = -40; play(6, 2, 1); }
static void r_slot0(void) { scene(2, 50, 100, 0); play(2, 2, 0); }
static void r_slot1(void) { scene(2, 50, 100, 0); trk[1].p[P_M1SRC] = MS_LFO; trk[1].p[P_M1DST] = MD_LIFE; trk[1].p[P_M1AMT] = 63; play(2, 2, 0); }

/* (V10.7.43) the cross-FM's own test: a drone duet (T1 ANALOG's bass moving T2), FOLLOW 0 (nothing but the cross-FM),
 * its mono output kept from 1 s; the spectrum of 8192 samples (Hann, the bins to 12 kHz) */
#define XN 8192u
static float xbuf[XN];
static double xspec[XN / 2u];
static void xrender(uint32_t e2, int32_t amt)
{
    uint32_t b, i, k = 0;
    scene(2, amt, 100, 100);
    life_v[LIFE_FOL] = 0; life_v[LIFE_SWM] = 30;
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

static void wav_go(const char *dir, const char *name, uint32_t secs)
{
    char p[512];
    snprintf(p, sizeof p, "%s/%s", dir, name);
    wav = fopen(p, "wb");
    if (wav) wav_hdr(wav, secs * FS / CTL * CTL);
}
static void wav_end(void) { if (wav) fclose(wav); wav = 0; }
static void ppm(const char *dir, const char *name)       /* the screen as drawn, a PPM (renders/: PNG by tools) */
{
    char p[512];
    FILE *f;
    uint32_t i;
    snprintf(p, sizeof p, "%s/%s", dir, name);
    if (!(f = fopen(p, "wb"))) return;
    fprintf(f, "P6\n240 240\n255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        fputc((c >> 11) << 3 | (c >> 13), f); fputc(((c >> 5) & 63u) << 2 | ((c >> 9) & 3u), f); fputc((c & 31u) << 3 | ((c >> 2) & 7u), f);
    }
    fclose(f);
}

static void demos(const char *dir)
{
    static const int32_t L[3] = {0, LIFE_DEF, 90};
    static const char *const N[3] = {"01_chord_life0.wav", "02_chord_life25.wav", "03_chord_life90.wav"};
    uint32_t i, b;
    scene(2, 0, -1, 100);                               /* (a block of warm-up: the tests above left the UI's state) */
    play(2, 1, 1);
    for (i = 0; i < 3u; i++) {                          /* a held pad chord, alone: the voices breathing */
        scene(2, L[i], -1, 100);
        trk_live = 2; trk[0].p[P_MUTE] = 1;
        wav_go(dir, N[i], 10);
        trk_note_on(&trk[1], 57, 100); trk_note_on(&trk[1], 60, 90); trk_note_on(&trk[1], 64, 80); trk_note_on(&trk[1], 69, 70);
        blk(8u * FS / CTL);
        trk_all_off(&trk[1]);
        blk(2u * FS / CTL);
        wav_end();
    }
    for (i = 0; i < 2u; i++) {                          /* the drone duet: two drones moving each other */
        scene(2, i ? 100 : 0, -1, 100);
        host_preset(&trk[0], eng("ANALOG"), 1); host_preset(&trk[1], eng("FM6"), 0);
        memset(&trk[0].p[P_M1SRC], 0, 12u * sizeof trk[0].p[0]); memset(&trk[1].p[P_M1SRC], 0, 12u * sizeof trk[1].p[0]);
        life_v[LIFE_FOL] = 30; life_v[LIFE_XFM] = 100; life_v[LIFE_SWM] = 30;
        wav_go(dir, i ? "05_drone_duet_life.wav" : "04_drone_duet_life0.wav", 14);
        trk_note_on(&trk[0], 45, 110); trk_note_on(&trk[1], 57, 100);
        for (b = 0; b < 12u * FS / CTL; b++) {
            if (b % (3u * FS / CTL) == 0u) trk_note_on(&trk[1], b % (6u * FS / CTL) ? 52 : 57, 100);
            blk(1);
        }
        trk_all_off(&trk[0]); trk_all_off(&trk[1]);
        blk(2u * FS / CTL);
        wav_end();
    }
    {   /* six tracks, everything at 100 (the bounded test's mix) */
        scene(6, 100, 100, 100);
        wav_go(dir, "06_six_tracks_life100.wav", 7);
        play(6, 6, 1);
        wav_end();
    }
}

int main(int argc, char **argv)
{
    uint64_t a, b, c;
    uint32_t i;
    setvbuf(stdout, 0, _IONBF, 0);
    /* 1 */
    a = child(r_still); b = child(r_tracks0); c = child(r_def);
    ck("1. LIFE 0 == every track's LIFE 0 under LIFE 100 (FOLLOW X-FM SWIM 100): bit for bit", a && a == b);
    a = child(r_mx0); b = child(r_mx1);
    ck("   a matrix's T1 ENV -> CUT: the same, LIFE's own followers running beside it", a && a == b);
    /* 2 */
    ck("2. the default LIFE moves the sound (a mix of six against LIFE 0)", c && c != child(r_still));
    {
        int32_t cmin = 0, hmax = 0;
        scene(2, 50, 100, 100);                         /* (T1 the kit, T2 the pad) */
        trk_note_on(&trk[1], 60, 100); trk_note_on(&trk[1], 64, 100);
        for (i = 0; i < 3u * FS / CTL; i++) {
            if (!(i % (FS / 2u / CTL))) trk_note_on(&trk[0], 36, 120);
            blk(1);
            cmin = life.xc[1] < cmin ? life.xc[1] : cmin;
            hmax = life.xh[1] > hmax ? life.xh[1] : hmax;
        }
        ck("   FOLLOW: the kit ducks the pad's cutoff, the pad's own sound on its SHAPE", cmin < -64 && hmax > 64);
        life_v[LIFE_FOL] = 0;
        blk(8);
        ck("   FOLLOW 0: none of it", !life.xc[1] && !life.xh[1]);
    }
    /* 3 */
    {
        int32_t pk0, nc0;
        scene(6, 0, -1, 100); play(6, 3, 0); pk0 = pk; nc0 = nclip;
        scene(6, 100, 100, 100); play(6, 3, 3);
        ck("3. everything at 100, six tracks: bounded (no more samples at full scale than LIFE 0's mix)", nclip <= nc0 + 64 && pk <= 32767);
        pk = 0;
        blk(FS / CTL);
        ck("   .. and silence after the notes end (no runaway)", pk < 64);
        for (i = 0; i < LPM_N && life.pm[i].dc <= LPM_D && life.pm[i].b <= LPM_D; i++) {}
        ck("   the cross-FM's depth inside its line (+-13 samples about its centre, the line 32)", i == LPM_N &&
           2 * LPM_D < (int32_t)(LPM_L - 2u) << 8);
    }
    /* 4 */
    {
        uint32_t k, np = 0, kit = 0;
        scene(2, 50, 100, 100);
        play(2, 2, 0);
        ck("4. two tracks: a duet, each moving the other", life.w[0][1] > 8000 && life.w[1][0] > 8000);
        scene(3, 50, 50, 100);
        play(3, 2, 0);
        ck("   three tracks: a ring, each held most by the one before it", life.w[1][0] > life.w[1][2] && life.w[2][1] > life.w[2][0] &&
           life.w[0][2] > life.w[0][1]);
        trk[1].p[P_MUTE] = 1;
        blk(4u * FS / CTL);
        ck("   a track muted leaves the ring (its lines fade)", life.w[2][1] < 2000 && life.w[2][0] > 8000);
        scene(6, 50, 100, 100);
        play(6, 2, 0);
        for (k = 0; k < LPM_N; k++) np += life.pm[k].car < NTRK && life.pm[k].dc > 0;
        ck("   the cross-FM: LIFE 50, one pair, its samples kept for the next block", np == 1 && life.tapm && life.pmc);
        scene(6, 100, 100, 100);
        play(6, 2, 0);
        for (k = 0, np = 0; k < LPM_N; k++) {
            np += life.pm[k].car < NTRK && life.pm[k].dc > 0;
            kit |= life.pm[k].car == 0u;
        }
        ck("   LIFE 100: two pairs, the kit (T1) never the one moved", np == 2 && !kit && !(life.pmc & 1u));
        scene(6, 100, 0, 100);
        play(6, 2, 0);
        ck("   X-FM 0: no pair, nothing kept", !life.pmc && !life.tapm);
    }
    /* 5 */
    {
        static double ref[XN / 2u];
        double sd[3] = {0, 0, 0}, sb[3];
        int32_t A[3] = {0, 50, 100};
        uint32_t k, f, e, nb = 12000u * XN / FS, moved = 0, tried = 0;
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
        ck("5. the cross-FM modulates: the spectrum further from LIFE 0's at 50, further again at 100 (X-FM 100, FOLLOW 0)",
           sd[1] > 1.0 && sd[2] > sd[1]);
        ck("   .. its sidebands: more energy between the partials as LIFE goes up", sb[1] > sb[0] * 1.5 && sb[2] > sb[1]);
        for (e = 0; e < NENGINES; e++) {                /* every engine moved by it (a kit moves, it is never moved) */
            uint64_t h0;
            uint32_t on = 0, b;
            if (e == ENGI_DRUM || e == ENGI_INPUT || (!FELUCCA_FM4 && e == ENGI_DIGITAL) || !ENGINES[e]->name[0] || ENGINES[e]->name[0] == '-') continue;
            tried++;
            for (k = 0; k < 2u; k++) {
                scene(2, k ? 100 : 0, 100, 100);
                life_v[LIFE_FOL] = 0;
                host_preset(&trk[0], eng("ANALOG"), 2); host_preset(&trk[1], e, 0);
                memset(&trk[0].p[P_M1SRC], 0, 12u * sizeof trk[0].p[0]); memset(&trk[1].p[P_M1SRC], 0, 12u * sizeof trk[1].p[0]);
                trk_note_on(&trk[0], 45, 110); trk_note_on(&trk[1], 60, 110);
                for (b = 0; b < FS / CTL; b++) {
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
    {
        int32_t lo = 32767, hi = -32768, blo = 32767, bhi = -32768;
        a = child(r_slot0); b = child(r_slot1);
        ck("6. the matrix: LFO -> LIFE lives a track whose own LIFE is 0", a && b && a != b);
        scene(2, 0, -1, 100);
        play(2, 1, 0);
        for (i = 0; i < 60u * FS / CTL; i++) {
            int32_t x = mod_tsrc(&trk[1], MS_LIFE, 0), y = mod_tsrc(&trk[1], MS_BEING, 0);
            blk(1);
            lo = x < lo ? x : lo; hi = x > hi ? x : hi; blo = y < blo ? y : blo; bhi = y > bhi ? y : bhi;
        }
        ck("   LIFE and BEING sources: bipolar, moving (with LIFE 0 too)", lo < -8000 && hi > 8000 && bhi - blo > 16000 &&
           MS_BIPOLAR(MS_LIFE) && MS_BIPOLAR(MS_BEING));
        ck("   their names appended (LIFE BEING; destination LIFE), the old ids kept",
           str_eq(N_MSRC[MS_LIFE], "LIFE") && str_eq(N_MSRC[MS_BEING], "BEING") && MS_LIFE == MS_FNZ + 1 &&
           str_eq(N_MDST[MD_LIFE], "LIFE") && MD_LIFE == MD_LEVEL + 1 && MS_N <= 32 && MD_N <= 64);
    }
    {   /* HOME > LIFE drawn: beings and lines */
        static uint16_t s0[240 * 240];
        ui_power_on();
        frame();
        nav_go(SEC_HOME, HE_LIFE, 0);
        scene(4, 60, 60, 100);
        play(4, 2, 0);
        for (i = 0; i < 4u; i++) trk_note_on(&trk[i], 48 + 7 * i, 110);
        blk(FS / 4u / CTL);
        ui.force = 1; frames(64);
        ck("   HOME > LIFE: its page, LIFE FOLLOW X-FM SWIM on its cards", home_life() && !strncmp(ui.col[0], "LIFE|", 5) &&
           !strncmp(ui.col[1], "FOLLOW|", 7) && !strncmp(ui.col[2], "X-FM|", 5) && !strncmp(ui.col[3], "SWIM|", 5));
        memcpy(s0, host_screen, sizeof s0);
        if (argc > 1) ppm(argv[1], "life_page.ppm");
        life_v[LIFE_AMT] = 0;
        blk(FS / 8u / CTL);
        ui.force = 1; frames(64);
        ck("   .. drawn: its lines go with LIFE 0 (the beings stay)", memcmp(s0, host_screen, sizeof s0) != 0);
        if (argc > 1) ppm(argv[1], "life_page_still.ppm");
        life_v[LIFE_AMT] = 60;
        for (i = 0; i < 4u; i++) trk_all_off(&trk[i]);
    }
    {   /* kept with a project */
        static project_store_t st;
        int ok;
        ui_power_on();
        ck("   power-on: LIFE at its defaults (25 %, FOLLOW 30, X-FM 70, SWIM 30, every track 100)",
           life_v[LIFE_AMT] == LIFE_DEF && life_v[LIFE_FOL] == 30 && life_v[LIFE_XFM] == 70 && life_v[LIFE_SWM] == 30 &&
           life_v[LIFE_N] == 100 && life_v[LIFE_N + NTRK - 1] == 100);
        life_v[LIFE_AMT] = 70; life_v[LIFE_XFM] = 66; life_v[LIFE_N + 2] = 0;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        life_defaults();
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   a project keeps LIFE 70, X-FM 66, T3's LIFE 0", ok && life_v[LIFE_AMT] == 70 && life_v[LIFE_XFM] == 66 &&
           life_v[LIFE_N + 2] == 0 && life_v[LIFE_N + 1] == 100);
        life_defaults();
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        life_v[LIFE_AMT] = 0;
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   one without 'X' 'L' (every project before V10.7.34) opens with LIFE on, at its defaults", ok && life_v[LIFE_AMT] == LIFE_DEF);
    }
    {   /* EDIT > VOICES 3 KNOB 2: the track's LIFE */
        int16_t *vp = 0;
        const param_desc_t *d;
        for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, "VOICES 3"); i++) {}
        d = i < NPAGES ? page_desc(&PAGES[i], 1, &vp) : 0;
        ck("   EDIT > VOICES 3 KNOB 2: the track's LIFE (0 .. 100 %)", d && str_eq(d->label, "LIFE") && vp == &life_v[LIFE_N + song.sel] &&
           d->max == 100);
        for (i = 0; i < NPAGES && !str_eq(PAGES[i].title, "TONE"); i++) {}   /* (V10.7.35: a synth's on TONE, KNOB 3) */
        d = i < NPAGES ? page_desc(&PAGES[i], 2, &vp) : 0;
        ck("   EDIT > TONE KNOB 3: the track's LIFE too (V10.7.35)", d && str_eq(d->label, "LIFE") && vp == &life_v[LIFE_N + song.sel]);
    }
    if (argc > 1)
        demos(argv[1]);
    printf("life: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
