/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.34) LIFE (firmware/src/life.c): build/host/life_test [DEMO_DIR]
 * 1. still: LIFE 0 renders bit for bit as every track's LIFE 0 with the global one and its three knobs at 100 (and so as
 *    before LIFE: tests/golden_life0.txt, run_tests.sh's LIFE=0 regress); a matrix's T1 ENV the same with LIFE's followers
 *    running (its own are scanned every 4th sample, the matrix's never).
 * 2. alive: the default LIFE moves a held chord (its voices each their own way: their pitch, cutoff and level wander),
 *    a note's arc (energy, bloom, tiring) over its first seconds.
 * 3. bounded: everything at 100 on six tracks (drums, a pad, FM, noise, a lead, a bass): no sample past full scale more
 *    than the same mix at LIFE 0 has, no runaway (the cross-FM line's delay inside it), silence after the notes end.
 * 4. who moves whom: two tracks a duet (each the other's), three a ring (the one before most), a kit ducking a pad's
 *    filter, a pad opening one's, the sends up, the cross-FM's samples kept for the next block only when asked.
 * 5. the matrix: LIFE and BEING as sources (bipolar, moving), LIFE as a destination (a slot lives a track whose LIFE is 0),
 *    the names appended; HOME > LIFE drawn (the beings, their lines), its knobs; kept in a project ('X' 'L'), one without
 *    it at the defaults.
 * DEMO_DIR: WAVs (LIFE 0 / default / high; a cross-mod drone duet; a GEN song with LIFE) and the LIFE page's PNG data */
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
/* the boot state, n tracks: T1 a kit, T2 a pad, T3 FM6, T4 NOISE, T5 a lead, T6 a bass (the rest asleep) */
static void scene(uint32_t n, int32_t life, int32_t knobs, int32_t tl)
{
    static const char *const E[6] = {"DRUM", "ANALOG", "FM6", "NOISE", "PHASE", "ANALOG"};
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
    if (knobs >= 0) life_v[LIFE_FILT] = life_v[LIFE_PIT] = life_v[LIFE_FX] = (int16_t)knobs;
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
static void r_slot0(void) { scene(2, 50, 0, 0); play(2, 2, 0); }
static void r_slot1(void) { scene(2, 50, 0, 0); trk[1].p[P_M1SRC] = MS_LFO; trk[1].p[P_M1DST] = MD_LIFE; trk[1].p[P_M1AMT] = 63; play(2, 2, 0); }

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
        scene(2, i ? 60 : 0, -1, 100);
        host_preset(&trk[0], eng("ANALOG"), 1); host_preset(&trk[1], eng("FM6"), 0);
        memset(&trk[0].p[P_M1SRC], 0, 12u * sizeof trk[0].p[0]); memset(&trk[1].p[P_M1SRC], 0, 12u * sizeof trk[1].p[0]);
        life_v[LIFE_FILT] = 70; life_v[LIFE_PIT] = 80; life_v[LIFE_FX] = 40;
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
    ck("1. LIFE 0 == every track's LIFE 0 under LIFE 100 (FILTER PITCH FX 100): bit for bit", a && a == b);
    a = child(r_mx0); b = child(r_mx1);
    ck("   a matrix's T1 ENV -> CUT: the same, LIFE's own followers running beside it", a && a == b);
    /* 2 */
    ck("2. the default LIFE moves the sound (a mix of six against LIFE 0)", c && c != child(r_still));
    {
        int32_t lo[NVOICE], hi[NVOICE], d = 0;
        uint32_t k;
        scene(2, LIFE_DEF, 0, 100);
        trk[0].p[P_MUTE] = 1;
        trk_note_on(&trk[1], 60, 100); trk_note_on(&trk[1], 64, 100); trk_note_on(&trk[1], 67, 100);
        for (k = 0; k < NVOICE; k++) lo[k] = 1 << 30, hi[k] = -(1 << 30);
        for (i = 0; i < 4u * FS / CTL; i++) {
            blk(1);
            for (k = 0; k < 3u; k++) {
                int32_t x = life.vn[NVOICE + k][0];
                lo[k] = x < lo[k] ? x : lo[k];
                hi[k] = x > hi[k] ? x : hi[k];
            }
        }
        for (k = 0; k < 3u; k++) d += hi[k] - lo[k] > 3000;
        ck("   each voice of a held chord wanders its own way (three voices, three wanders)",
           d == 3 && life.vn[NVOICE][0] != life.vn[NVOICE + 1][0] && life.vn[NVOICE + 1][0] != life.vn[NVOICE + 2][0]);
        ck("   a held note's arc: blooming, then tiring (its blocks counted)", life.vblk[NVOICE] > 2000u);
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
        for (i = 0; i < NTRK; i++) {
            int32_t d0 = (((life.pa[i] >> 7) * (65535 >> 7)) * 14) >> 8;
            if ((d0 >> 8) >= (int32_t)LPM_L - 2) break;
        }
        ck("   the cross-FM line's delay always inside it", i == NTRK && nc0 >= 0);
    }
    /* 4 */
    {
        scene(2, 50, 100, 100);
        play(2, 2, 0);
        ck("4. two tracks: a duet, each moving the other", life.w[0][1] > 8000 && life.w[1][0] > 8000);
        ck("   the kit (T1) ducks the pad's (T2) filter, the pad opens the kit's; the sends up", life.xc[1] <= 0 && life.xc[0] >= 0 && life.xs[1] > 0);
        ck("   PITCH 100: the cross-FM on, the samples of the one moving kept for the next block", life.pa[1] > 30000 && (life.tapm & 3u) == 3u);
        scene(3, 50, 50, 100);
        play(3, 2, 0);
        ck("   three tracks: a ring, each held most by the one before it", life.w[1][0] > life.w[1][2] && life.w[2][1] > life.w[2][0] &&
           life.w[0][2] > life.w[0][1]);
        ck("   PITCH 50: no cross-FM, nothing kept", !life.pa[0] && !life.tapm);
        trk[1].p[P_MUTE] = 1;
        blk(4u * FS / CTL);
        ck("   a track muted leaves the ring (its lines fade)", life.w[2][1] < 2000 && life.w[2][0] > 8000);
    }
    /* 5 */
    {
        int32_t lo = 32767, hi = -32768, blo = 32767, bhi = -32768;
        a = child(r_slot0); b = child(r_slot1);
        ck("5. the matrix: LFO -> LIFE lives a track whose own LIFE is 0", a && b && a != b);
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
        ck("   HOME > LIFE: its page, LIFE FILTER PITCH FX on its cards", home_life() && !strncmp(ui.col[0], "LIFE|", 5) &&
           !strncmp(ui.col[3], "FX|", 3));
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
        ck("   power-on: LIFE at its defaults (25 %, FILTER 30, PITCH 10, FX 25, every track 100)",
           life_v[LIFE_AMT] == LIFE_DEF && life_v[LIFE_FILT] == 30 && life_v[LIFE_PIT] == 10 && life_v[LIFE_FX] == 25 &&
           life_v[LIFE_N] == 100 && life_v[LIFE_N + NTRK - 1] == 100);
        life_v[LIFE_AMT] = 70; life_v[LIFE_PIT] = 66; life_v[LIFE_N + 2] = 0;
        project_capture(&proj_scratch);
        ok = proj_pack(&st, &proj_scratch);
        life_defaults();
        ok = ok && proj_import(&proj_scratch, &st, sizeof st) && project_restore_runtime(&proj_scratch) == 0;
        ck("   a project keeps LIFE 70, PITCH 66, T3's LIFE 0", ok && life_v[LIFE_AMT] == 70 && life_v[LIFE_PIT] == 66 &&
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
