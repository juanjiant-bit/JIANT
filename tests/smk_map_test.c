/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Host test of the SMK-37's control mapping, Rhythmer's layout on V10.8.48 (docs/SMK37.md), built with -DBOARD_SMK37=1:
 * firmware/src/panel.c's SMK table, hal/smk_input.h's keybed and pad routing, firmware/src/smk_map.h's rules and
 * firmware/src/smk_kb.c (the keys' block's SMK step, the outer keys, the pedal's sustain, the wheels) on stand-ins of
 * seq.c's key path (kb_map / key_on / key_off, keyboard_block's loop over the 27 keys as seq.c has it):
 *   - every FM-1 function's default control is the printed SMK control (FX->FX, SCALE->SCL, CHORD->ENV,
 *     NOTE REPEAT->LFO, PARA->EDIT, GLOBE->GLO, PATCH->HOME, BT->SAVE, ARP, SEQ, SEQ PLAY->PLAY, SEQ REC->REC,
 *     OCTAVE-/+ -> OCT-/+); K1 MASTER, K2 SELECT, K3 ALGORITHM, K4 PRESETS, K5..K8 KNOB 1..4 ("PNS7");
 *   - EVERY one of the 37 keys (ids 0..36, both ends) through the HAL and the key path: ids 5..31 as the FM-1's keys
 *     0..26 (fm1_in.notes), the outer ten through the queue as keys 27..36; each a key_on of a note of its own on the
 *     selected track at its velocity (MIDI 48..84 at OCT 0, moved by OCT), at OCT -3..+3 on T1..T6; each release a
 *     key_off of that key on the track it started on; struck again: off then on; a layer / NAME / the splash: the
 *     outer keys silent; the pedal holds a let-go key until it is up;
 *   - the wheels on genkigenki's measured ranges; the pads PLAY (white keys) / STEPS (the UI's), the change waiting
 *     for the pads' release; the fader banks, the track window, the pick-up. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#define __attribute__(x)
#include "../firmware/hal/smk_input.h"
#undef __attribute__
#define __attribute__(x)
#define fm1__lock() 0u                    /* (the HAL's cli / sti: not on the host) */
#define fm1__unlock(l) ((void)(l))
/* what panel.c needs from the rest of the firmware */
#define NPALETTES 8u
#define UI_JIANT_INDEX 0u
static uint8_t fx_lowcut;
static uint32_t palette_from_stored(uint32_t v) { return v; }
static void palette_set(uint32_t p) { (void)p; }
#include "../firmware/src/panel.c"
#include "../firmware/src/smk_map.h"
/* what smk_kb.c needs: seq.c's key path, as stand-ins that record */
#define NTRK 6
#define KB_NK 37u
#define KB_SILENT 255u
typedef struct { int16_t p[4]; int drum; } track_t;
static track_t trk[NTRK];
static struct { uint8_t sel, grid; int8_t octave; } song;
static uint8_t kb_velx[KB_NK], kb_note[KB_NK], kb_trk[KB_NK], kb_boot_hold, kb_asleep, fx_lock_on, mo_on = 1;
static int8_t kb_oct[KB_NK];
static uint8_t split_cfg[1];
static uint32_t kb_prev, kb_mask = 1u << 10;   /* (a layer's button: GLOBE) */
static uint32_t split_track(uint32_t high) { return high; }
static int32_t split_oct(uint32_t high) { (void)high; return 0; }
static int32_t clamp(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : v > hi ? hi : v; }
static int drum_track(const track_t *t) { return t->drum; }
static uint32_t kb_map(const track_t *t, uint32_t k)   /* seq.c kb_map with QUANT OFF and no engine key map */
{
    (void)t;
    return (uint32_t)clamp((k < 27u ? 53 + (int32_t)k : 53 + smk_kb_off(k)) + 12 * song.octave, 0, 127);
}
typedef struct { uint8_t on, key, trk, note, vel; } kev_t;
static kev_t kev[256];
static uint32_t nkev;
static void key_on(uint32_t k, track_t *t)
{
    if (nkev < 256u) kev[nkev] = (kev_t){1, (uint8_t)k, (uint8_t)(t - trk), kb_note[k], (uint8_t)(kb_velx[k] ^ 100u)};
    nkev++;
}
static void key_off(uint32_t k, track_t *t)
{
    if (nkev < 256u) kev[nkev] = (kev_t){0, (uint8_t)k, (uint8_t)(t - trk), kb_note[k], 0};
    nkev++;
}
static void smk_key_up(uint32_t k, track_t *t);
static void (*key_off_fp)(uint32_t, track_t *) = smk_key_up;
typedef struct { uint8_t st, ch, d1, d2; } ev_t;
static ev_t ev[512];
static uint32_t nev, nout;
static void midi_event(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2)
{
    if (nev < 512u) { ev[nev].st = (uint8_t)st; ev[nev].ch = (uint8_t)ch; ev[nev].d1 = (uint8_t)d1; ev[nev].d2 = (uint8_t)d2; }
    nev++;
}
static uint32_t mo_key(const track_t *t) { (void)t; return mo_on; }
static void mo_send(uint32_t st, uint32_t d1, uint32_t d2) { (void)st; (void)d1; (void)d2; nout++; }
#include "../firmware/src/smk_kb.c"
#undef __attribute__

static int bad;
static void check(const char *what, int ok) { printf("smk map: %-100s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

/* smk_input.h: button id -> printed name (confirmed on a unit, genkigenki's 01-hardware.md 5.3) */
static const char *const PRINTED[FM1_NBTN] = {"KNOB BANK", "FADER BANK", "<", ">", "OCTAVE-", "OCTAVE+", "ARP",
                                              "NOTE REPEAT", "SCALE", "CHORD", "GLOBE", "BT", "PATCH", "PARA", "FX",
                                              "SEQ", "SEQ PLAY", "SEQ REC"};
static const char *const NOTE[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};

/* seq.c keyboard_block on the stand-ins: smk_kb_block, then the 27 keys' edges (press: kb_note = kb_map, key_on on
 * the selected track; release: key_off_fp on the track it started on), as seq.c does with no layer, grid or SCENE */
static void block(void)
{
    uint32_t cur, ch, k;
    fm1_in.frames = 100;
    cur = smk_kb_block();
    ch = cur ^ kb_prev;
    for (k = 0; k < 27u; k++) {
        if (!((ch >> k) & 1u))
            continue;
        if ((cur >> k) & 1u) {
            kb_trk[k] = song.sel;
            if (kb_boot_hold) { kb_note[k] = KB_SILENT; continue; }
            kb_note[k] = (uint8_t)kb_map(&trk[kb_trk[k]], k);
            key_on(k, &trk[kb_trk[k]]);
        } else if (kb_note[k] != KB_SILENT) {
            key_off_fp(k, &trk[kb_trk[k] % NTRK]);
        }
    }
    kb_prev = cur;
}
static void hal_reset(void)
{
    fm1_in.notes = fm1_in.knotes = fm1_in.pnotes = fm1_in.notes_pressed = fm1_in.notes_retrig = 0;
    fm1_in.keys = 0;
    smk_extq_r = smk_extq_w = 0;
}

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
    check("panel_init: a blank table becomes the SMK default (\"PNS7\", Rhythmer's layout)",
          panel.magic == PANEL_MAGIC && PANEL_MAGIC == 0x504E5337u);
    for (i = 0; i < sizeof WANT / sizeof WANT[0]; i++) {
        uint32_t id = panel.btn[WANT[i].label];
        ok &= id < FM1_NBTN && !strcmp(PRINTED[id], WANT[i].smk) && !strcmp(B_NAME[WANT[i].label], WANT[i].smk);
        used |= 1u << id;
    }
    check("the 14 FM-1 functions on their printed SMK buttons (PATCH HOME, PARA EDIT, CHORD ENV, NOTE REPEAT LFO, BT SAVE)",
          ok && sizeof WANT / sizeof WANT[0] == NB);
    check("KNOB BANK, FADER BANK, < and > carry no FM-1 function (ids 0..3)", (used & 0xFu) == 0 &&
          panel_btn_of(SMK_BTN_KNOB_BANK) == NB && panel_btn_of(SMK_BTN_LEFT) == NB && panel_btn_of(SMK_BTN_RIGHT) == NB &&
          panel_btn_of(SMK_BTN_FADER_BANK) == NB && panel.btn[B_OCTDN] == FM1_BTN_OCT_DOWN && panel.btn[B_OCTUP] == FM1_BTN_OCT_UP);
    check("K2 SELECT (encoder 1), K3 ALGORITHM (2), K4 PRESETS (3), K5..K8 KNOB 1..4 (4..7), clockwise +",
          panel.enc[EN_SELECT] == 1 && panel.enc[EN_ALGO] == 2 && panel.enc[EN_PRESET] == 3 && panel.enc[EN_K1] == 4 &&
          panel.enc[EN_K2] == 5 && panel.enc[EN_K3] == 6 && panel.enc[EN_K4] == 7 && !strcmp(E_NAME[EN_SELECT], "K2") &&
          !strcmp(E_NAME[EN_ALGO], "K3") && !strcmp(E_NAME[EN_PRESET], "K4") && !strcmp(E_NAME[EN_K1], "K5") &&
          !strcmp(E_NAME[EN_K4], "K8") && panel.dir[0] == 1 && panel.dir[6] == 1);
    for (i = 0, k = 0; i < NE; i++)
        k |= panel.enc[i] == SMK_ENC_MASTER;
    check("K1 (encoder 0) is no panel role: the MASTER, a virtual pot from 724, 24 a detent",
          !k && SMK_ENC_MASTER == 0u && smk_master == 724 && FM1_ADC_MASTER == 4);
    panel.btn[B_FX] = 17u; panel.btn[B_REC] = 14u;     /* a calibrated table with the high ids */
    panel_init();
    check("panel_init keeps a calibrated table with button ids up to 17 and knob 7", panel.btn[B_FX] == 17u && panel.enc[EN_K4] == 7u);
    panel.btn[B_FX] = 18u;
    panel_init();
    check("... and refuses id 18 (back to the default)", panel.btn[B_FX] == 14u);
    panel.enc[EN_K1] = 8u;
    panel_init();
    check("... and knob 8", panel.enc[EN_K1] == 4u);

    /* ---- the keybed through the HAL: ids 5..31 the FM-1's keys, the outer ten the queue ---- */
    for (k = 0, ok = 1; k < FM1_NKEY; k++) {
        uint32_t c = k >= 5u && k <= 31u, bit = c ? 1u << (k - 5u) : 0u;
        int32_t e;
        hal_reset();
        smk__key_event(k, 90);
        if (c) {
            ok &= fm1_in.notes == bit && fm1_in.notes_pressed == bit && fm1_in.note_vel[k - 5u] == 90u && smk_extkey_take() < 0;
        } else {
            e = smk_extkey_take();
            ok &= fm1_in.notes == 0u && e == (int32_t)(k << 8 | 90u);
        }
        ok &= (uint32_t)(fm1_in.keys >> k) == 1u && fm1_in.key_vel[k] == 90u;
        smk__key_event(k, 0);
        ok &= fm1_in.notes == 0u && fm1_in.keys == 0u;
        if (!c)
            ok &= smk_extkey_take() == (int32_t)(k << 8) && smk_extkey_take() < 0;
    }
    check("hal: ids 5..31 (F3..G5) are the FM-1's keys 0..26 (fm1_in.notes, the UI's edges, their velocity); "
          "ids 0..4, 32..36 the queue", (int)ok);
    {
        uint32_t seen = 0, map = 1;
        uint64_t all = 0;
        for (k = 0; k < SMK_KEYS; k++) {
            uint32_t L = smk_kb_key(k);
            map &= L < SMK_KEYS && !((all >> L) & 1u) && smk_key_note(k, 0) == 48u + k;
            all |= (uint64_t)1u << L;
            seen += smk_kb_off(L) == (int32_t)k - 5;
        }
        check("smk_kb_key: the 37 ids onto keys 0..36 one to one (5..31 -> 0..26, 0..4 -> 27..31, 32..36 -> 32..36); "
              "at OCT 0 id k plays MIDI 48 + k", map && seen == SMK_KEYS && all == ((uint64_t)1u << 37) - 1u &&
              smk_kb_key(0) == 27u && smk_kb_key(4) == 31u && smk_kb_key(5) == 0u && smk_kb_key(31) == 26u &&
              smk_kb_key(32) == 32u && smk_kb_key(36) == 36u && smk_kb_key(37) == 0xFFu);
    }
    /* ---- all 37 key ids through the key path, every octave and track ---- */
    {
        uint32_t good = 1, vels = 1, offs = 1, ends = 1;
        memset(&smk_kb, 0, sizeof smk_kb);
        fm1_in.wheel[0] = 535; fm1_in.wheel[1] = 629; fm1_in.pedal = 985;
        for (o = -3; o <= 3; o++)
            for (song.sel = 0; song.sel < NTRK; song.sel++) {
                uint8_t seen[128] = {0};
                song.octave = (int8_t)o;
                for (k = 0; k < SMK_KEYS; k++) {
                    uint32_t v = 1u + (k * 7u + (uint32_t)(o + 3)) % 127u, n = smk_key_note(k, o);
                    nkev = 0;
                    smk__key_event(k, v);
                    block();
                    good &= nkev == 1u && kev[0].on && kev[0].key == smk_kb_key(k) && kev[0].trk == song.sel &&
                            kev[0].note == n && !seen[n] && n == (uint32_t)(48 + 12 * o + (int32_t)k);
                    vels &= kev[0].vel == v;
                    seen[n] = 1;
                }
                for (k = 0; k < SMK_KEYS; k++) {
                    nkev = 0;
                    smk__key_event(k, 0);
                    block();
                    offs &= nkev == 1u && !kev[0].on && kev[0].key == smk_kb_key(k) && kev[0].trk == song.sel &&
                            kev[0].note == smk_key_note(k, o);
                }
                ends &= seen[48 + 12 * o] && seen[84 + 12 * o];
            }
        check("key path: all 37 key ids x OCT -3..+3 x T1..T6: a key_on each, on the selected track, 37 notes apart "
              "(C3 + id + 12 OCT)", (int)good);
        check("... at the key's own velocity; each release a key_off of that key on its track; both ends sound "
              "(C3 / C6 at OCT 0)", vels && offs && ends);
        printf("smk map: key ids 0 / 4 / 5 / 31 / 32 / 36 at OCT 0: %s%d %s%d %s%d %s%d %s%d %s%d\n",
               NOTE[smk_key_note(0, 0) % 12u], (int)smk_key_note(0, 0) / 12 - 1, NOTE[smk_key_note(4, 0) % 12u],
               (int)smk_key_note(4, 0) / 12 - 1, NOTE[smk_key_note(5, 0) % 12u], (int)smk_key_note(5, 0) / 12 - 1,
               NOTE[smk_key_note(31, 0) % 12u], (int)smk_key_note(31, 0) / 12 - 1, NOTE[smk_key_note(32, 0) % 12u],
               (int)smk_key_note(32, 0) / 12 - 1, NOTE[smk_key_note(36, 0) % 12u], (int)smk_key_note(36, 0) / 12 - 1);
    }
    {   /* held across a track and an octave change, struck again, layers, the splash */
        uint32_t ok2;
        song.sel = 2; song.octave = 1;
        nkev = 0;
        smk__key_event(0, 80);
        smk__key_event(20, 80);
        smk__key_event(36, 80);
        block();
        song.sel = 4; song.octave = -2;
        nkev = 0;
        smk__key_event(0, 0);
        smk__key_event(20, 0);
        smk__key_event(36, 0);
        block();
        ok2 = nkev == 3u && kev[0].trk == 2u && kev[1].trk == 2u && kev[2].trk == 2u && !kev[0].on && !kev[1].on &&
              !kev[2].on;
        check("keys held while the track and OCT change: each release ends the key on the track it started on (T3)", (int)ok2);
        nkev = 0;
        smk__key_event(10, 70);                   /* a central key struck again: the HAL's retrig bit */
        block();
        fm1_in.notes_retrig |= 1u << 5;
        smk__key_event(10, 110);
        block();
        ok2 = nkev == 2u && kev[0].on && !kev[1].on;
        block();
        check("a central key struck again before a full lift (notes_retrig): off at this block, on at the next at the new "
              "velocity", ok2 && nkev == 3u && kev[2].on && kev[2].vel == 110u && kev[2].key == 5u);
        smk__key_event(10, 0);
        block();
        nkev = 0;
        smk__key_event(33, 70);
        smk__key_event(33, 0);
        smk__key_event(33, 110);
        block();
        check("an outer key struck again (release + press in the queue): off, then on at the new velocity, in one block",
              nkev == 3u && kev[0].on && !kev[1].on && kev[2].on && kev[2].vel == 110u && kev[2].key == 33u);
        smk__key_event(33, 0);
        block();
        nkev = 0;
        fm1_in.buttons = kb_mask;                /* a layer's button held */
        smk__key_event(1, 90);
        block();
        smk__key_event(1, 0);
        block();
        fm1_in.buttons = 0;
        song.grid = 1;                           /* NAME / the DRUM grid */
        smk__key_event(35, 90);
        block();
        smk__key_event(35, 0);
        block();
        song.grid = 0;
        kb_boot_hold = 1;
        smk__key_event(5, 90);
        smk__key_event(0, 90);
        block();
        smk__key_event(5, 0);
        smk__key_event(0, 0);
        block();
        kb_boot_hold = 0;
        check("a layer, NAME / the DRUM grid, the splash: the outer keys silent (their release too); the splash the 27's",
              nkev == 0u);
        trk[3].drum = 1;
        song.sel = 3;
        song.octave = 0;
        nkev = 0;
        smk__key_event(0, 90);
        smk__key_event(36, 90);
        block();
        smk__key_event(0, 0);
        smk__key_event(36, 0);
        block();
        trk[3].drum = 0;
        check("a DRUM track: the outer keys sound too (seq.c kb_map gives each a lane)", nkev == 4u && kev[0].on && kev[1].on);
    }
    {   /* the pedal: sustain */
        uint32_t ok4;
        memset(&smk_kb, 0, sizeof smk_kb);
        song.sel = 0;
        fm1_in.pedal = 985;
        block();                                 /* (the rest taken) */
        fm1_in.pedal = 20;                       /* pressed (reads low) */
        nkev = nev = 0;
        smk__key_event(12, 90);
        smk__key_event(2, 90);
        block();
        smk__key_event(12, 0);
        smk__key_event(2, 0);
        block();
        ok4 = nkev == 2u && kev[0].on && kev[1].on && nev >= 1u && ev[0].st == 0xB0u && ev[0].d1 == 64u && ev[0].d2 == 127u;
        smk__key_event(12, 60);                  /* struck again while held by the pedal: the old note ends first */
        block();
        ok4 &= nkev == 4u && !kev[2].on && kev[2].key == 7u && kev[3].on && kev[3].key == 7u;
        smk__key_event(12, 0);
        block();
        ok4 &= nkev == 4u;
        fm1_in.pedal = 985;                      /* up: both let go */
        block();
        check("SUSTAIN: keys let go with the pedal down keep sounding; struck again: off, on; pedal up: they end; CC64 sent",
              ok4 && nkev == 6u && !kev[4].on && !kev[5].on && smk_kb.sus == 0u);
    }

    /* ---- the wheels (genkigenki's measured ranges: PITCH rest ~535, 445..640; MOD 474..629, up = lower) ---- */
    {
        uint32_t ok3;
        int32_t lo, hi;
        memset(&smk_kb, 0, sizeof smk_kb);
        song.sel = 1;
        fm1_in.wheel[0] = 535; fm1_in.wheel[1] = 629; fm1_in.pedal = 985;
        nev = 0;
        block();
        ok3 = nev == 0u && smk_kb.cal;
        fm1_in.wheel[0] = 543;
        block();
        ok3 &= nev == 0u;                        /* (inside the dead zone) */
        fm1_in.wheel[0] = 640;
        block();
        hi = nev ? (int32_t)(ev[nev - 1u].d1 | ev[nev - 1u].d2 << 7) - 8192 : 0;
        fm1_in.wheel[0] = 445;
        block();
        lo = nev ? (int32_t)(ev[nev - 1u].d1 | ev[nev - 1u].d2 << 7) - 8192 : 0;
        fm1_in.wheel[0] = 535;
        block();
        check("PITCH: centred at its rest (a dead zone), its measured travel 445..640 the full -8192..+8191, back to 0, "
              "on T2's channel", ok3 && hi == 8191 && lo == -8192 && ev[nev - 1u].st == 0xE0u && ev[nev - 1u].ch == 1u &&
              ev[nev - 1u].d1 == 0u && ev[nev - 1u].d2 == 64u);
        nev = 0;
        fm1_in.wheel[1] = 474;
        block();
        ok3 = nev == 1u && ev[0].st == 0xB0u && ev[0].d1 == 1u && ev[0].d2 == 127u;
        fm1_in.wheel[1] = 629;
        block();
        check("MOD: CC1 0 at its rest (629) to 127 at the top of its travel (474: up = the lower reading)",
              ok3 && nev == 2u && ev[1].d2 == 0u);
        nev = 0;
        song.sel = 3;                            /* another track: nothing was off centre, nothing let go */
        fm1_in.wheel[0] = 640;
        block();
        check("another track: the wheels go to the new one", nev == 1u && ev[0].ch == 3u && ev[0].st == 0xE0u);
        fm1_in.wheel[0] = 535;
        block();
    }

    /* ---- the pads ---- */
    {
        static const uint8_t WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
        smk_pads_t p = {SMK_PADS_PLAY, 0};
        uint32_t seen = 0, m;
        int32_t e;
        for (i = 0; i < SMK_NPAD; i++)
            if (SMK_PK_GRID[i] == WHITE[i]) seen |= 1u << i;
        check("pad n = hardware pad n - 1 (1..8 the top row, mux A; 9..16 the bottom); PLAY: pad n the FM-1's n-th white "
              "key (F3 .. G5)", seen == 0xFFFFu && smk_pad_num(0) == 1u && smk_pad_num(15) == 16u);
        hal_reset();
        smk_pad_key = SMK_PK_GRID;
        smk__padq(0, 100);
        smk__padq(15, 77);
        ok = fm1_in.notes == (1u << 0 | 1u << 26) && fm1_in.note_vel[0] == 100u && fm1_in.note_vel[26] == 77u;
        smk__key_event(5, 50);                   /* the keybed's F3 too: one note, two sources */
        smk__padq(0, 0);
        ok &= fm1_in.notes == (1u << 0 | 1u << 26);
        smk__key_event(5, 0);
        smk__padq(15, 0);
        ok &= fm1_in.notes == 0u && smk_pad_take() < 0;
        smk_pad_key = SMK_PK_NONE;
        smk__padq(3, 64);
        e = smk_pad_take();
        smk__padq(3, 0);
        check("... a hit presses its white key at its velocity (a key of the keybed alongside); STEPS: the UI's queue",
              ok && e == (3 << 8 | 64) && smk_pad_take() == (3 << 8) && fm1_in.notes == 0u);
        ok = smk_pads_white(&p);
        smk_pads_toggle(&p);                     /* KNOB BANK with a pad down: waits */
        ok &= !smk_pads_white(&p) && !smk_pads_settle(&p, 1) && p.mode == SMK_PADS_PLAY && p.gate;
        ok &= smk_pads_settle(&p, 0) && p.mode == SMK_PADS_STEPS && !p.gate && !smk_pads_white(&p);
        smk_pads_toggle(&p);
        ok &= smk_pads_settle(&p, 0) && p.mode == SMK_PADS_PLAY && smk_pads_white(&p);
        smk_pads_toggle(&p);
        smk_pads_toggle(&p);                     /* (pressed again while waiting: undone) */
        m = !smk_pads_settle(&p, 0) && p.mode == SMK_PADS_PLAY;
        check("KNOB BANK: PLAY <-> STEPS; asked with a pad down, the change waits for the release (pads idle meanwhile)",
              ok && m);
    }

    /* ---- the faders ---- */
    {
        uint32_t b = SMK_FB_LEVEL, cyc = 1, win = 1;
        for (i = 0; i < 4u; i++) {
            b = smk_fb_next(b);
            cyc &= b == (i + 1u) % 4u;
        }
        for (k = 0; k < 6u; k++)
            for (i = 0; i < 4u; i++)
                win &= smk_fader_track(i, k) == (k < 4u ? (int)i : i < 2u ? (int)(4u + i) : -1);
        check("FADER BANK: LEVEL -> PAN -> REVERB -> DELAY -> LEVEL; F1..F4 T1..T4, T5 / T6 selected: F1 F2 T5 T6, F3 F4 none",
              cyc && win && SMK_FB_LEVEL == 0 && SMK_FB_PAN == 1 && SMK_FB_REVERB == 2 && SMK_FB_DELAY == 3);
        check("... the pick-up's context changes with the bank and the window (re-armed), not within a window",
              smk_fader_ctx(0, 0) == smk_fader_ctx(0, 3) && smk_fader_ctx(0, 3) != smk_fader_ctx(0, 4) &&
              smk_fader_ctx(0, 4) == smk_fader_ctx(0, 5) && smk_fader_ctx(1, 0) != smk_fader_ctx(0, 0) &&
              smk_fader_ctx(3, 5) == 7u);
    }
    {
        smk_pick_t p;
        uint32_t a, b, c, d, e2, f;
        smk_pick_reset(&p);
        a = !smk_pickup(&p, 10, 100);                   /* far below: not yet */
        b = !smk_pickup(&p, 60, 100);                   /* still below */
        c = smk_pickup(&p, 101, 100);                   /* crossed it */
        d = smk_pickup(&p, 90, 101);                    /* picked: follows */
        e2 = !smk_pickup(&p, 92, 40);                   /* the value moved elsewhere (a knob, a load): lost */
        f = smk_pickup(&p, 41, 40);                     /* met again */
        check("pick-up: a fader moves its value only once it has met it (or crossed it); lost when the value moves elsewhere",
              a && b && c && d && e2 && f);
        smk_pick_reset(&p);
        check("... a first reading right at the value takes it, one far from it does not (a bank / window change resets)",
              smk_pickup(&p, 64, 65) && (smk_pick_reset(&p), !smk_pickup(&p, 0, 127)));
    }
    printf("%s\n", bad ? "SMK MAP TEST FAILED" : "smk map test passed");
    return bad != 0;
}
