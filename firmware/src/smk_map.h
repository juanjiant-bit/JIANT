/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The layout is Rhythmer's SMK-37 Pro layout (Rhythmer, a GPL-3.0 fork of JIANT for the FM-1, by Jazzy Rhythmer:
 * its smk_controls.h, the outer keys' order and the faders' banks, adapted here). */
/* (SMK-37, BOARD_SMK37 only; docs/SMK37.md "The control mapping") The layout's pure rules, no HAL and no UI state, so
 * tests/smk_map_test.c checks them on the host: the keybed's keys as the key path's, the pads' two modes, the faders'
 * banks and track window, the faders' pick-up. firmware/src/smk.c, smk_kb.c and seq.c apply them. */
#pragma once
#include <stdint.h>

/* ------------------------------------------------------------------ the keybed --- */
/* keybed key id (hal/smk_input.h: 0 = C3 .. 36 = C6) -> its key in seq.c's key path (kb_note[], key_on): ids 5..31
 * (F3..G5) are the FM-1's 27 keys 0..26 (fm1_in.notes); the outer ones follow them: ids 0..4 (C3..E3) keys 27..31,
 * ids 32..36 (G#5..C6) keys 32..36 (Rhythmer's order) */
#define SMK_KEYS 37u
#define SMK_KB_F3 5u
static inline uint32_t smk_kb_key(uint32_t id)
{
    return id - SMK_KB_F3 < 27u ? id - SMK_KB_F3 : id < SMK_KB_F3 ? 27u + id : id < SMK_KEYS ? id : 0xFFu;
}
/* key k of the key path: its semitones from F3 (kb_map: 53 + this, + 12 OCT; -5..31) */
static inline int32_t smk_kb_off(uint32_t k)
{
    return k < 27u ? (int32_t)k : k < 32u ? (int32_t)k - 32 : (int32_t)k - 5;
}
/* the note keybed key id plays with no scale map (QUANT OFF, no CHORD) at OCT o: C3 + id + 12 o (48..84 at 0) */
static inline uint32_t smk_key_note(uint32_t id, int32_t oct)
{
    int32_t n = 53 + smk_kb_off(smk_kb_key(id)) + 12 * oct;
    return (uint32_t)(n < 0 ? 0 : n > 127 ? 127 : n);
}

/* -------------------------------------------------------------------- the pads --- */
/* pad n (1..16) is hardware pad n - 1: the top row 1..8, the bottom row 9..16, left to right */
static inline uint32_t smk_pad_num(uint32_t hw) { return hw + 1u; }
/* KNOB BANK: PLAY (its LED dark) the pads are the FM-1's 16 white keys (hal/smk_input.h SMK_PK_GRID); STEPS (lit) the
 * 16 steps of the page shown (on the DRUM grid the lane picked). A change asked while a pad is down waits for every
 * pad to be let go (gate): no stray note, no half a step (Rhythmer's rule). */
enum { SMK_PADS_PLAY, SMK_PADS_STEPS };
typedef struct { uint8_t mode, gate; } smk_pads_t;
static inline void smk_pads_toggle(smk_pads_t *p) { p->gate = (uint8_t)!p->gate; }   /* (again while waiting: undone) */
/* each frame: down, a pad is down; returns 1 when the mode changed now */
static inline int smk_pads_settle(smk_pads_t *p, int down)
{
    if (!p->gate || down)
        return 0;
    p->gate = 0;
    p->mode = (uint8_t)!p->mode;
    return 1;
}
/* the pads are white keys now (else the UI's: steps, or nothing while the change waits) */
static inline int smk_pads_white(const smk_pads_t *p) { return p->mode == SMK_PADS_PLAY && !p->gate; }

/* ---------------------------------------------------------------- the faders --- */
/* FADER BANK: LEVEL -> PAN -> REVERB -> DELAY (the sends) -> LEVEL; faders 1..4 are T1..T4, or with T5 / T6 selected
 * faders 1..2 T5 / T6 (3..4 nothing). The pick-up's context: the bank and the window (a change re-arms it) */
enum { SMK_FB_LEVEL, SMK_FB_PAN, SMK_FB_REVERB, SMK_FB_DELAY, SMK_FB_N };
static inline uint32_t smk_fb_next(uint32_t b) { return (b + 1u) % SMK_FB_N; }
static inline int smk_fader_track(uint32_t f, uint32_t sel)   /* the track fader f (0..3) moves, -1 none */
{
    uint32_t t = (sel >= 4u ? 4u : 0u) + f;
    return f < 4u && t < 6u ? (int)t : -1;
}
static inline uint32_t smk_fader_ctx(uint32_t bank, uint32_t sel) { return bank * 2u + (sel >= 4u); }

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

/* ---------------------------------------------------------------- the wheels --- */
/* (genkigenki's docs/01-hardware.md 6.5, verified on an SMK-37 Pro: the travel is short and differs between units:
 * PITCH rests ~535 and moves ~445..640, MOD ~474..629 with up = the lower reading) A self-calibrating range, as
 * genkigenki's and Rhythmer's: the pitch centre is the reading at rest (its drift followed while resting), each range
 * widened to the extremes seen, from a start of +-SMK_W_SPAN0 round the centre (pitch) and SMK_W_MLO..SMK_W_MHI (mod). */
#define SMK_W_DEAD 12
#define SMK_W_SPAN0 80
#define SMK_W_MLO 485
#define SMK_W_MHI 615
typedef struct { int16_t pc, plo, phi, mlo, mhi; uint8_t rest, on; } smk_wheels_t;
static inline void smk_wheels_feed(smk_wheels_t *w, int32_t p, int32_t m)   /* the raw readings, each audio block */
{
    if (!w->on) {
        w->on = 1;
        w->pc = (int16_t)p, w->plo = (int16_t)(p - SMK_W_SPAN0), w->phi = (int16_t)(p + SMK_W_SPAN0);
        w->mlo = (int16_t)(m < SMK_W_MLO ? m : SMK_W_MLO), w->mhi = (int16_t)(m > SMK_W_MHI ? m : SMK_W_MHI);
    }
    if (p < w->plo) w->plo = (int16_t)p;
    if (p > w->phi) w->phi = (int16_t)p;
    if (m < w->mlo) w->mlo = (int16_t)m;
    if (m > w->mhi) w->mhi = (int16_t)m;
    if (p - w->pc > -SMK_W_DEAD && p - w->pc < SMK_W_DEAD) {
        if (++w->rest > 200u) {                  /* resting: the centre follows a slow drift */
            w->pc = (int16_t)(w->pc + (p > w->pc) - (p < w->pc));
            w->rest = 0;
        }
    } else {
        w->rest = 0;
    }
}
static inline int32_t smk_bend14(const smk_wheels_t *w, int32_t p)   /* -8192..8191, 0 inside the dead zone */
{
    int32_t d = p - w->pc, up = w->phi - w->pc - SMK_W_DEAD, dn = w->pc - w->plo - SMK_W_DEAD, b;
    if (d > -SMK_W_DEAD && d < SMK_W_DEAD)
        return 0;
    b = d > 0 ? (d - SMK_W_DEAD) * 8191 / (up > 1 ? up : 1) : (d + SMK_W_DEAD) * 8192 / (dn > 1 ? dn : 1);
    return b < -8192 ? -8192 : b > 8191 ? 8191 : b;
}
static inline uint32_t smk_mod7(const smk_wheels_t *w, int32_t m)    /* 0 at rest (the high reading) .. 127 */
{
    int32_t r = w->mhi - m - SMK_W_DEAD, span = w->mhi - w->mlo - SMK_W_DEAD;
    r = r <= 0 ? 0 : r * 127 / (span > 1 ? span : 1);
    return (uint32_t)(r > 127 ? 127 : r);
}
