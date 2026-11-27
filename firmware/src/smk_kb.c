/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (SMK-37, BOARD_SMK37 only; included by smk.c) The audio side of the SMK-37's controls: the 37 keys, the PITCH and
 * MOD wheels and the sustain pedal, as MIDI in on the selected track's channel, from the audio block (seq.c
 * keyboard_block, ahead of the MIDI queue: the same context as every MIDI event, so no lock). Needs from the firmware:
 * song (sel, octave), NPART, trk, midi_event, mo_key / mo_kon / mo_koff, kb_velx, kb_boot_hold, clamp, the HAL's
 * fm1_in and smk_extkey_take; tests/smk_map_test.c gives it stand-ins and walks every key. */
#include "smk_map.h"
/* ------------------------------------------------- the audio side (keybed) --- */
/* each key sounding: its channel + 1 (0: none) and note, so its release ends that note whatever the track or the
 * octave are by then; the wheels' and the pedal's rest readings (taken once the scan has settled) and what was sent */
static struct {
    uint8_t ch[SMK_KEYS], note[SMK_KEYS];
    uint8_t cal, mod, ped, sel;          /* cal: rests taken; mod, ped: sent (CC1, CC64 >= 64); sel: their channel + 1 */
    int16_t bend;                        /* sent, -8192..8191 */
    uint16_t rest[3];                    /* the PITCH and MOD wheels' and the pedal's readings at rest */
} smk_kb __attribute__((section(".pool")));

#define SMK_BEND_DZ 24                   /* the pitch wheel's dead zone round its rest (0..1023 counts) */
#define SMK_PED_ON 256                   /* the pedal pressed: this far from its rest (either way: either polarity) */
static void smk_send(uint32_t st, uint32_t ch, uint32_t d1, uint32_t d2) { midi_event(st, ch & 15u, d1 & 127u, d2 & 127u); }
static int32_t smk_bend_of(uint32_t raw)
{
    int32_t c = smk_kb.rest[0], d = (int32_t)raw - c;
    if (d > -SMK_BEND_DZ && d < SMK_BEND_DZ)
        return 0;
    d += d > 0 ? -SMK_BEND_DZ : SMK_BEND_DZ;
    return d > 0 ? clamp(d * 8191 / (1023 - c - SMK_BEND_DZ > 1 ? 1023 - c - SMK_BEND_DZ : 1), 0, 8191)
                 : clamp(d * 8192 / (c - SMK_BEND_DZ > 1 ? c - SMK_BEND_DZ : 1), -8192, 0);
}
static __attribute__((noinline)) void smk_kb_block(void)   /* (out of the RAM mix: in flash) */
{
    uint32_t ch = song.sel % NPART, k;
    int32_t e;
    for (k = 0; k < 27u; k++)                    /* the pads as white keys: their velocity (seq.c KB_VEL) */
        kb_velx[k] = (uint8_t)(fm1_in.pad_vel[k] ^ 100u);
    while ((e = smk_extkey_take()) >= 0) {       /* the keybed, every key (hal/smk_input.h smk__key_event) */
        uint32_t key = ((uint32_t)e >> 8) % SMK_KEYS, vel = (uint32_t)e & 127u;
        if (smk_kb.ch[key]) {                    /* (its note before: a release, or struck again) */
            uint32_t c = smk_kb.ch[key] - 1u, n = smk_kb.note[key];
            smk_send(0x80u, c, n, 0);
            mo_koff(c, n);
            smk_kb.ch[key] = 0;
        }
        if (vel && !kb_boot_hold) {              /* (#137: under the splash, silent) */
            uint32_t n = smk_key_note(key, song.octave);
            smk_send(0x90u, ch, n, vel);
            if (mo_key(&trk[ch]))
                mo_kon(ch, n, vel);
            smk_kb.ch[key] = (uint8_t)(ch + 1u);
            smk_kb.note[key] = (uint8_t)n;
        }
    }
    if (!smk_kb.cal) {                           /* the rests, once the scan has read every analog slot a few times */
        if (fm1_in.frames < 64u)
            return;
        smk_kb.rest[0] = fm1_in.wheel[0];
        smk_kb.rest[1] = fm1_in.wheel[1];
        smk_kb.rest[2] = fm1_in.pedal;
        smk_kb.cal = 1;
        smk_kb.sel = (uint8_t)(ch + 1u);
    }
    if (smk_kb.sel != ch + 1u) {                 /* another track: the old one let go (centre, 0, pedal up) */
        uint32_t o = smk_kb.sel - 1u;
        if (smk_kb.bend) smk_send(0xE0u, o, 0, 64);
        if (smk_kb.mod) smk_send(0xB0u, o, 1, 0);
        if (smk_kb.ped) smk_send(0xB0u, o, 64, 0);
        smk_kb.bend = 0;
        smk_kb.mod = smk_kb.ped = 0;
        smk_kb.sel = (uint8_t)(ch + 1u);
    }
    {
        int32_t b = smk_bend_of(fm1_in.wheel[0]), r = smk_kb.rest[1], w = fm1_in.wheel[1];
        uint32_t m, p;
        if (b != smk_kb.bend && (b == 0 || b == 8191 || b == -8192 || b - smk_kb.bend > 31 || smk_kb.bend - b > 31)) {
            uint32_t v = (uint32_t)(b + 8192);
            smk_send(0xE0u, ch, v & 127u, v >> 7);
            smk_kb.bend = (int16_t)b;
        }
        /* MOD: 0 at its rest, 127 at the other end (the end it rests at: either, as read at power-on) */
        m = (uint32_t)clamp((r < 512 ? w - r : r - w) * 127 / (r < 512 ? 1023 - r > 1 ? 1023 - r : 1 : r > 1 ? r : 1),
                            0, 127);
        if (m != smk_kb.mod && (m == 0u || m == 127u || m > smk_kb.mod + 1u || m + 1u < smk_kb.mod)) {
            smk_send(0xB0u, ch, 1, m);
            smk_kb.mod = (uint8_t)m;
        }
        p = (uint32_t)((int32_t)fm1_in.pedal - smk_kb.rest[2] > SMK_PED_ON ||
                       (int32_t)smk_kb.rest[2] - fm1_in.pedal > SMK_PED_ON);
        if (p != smk_kb.ped) {
            smk_send(0xB0u, ch, 64, p ? 127u : 0u);
            smk_kb.ped = (uint8_t)p;
        }
    }
}
