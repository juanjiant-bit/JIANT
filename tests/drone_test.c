/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.33) DRONE, the voice mode (EDIT > PLAY POLY's fifth value; voice.c drone_on): a note held after its key is let
 * go; the next note glides there (GLIDE's time, 58 ms at least) on the same voice, its envelope going on; a chord held
 * as a chord, the next one gliding voice by voice; a chord into one note: the one glides, the rest let go; the droning
 * note's key struck alone again lets it go; OCT- + OCT+ (the octave as it was), STOP, PANIC and leaving DRONE let it all
 * go with RELEASE; the sequencer's notes held to the next one (a note repeated holds); MIDI IN as the keys; a kit
 * ignores it; every engine but the kit's holds and glides.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src tests/drone_test.c -lm -o build/host/drone_test
 *   build/host/drone_test [OUT.wav]      (OUT.wav: an ANALOG pad in DRONE, three notes gliding, then let go) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int nbad;
static void ck(const char *what, int ok)
{
    printf("drone: %-104s %s\n", what, ok ? "ok" : "FAIL");
    nbad += !ok;
}
static FILE *wf;
static uint32_t wn;
static void blocks(uint32_t n)                          /* the audio: events, then the whole mix */
{
    static int32_t o[2 * CTL];
    uint32_t i;
    while (n--) {
        events_block(CTL);
        mix_block(o, CTL);
        if (wf)
            for (i = 0; i < CTL; i++, wn++) wav_put(wf, o[2 * i], o[2 * i + 1]);
    }
}
static uint32_t ms(uint32_t m) { return m * (uint32_t)FS / 1000u / CTL + 1u; }   /* blocks of m ms */
static track_t *T;
static uint32_t held(void)                              /* voices gated */
{
    uint32_t i, n = 0;
    for (i = 0; i < NVOICE; i++) n += T->v[i].active && T->v[i].gate;
    return n;
}
static int holds(uint32_t note)                         /* a gated voice plays it: its index + 1 */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (T->v[i].active && T->v[i].gate && T->v[i].note == note) return (int)i + 1;
    return 0;
}
static void on(uint32_t n) { trk_note_on(T, n, 100); }
static void off(uint32_t n) { trk_note_off(T, n); }
static void tap(uint32_t n, uint32_t m) { on(n); blocks(ms(m)); off(n); blocks(2); }
static void setup(uint32_t engine, uint32_t preset)
{
    uint32_t i;
    ui_power_on();
    song.master_q12 = 4096;
    for (i = 1; i < NTRK; i++) trk[i].p[P_MUTE] = 1;
    T = &trk[0];
    song.sel = 0;
    set_engine_of(T, engine);
    T->engine = T->eng_req;
    apply_preset_to(T, preset);
    T->p[P_MUTE] = 0;
    T->p[P_VOICE] = V_DRONE;
    T->p[P_GLIDE] = 0;
    T->p[P_AMODE] = 0; T->p[P_AHOLD] = 0;
    blocks(200);
}
static uint32_t analog_preset(const char *name)
{
    uint32_t i;
    for (i = 0; i < ENG_ANALOG.npresets; i++)
        if (!strcmp(ENG_ANALOG.presets[i].name, name)) return i;
    return 0;
}

static void test_keys(void)
{
    uint32_t k, a, age, steps;
    int32_t prev;
    setup(ENGI_ANALOG, analog_preset("SOFT PAD"));
    tap(60, 200);
    blocks(ms(500));
    ck("a note let go keeps sounding (DRONE)", held() == 1u && holds(60) && T->v[holds(60) - 1].stage < 3u);
    k = (uint32_t)holds(60) - 1u;
    age = T->v[k].age;
    on(67); blocks(1);
    ck("the next note takes the same voice, its envelope going on (no retrigger)", holds(67) == (int)k + 1 && T->v[k].age == age &&
       T->v[k].stage < 3u && held() == 1u);
    ck(".. and glides (GLIDE 0: a singer's 58 ms at least), not a jump", T->v[k].pitch_cur != T->v[k].pitch16 && T->v[k].pitch_cur > 60 * 16 - 64);
    prev = T->v[k].pitch_cur; steps = 0;
    for (a = 0; a < ms(400) && T->v[k].pitch_cur != T->v[k].pitch16; a++) {
        blocks(1);
        steps += T->v[k].pitch_cur > prev;
        prev = T->v[k].pitch_cur;
    }
    ck(".. in time (there within 50..100 ms)", T->v[k].pitch_cur == T->v[k].pitch16 && a + 1u >= ms(50) && a <= ms(100));
    off(67); blocks(ms(300));
    ck(".. and held when let go", held() == 1u && holds(67));
    T->p[P_GLIDE] = 90; T->p[P_GLMODE] = 1;              /* a longer GLIDE (GL MODE TIME): a longer glide */
    on(72); blocks(ms(300)); off(72);
    ck("GLIDE 90 (TIME): still gliding after 300 ms", holds(72) && T->v[k].pitch_cur != T->v[k].pitch16);
    blocks(ms(3000));
    ck(".. there in the end", holds(72) && T->v[k].pitch_cur == T->v[k].pitch16);
    T->p[P_GLIDE] = 0;
    on(72); blocks(2);
    ck("the droning note's key struck alone again lets it go (RELEASE)", !held() && T->v[k].stage == 3u);
    off(72); blocks(ms(3000));
    ck(".. and it fades (RELEASE)", !T->v[k].active || T->v[k].stage != 3u || T->v[k].env < (1 << 17));

    on(60); on(64); on(67); blocks(ms(100)); off(60); off(64); off(67); blocks(ms(400));
    ck("a chord played together holds as a chord", held() == 3u && holds(60) && holds(64) && holds(67));
    {
        int v60 = holds(60), v64 = holds(64), v67 = holds(67);
        on(62); on(65); on(69); blocks(1);
        ck("the next chord glides voice by voice (each old voice to the nearest new note, none restarted)",
           held() == 3u && holds(62) == v60 && holds(65) == v64 && holds(69) == v67);
        off(62); off(65); off(69); blocks(ms(400));
        ck(".. held when let go", held() == 3u && holds(62) && holds(65) && holds(69));
        on(64); blocks(1);
        ck("a chord into one note: the nearest voice glides ..", holds(64) == v64 && held() == 3u);
        off(64); blocks(2);
        ck(".. the others let go when the key is up", held() == 1u && holds(64) && T->v[v60 - 1].stage == 3u && T->v[v67 - 1].stage == 3u);
    }
    on(60); on(64); blocks(ms(50)); off(60); off(64); blocks(2);
    ck("a chord after one droning note: both held (the drone glides to one, the other a voice of its own)", held() == 2u && holds(60) && holds(64));

    /* OCT- + OCT+ on the track's pages: let go, the octave as it was */
    song.octave = 1;
    btn_down(B_OCTDN); frame(); btn_down(B_OCTUP); frame(); btn_up(B_OCTDN); btn_up(B_OCTUP); frame();
    blocks(2);
    ck("OCT- + OCT+ lets the drone go (RELEASE), the octave as it was", !held() && song.octave == 1);
    blocks(ms(3000));
    press(B_OCTDN); blocks(1);
    ck("OCT- alone: the octave, as ever", song.octave == 0);
    song.octave = 0;

    tap(60, 50); tap(64, 50);
    ck("held before STOP / PANIC", held() == 1u);
    panic_all = 1; blocks(2);
    ck("PANIC lets it go", !held());
    blocks(ms(2000));

    tap(55, 50);
    {
        int pg = nav_put(SEC_EDIT, "PLAY") || nav_put(SEC_EDIT, "VOICES");   /* (EDIT > PLAY, VOICES after its rename) */
        uint32_t i;
        for (i = 0; i < 8u && T->p[P_VOICE] != V_UNISON; i++) turn(EN_K1, -1);
        blocks(2);
        ck("POLY turned off DRONE (KNOB 1 on EDIT > PLAY): what it held ends", pg && T->p[P_VOICE] == V_UNISON && !held());
        tap(55, 50);
        ck(".. and UNI as ever: a note ends with its key", !held());
        for (i = 0; i < 8u && T->p[P_VOICE] != V_DRONE; i++) turn(EN_K1, 1);
        ck(".. turned back to DRONE: the screen says how it ends", T->p[P_VOICE] == V_DRONE && msg_is("OCT- + OCT+: DRONE OFF"));
        go_home(); frame();
    }
    blocks(ms(2000));

    /* MIDI IN on channel 1 (track 1): as the keys */
    midi_enqueue(0x9u | 0x90u << 8 | 62u << 16 | 90u << 24, 1); blocks(ms(50));
    midi_enqueue(0x8u | 0x80u << 8 | 62u << 16, 1); blocks(ms(200));
    ck("MIDI IN: a note held after its note-off", held() == 1u && holds(62));
    midi_enqueue(0x9u | 0x90u << 8 | 69u << 16 | 90u << 24, 1); blocks(1);
    k = (uint32_t)holds(69);
    midi_enqueue(0x8u | 0x80u << 8 | 69u << 16, 1); blocks(ms(200));
    ck(".. the next one glides there", k && held() == 1u && holds(69));
    midi_enqueue(0x9u | 0x90u << 8 | 69u << 16 | 90u << 24, 1); blocks(1);
    midi_enqueue(0x8u | 0x80u << 8 | 69u << 16, 1); blocks(2);
    ck(".. the same note again lets it go", !held());
    blocks(ms(2000));
}

static void to_step(uint32_t i, uint32_t s1)              /* to the sequencer's step i, then half of it */
{
    uint32_t n = 0;
    while (T->seq_idx != i && n++ < 64u * s1) blocks(1);
    blocks(s1 / 2u);
}
static void test_seq(void)
{
    uint32_t i, k, age, s1;
    setup(ENGI_ANALOG, analog_preset("SOFT PAD"));
    T->p[P_SLEN] = 8; T->p[P_SDIV] = 2; T->p[P_SGATE] = 30;
    for (i = 0; i < NSTEP; i++) step_clear(&T->step[i]);
    T->step[0].time = ST_NOTE; T->step[0].n = 1; T->step[0].note[0] = 60; T->step[0].vel = 100;
    T->step[2].time = ST_NOTE; T->step[2].n = 1; T->step[2].note[0] = 60; T->step[2].vel = 100;   /* repeated: holds */
    T->step[4].time = ST_NOTE; T->step[4].n = 3; T->step[4].note[0] = 62; T->step[4].note[1] = 65; T->step[4].note[2] = 69;
    T->step[4].vel = 100;
    song.g[G_BPM] = 120;
    s1 = step_samples(T, div_samples((uint32_t)T->p[P_SDIV]), 0) / CTL;
    transport_req = 1; ui_input();
    to_step(0, s1);                                      /* step 1's note, past its gate (30 %) */
    k = (uint32_t)holds(60);
    ck("the sequencer's note held past its gate", k && held() == 1u);
    age = k ? T->v[k - 1].age : 0;
    to_step(1, s1);
    to_step(2, s1);                                      /* step 3: the same note again */
    ck(".. the same note again: held on, not retriggered (no tap off)", holds(60) == (int)k && T->v[k - 1].age == age);
    to_step(4, s1);                                      /* step 5: a chord */
    ck(".. a chord: the note glides into it, the others voices of their own, held", held() == 3u && holds(62) == (int)k &&
       holds(65) && holds(69) && T->v[k - 1].age == age);
    to_step(5, s1);
    to_step(0, s1);                                      /* round again to step 1 (past its gate) */
    ck(".. the next step's one note: the chord into it (one held)", held() == 1u && holds(60));
    transport_req = 2; ui_input(); blocks(2);
    ck("STOP lets it go", !held());
    blocks(ms(2000));
}

static void test_engines(void)
{
    uint32_t e, ok = 1, bads = 0;
    char what[160];
    for (e = 0; e < NENGINES; e++) {
        int a, b;
        if (eng_gone(e) || e == ENGI_DRUM || e == ENGI_INPUT || (ENGINES[e]->keys && ENGINES[e]->keys(&trk[0], 0) >= 0))
            continue;
        setup(e, 0);
        if (ENGINES[e]->oneshot) continue;
        tap(60, 60); blocks(ms(200));
        a = holds(60);
        on(64); blocks(1); off(64); blocks(ms(200));
        b = holds(64);
        if (!(a && b == a && held() == 1u)) { ok = 0; bads++; printf("drone:   %s: held %d, glided %d\n", ENGINES[e]->name, a, b); }
        T->p[P_VOICE] = V_POLY; blocks(ms(1000));
    }
    snprintf(what, sizeof what, "every synth engine holds and glides in DRONE (%u failing)", bads);
    ck(what, ok);
    setup(ENGI_DRUM, 0);
    T->p[P_VOICE] = V_DRONE;
    ck("a kit: DRONE ignored (voice mode POLY, no drone)", trk_vmode(T) == V_POLY && !trk_drone(T));
    on(36); blocks(ms(50)); off(36); blocks(ms(1500));
    ck(".. its hits end by themselves", !held());
}

static void demo(const char *path)
{
    static const uint8_t N[3] = {50, 57, 53};
    uint32_t i;
    setup(ENGI_ANALOG, analog_preset("SOFT PAD"));
    T->p[P_GLIDE] = 70; T->p[P_GLMODE] = 1;             /* (GL MODE TIME: ~0.45 s whatever the interval) */
    wf = fopen(path, "wb");
    if (!wf) return;
    wav_hdr(wf, 0);
    wn = 0;
    blocks(ms(300));
    for (i = 0; i < 3u; i++) { on(N[i]); blocks(ms(150)); off(N[i]); blocks(ms(2600)); }
    song.octave = 0;
    btn_down(B_OCTDN); frame(); btn_down(B_OCTUP); frame(); btn_up(B_OCTDN); btn_up(B_OCTUP); frame();
    blocks(ms(3500));
    fseek(wf, 0, SEEK_SET);
    wav_hdr(wf, wn);
    fclose(wf);
    wf = 0;
    printf("drone: demo %s (%u frames)\n", path, wn);
}

int main(int argc, char **argv)
{
    test_keys();
    test_seq();
    test_engines();
    if (argc > 1)
        demo(argv[1]);
    printf(nbad ? "drone: %d FAILED\n" : "drone: all passed\n", nbad);
    return nbad != 0;
}
