/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * After Rhythmer's SMK-37 Pro port by Jazzy Rhythmer (GPL-3.0: all 37 keys through the original key path, the
 * outer ten as extra notes; the wheels' self-calibration, which is genkigenki's, github.com/genkigenki/Felucca smk37). */
/* (SMK-37, BOARD_SMK37 only; included by smk.c) The audio side of the SMK-37's keybed, wheels and pedal, from the keys'
 * block (seq.c keyboard_block calls smk_kb_block first and plays what it returns: the same context as the FM-1's keys):
 *   KEYS   all 37 through seq.c's key path (kb_map, key_on / key_off: SCL's maps, CHORD, STRUM, ARP, LATCH, KEY SPLIT,
 *          recording, MIDI OUT, at the key's velocity). Ids 5..31 are the FM-1's 27 keys (fm1_in.notes: returned to
 *          keyboard_block, which does with them all the FM-1 does: the layers' shortcuts, NAME, the DRUM grid, SCENE);
 *          the outer ten (0..4, 32..36: smk_map.h smk_kb_key, keys 27..36) play extra notes from here (C3..E3 and
 *          G#5..C6 at OCT 0, moved by OCT), silent where the FM-1's keys are the UI's (a layer, NAME, the DRUM grid)
 *   PEDAL  sustain: a key let go while it is down keeps sounding until it is up (or the key is struck again); also
 *          CC64 as MIDI in on the selected track (notes from MIDI) and to MIDI OUT with the keys
 *   WHEELS pitch bend and CC1 (the matrix's MOD WHEEL) as MIDI in on the selected track, and to MIDI OUT with the keys
 * Needs from the firmware: song, NTRK, trk, kb_*, key_on, key_off, kb_map, split_*, drum_track, midi_event, mo_key,
 * mo_send, kb_mask, fx_lock_on, kb_asleep, kb_boot_hold, clamp, the HAL's fm1_in, fm1__lock and smk_extkey_take;
 * tests/smk_map_test.c gives it stand-ins and walks every key. */
#include "smk_map.h"
static struct {
    uint64_t sus;                        /* keys whose note the pedal holds (let go while it was down) */
    uint16_t out;                        /* the outer keys sounding (bit k - 27) */
    uint8_t cal, mod, ped, sel;          /* cal: rests taken; mod, ped: sent (CC1, CC64 >= 64); sel: their track + 1 */
    uint8_t mo;                          /* (TRS) MIDI OUT on + 1, as the jack last had it (0: not yet) */
    int16_t bend;                        /* sent, -8192..8191 */
    uint16_t ped0;                       /* the pedal's reading at rest */
    smk_wheels_t w;
} smk_kb __attribute__((section(".pool")));

#define SMK_PED_ON 256                   /* the pedal pressed: this far from its rest (either way: either polarity) */
static void smk_send(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2, int out)
{
    midi_event(st, ch & 15u, d1 & 127u, d2 & 127u);
    if (out && mo_key(&trk[ch % NTRK]))
        mo_send(st | (ch & 15u), d1 & 127u, d2 & 127u);
}
/* seq.c key_off_fp: a key let go; the pedal down: held until it is up */
static void smk_key_up(uint32_t k, track_t *t)
{
    if (smk_kb.ped && k < SMK_KEYS) {
        smk_kb.sus |= (uint64_t)1u << k;
        return;
    }
    key_off(k, t);
}
static void smk_sus_end(uint32_t k)       /* a held key's note ends now (pedal up, the key struck again) */
{
    if ((smk_kb.sus >> k) & 1u) {
        smk_kb.sus &= ~((uint64_t)1u << k);
        key_off(k, &trk[kb_trk[k] % NTRK]);
    }
}
/* an outer key (k 27..36) down (vel) or up: seq.c keyboard_block's press and release, for a key no layer has */
static void smk_outer(uint32_t k, uint32_t vel)
{
    uint32_t b = 1u << (k - 27u), high = k >= 32u, n;
    track_t *t;
    if (smk_kb.out & b) {                        /* (its note before: a release, or struck again) */
        smk_kb.out &= (uint16_t)~b;
        key_off_fp(k, &trk[kb_trk[k] % NTRK]);
    }
    if (!vel)
        return;
    smk_sus_end(k);
    kb_velx[k] = (uint8_t)(vel ^ 100u);
    kb_trk[k] = (uint8_t)song.sel;
    if (kb_boot_hold || kb_asleep || song.grid || (fm1_in.buttons & kb_mask) != 0u || fx_lock_on)
        return;                                  /* (the splash, SCREEN OFF, NAME / the DRUM grid, a layer: silent) */
    if (split_cfg[0])
        kb_trk[k] = (uint8_t)split_track(high);
    t = &trk[kb_trk[k] % NTRK];
    n = kb_map(t, k);
    if (n == KB_SILENT)
        return;
    if (split_cfg[0] && !drum_track(t))
        n = (uint32_t)clamp((int32_t)n + 12 * split_oct(high), 0, 127);
    kb_note[k] = (uint8_t)n;
    kb_oct[k] = 0;                               /* (a DRUM track: a lane's own note, kb_map) */
    key_on(k, t);
    smk_kb.out |= (uint16_t)b;
}
/* the keys' block's first step: returns the 27 keys' state for keyboard_block */
static __attribute__((noinline)) uint32_t smk_kb_block(void)   /* (out of the RAM mix: in flash) */
{
    uint32_t ch = song.sel % NTRK, k, cur, rt, fresh;
    int32_t e;
    {
        uint32_t l = fm1__lock();
        rt = fm1_in.notes_retrig;                /* struck again while down: let go now, struck at the next block */
        fm1_in.notes_retrig = 0;
        cur = fm1_in.notes & ~rt;
        fm1__unlock(l);
    }
    fresh = cur & ~kb_prev;
    for (k = 0; k < 27u; k++)                    /* the key's or the pad's velocity (seq.c KB_VEL) */
        if ((fresh >> k) & 1u) {
            kb_velx[k] = (uint8_t)(fm1_in.note_vel[k] ^ 100u);
            smk_sus_end(k);                      /* (a held note struck again: the old one ends first) */
        }
#if FELUCCA_SMK_TRS
    if (smk_kb.mo != (mo_lvl < MO_OFF) + 1u) {   /* MIDI OUT's setting: the jack on or off (its held notes let go) */
        smk_kb.mo = (uint8_t)((mo_lvl < MO_OFF) + 1u);
        smk_mo_enable(mo_lvl < MO_OFF);
    }
#endif
    while ((e = smk_extkey_take()) >= 0) {       /* the outer ten (hal/smk_input.h smk__key_event) */
        k = smk_kb_key(((uint32_t)e >> 8) & 63u);
        if (k >= 27u && k < SMK_KEYS)
            smk_outer(k, (uint32_t)e & 127u);
    }
    if (!smk_kb.cal) {                           /* the rests, once the scan has read every analog slot a few times */
        if (fm1_in.frames < 64u)
            return cur;
        smk_kb.ped0 = fm1_in.pedal;
        smk_kb.cal = 1;
        smk_kb.sel = (uint8_t)(ch + 1u);
    }
    smk_wheels_feed(&smk_kb.w, fm1_in.wheel[0], fm1_in.wheel[1]);
    if (smk_kb.sel != ch + 1u) {                 /* another track: the old one let go (centre, 0) */
        uint32_t o = smk_kb.sel - 1u;
        if (smk_kb.bend) smk_send(0xE0u, o, 0, 64, 1);
        if (smk_kb.mod) smk_send(0xB0u, o, 1, 0, 1);
        if (smk_kb.ped) smk_send(0xB0u, o, 64, 0, 1);
        smk_kb.bend = 0;
        smk_kb.mod = smk_kb.ped = 0;
        smk_kb.sel = (uint8_t)(ch + 1u);
    }
    {
        int32_t b = smk_bend14(&smk_kb.w, fm1_in.wheel[0]);
        uint32_t m = smk_mod7(&smk_kb.w, fm1_in.wheel[1]), p;
        if (b != smk_kb.bend && (b == 0 || b == 8191 || b == -8192 || b - smk_kb.bend > 31 || smk_kb.bend - b > 31)) {
            uint32_t v = (uint32_t)(b + 8192);
            smk_send(0xE0u, ch, v & 127u, v >> 7, 1);
            smk_kb.bend = (int16_t)b;
        }
        if (m != smk_kb.mod && (m == 0u || m == 127u || m > smk_kb.mod + 1u || m + 1u < smk_kb.mod)) {
            smk_send(0xB0u, ch, 1, m, 1);
            smk_kb.mod = (uint8_t)m;
        }
        p = (uint32_t)((int32_t)fm1_in.pedal - smk_kb.ped0 > SMK_PED_ON || (int32_t)smk_kb.ped0 - fm1_in.pedal > SMK_PED_ON);
        if (p != smk_kb.ped) {
            smk_send(0xB0u, ch, 64, p ? 127u : 0u, 1);
            smk_kb.ped = (uint8_t)p;
            if (!p)                              /* up: what it held ends */
                for (k = 0; k < SMK_KEYS; k++)
                    smk_sus_end(k);
        }
    }
    return cur;
}
