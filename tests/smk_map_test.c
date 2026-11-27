/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Host test of the SMK-37's control mapping, the V10.8 layout (docs/SMK37.md), built with -DBOARD_SMK37=1:
 * firmware/src/panel.c's SMK table, hal/smk_input.h's keybed / pad / knob maps, firmware/src/smk_map.h's rules and
 * firmware/src/smk_kb.c (the keybed, wheels and pedal as MIDI in) on stand-ins:
 *   - every FM-1 function's default control is the printed SMK control (FX->FX, SCALE->SCL, CHORD->ENV,
 *     NOTE REPEAT->LFO, PARA->EDIT, GLOBE->GLO, PATCH->HOME, BT->SAVE, ARP, SEQ, SEQ PLAY->PLAY, SEQ REC->REC,
 *     OCTAVE-/+ -> OCT-/+); K1..K4 = KNOB 1..4, K5 SELECT, K6 PRESETS, K7 ALGORITHM, K8 MASTER ("PNS6");
 *   - EVERY one of the 37 keys (ids 0..36, both ends, black and white) goes through the queue and plays a note of its
 *     own on the selected track, at its velocity, at every octave -8..+8 (always 0..127, never two keys the same);
 *     its release ends that note on the channel it started on, whatever the track and the octave are by then;
 *   - the pads: OFF / WHITE / STEPS / TRACKS by the page, as white keys only in WHITE, numbered 1..16;
 *   - the knob and fader banks' targets; the faders' pick-up;
 *   - the wheels: bend centred with a dead zone, both ends reached; MOD from its rest; the pedal either polarity. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#define __attribute__(x)
#include "../firmware/hal/smk_input.h"
#undef __attribute__
#define __attribute__(x)
/* what panel.c needs from the rest of the firmware */
#define NPALETTES 8u
#define UI_JIANT_INDEX 0u
static uint8_t fx_lowcut;
static uint32_t palette_from_stored(uint32_t v) { return v; }
static void palette_set(uint32_t p) { (void)p; }
#include "../firmware/src/panel.c"
/* what smk_kb.c needs */
#define NPART 6
typedef struct { int16_t p[4]; } track_t;
static track_t trk[NPART];
static struct { uint8_t sel; int8_t octave; } song;
static uint8_t kb_velx[27], kb_boot_hold, mo_on = 1;
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
typedef struct { uint8_t st, ch, d1, d2; } ev_t;
static ev_t ev[512];
static uint32_t nev, nout;
static void midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2)
{
    if (nev < 512u) { ev[nev].st = (uint8_t)st; ev[nev].ch = (uint8_t)ch; ev[nev].d1 = (uint8_t)d1; ev[nev].d2 = (uint8_t)d2; }
    nev++;
}
static uint32_t mo_key(const track_t *t) { (void)t; return mo_on; }
static void mo_kon(uint32_t i, uint32_t x, uint32_t v) { (void)i; (void)x; (void)v; nout++; }
static void mo_koff(uint32_t i, uint32_t x) { (void)i; (void)x; nout--; }
#include "../firmware/src/smk_kb.c"
#undef __attribute__

static int bad;
static void check(const char *what, int ok) { printf("smk map: %-100s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

/* smk_input.h: button id -> printed name (confirmed on a unit by Melodee) */
static const char *const PRINTED[FM1_NBTN] = {"KNOB BANK", "FADER BANK", "<", ">", "OCTAVE-", "OCTAVE+", "ARP",
                                              "NOTE REPEAT", "SCALE", "CHORD", "GLOBE", "BT", "PATCH", "PARA", "FX",
                                              "SEQ", "SEQ PLAY", "SEQ REC"};
static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

static void block(void) { fm1_in.frames = 100; smk_kb_block(); }

int main(void)
{
    static const struct { uint32_t label; const char *smk; } WANT[] = {
        {B_FX, "FX"}, {B_SCL, "SCALE"}, {B_ENV, "CHORD"}, {B_LFO, "NOTE REPEAT"}, {B_EDIT, "PARA"}, {B_GLO, "GLOBE"},
        {B_HOME, "PATCH"}, {B_SAVE, "BT"}, {B_ARP, "ARP"}, {B_SEQ, "SEQ"}, {B_PLAY, "SEQ PLAY"}, {B_REC, "SEQ REC"},
        {B_OCTDN, "OCTAVE-"}, {B_OCTUP, "OCTAVE+"}};
    uint32_t i, ok = 1, used = 0, k;
    int32_t o;
    memset(&panel, 0, sizeof panel);
    panel_init();
    check("panel_init: a blank table becomes the SMK default (\"PNS6\", the V10.8 layout)",
          panel.magic == PANEL_MAGIC && PANEL_MAGIC == 0x504E5336u);
    for (i = 0; i < sizeof WANT / sizeof WANT[0]; i++) {
        uint32_t id = panel.btn[WANT[i].label];
        ok &= id < FM1_NBTN && !strcmp(PRINTED[id], WANT[i].smk) && !strcmp(B_NAME[WANT[i].label], WANT[i].smk);
        used |= 1u << id;
    }
    check("the 14 FM-1 functions on their printed SMK buttons; calibration asks by that name", ok && sizeof WANT / sizeof WANT[0] == NB);
    check("KNOB BANK, FADER BANK, < and > carry no FM-1 function (ids 0..3)", (used & 0xFu) == 0 &&
          panel_btn_of(SMK_BTN_KNOB_BANK) == NB && panel_btn_of(SMK_BTN_LEFT) == NB && panel_btn_of(SMK_BTN_RIGHT) == NB &&
          panel_btn_of(SMK_BTN_FADER_BANK) == NB && panel.btn[B_OCTDN] == FM1_BTN_OCT_DOWN && panel.btn[B_OCTUP] == FM1_BTN_OCT_UP);
    check("K1..K4 = KNOB 1..4 (encoders 0..3), K5 SELECT (4), K6 PRESETS (5), K7 ALGORITHM (6), clockwise +",
          panel.enc[EN_K1] == 0 && panel.enc[EN_K2] == 1 && panel.enc[EN_K3] == 2 && panel.enc[EN_K4] == 3 &&
          panel.enc[EN_SELECT] == 4 && panel.enc[EN_PRESET] == 5 && panel.enc[EN_ALGO] == 6 &&
          !strcmp(E_NAME[EN_SELECT], "K5") && !strcmp(E_NAME[EN_PRESET], "K6") && !strcmp(E_NAME[EN_ALGO], "K7") &&
          !strcmp(E_NAME[EN_K1], "K1") && panel.dir[0] == 1 && panel.dir[6] == 1);
    for (i = 0, k = 0; i < NE; i++)
        k |= panel.enc[i] == SMK_ENC_MASTER;
    check("K8 (encoder 7) is no panel role: the MASTER, a virtual pot from 724, 24 a detent",
          !k && SMK_ENC_MASTER == 7u && smk_master == 724 && FM1_ADC_MASTER == 4);
    panel.btn[B_FX] = 17u; panel.btn[B_REC] = 14u;     /* a calibrated table with the high ids */
    panel_init();
    check("panel_init keeps a calibrated table with button ids up to 17 and knob 7", panel.btn[B_FX] == 17u && panel.enc[EN_ALGO] == 6u);
    panel.btn[B_FX] = 18u;
    panel_init();
    check("... and refuses id 18 (back to the default)", panel.btn[B_FX] == 14u);
    panel.enc[EN_K1] = 8u;
    panel_init();
    check("... and knob 8", panel.enc[EN_K1] == 0u);

    /* ---- the keybed: all 37 keys, every octave ---- */
    for (k = 0, ok = 1; k < FM1_NKEY; k++) {
        uint32_t q0 = smk_extq_w;
        int32_t e;
        smk__key_event(k, 90);
        ok &= fm1_in.notes == 0u && smk_extq_w != q0;
        e = smk_extkey_take();
        ok &= e == (int32_t)(k << 8 | 90u);
        smk__key_event(k, 0);
        e = smk_extkey_take();
        ok &= e == (int32_t)(k << 8) && smk_extkey_take() < 0 && fm1_in.notes == 0u;
    }
    check("hal: every key 0..36 (C3..C6, the outer 10 too) through the queue with its velocity, none an FM-1 key", (int)ok);
    {
        uint32_t range = 1, distinct = 1, step = 1, c3 = 1;
        for (o = -8; o <= 8; o++) {
            uint8_t seen[128] = {0};
            for (k = 0; k < SMK_KEYS; k++) {
                uint32_t n = smk_key_note(k, o);
                range &= n <= 127u;
                distinct &= !seen[n & 127u];
                seen[n & 127u] = 1;
                step &= !k || n == smk_key_note(k - 1u, o) + 1u;
            }
            if (o >= -3 && o <= 3)
                c3 &= smk_key_note(0, o) == (uint32_t)(48 + 12 * o) && smk_key_note(36, o) == (uint32_t)(84 + 12 * o);
        }
        check("smk_key_note: at OCT -8..+8 all 37 notes in 0..127, one semitone apart, never two the same", range && distinct && step);
        check("... OCT -3..+3 (the FM-1's range): key 0 = C3 + 12 OCT .. key 36 = C6 + 12 OCT, no window moved",
              (int)c3 && smk_key_note(0, -4) == 0u && smk_key_note(36, 4) == 127u - 36u + 36u);
    }
    {   /* smk_kb_block: each key down plays its note on the selected track, each release ends it */
        uint32_t good = 1, outs = 1, vels = 1;
        for (o = -3; o <= 3; o++)
            for (song.sel = 0; song.sel < NPART; song.sel++) {
                uint8_t seen[128] = {0};
                song.octave = (int8_t)o;
                for (k = 0; k < SMK_KEYS; k++) {
                    uint32_t v = 1u + (k * 7u + (uint32_t)(o + 3)) % 127u;
                    nev = 0;
                    smk__key_event(k, v);
                    block();
                    good &= nev >= 1u && ev[0].st == 0x90u && ev[0].ch == song.sel && !seen[ev[0].d1] &&
                            ev[0].d1 == smk_key_note(k, o);
                    vels &= ev[0].d2 == v;
                    seen[ev[0].d1] = 1;
                }
                outs &= nout == SMK_KEYS;
                for (k = 0; k < SMK_KEYS; k++) {
                    nev = 0;
                    smk__key_event(k, 0);
                    block();
                    good &= nev >= 1u && ev[0].st == 0x80u && ev[0].ch == song.sel && ev[0].d1 == smk_key_note(k, o);
                }
                outs &= nout == 0u;
            }
        check("smk_kb_block: all 37 keys x OCT -3..+3 x T1..T6: a note-on each, on the selected track, 37 different notes",
              (int)good);
        check("... at the key's own velocity; each release a note-off of that note; MIDI OUT: as many offs as ons",
              vels && outs);
        printf("smk map: keys 0 / 36 at OCT 0: %s%d / %s%d\n", NOTE[smk_key_note(0, 0) % 12u], (int)smk_key_note(0, 0) / 12 - 1,
               NOTE[smk_key_note(36, 0) % 12u], (int)smk_key_note(36, 0) / 12 - 1);
    }
    {   /* held across a track and an octave change, struck again, under the splash */
        uint32_t ok2;
        song.sel = 2; song.octave = 1;
        nev = 0;
        smk__key_event(0, 80);
        smk__key_event(36, 80);
        block();
        song.sel = 4; song.octave = -2;
        nev = 0;
        smk__key_event(0, 0);
        smk__key_event(36, 0);
        block();
        ok2 = nev >= 2u && ev[0].st == 0x80u && ev[0].ch == 2u && ev[0].d1 == 60u && ev[1].st == 0x80u && ev[1].ch == 2u &&
              ev[1].d1 == 96u;
        check("a key held while the track and OCT change: its release ends the note it started (T3, C4 / C7)", (int)ok2);
        nev = 0;
        smk__key_event(10, 70);
        smk__key_event(10, 0);
        smk__key_event(10, 110);
        block();
        check("struck again before a full lift (release + press in the queue): off, then on at the new velocity",
              nev >= 3u && ev[0].st == 0x90u && ev[1].st == 0x80u && ev[2].st == 0x90u && ev[2].d2 == 110u);
        smk__key_event(10, 0);
        block();
        kb_boot_hold = 1;
        nev = 0;
        smk__key_event(5, 90);
        block();
        smk__key_event(5, 0);
        block();
        check("under the splash (kb_boot_hold): silent, its release too", nev == 0u);
        kb_boot_hold = 0;
    }

    /* ---- the wheels and the pedal ---- */
    {
        uint32_t ok3;
        int32_t lo, hi;
        memset(&smk_kb, 0, sizeof smk_kb);
        song.sel = 1;
        fm1_in.wheel[0] = 512; fm1_in.wheel[1] = 3; fm1_in.pedal = 1000;
        nev = 0;
        block();
        ok3 = nev == 0u && smk_kb.cal;
        fm1_in.wheel[0] = 530;
        block();
        ok3 &= nev == 0u;                               /* (inside the dead zone) */
        fm1_in.wheel[0] = 1023;
        block();
        hi = nev ? (int32_t)(ev[nev - 1u].d1 | ev[nev - 1u].d2 << 7) - 8192 : 0;
        fm1_in.wheel[0] = 0;
        block();
        lo = nev ? (int32_t)(ev[nev - 1u].d1 | ev[nev - 1u].d2 << 7) - 8192 : 0;
        fm1_in.wheel[0] = 512;
        block();
        check("PITCH: centred at its rest (a dead zone), the top end +8191, the bottom -8192, back to 0, on T2's channel",
              ok3 && hi == 8191 && lo == -8192 && ev[nev - 1u].st == 0xE0u && ev[nev - 1u].ch == 1u &&
              ev[nev - 1u].d1 == 0u && ev[nev - 1u].d2 == 64u);
        nev = 0;
        fm1_in.wheel[1] = 1023;
        block();
        check("MOD: CC1 from 0 at its rest to 127 at the other end", nev == 1u && ev[0].st == 0xB0u && ev[0].d1 == 1u && ev[0].d2 == 127u);
        nev = 0;
        fm1_in.pedal = 20;                              /* (a pedal that reads low when pressed) */
        block();
        ok3 = nev == 1u && ev[0].d1 == 64u && ev[0].d2 == 127u;
        nev = 0;
        song.sel = 3;                                   /* another track: the old one let go */
        block();
        check("SUSTAIN: CC64 127 pressed (either polarity, from its rest); another track: T2 let go, T4 gets them",
              ok3 && nev >= 4u && ev[0].ch == 1u && ev[0].st == 0xB0u && ev[1].ch == 1u && ev[1].d1 == 64u && ev[1].d2 == 0u &&
              ev[nev - 1u].ch == 3u && ev[nev - 1u].d1 == 64u && ev[nev - 1u].d2 == 127u);
    }

    /* ---- the pads ---- */
    check("the pads' rule: the MENU / a dialog OFF; the FM-1's keys the UI's (layer, NAME, DRUM grid, STEP FX) WHITE; "
          "SEQ STEPS; else TRACKS",
          smk_pads_rule(1, 1, 1) == SMK_PM_OFF && smk_pads_rule(0, 1, 1) == SMK_PM_WHITE &&
          smk_pads_rule(0, 1, 0) == SMK_PM_WHITE && smk_pads_rule(0, 0, 1) == SMK_PM_STEPS &&
          smk_pads_rule(0, 0, 0) == SMK_PM_TRACKS);
    check("pad n = hardware pad n - 1 (1..8 the top row, 9..16 the bottom); 7 UNDO, 8 TAP, 9..16 pages",
          smk_pad_num(0) == 1u && smk_pad_num(7) == 8u && smk_pad_num(8) == 9u && smk_pad_num(15) == 16u &&
          SMK_PAD_UNDO == 7u && SMK_PAD_TAP == 8u && SMK_PAD_PAGE0 == 9u && !strcmp(SMK_PAGES[0].nm, "MIXER") &&
          !strcmp(SMK_PAGES[7].nm, "GENERATOR 1"));
    {
        static const uint8_t WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
        uint32_t seen = 0;
        int32_t e;
        for (i = 0; i < SMK_NPAD; i++) {
            uint32_t w;
            for (w = 0; w < 16u && WHITE[w] != SMK_PK_GRID[i]; w++) {}
            if (w < 16u && w == i) seen |= 1u << w;
        }
        check("WHITE: pad n is the FM-1's n-th white key (F3 G3 .. G5: steps 1..16, the layers' keys)",
              seen == 0xFFFFu && smk_pad_key == SMK_PK_NONE);
        fm1_in.notes = fm1_in.pnotes = 0;
        smk_pad_key = SMK_PK_GRID;
        smk__padq(0, 100);
        smk__padq(15, 77);
        ok = fm1_in.notes == (1u << 0 | 1u << 26) && fm1_in.pad_vel[0] == 100u && fm1_in.pad_vel[26] == 77u;
        smk__padq(0, 0);
        smk__padq(15, 0);
        ok &= fm1_in.notes == 0u && smk_pad_take() < 0;
        smk_pad_key = SMK_PK_NONE;
        smk__padq(3, 64);
        e = smk_pad_take();
        smk__padq(3, 0);
        check("... a hit presses its white key at its velocity (pad 1 F3, pad 16 G5); STEPS / TRACKS: the UI's queue",
              ok && e == (3 << 8 | 64) && smk_pad_take() == (3 << 8) && fm1_in.notes == 0u);
    }

    /* ---- the banks ---- */
    ok = 1;
    for (i = 0; i < 6u; i++)
        ok &= SMK_MACRO[i].trk == i && SMK_MACRO[i].what == SMK_TG_CUT;
    check("KNOB BANK: K1..K6 the cutoff of T1..T6, K7 the delay's MIX, K8 the reverb's SIZE",
          ok && SMK_MACRO[6].trk == SMK_T_G && SMK_MACRO[6].what == SMK_TG_DMIX && SMK_MACRO[7].what == SMK_TG_RSIZE);
    ok = 1;
    for (i = 0; i < 4u; i++)
        ok &= SMK_FADER[0][i].trk == i && SMK_FADER[0][i].what == SMK_TG_LEVEL;
    check("FADER BANK A: the levels of T1..T4; B: T5, T6, the selected track's delay and reverb sends",
          ok && SMK_FADER[1][0].trk == 4u && SMK_FADER[1][1].trk == 5u && SMK_FADER[1][1].what == SMK_TG_LEVEL &&
          SMK_FADER[1][2].trk == SMK_T_SEL && SMK_FADER[1][2].what == SMK_TG_DSEND && SMK_FADER[1][3].what == SMK_TG_RSEND);
    {
        smk_pick_t p;
        uint32_t a, b, c, d, e2, f;
        smk_pick_reset(&p);
        a = !smk_pickup(&p, 10, 100);                   /* far below: not yet */
        b = !smk_pickup(&p, 60, 100);                   /* still below */
        c = smk_pickup(&p, 101, 100);                   /* crossed it */
        d = smk_pickup(&p, 90, 101);                    /* picked: follows */
        e2 = !smk_pickup(&p, 92, 40);                   /* the value moved elsewhere (a knob): lost */
        f = smk_pickup(&p, 41, 40);                     /* met again */
        check("pick-up: a fader moves its value only once it has met it (or crossed it); lost when the value moves elsewhere",
              a && b && c && d && e2 && f);
        smk_pick_reset(&p);
        check("... a first reading right at the value takes it, one far from it does not",
              smk_pickup(&p, 64, 65) && (smk_pick_reset(&p), !smk_pickup(&p, 0, 127)));
    }
    printf("%s\n", bad ? "SMK MAP TEST FAILED" : "smk map test passed");
    return bad != 0;
}
