/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (SMK-37, BOARD_SMK37 only; docs/SMK37.md "The control mapping") The V10.8 layout's pure rules, no HAL and no UI
 * state, so tests/smk_map_test.c checks them on the host: the keybed's notes, what the pads are on each kind of page,
 * the pads' numbers, the knob and fader banks' targets and the faders' pick-up. firmware/src/smk.c applies them. */
#pragma once
#include <stdint.h>

/* ------------------------------------------------------------------ the keybed --- */
/* keybed key k (0 = the lowest C .. 36 = the top C, hal/smk_input.h) at OCT o (song.octave, -3..+3): C3 + k + 12 o,
 * the whole window moved (never folded) so that all 37 notes stay in 0..127 and apart whatever the octave */
#define SMK_KEYS 37u
#define SMK_KEY_C3 48
static inline uint32_t smk_key_note(uint32_t k, int32_t oct)
{
    int32_t b = SMK_KEY_C3 + 12 * oct;
    b = b < 0 ? 0 : b > 127 - (int32_t)(SMK_KEYS - 1u) ? 127 - (int32_t)(SMK_KEYS - 1u) : b;
    return (uint32_t)b + (k < SMK_KEYS ? k : SMK_KEYS - 1u);
}

/* -------------------------------------------------------------------- the pads --- */
/* pad n (1..16) is hardware pad n - 1: the top row 1..8, the bottom row 9..16, left to right (hal/smk_input.h:
 * pads 0..7 mux A, the top row; 8..15 mux B, the bottom one) */
static inline uint32_t smk_pad_num(uint32_t hw) { return hw + 1u; }
/* what the pads are now:
 *   OFF     the MENU, a dialog: nothing (their presses dropped, dark)
 *   WHITE   the FM-1's 16 white keys, as GRID (hal/smk_input.h SMK_PK_GRID): wherever the FM-1 gives its keys to the
 *           UI: a layer's button down or its map up (the layers' shortcuts: GLO + G4 UNDO, SEQ + G5 CLR STEP ...),
 *           NAME, the DRUM grid (SEQ > STEP on a DRUM track: steps 1..16 of the page, with hold + KNOB locks), STEP
 *           FX and the song pages (keys_mode)
 *   STEPS   the other SEQ pages: pad n the page's step n (toggle; held + K1..K4 a parameter lock)
 *   TRACKS  everywhere else: 1..6 the track (PATCH held: its mute), 7 UNDO, 8 TAP, 9..16 the page shortcuts */
enum { SMK_PM_OFF, SMK_PM_WHITE, SMK_PM_STEPS, SMK_PM_TRACKS };
static inline uint32_t smk_pads_rule(int menu_or_dialog, int fm1_keys_ui, int seq_page)
{
    return menu_or_dialog ? SMK_PM_OFF : fm1_keys_ui ? SMK_PM_WHITE : seq_page ? SMK_PM_STEPS : SMK_PM_TRACKS;
}
#define SMK_PAD_UNDO 7u
#define SMK_PAD_TAP 8u
#define SMK_PAD_PAGE0 9u                 /* pads 9..16: SMK_PAGES[n - 9] */
/* the page shortcuts: a section (smk.c: ui_lens.c's SEC_*, in this order) and the page's name in its chain */
enum { SMK_S_HOME, SMK_S_EDIT, SMK_S_SEQ, SMK_S_FX, SMK_S_GLO, SMK_S_ENV, SMK_S_LFO };
static const struct { uint8_t sec; char nm[12]; } SMK_PAGES[8] = {
    {SMK_S_GLO, "MIXER"},                /* 9   GLO > MIXER: the six levels, pans, mutes */
    {SMK_S_EDIT, "FILTER"},              /* 10  EDIT > TONE > FILTER (FM6 and DRUM-X: theirs) */
    {SMK_S_ENV, "ENV"},                  /* 11  ENV > ENV */
    {SMK_S_LFO, "MOD"},                  /* 12  LFO > MOD: the matrix */
    {SMK_S_FX, "DLY"},                   /* 13  FX > DELAY */
    {SMK_S_FX, "REVERB"},                /* 14  FX > REVERB */
    {SMK_S_HOME, "ARRANGE"},             /* 15  HOME > SONG > ARRANGE */
    {SMK_S_HOME, "GENERATOR 1"},         /* 16  HOME > GENERATOR (GEN, as HOME held + SELECT) */
};

/* -------------------------------------------------------- the banks' targets --- */
/* a target: a track's parameter (trk 0..5, or SMK_T_SEL the selected track) or a global (trk SMK_T_G); id SMK_ID_CUT:
 * the track's cutoff as CC74 finds it (the engine's own brightness, else the FILTER page's CUT: midi_control.c) */
#define SMK_T_SEL 6u
#define SMK_T_G 7u
#define SMK_ID_CUT 0xFFu
enum { SMK_TG_LEVEL, SMK_TG_CUT, SMK_TG_DSEND, SMK_TG_RSEND, SMK_TG_DMIX, SMK_TG_RSIZE };   /* (resolved in smk.c) */
typedef struct { uint8_t trk, what; } smk_tgt_t;
/* KNOB BANK's macros: K1..K6 the cutoff of T1..T6, K7 the delay's MIX, K8 the reverb's SIZE (the master effects) */
static const smk_tgt_t SMK_MACRO[8] = {{0, SMK_TG_CUT}, {1, SMK_TG_CUT}, {2, SMK_TG_CUT}, {3, SMK_TG_CUT},
                                       {4, SMK_TG_CUT}, {5, SMK_TG_CUT}, {SMK_T_G, SMK_TG_DMIX}, {SMK_T_G, SMK_TG_RSIZE}};
/* FADER BANK: A the levels of T1..T4; B T5, T6 and the selected track's delay and reverb sends */
static const smk_tgt_t SMK_FADER[2][4] = {
    {{0, SMK_TG_LEVEL}, {1, SMK_TG_LEVEL}, {2, SMK_TG_LEVEL}, {3, SMK_TG_LEVEL}},
    {{4, SMK_TG_LEVEL}, {5, SMK_TG_LEVEL}, {SMK_T_SEL, SMK_TG_DSEND}, {SMK_T_SEL, SMK_TG_RSEND}},
};

/* ------------------------------------------------------------ the pick-up --- */
/* (soft takeover) a fader at f (0..127) moves a value now at v (0..127 on its range) only once it has met it: within
 * SMK_PICK of it, or crossed it since its last reading (last). A value changed by anything else (a knob, a load, the
 * other bank) since the fader last set it loses the pick-up again (more than 1 off what it set: a range of other
 * than 128 values rounds). st: bit 7 picked, bits 0..6 the value it set; last 0xFF: no reading yet (smk_pick_reset) */
#define SMK_PICK 2
typedef struct { uint8_t st, last; } smk_pick_t;
static inline void smk_pick_reset(smk_pick_t *p) { p->st = 0; p->last = 0xFFu; }
static inline int smk_pickup(smk_pick_t *p, uint32_t f, uint32_t v)
{
    int32_t d = (int32_t)f - (int32_t)v, d0 = (int32_t)p->last - (int32_t)v, s = (int32_t)(p->st & 0x7Fu) - (int32_t)v;
    int ok;
    if ((p->st & 0x80u) && (s > 1 || s < -1))          /* moved elsewhere: pick it up again */
        p->st = 0;
    ok = (p->st & 0x80u) || (d <= SMK_PICK && d >= -SMK_PICK) || (p->last != 0xFFu && (d0 < 0) != (d < 0));
    p->last = (uint8_t)f;
    if (ok)
        p->st = (uint8_t)(0x80u | (f & 0x7Fu));
    return ok;
}
