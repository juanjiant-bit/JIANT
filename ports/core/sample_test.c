/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The sample engines of the big CPUs (ports/core/build.sh test): SAMPLE (eng_sample.c), GRAIN (eng_grain.c) and DRUM-X's
 * sample layer (eng_drum.c dxs_*) against synthetic slots, through the whole core (the firmware's internals at hand:
 * it includes jiant_core.c). Every mode and trigger makes the sound it should, feedback stays bounded at its top, DESTROY
 * never touches the loaded original, REC records the bars asked for, the output always finite; the engines' cost at
 * heavy settings printed. */
#include "jiant_core.c"
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

static int fails;
#define CHECK(c, ...) do { if (c) printf("  ok    " __VA_ARGS__); else { printf("  FAIL  " __VA_ARGS__); fails++; } \
                           printf("\n"); } while (0)

#define MAXR (10u * 44100u)
static float ol[MAXR], or_[MAXR];
static int nonfinite;
static void ren(uint32_t n)                    /* n frames into ol / or_ (finite checked) */
{
    uint32_t i;
    if (n > MAXR)
        n = MAXR;
    jiant_render(ol, or_, n);
    for (i = 0; i < n; i++)
        if (!(ol[i] == ol[i]) || !(or_[i] == or_[i]) || fabsf(ol[i]) > 4.0f || fabsf(or_[i]) > 4.0f)
            nonfinite++;
}
static double wrms(const float *x, uint32_t a, uint32_t b)   /* rms of x[a, b) */
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += (double)x[i] * x[i];
    return b > a ? sqrt(s / (b - a)) : 0.0;
}
static uint32_t zc(const float *x, uint32_t a, uint32_t b)   /* zero crossings, upwards */
{
    uint32_t i, n = 0;
    for (i = a + 1u; i < b; i++)
        n += x[i - 1u] <= 0.0f && x[i] > 0.0f;
    return n;
}
static void note(uint32_t k, uint32_t vel) { jiant_midi(0x90, (uint8_t)k, (uint8_t)vel); }
static void off(uint32_t k) { jiant_midi(0x80, (uint8_t)k, 0); }
static uint32_t sounding(const track_t *t)
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].stage != 4u;
    return n;
}

/* track 1 on engine e, preset pr, dry (no sends), flat envelope */
static void use(uint32_t e, uint32_t pr)
{
    track_t *t = &trk[0];
    set_engine_of(t, e);
    ren(2048);
    apply_preset_to(t, pr);
    ren(2048);
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = t->p[P_DIST] = 0;
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    t->p[P_REL] = 4;
    t->p[P_LEVEL] = 100;
}

/* the cost: every part on engine e (preset pr, its values set by fn), 8 notes each held; seconds of audio a second */
static double cost(uint32_t e, uint32_t pr, void (*fn)(track_t *), uint32_t *voices)
{
    struct timespec t0, t1;
    uint32_t k, j;
    double secs;
    ui_rec_prefs |= PREF_SIX >> 8;             /* (MENU > TRACKS 6) */
    trk_live = NTRK;
    for (k = 0; k < 6u; k++) {
        trk[k].p[P_MUTE] = 0;
        set_engine_of(&trk[k], e);
    }
    ren(4096);
    for (k = 0; k < 6u; k++) {
        apply_preset_to(&trk[k], pr);
        trk[k].p[P_VOICE] = V_POLY;
        if (fn)
            fn(&trk[k]);
    }
    ren(4096);
    for (k = 0; k < 6u; k++)
        for (j = 0; j < 8u; j++)
            trk_note_on(&trk[k], 48 + j * 3u, 100);
    ren(4410);
    *voices = jiant_voices_sounding();
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (k = 0; k < 10u; k++)
        ren(44100);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    secs = (double)(t1.tv_sec - t0.tv_sec) + (double)(t1.tv_nsec - t0.tv_nsec) * 1e-9;
    for (k = 0; k < 6u; k++) {
        for (j = 0; j < 8u; j++)
            trk_note_off(&trk[k], 48 + j * 3u);
        trk[k].p[P_MUTE] = k != 0u;
    }
    ren(44100);
    return 10.0 / secs;
}
static void heavy_sample(track_t *t)
{
    t->p[P_E0] = 6;
    t->p[P_E3] = SM_LOOP;
    t->p[P_E5] = 110;                          /* the comb on */
    t->p[P_E7] = 40;                           /* DESTROY on */
}

static void heavy_grain(track_t *t)
{
    t->p[P_E0] = 6;
    t->p[P_E3] = 100;                          /* 190 ms grains, */
    t->p[P_E4] = 127;                          /* 200 a second: clouds at their cap */
    t->p[P_E6] = 40;
    t->p[P_GXFB] = 60;
    t->p[P_GXWIDTH] = 127;
}

/* DRUM-X with its sample layer set up but SMORPH 0 (variant 1) or as it boots, no slots (0): a pattern of hits rendered
 * from boot, its hash. In a child process (each from its own boot) */
static uint64_t drum_hash(int variant)
{
    int fd[2];
    uint64_t h = 0;
    pid_t pid;
    if (pipe(fd))
        return 1;
    pid = fork();
    if (pid == 0) {
        uint32_t k, j;
        uint64_t x = 1469598103934665603ull;
        static float a[44100];
        jiant_boot(11, 1);                     /* (booted before the fork: the parent's state, the same for both) */
        for (k = 0; k < 16u; k++)
            jiant_slot_clear(k);
        for (k = 1; k < 6u; k++)
            trk[k].p[P_MUTE] = 1;
        set_engine_of(&trk[0], ENGI_DRUM);
        ren(4096);
        if (variant) {
            for (k = 0; k < 44100u; k++)
                a[k] = 0.5f * (float)sin(2.0 * M_PI * 1000.0 * k / 44100.0);
            for (k = 0; k < 16u; k++)
                jiant_slot_load(k, a, a, 22050, 44100, "SINE");
            trk[0].p[P_DXSBLEND] = DXB_RING;
            trk[0].p[P_DXSMAP] = 126;
            trk[0].p[P_DXSSLOT] = 115;
            trk[0].p[P_DXSLEVEL] = 30;
        }
        for (k = 0; k < 16u; k++) {
            static const uint8_t N[4] = {36, 42, 38, 46};
            note(N[k & 3u], 90 + k);
            ren(5512);
            for (j = 0; j < 5512u; j++) {
                uint32_t b;
                memcpy(&b, &ol[j], 4);
                x = (x ^ b) * 1099511628211ull;
                memcpy(&b, &or_[j], 4);
                x = (x ^ b) * 1099511628211ull;
            }
            off(N[k & 3u]);
        }
        if (write(fd[1], &x, sizeof x) != sizeof x)
            _exit(2);
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &h, sizeof h) != sizeof h)
        h = 2;
    close(fd[0]);
    waitpid(pid, 0, 0);
    return h;
}

static float S1[44100], S2[44100];
int main(int argc, char **argv)
{
    track_t *t = &trk[0];
    uint32_t i;
    printf("JIANT sample engines %s\n", jiant_version());
    jiant_boot(7, 1);
    for (i = 0; i < 6u; i++)                   /* the other parts quiet */
        trk[i].p[P_MUTE] = i != 0u;
    /* slot 1: 1 s of 441 Hz, its level rising 0 -> 1 (a ramp to tell forward from backward); slot 2: four quarters
     * of 220, 440, 880, 1760 Hz (slices); slot 3: a 30 ms burst (ratchets) */
    for (i = 0; i < 44100u; i++) {
        S1[i] = 0.8f * (float)i / 44100.0f * (float)sin(2.0 * M_PI * 441.0 * i / 44100.0);
        S2[i] = 0.5f * (float)sin(2.0 * M_PI * 220.0 * (1u << (i / 11025u)) * i / 44100.0);
    }
    jiant_slot_load(0, S1, 0, 44100, 44100, "RAMP");
    jiant_slot_load(1, S2, 0, 44100, 44100, "QUARTERS");
    for (i = 0; i < 44100u; i++)
        S1[i] = i < 1323u ? 0.8f * (float)sin(2.0 * M_PI * 300.0 * i / 44100.0) : 0.0f;
    jiant_slot_load(2, S1, 0, 4410, 44100, "BURST");
    for (i = 0; i < 44100u; i++) {             /* slot 7: stereo, two tones */
        S1[i] = 0.4f * (float)sin(2.0 * M_PI * 330.0 * i / 44100.0);
        S2[i] = 0.4f * (float)sin(2.0 * M_PI * 495.0 * i / 44100.0);
    }
    jiant_slot_load(6, S1, S2, 44100, 44100, "STEREO");
    jiant_service();

    printf("SAMPLE\n");
    use(ENGI_SAMPLE, 0);                       /* ONE SHOT */
    CHECK(t->engine == ENGI_SAMPLE && !strcmp(ENGINES[t->engine]->name, "SAMPLE"), "track 1 plays SAMPLE");
    note(60, 110);
    ren(2205);
    off(60);
    ren(2u * 44100u);
    {
        double a = wrms(ol, 4410, 13230), b = wrms(ol, 26460, 35280), c = wrms(ol, 50000, 60000);
        CHECK(b > 1.8 * a && b > 0.01 && c < 0.001, "ONE: the ramp plays on after the note-off, rising (%.3f -> %.3f), then ends (%.5f)",
              a, b, c);
        CHECK(zc(ol, 22050, 33075) > 105 && zc(ol, 22050, 33075) < 116, "C4 the original speed (%u cycles in 0.25 s)",
              zc(ol, 22050, 33075));
    }
    note(72, 110);
    ren(44100);
    off(72);
    CHECK(zc(ol, 4410, 15435) > 215 && zc(ol, 4410, 15435) < 226 && wrms(ol, 30000, 44100) < 0.0005,
          "an octave up: twice the speed, over in half the time (%u cycles)", zc(ol, 4410, 15435));
    t->p[P_E3] = SM_GATE;
    ren(4096);
    note(60, 110);
    ren(13230);
    off(60);
    ren(22050);
    CHECK(wrms(ol, 8820, 22050) < 0.0005, "GATE: stops on the release (%.5f)", wrms(ol, 8820, 22050));
    t->p[P_E3] = SM_REV;
    ren(4096);
    note(60, 110);
    ren(44100);
    off(60);
    CHECK(wrms(ol, 4410, 13230) > 1.8 * wrms(ol, 26460, 35280), "REV: backwards, falling (%.3f -> %.3f)",
          wrms(ol, 4410, 13230), wrms(ol, 26460, 35280));
    t->p[P_E3] = SM_LOOP;
    ren(4096);
    note(60, 110);
    ren(3u * 44100u);
    off(60);
    CHECK(wrms(ol, 2u * 44100u + 22050u, 3u * 44100u - 2000u) > 0.02, "LOOP: still playing at 2.5 s (%.3f)",
          wrms(ol, 2u * 44100u + 22050u, 3u * 44100u - 2000u));
    ren(22050);
    t->p[P_E3] = SM_PING;
    ren(4096);
    t->p[P_E2] = 64;
    note(60, 110);
    ren(2u * 44100u);
    off(60);
    CHECK(wrms(ol, 44100, 88200) > 0.01, "PING: forth and back while held (%.3f)", wrms(ol, 44100, 88200));
    ren(22050);

    use(ENGI_SAMPLE, 5);                       /* SLICE 16 */
    t->p[P_E0] = 1;
    t->p[P_SXSLC] = 4;
    {
        static const uint32_t HZ[4] = {220, 440, 880, 1760};
        uint32_t k, good = 0;
        for (k = 0; k < 4u; k++) {
            uint32_t c;
            note(60 + k, 110);
            ren(8820);
            off(60 + k);
            c = zc(ol, 1000, 5410) * 10u;
            ren(4410);
            good += c > HZ[k] * 9u / 10u && c < HZ[k] * 11u / 10u;
            printf("        slice %u: %u Hz (%u)\n", k + 1u, c, HZ[k]);
        }
        CHECK(good == 4u, "SLICE: C4 + n plays slice n + 1, at its own speed");
    }
    t->p[P_SXTRIG] = ST_CHOKE;
    note(60, 110);
    ren(1000);
    note(61, 110);
    ren(1000);
    note(62, 110);
    ren(2000);
    CHECK(sounding(t) == 1u, "CHOKE: one voice (%u)", sounding(t));
    off(60); off(61); off(62);
    ren(22050);
    t->p[P_SXTRIG] = ST_VEL;
    t->p[P_SXAMT] = 127;
    note(60, 127);
    ren(8820);
    off(60);
    CHECK(zc(ol, 1000, 5410) * 10u > 1500u, "VEL: velocity 127 picks the last slice (%u Hz)", zc(ol, 1000, 5410) * 10u);
    ren(22050);

    use(ENGI_SAMPLE, 11);                      /* RATCHET */
    t->p[P_E0] = 2;
    t->p[P_SXAMT] = 19;                        /* 4 hits */
    t->p[P_SXRATE] = 3;                        /* 1/16 */
    note(60, 110);
    ren(44100);
    off(60);
    {
        uint32_t q = beat_samples() / 4u, k, on = 0;
        for (k = 0; k < 6u; k++)
            on += wrms(ol, k * q, k * q + 1000u) > 0.003;
        CHECK(on == 4u, "RATCH: 4 hits a 1/16 apart (%u of 6 sixteenths sound)", on);
    }
    use(ENGI_SAMPLE, 15);                      /* CHANCE */
    t->p[P_E0] = 2;
    t->p[P_SXAMT] = 64;
    {
        uint32_t k, hit = 0;
        for (k = 0; k < 40u; k++) {
            note(60, 110);
            ren(1500);
            off(60);
            hit += wrms(ol, 0, 1500) > 0.003;
            ren(3000);
        }
        CHECK(hit > 8u && hit < 32u, "CHANCE 50 %%: %u of 40 hits play", hit);
    }
    use(ENGI_SAMPLE, 13);                      /* ROUND ROBIN */
    t->p[P_E0] = 0;
    t->p[P_SXAMT] = 127 * 2 / 15 + 1;          /* slots 1..3 */
    {
        uint32_t k, sl[3];
        for (k = 0; k < 3u; k++) {
            note(60, 110);
            ren(200);
            sl[k] = smp_v[0][t->m_vi].slot;
            off(60);
            ren(4000);
        }
        CHECK(sl[0] != sl[1] && sl[1] != sl[2] && sl[0] != sl[2], "ROUND: slots %u %u %u", sl[0] + 1u, sl[1] + 1u,
              sl[2] + 1u);
    }
    use(ENGI_SAMPLE, 14);                      /* LEGATO */
    t->p[P_E3] = SM_GATE;
    ren(4096);
    t->p[P_VOICE] = V_POLY;
    note(60, 110);
    ren(22050);
    note(62, 110);
    ren(100);
    {
        double at = smp_v[0][t->m_vi].pos;
        CHECK(at > 20000.0, "LEGATO: the new note goes on from where the last was (frame %.0f)", at);
    }
    off(60); off(62);
    ren(22050);
    use(ENGI_SAMPLE, 9);                       /* SCRUB */
    note(60, 30);
    ren(22050);
    {
        double p1 = smp_v[0][t->m_vi].pos;
        off(60);
        ren(4410);
        note(60, 120);
        ren(22050);
        CHECK(p1 < 15000.0 && smp_v[0][t->m_vi].pos > 30000.0 && wrms(ol, 11025, 22050) > 0.003,
              "SCRUB: the velocity sends the playhead (frame %.0f, then %.0f)", p1, smp_v[0][t->m_vi].pos);
        off(60);
        ren(22050);
    }
    use(ENGI_SAMPLE, 10);                      /* TAPE STOP */
    note(60, 110);
    ren(22050);
    off(60);
    ren(3u * 44100u);
    CHECK(wrms(ol, 0, 4410) > 0.005 && wrms(ol, 2u * 44100u, 3u * 44100u) < 0.0005 && zc(ol, 0, 4410) > zc(ol, 13230, 17640),
          "TAPE: slows to a stop after the release (%u -> %u cycles a 0.1 s)", zc(ol, 0, 4410), zc(ol, 13230, 17640));
    jiant_slot_clear(3);
    use(ENGI_SAMPLE, 0);
    t->p[P_E0] = 3;
    note(60, 110);
    ren(4410);
    off(60);
    CHECK(wrms(ol, 0, 4410) == 0.0 && ENGINES[t->engine]->desc(t, 0) == &SMP_EMPTY, "an empty slot: silent, EDIT says EMPTY");

    use(ENGI_SAMPLE, 17);                      /* METAL COMB, FEEDBK at its top */
    t->p[P_E5] = 127;
    t->p[P_E6] = 127;
    {
        float pk = 0;
        uint32_t k;
        nonfinite = 0;
        for (k = 0; k < 8u; k++)
            note(36 + 7 * k, 127);
        ren(3u * 44100u);
        for (i = 0; i < 3u * 44100u; i++)
            pk = fabsf(ol[i]) > pk ? fabsf(ol[i]) : pk;
        CHECK(!nonfinite && pk < 1.5f && wrms(ol, 2u * 44100u, 3u * 44100u) > 0.01,
              "FEEDBK 127, 8 voices: rings on by itself, bounded (peak %.2f), finite", pk);
        for (k = 0; k < 8u; k++)
            off(36 + 7 * k);
        ren(44100);
    }
    {                                          /* DESTROY */
        const smp_obj_t *o = smp_get(0);
        static int16_t keep[44100];
        uint32_t diff = 0, k;
        jiant_slot_info_t in;
        memcpy(keep, o->orig[0], sizeof keep);
        use(ENGI_SAMPLE, 18);
        t->p[P_E7] = 127;
        for (k = 0; k < 4u; k++) {
            note(60 + k, 120);
            ren(30000);
            off(60 + k);
            ren(2000);
        }
        for (k = 0; k < 44100u; k++)
            diff += o->work[0][k] != keep[k];
        jiant_slot_info(0, &in);
        CHECK(!memcmp(keep, o->orig[0], sizeof keep) && diff > 10000u && in.changed,
              "DESTROY: the working copy worn (%u frames), the original untouched, the slot changed", diff);
        jiant_slot_load(0, S1, 0, 100, 44100, "X");   /* (a reload brings it back: a new object) */
    }
    {                                          /* REC from inside: MST 1 into slot 5 */
        jiant_slot_info_t in;
        uint32_t want = beat_samples() * 4u, k;
        use(ENGI_SAMPLE, 19);
        t->p[P_E0] = 4;
        ren(256);
        t->p[P_SXREC] = 6;                     /* MST 1 */
        ren(256);
        CHECK(smp_rec.st == SR_REC && smp_rec.owner == 0 && !strcmp(track_desc(t, P_SXREC)->names[6], "REC"),
              "REC MST 1 armed from the engine: recording (stopped: at once), the screen says REC");
        note(67, 110);
        for (k = 0; k < 30u && smp_rec.st != SR_IDLE; k++)
            ren(8192);
        off(67);
        ren(512);
        jiant_slot_info(4, &in);
        CHECK(in.frames == want && t->p[P_SXREC] == 0 && in.changed, "a bar recorded (%u frames, %u wanted), REC back to OFF",
              in.frames, want);
        jiant_service();
    }
    {
        uint32_t nv;
        double x = cost(ENGI_SAMPLE, 2, heavy_sample, &nv);
        printf("  cost  SAMPLE LOOP + FEEDBK + DESTROY, stereo slot: %u voices at %.1fx real time (%.2f %% of a core a voice)\n",
               nv, x, 100.0 / x / (nv ? nv : 1u));
        CHECK(nv == 48u && x > 2.0, "48 SAMPLE voices at once, faster than real time");
    }
    printf("GRAIN\n");
    for (i = 0; i < 44100u; i++)               /* slot 1 again: the ramp */
        S1[i] = 0.8f * (float)i / 44100.0f * (float)sin(2.0 * M_PI * 441.0 * i / 44100.0);
    jiant_slot_load(0, S1, 0, 44100, 44100, "RAMP");
    use(ENGI_GRAIN, 1);                        /* FROZEN CLOUD */
    CHECK(t->engine == ENGI_GRAIN && !strcmp(ENGINES[t->engine]->name, "GRAIN"), "track 1 plays GRAIN");
    t->p[P_E0] = 1;                            /* the quarters: 220 Hz at the start */
    t->p[P_E1] = 0;
    t->p[P_E5] = 0;                            /* no spray, */
    t->p[P_E6] = 0;                            /* no detune */
    t->p[P_GXREG] = 20;
    ren(4096);
    note(60, 110);
    ren(44100);
    {
        uint32_t c = zc(ol, 11025, 33075) * 2u;
        double ph = gr_v[0][t->m_vi].ph;
        CHECK(wrms(ol, 11025, 44100) > 0.002, "it sounds (%.4f: frozen, unsprayed grains of one place partly cancel)", wrms(ol, 11025, 44100));
        CHECK(c > 200u && c < 240u, "C4: the source's pitch (%u Hz, 220)", c);
        ren(22050);
        CHECK(gr_v[0][t->m_vi].ph == ph, "FREEZE: the playhead stays (frame %.0f)", ph);
        off(60);
        ren(22050);
        note(72, 110);
        ren(44100);
        c = zc(ol, 11025, 33075) * 2u;
        CHECK(c > 410u && c < 470u, "C5: an octave up (%u Hz)", c);
        off(72);
        ren(22050);
    }
    t->p[P_GXTRAV] = GT_FWD;
    t->p[P_E2] = 0;
    ren(4096);
    note(60, 110);
    ren(22050);
    {
        double ph0 = gr_v[0][t->m_vi].ph;
        ren(22050);
        CHECK(gr_v[0][t->m_vi].ph == ph0, "FWD at SCAN 0: frozen (frame %.0f)", ph0);
        off(60);
        ren(22050);
    }
    t->p[P_GXREG] = 127;
    t->p[P_GXTRAV] = GT_STRETCH;
    t->p[P_E2] = 21;                           /* 2x */
    ren(4096);
    note(72, 110);
    ren(2205);
    {
        double ph0 = gr_v[0][t->m_vi].ph, d;
        ren(8820);
        d = gr_v[0][t->m_vi].ph - ph0;
        CHECK(d > 17000.0 && d < 18300.0, "STRETCH 2x: the playhead at twice real time (%.0f frames in 8820), the pitch the key's",
              d);
        off(72);
        ren(22050);
    }
    t->p[P_GXTRAV] = GT_REV;
    t->p[P_E2] = 30;
    ren(4096);
    note(60, 110);
    ren(2205);
    {
        double ph0 = gr_v[0][t->m_vi].ph;
        ren(4410);
        CHECK(gr_v[0][t->m_vi].ph < ph0 || gr_v[0][t->m_vi].ph > 40000.0, "REV: the playhead backwards (%.0f -> %.0f)", ph0,
              gr_v[0][t->m_vi].ph);
        off(60);
        ren(22050);
    }
    t->p[P_GXTRAV] = GT_PING;
    t->p[P_E2] = 63;
    t->p[P_GXREG] = 10;
    ren(4096);
    note(60, 110);
    {
        uint32_t k, flips = 0;
        float d0 = 1.0f;
        for (k = 0; k < 40u; k++) {
            ren(441);
            flips += gr_v[0][t->m_vi].dir != d0;
            d0 = gr_v[0][t->m_vi].dir;
        }
        CHECK(flips >= 4u, "PING: forth and back (%u turns in 0.4 s)", flips);
        off(60);
        ren(22050);
    }
    t->p[P_GXTRAV] = GT_KEY;
    t->p[P_GXREG] = 127;
    ren(4096);
    note(36, 110);
    ren(2205);
    {
        double lo = gr_v[0][t->m_vi].ph, hi;
        off(36);
        ren(22050);
        note(84, 110);
        ren(2205);
        hi = gr_v[0][t->m_vi].ph;
        off(84);
        ren(22050);
        CHECK(lo < 100.0 && hi > 43000.0, "KEY: C2 the region's start, C6 its end (%.0f, %.0f)", lo, hi);
    }
    use(ENGI_GRAIN, 9);                        /* GLITCH: RAND */
    t->p[P_E0] = 0;
    ren(4096);
    note(60, 110);
    {
        uint32_t k, jumps = 0;
        double p0 = -1;
        for (k = 0; k < 20u; k++) {
            ren(2205);
            jumps += gr_v[0][t->m_vi].ph != p0;
            p0 = gr_v[0][t->m_vi].ph;
        }
        CHECK(jumps >= 5u, "RAND: the playhead jumps (%u places in 1 s)", jumps);
        off(60);
        ren(22050);
    }
    use(ENGI_GRAIN, 0);                        /* CLOUD at its most: DENSITY, SIZE, SPRAY, SPREAD, FEEDBK, SHIMMR */
    t->p[P_E0] = 6;
    t->p[P_E3] = t->p[P_E4] = t->p[P_E5] = t->p[P_E6] = 127;
    t->p[P_GXFB] = 127;
    t->p[P_GXSHIM] = 1;
    t->p[P_GXWIDTH] = 127;
    ren(4096);
    {
        float pk = 0;
        uint32_t k, most = 0;
        nonfinite = 0;
        for (k = 0; k < 8u; k++)
            note(48 + 5 * k, 127);
        for (k = 0; k < 10u; k++) {
            uint32_t j, c = 0;
            ren(22050);
            for (j = 0; j < 22050u; j++)
                pk = fabsf(ol[j]) > pk ? fabsf(ol[j]) : pk;
            for (j = 0; j < 64u; j++)
                c += (uint32_t)((gr_v[0][t->m_vi].live >> j) & 1u);
            most = c > most ? c : most;
        }
        CHECK(!nonfinite && pk < 1.5f && most <= GR_N && most > 20u,
              "DENSITY SIZE FEEDBK SHIMMR at their top, 8 voices, 5 s: finite, bounded (peak %.2f), %u grains a voice", pk, most);
        CHECK(wrms(ol, 0, 22050) > 0.01 && fabs(wrms(ol, 0, 22050) - wrms(or_, 0, 22050)) + fabsf(ol[1000] - or_[1000]) > 0.0,
              "WIDTH: the grains in stereo (L %.3f R %.3f)", wrms(ol, 0, 22050), wrms(or_, 0, 22050));
        for (k = 0; k < 8u; k++)
            off(48 + 5 * k);
        ren(44100);
    }
    use(ENGI_GRAIN, 12);                       /* LIVE FREEZE: the input */
    t->p[P_GXTRAV] = GT_FWD;
    t->p[P_E1] = 10;
    ren(4096);
    {
        static float il[4410], ir[4410];
        uint32_t k, c;
        for (k = 0; k < 4410u; k++)
            il[k] = ir[k] = 0.5f * (float)sin(2.0 * M_PI * 300.0 * k / 44100.0);
        ren(8192);
        note(60, 110);
        ren(22050);
        CHECK(wrms(ol, 2000, 22050) < 0.0005, "LIVE IN, no input: silent (%.5f)", wrms(ol, 2000, 22050));
        for (k = 0; k < 20u; k++) {
            jiant_input(il, ir, 4410);
            ren(4410);
        }
        c = zc(ol, 0, 4410) * 10u;
        CHECK(wrms(ol, 0, 4410) > 0.005 && c > 270u && c < 330u, "LIVE IN: grains of the input as it plays (%u Hz, 300)", c);
        t->p[P_GXTRAV] = GT_FREEZE;
        for (k = 0; k < 10u; k++)
            ren(4410);
        CHECK(wrms(ol, 0, 4410) > 0.005, "LIVE FREEZE: what was played held after the input stopped (%.3f)", wrms(ol, 0, 4410));
        off(60);
        ren(44100);
    }
    {
        uint32_t nv;
        double x = cost(ENGI_GRAIN, 0, heavy_grain, &nv);
        printf("  cost  GRAIN DENSITY 127, SIZE 100, FEEDBK, stereo: %u voices at %.1fx real time (%.2f %% of a core a voice)\n",
               nv, x, 100.0 / x / (nv ? nv : 1u));
        CHECK(nv == 48u && x > 1.0, "48 GRAIN voices at once (clouds of up to %u grains), faster than real time", GR_N);
    }
    printf("DRUM-X's sample layer\n");
    {
        uint64_t h0 = drum_hash(0), h1 = drum_hash(1);
        CHECK(h0 == h1 && h0 > 2u, "SMORPH 0, slots loaded, BLEND SLOT MAP SLEVEL set: bit for bit DRUM-X as it is (%016llx)",
              (unsigned long long)h0);
    }
    for (i = 0; i < 44100u; i++) {             /* slots 9..16: 1 kHz, 16 slices of 100 Hz steps in slot 9 */
        S1[i] = 0.6f * (float)sin(2.0 * M_PI * 1000.0 * i / 44100.0);
        S2[i] = 0.6f * (float)sin(2.0 * M_PI * 100.0 * (1u + i / 2756u) * i / 44100.0);
    }
    for (i = 9; i < 16u; i++)
        jiant_slot_load(i, S1, 0, 44100, 44100, "KHZ");
    jiant_slot_load(8, S2, 0, 44100, 44100, "STEPS");
    use(ENGI_DRUM, 0);
    CHECK(t->p[P_DXSSLOT] == 112 && t->p[P_DXSMAP] == 125 && !t->p[P_DXSMORPH] &&
          !strcmp(track_desc(t, P_DXSMORPH)->label, "SMORPH") && !strcmp(track_desc(t, P_DXSSLOT)->names[112], "1"),
          "a DRUM preset: the layer off, SLOT 1, MAP SLOTS");
    t->p[P_DXSSLOT] = 112 + 9;                 /* SLOTS from 10: the kick (lane 0) slot 10 */
    t->p[P_DXSMORPH] = 127;
    t->p[P_E3] = 127;
    {
        uint32_t c;
        note(36, 90);
        ren(22050);
        off(36);
        c = zc(ol, 4410, 13230) * 5u;
        CHECK(c > 950u && c < 1050u && wrms(ol, 4410, 13230) > 0.01, "SMORPH 127 XFADE: the kick is its sample (%u Hz)", c);
        t->p[P_DXSMORPH] = 64;
        note(36, 90);
        ren(22050);
        off(36);
        CHECK(wrms(ol, 4410, 13230) > 0.005, "SMORPH 64: both (%.3f)", wrms(ol, 4410, 13230));
    }
    {
        uint32_t b, bad = 0;
        nonfinite = 0;
        t->p[P_DXSMORPH] = 127;
        for (b = 0; b < DXB_N; b++) {
            t->p[P_DXSBLEND] = (int16_t)b;
            note(36, 90);
            note(38, 90);
            note(42, 90);
            ren(11025);
            off(36); off(38); off(42);
            bad += wrms(ol, 0, 11025) < 0.001;
            if (b == DXB_SB)                       /* the synth's transient first, the sample's body after */
                CHECK(zc(ol, 8000, 11025) * 44100u / 3025u > 900u, "S>B: the sample's body after the hit (%u Hz)",
                      zc(ol, 8000, 11025) * 44100u / 3025u);
        }
        CHECK(!bad && !nonfinite, "every BLEND sounds, finite (XFADE S>B B>S FOLW RING)");
    }
    t->p[P_DXSBLEND] = DXB_XFADE;
    ren(2u * 44100u);
    t->p[P_DXSMAP] = 127;                      /* KEYS: slot 9 in 16, 36 + n its slice n */
    t->p[P_DXSSLOT] = 112 + 8;
    {
        uint32_t c;
        note(36 + 5, 100);                     /* (41: a tom: slice 6, 600 Hz) */
        ren(4410);
        off(41);
        c = zc(ol, 400, 2400) * 44100u / 2000u;
        CHECK(c > 520u && c < 680u, "KEYS: note 41 plays slice 6 (%u Hz, 600)", c);
        ren(11025);
    }
    t->p[P_DXSMAP] = 125;
    t->p[P_DXSSLOT] = 112 + 9;
    {   /* a closed hat chokes the open hat's sample */
        note(46, 100);
        ren(4410);
        note(42, 100);
        ren(4410);
        {
            uint32_t k, open = 0;
            for (k = 0; k < NVOICE; k++)
                open += dxs_v[0][k].on && dxs_v[0][k].lane == DV_HATO && !dxs_v[0][k].choke;
            CHECK(!open, "the closed hat chokes the open one's sample");
        }
        off(46); off(42);
        ren(22050);
    }
    t->p[P_DXSMORPH] = 0;
    if (argc > 2) {                            /* screenshots of the pages (argv[2]: a folder; PPM) */
        static const char *const PG[][2] = {{"SAMPLE", "EDIT 1"}, {"SAMPLE", "TRIG"}, {"SAMPLE", "REC"}, {"GRAIN", "EDIT 1"},
                                            {"GRAIN", "TRAVEL"}, {"GRAIN", "PITCH"}, {"DRUM", "SAMPLES"}};
        uint32_t s, pg;
        for (s = 0; s < NELEM(PG); s++) {
            char fn[256];
            FILE *f;
            uint32_t e = !strcmp(PG[s][0], "SAMPLE") ? ENGI_SAMPLE : !strcmp(PG[s][0], "GRAIN") ? ENGI_GRAIN : ENGI_DRUM;
            use(e, e == ENGI_SAMPLE ? 5 : e == ENGI_GRAIN ? 0 : 0);
            t->p[P_E0] = e == ENGI_SAMPLE ? 1 : 6;
            song.sel = 0;
            for (pg = 0; pg < NPAGES; pg++)
                if (!strcmp(PAGES[pg].title, PG[s][1]) && page_visible(pg))
                    break;
            ui.home = 0;
            ui.page = (uint8_t)pg;
            ui.force = 1;
            note(62, 100);
            ren(8820);
            snprintf(fn, sizeof fn, "%s/%s_%s.ppm", argv[2], PG[s][0], PG[s][1]);
            f = fopen(fn, "wb");
            if (f) {
                const uint16_t *px = jiant_screen();
                fprintf(f, "P6 240 240 255\n");
                for (i = 0; i < 240u * 240u; i++) {
                    uint16_t c = (uint16_t)(px[i] >> 8 | px[i] << 8);
                    fputc((c >> 11) << 3, f);
                    fputc(((c >> 5) & 63u) << 2, f);
                    fputc((c & 31u) << 3, f);
                }
                fclose(f);
            }
            off(62);
            ren(8820);
        }
    }
    CHECK(!nonfinite, "the output always finite");
    if (argc > 1) {                            /* a project for the FM-1's side (fm1_load_test): T1 DRUM-X with its
                                                * samples on, T2 SAMPLE, T3 GRAIN */
        FILE *f;
        size_t n;
        uint8_t *st;
        use(ENGI_DRUM, 0);
        t->p[P_DXSMORPH] = 100;
        t->p[P_DXSBLEND] = DXB_SB;
        set_engine_of(&trk[1], ENGI_SAMPLE);
        set_engine_of(&trk[2], ENGI_GRAIN);
        ren(4096);
        apply_preset_to(&trk[1], 16);
        apply_preset_to(&trk[2], 5);
        ren(4096);
        n = jiant_state_save(0, 0);
        st = malloc(n);
        f = fopen(argv[1], "wb");
        CHECK(st && f && jiant_state_save(st, n) == n && fwrite(st, 1, n, f) == n && trk[1].engine == ENGI_SAMPLE &&
              trk[2].engine == ENGI_GRAIN, "a project with SAMPLE, GRAIN and DRUM-X's samples saved for the FM-1 (%zu bytes)", n);
        if (f)
            fclose(f);
        free(st);
    }
    return fails != 0;
}
