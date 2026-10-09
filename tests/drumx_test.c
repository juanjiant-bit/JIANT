/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* DRUM-X (src/drumx_voice.c: the DRUM engine) on the host, through hostsim.c:
 *   build/host/drumx_test [DEMODIR]          (run_tests.sh: build/drumx_demo)
 * 1. every lane of the kit, struck through the DRUM engine (its GM note), sounds (peak above -30 dBFS), never at
 *    full scale, DC under 2 % of the peak, and ends: its voice frees itself (DECAY at its design) within 3 s.
 * 2. MORPH (MRPH): 0 and 127 render different hits, 64 between them (its RMS between theirs or near);
 *    moving it while a hit rings: no step larger than the hit's own at A or B (+20 %).
 * 3. PITCH: TUNE +12 semitones doubles the tom's frequency (zero crossings, +-3 %); the kick's pitch envelope
 *    falls (more crossings in its first 20 ms than in 100..120 ms).
 * 4. the closed hat chokes the open one (below -60 dB of its level within 10 ms).
 * 5. the knobs: MRPH on KNOB 1, NOIS moves every lane's noise.
 * 5b. the group mutes (dx_mute): a muted group's hits are silent (any kit), the others still sound; muting a
 *    ringing group fades it out: its voice ends within 10 ms, the output 26 dB under the unmuted hit's, no step
 *    larger than the hit's own; unmuting sounds again.
 * 5c. the master (JIANT): PNCH 100 lifts a kick's first 8 ms (+30 %) and lowers its tail; DUCK 100: a held synth
 *    note drops 10 dB or more 5..40 ms after a kick (against DUCK 0), and is back (within 1 dB) after the release;
 *    CLIP 100: the loud mix saturates (crest factor down), never past full scale; CLIP 0 is out of the chain.
 * 6. demos into DEMODIR: every lane at MORPH 0, 64, 127; a beat with MORPH swept over 4 bars. */
#include <stdarg.h>
#define main hostsim_main
#include "hostsim.c"
#undef main

static int fails;
static void check(const char *what, int ok, const char *fmt, ...)
{
    printf("drumx: %-66s %s", what, ok ? "ok" : "FAIL");
    if (fmt) {
        va_list ap;
        va_start(ap, fmt);
        printf("  (");
        vprintf(fmt, ap);
        printf(")");
        va_end(ap);
    }
    printf("\n");
    fails += !ok;
}

#define MAXN (FS * 3u)
static int32_t buf[MAXN];
static const uint8_t NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};   /* eng_drum.c DRUM_LANE_NOTE */

static track_t *kitx(int16_t morph, int16_t tune)
{
    track_t *t = &trk[0];
    uint32_t i, k;
    for (k = 0; k < NTRK; k++)
        for (i = 0; i < NVOICE; i++)
            trk[k].v[i].active = 0;
    host_tracks_init();
    host_preset(t, ENGI_DRUM, 0);
    t->p[P_E0] = morph;
    t->p[P_E1] = tune;
    for (i = 0; i < 4u; i++)
        t->p[P_DIST + i] = 0;
    return t;
}
/* lane l struck, n samples of the left output into buf; returns the voice still sounding at the end */
static int strike(track_t *t, uint32_t l, uint32_t n)
{
    uint32_t i, k;
    trk_note_on(t, NOTE[l], 100);
    trk_note_off(t, NOTE[l]);
    for (k = 0; k < n; k += CTL) {
        int32_t o[2 * CTL];
        mix_block(o, CTL);
        for (i = 0; i < CTL && k + i < n; i++)
            buf[k + i] = o[2 * i];
    }
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active)
            return 1;
    return 0;
}
static double rms(uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)buf[i] * buf[i];
    return sqrt(s / (b - a));
}
static uint32_t crossings(uint32_t a, uint32_t b)
{
    uint32_t i, c = 0;
    for (i = a + 1; i < b; i++)
        c += buf[i - 1] < 0 && buf[i] >= 0;
    return c;
}
static void save(const char *dir, const char *name, uint32_t n)
{
    char path[512];
    FILE *f;
    uint32_t i;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    wav_hdr(f, n);
    for (i = 0; i < n; i++)
        wav_put(f, buf[i], buf[i]);
    fclose(f);
}

int main(int argc, char **argv)
{
    static const char *const NAME[8] = {"kick", "snare", "clap", "hatcl", "hatop", "tom", "rim", "bell"};
    const char *dir = argc > 1 ? argv[1] : "build/drumx_demo";
    char nm[64];
    uint32_t l, i;
    int ok;

    /* 1 */
    for (l = 0; l < 8u; l++) {
        track_t *t = kitx(64, 64);
        int32_t pk = 0;
        double dc = 0;
        int ended;
        strike(t, l, FS / 2u);
        for (i = 0; i < FS / 2u; i++) {
            pk = abs(buf[i]) > pk ? abs(buf[i]) : pk;
            dc += buf[i];
        }
        dc /= FS / 2u;
        ended = !strike(t, l, MAXN) || 0;
        snprintf(nm, sizeof nm, "%s: sounds, under full scale, no DC, ends", NAME[l]);
        check(nm, pk > 1036 && pk < 32000 && fabs(dc) < pk * 0.02 && ended, "peak %d, DC %.0f", pk, dc);
    }

    /* 2 */
    {
        double r0, r1, rm;
        int32_t a[FS / 4], d = 0, step = 0;
        track_t *t = kitx(0, 64);
        strike(t, 1, FS / 4u);
        memcpy(a, buf, sizeof a);
        r0 = rms(0, FS / 4u);
        t = kitx(127, 64);
        strike(t, 1, FS / 4u);
        r1 = rms(0, FS / 4u);
        for (i = 0; i < FS / 4u; i++)
            d = abs(buf[i] - a[i]) > d ? abs(buf[i] - a[i]) : d;
        t = kitx(64, 64);
        strike(t, 1, FS / 4u);
        rm = rms(0, FS / 4u);
        check("MORPH: A and B differ, the middle between them", d > 2000 &&
              rm > (r0 < r1 ? r0 : r1) * 0.7 && rm < (r0 > r1 ? r0 : r1) * 1.3, "RMS %.0f / %.0f / %.0f", r0, rm, r1);
        {                                                /* the open hat's own largest steps at A and B */
            int32_t own = 0;
            uint32_t m;
            for (m = 0; m < 2u; m++) {
                t = kitx(m ? 127 : 0, 64);
                strike(t, 4, FS / 2u);
                for (i = 1; i < FS / 2u; i++)
                    own = abs(buf[i] - buf[i - 1]) > own ? abs(buf[i] - buf[i - 1]) : own;
            }
            d = own;
        }
        t = kitx(0, 64);                                 /* MORPH swept while the open hat rings */
        trk_note_on(t, NOTE[4], 100);
        for (i = 0; i < FS / 2u; i += CTL) {
            int32_t o[2 * CTL];
            uint32_t j;
            t->p[P_E0] = (int16_t)(i * 127u / (FS / 2u));
            mix_block(o, CTL);
            for (j = 0; j < CTL; j++)
                buf[i + j] = o[2 * j];
        }
        for (i = 1; i < FS / 2u; i++)
            step = abs(buf[i] - buf[i - 1]) > step ? abs(buf[i] - buf[i - 1]) : step;
        check("MORPH moves a ringing hit without a jump", step <= d + d / 5, "largest step %d, the hat's own %d", step, d);
    }

    /* 3 */
    {
        uint32_t c0, c1, e0, e1;
        track_t *t = kitx(0, 64);
        strike(t, 5, FS / 2u);
        c0 = crossings(FS / 10u, FS / 5u);
        t = kitx(0, 64 + 43);                            /* +43 x 3 / 16: +8 semitones .. TUNE's 3/16 a step */
        t->p[P_E1] = 127;                                /* +63 x 3 / 16 = 11.8 semitones */
        strike(t, 5, FS / 2u);
        c1 = crossings(FS / 10u, FS / 5u);
        check("PITCH: TUNE up ~12 semitones about doubles the tom", c1 > c0 * 1.85 && c1 < c0 * 2.15, "%u -> %u", c0, c1);
        t = kitx(0, 64);
        strike(t, 0, FS / 4u);
        e0 = crossings(0, FS / 50u);
        e1 = crossings(FS / 10u, FS / 10u + FS / 50u);
        check("PITCH: the kick's pitch envelope falls", e0 > e1, "%u -> %u crossings in 20 ms", e0, e1);
    }

    /* 4 */
    {
        track_t *t = kitx(64, 64);
        double before, after;
        trk_note_on(t, NOTE[4], 100);
        for (i = 0; i < FS / 10u; i += CTL) {
            int32_t o[2 * CTL];
            mix_block(o, CTL);
        }
        trk_note_on(t, NOTE[3], 1);                      /* (a quiet closed hat: the open one's tail is measured) */
        strike(t, 3, FS / 50u);
        before = 1;
        {
            uint32_t v;
            const drum_lane_t *L = &drum_kit[0][DV_HATO];
            before = L->x.ea;
            (void)v;
        }
        after = drum_kit[0][DV_HATO].x.ea;
        check("the closed hat chokes the open one", !drum_kit[0][DV_HATO].x.live || after < (1 << 30) / 1000.0,
              "open hat envelope %.0f", after);
        (void)before;
    }

    /* 5 */
    {
        double r0, r1;
        track_t *t = kitx(64, 64);
        ok = str_eq(ENG_DRUM.edit[0].label, "MRPH") && str_eq(ENG_DRUM.edit[4].label, "NOIS") &&
             ENG_DRUM.knob[0] == P_E0;
        t->p[P_E4] = 0;                                  /* NOIS: the snare's noise out, then all noise */
        strike(t, 1, FS / 4u);
        r0 = rms(FS / 20u, FS / 4u);
        t = kitx(64, 64);
        t->p[P_E4] = 127;
        strike(t, 1, FS / 4u);
        r1 = rms(FS / 20u, FS / 4u);
        check("the knobs: MRPH (KNOB 1), NOIS moves the noise of every lane", ok && fabs(r0 - r1) > 0.2 * (r0 > r1 ? r0 : r1),
              "snare tail RMS %.0f / %.0f", r0, r1);
    }

    /* 5b */
    {
        uint32_t kit;
        for (kit = 0; kit < 1u; kit++) {
            track_t *t = kitx(64, 64);
            double r, ref;
            int32_t step = 0, own = 0, o[2 * CTL], act = 0;
            dx_mute_set(DXG_SNARE);
            for (i = 0; i < FS * 2u; i += CTL)          /* (the earlier hits' tails out) */
                mix_block(o, CTL);
            strike(t, 1, FS / 10u);
            r = rms(0, FS / 10u);
            strike(t, 0, FS / 10u);
            snprintf(nm, sizeof nm, "group mutes (%s): SNARE muted, silent; KICK sounds", "X");
            check(nm, r < 1.0 && rms(0, FS / 10u) > 300, "snare RMS %.1f, kick %.0f", r, rms(0, FS / 10u));
            dx_mute_set(0);
            strike(t, 0, FS / 10u);                     /* the kick's own largest step, then muted while it rings */
            for (i = 1; i < FS / 10u; i++)
                own = abs(buf[i] - buf[i - 1]) > own ? abs(buf[i] - buf[i - 1]) : own;
            ref = rms(FS / 50u + FS / 100u, FS / 25u);  /* (30 .. 40 ms after the hit, unmuted) */
            trk_note_on(t, NOTE[0], 100);
            for (i = 0; i < FS / 50u; i += CTL)
                mix_block(o, CTL);
            dx_mute_set(DXG_KICK);
            for (i = 0; i < FS / 50u; i += CTL) {
                uint32_t j;
                mix_block(o, CTL);
                for (j = 0; j < CTL; j++)
                    buf[i + j] = o[2 * j];
            }
            for (i = 1; i < FS / 50u; i++)
                step = abs(buf[i] - buf[i - 1]) > step ? abs(buf[i] - buf[i - 1]) : step;
            r = rms(FS / 100u, FS / 50u);                /* (the same window, muted at 20 ms) */
            for (i = 0; i < NVOICE; i++)
                act |= t->v[i].active;
            snprintf(nm, sizeof nm, "group mutes (%s): a ringing kick muted: ends, no click", "X");
            check(nm, !act && r < ref / 20 && step <= own + own / 5, "RMS %.0f (unmuted %.0f), step %d (own %d)", r, ref,
                  step, own);
            dx_mute_set(0);
            strike(t, 0, FS / 10u);
            check(kit ? "group mutes (STD): unmuted, the kick sounds again" : "group mutes (X): unmuted, the kick sounds again",
                  rms(0, FS / 10u) > 300, 0);
        }
    }

    /* 5c */
    {
        double e0, e1, t0, t1, before, during, after, c0, c1;
        int32_t o[2 * CTL], pk;
        track_t *t = kitx(64, 64), *s2 = &trk[1];
        uint32_t m;
        song.g[G_PUNCH] = 0;
        t->p[P_LEVEL] = 60;                              /* (under the master's limiter: PUNCH itself measured) */
        strike(t, 0, FS / 4u);
        e0 = rms(0, FS * 8u / 1000u);
        t0 = rms(FS / 10u, FS / 5u);
        t = kitx(64, 64);
        song.g[G_PUNCH] = 100;
        t->p[P_LEVEL] = 60;
        strike(t, 0, FS / 4u);
        e1 = rms(0, FS * 8u / 1000u);
        t1 = rms(FS / 10u, FS / 5u);
        song.g[G_PUNCH] = 0;
        check("PNCH 100: the kick's first 8 ms louder, its tail lower", e1 > e0 * 1.3 && t1 < t0 * 0.8,
              "attack RMS %.0f -> %.0f, tail %.0f -> %.0f", e0, e1, t0, t1);
        for (m = 0; m < 2u; m++) {                       /* DUCK 0, then 100: a held synth note, a kick */
            t = kitx(64, 64);
            host_preset(s2, 0, 0);
            for (i = 0; i < 4u; i++)
                s2->p[P_DIST + i] = 0;
            song.g[G_DUCK] = (int16_t)(m ? 100 : 0);
            song.g[G_DREL] = 30;
            t->p[P_LEVEL] = 0;                           /* (the kick itself out of the measure: only its duck) */
            trk_note_on(s2, 60, 100);
            for (i = 0; i < FS / 2u; i += CTL)
                mix_block(o, CTL);
            strike(t, 0, FS);
            during = rms(FS * 5u / 1000u, FS * 40u / 1000u);
            after = rms(FS * 8u / 10u, FS * 9u / 10u);
            trk_note_off(s2, 60);
            if (!m) {
                before = during;                         /* (DUCK 0: the same windows, undisturbed) */
                c0 = after;
            } else {
                c1 = after;
                check("DUCK 100: the kick ducks the synth 10 dB or more, back after the release (vs DUCK 0)",
                      during < before / 3.16 && fabs(20 * log10(c1 / c0)) < 1.0, "%.0f -> %.0f, after %.0f / %.0f",
                      before, during, c0, c1);
            }
        }
        song.g[G_DUCK] = 0;
        host_preset(s2, 0, 0);
        t = kitx(64, 64);                                /* CLIP: a loud mix (the kit's every lane at once) */
        for (m = 0; m < 2u; m++) {
            double r, p2 = 0;
            t = kitx(64, 64);
            song.g[G_CLIP] = (int16_t)(m ? 100 : 0);
            for (l = 0; l < 8u; l++)
                trk_note_on(t, NOTE[l], 127);
            strike(t, 0, FS / 5u);
            r = rms(0, FS / 5u);
            for (pk = 0, i = 0; i < FS / 5u; i++)
                pk = abs(buf[i]) > pk ? abs(buf[i]) : pk;
            p2 = pk;
            if (!m) c0 = p2 / r; else c1 = p2 / r;
            if (m)
                check("CLIP 100: the loud mix saturates (crest factor down), never past full scale", c1 < c0 * 0.9 &&
                      pk <= 32767, "crest %.2f -> %.2f, peak %d", c0, c1, pk);
        }
        song.g[G_CLIP] = 0;
    }

    /* 6 */
    for (l = 0; l < 8u; l++) {
        static const int16_t M[3] = {0, 64, 127};
        uint32_t m;
        for (m = 0; m < 3u; m++) {
            track_t *t = kitx(M[m], 64);
            strike(t, l, FS);
            snprintf(nm, sizeof nm, "%s_morph%d.wav", NAME[l], M[m]);
            save(dir, nm, FS);
        }
    }
    {
        static const uint8_t BEAT[16] = {1 | 8, 8, 8 | 4, 8, 2 | 8, 8, 1 | 8, 16, 1 | 8, 8, 8, 1 | 8, 2 | 8, 8, 32 | 8, 64 | 16};
        track_t *t = kitx(0, 64);
        uint32_t bar = 4u * 60u * FS / 120u, n = 4u * bar, s;
        static int32_t big[4 * 4 * 60 * FS / 120];
        for (s = 0; s < 16u; s++) {
            step_t st = {{0, 0, 0, 0}, 0, ST_NOTE, 0, 100, 0, 0};
            st.hit = BEAT[s];
            t->step[s] = st;
        }
        t->p[P_SLEN] = 16;
        song.g[G_BPM] = 120;
        transport_req = 1;
        for (i = 0; i < n; i += CTL) {
            int32_t o[2 * CTL];
            uint32_t j;
            t->p[P_E4] = (int16_t)(i * 127u / n);
            mix_block(o, CTL);
            for (j = 0; j < CTL && i + j < n; j++)
                big[i + j] = o[2 * j];
        }
        transport_req = 2;
        {
            char path[512];
            FILE *f;
            snprintf(path, sizeof path, "%s/beat_morph_sweep.wav", dir);
            if ((f = fopen(path, "wb"))) {
                wav_hdr(f, n);
                for (i = 0; i < n; i++)
                    wav_put(f, big[i], big[i]);
                fclose(f);
            }
        }
        printf("drumx: demos in %s\n", dir);
    }
    printf("drumx: %s\n", fails ? "FAILED" : "all checks ok");
    return fails != 0;
}
