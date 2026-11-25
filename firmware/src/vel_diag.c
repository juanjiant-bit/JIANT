/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.6z6) Stage 2, the keys' velocity and pressure (src/vel.c, vel_kb.h), shares this sampling: it runs whenever
 * MENU > CONTROL > VELOCITY or AFTERTOUCH is not OFF (vd_apply), the diagnostic's presses and captures only while the
 * diagnostic is on (vs.diag); the strike's window and the pressure: vd_sample's driven samples, vel.c vel_driven.
 * (V10.6z5) The velocity diagnostic: stage 1 of key velocity. It measures each key's analog contact so that we can
 * see whether a unit gives a usable velocity, and changes nothing else: no sound, no note, no key detection (the
 * digital scan decides every press and release as before; this only listens). Off at power-on.
 *
 * After Velo (Charles Vestal, github.com/charlesvestal/fm1-velocity, GPL-3.0; as CS-1 and fm1-dock carry it): the
 * FM-1's keys are a carbon pill under a rubber dome, whose resistance falls as the key is pressed harder. On rows PA7
 * and PA8 (the 20 keys F3..C5) the converter can read it: hal/fm1_input.h samples each of those keys every two scan
 * frames (~2.2 ms) and hands every sample to vd_sample (FM1_INPUT_ANALOG), 1023 open, lower the harder. A key touched
 * gets every other sample with its row driven high (DRIVE; Velo's measured finding: a seated key reads the matrix
 * diode's ~0.6 V against the pull-up, soft and firm then only 10..20 counts apart, driven 100..150 apart). The top row
 * (PA6, C#5..G5) has no converter: not measured.
 *
 * Per key (the scan's ISR, vd_sample): OPEN; a pulled-up sample under VD_TOUCH: TOUCHED (its time kept); one under
 * VD_PRESS: DOWN (touch -> press, 1/10 ms); two pulled-up samples in a row at VD_RELEASE or over (not seated yet: at
 * VD_TOUCH or over) OPEN again, and the press is queued (vd_ev): the key, touch -> press, the lowest pulled-up and the lowest driven reading of the whole
 * press (VD_NONE: none; touch -> press VD_NONE: a graze, never seated). Velo's thresholds (900 / 400 / 600). The
 * first key touched while no capture runs is also captured: its 8 pulled-up samples before the touch and the 48
 * samples from the touch on (pulled-up and driven as they come, ~105 ms).
 *
 * Read out (main loop): the editor protocol's command 81 VEL (editor.c, web/EDITOR_PROTOCOL.md; the page web/vel.html
 * turns it on, reads every press and capture and saves them as text); the message line, each press as
 * "VEL C4 D412 P188 2.2MS" (vd_poll); a build with the serial console (FELUCCA_CDC) also has `vel on [DRIVE] | off |
 * dump`, printing the same while the console is open. The MASTER knob keeps working while on: the scan reads it in a
 * slot of its own (hal/fm1_adc.h fm1_adc_read). tests/vel_diag_test.c runs it on synthetic ADC traces;
 * docs/VELOCITY_DIAG.es.md: how to use it. */
#define VD_NK 20u                       /* notes 0..19 (F3..C5): PA8 notes 0..8, PA7 notes 9..19 */
#define VD_PRE 8u
#define VD_POST 48u
#define VD_CAP (VD_PRE + VD_POST)
#define VD_TOUCH 900u                   /* pulled-up readings (1023 open): touched, */
#define VD_PRESS 400u                   /* seated, */
#define VD_RELEASE 600u                 /* and two in a row from here up: let go */
#define VD_DRIVEN 0x400u                /* a capture sample with this bit: a driven reading */
#define VD_NEV 8u
#define VD_NONE 0xFFFFu
#ifndef VD_NOW
#define VD_NOW() fm1_ticks()
#endif
enum { VD_OPEN, VD_TOUCHED, VD_DOWN };

#include "vel_kb.h"                   /* (V10.6z6) the velocity the keys play: vel.c, from here (vel_driven, vel_lift) */
typedef struct {
    uint16_t pre[VD_PRE];               /* the last pulled-up samples, a ring */
    uint16_t pmin, dmin;                /* the lowest pulled-up / driven reading since the touch */
    uint16_t dt;                        /* touch -> press, 1/10 ms (VD_NONE: not seated yet) */
    uint8_t st, pi, dnext, up;          /* state, ring index, drive the next sample, pulled-up samples let go */
    uint32_t t_touch;
} vd_key_t;

static vd_key_t vd_key[VD_NK];
static void vel_driven(uint32_t key, const vd_key_t *k, uint32_t v);   /* (vel.c) */
static void vel_lift(uint32_t key, uint32_t strike);

static uint32_t vd_note(uint32_t col, uint32_t rowbit)   /* the note on (column, row bit), VD_NK if none */
{
    int id = col < FM1_NCOL && rowbit < 6u ? FM1_KEYMAP[rowbit][col] : -1;
    return id >= 14 && (uint32_t)id - 14u < VD_NK ? (uint32_t)id - 14u : VD_NK;
}

/* (the scan's ISR) one sample of the key at (col, rowbit): v 0..1023; driven: taken with the row driven high */
static void vd_sample(uint32_t col, uint32_t rowbit, uint32_t v, uint32_t driven)
{
    uint32_t key = vd_note(col, rowbit), now = VD_NOW();
    vd_key_t *k;
    if (key >= VD_NK)
        return;
    k = &vd_key[key];
    if (driven) {                               /* only the force: no state change (a driven graze reads open too) */
        k->dnext = 0;
        if (k->st != VD_OPEN) {
            if (v < k->dmin)
                k->dmin = (uint16_t)v;
            vel_driven(key, k, v);              /* (V10.6z6) the strike's window, the pressure (vel.c) */
        }
        return;
    }
    if (k->st == VD_OPEN && v < VD_TOUCH) {
        uint32_t i;
        k->st = VD_TOUCHED;
        k->t_touch = now;
        k->pmin = k->dmin = k->dt = vs.strike[key] = (uint16_t)VD_NONE;
        k->up = 0;
        vk[key].pend = 1;                       /* (V10.6z6) the strike's window open (vel.c) */
        vk[key].vel = vk[key].at = 0;
    }
    if (k->st != VD_OPEN) {
        if (v < k->pmin)
            k->pmin = (uint16_t)v;
        if (k->st == VD_TOUCHED && v < VD_PRESS) {
            uint32_t d = (now - k->t_touch) / (FM1_TICKS_PER_US * 100u);
            k->st = VD_DOWN;
            k->dt = (uint16_t)(d < 8000u ? d : 8000u);
        }
        if (v < (k->st == VD_DOWN ? VD_RELEASE : VD_TOUCH))
            k->up = 0;
        else if (++k->up >= 2u) {               /* let go (a graze: back over VD_TOUCH): the press queued */
            k->st = VD_OPEN;
            vel_lift(key, k->dt == VD_NONE ? VD_NONE : vs.strike[key]);   /* (V10.6z6: no pressure; a seated one learnt) */
        }
    }
    k->pre[k->pi] = (uint16_t)v;
    k->pi = (uint8_t)((k->pi + 1u) % VD_PRE);
    k->dnext = k->st != VD_OPEN;                /* touched or held: the next sample driven */
}

/* (the scan's ISR) drive the next sample of the key at (col, rowbit)? */
static int vd_drive(uint32_t col, uint32_t rowbit)
{
    uint32_t key = vd_note(col, rowbit);
    return key < VD_NK && vd_key[key].dnext;
}

/* ---------------------------------------------------------------- main loop --- */
/* (V10.6z6) the scan's sampling as wanted: on while the diagnostic or the keys' velocity / pressure (vel_kb.h vk_on)
 * want it, the diagnostic's drive (else Velo's 2, the one calibrated); off, the scan lets go at its next tick */
static void vd_apply(void)
{
    if (!vk_on) {
        fm1_an.want = 0;
        return;
    }
    if (!fm1_an.want && !fm1_an.on) {           /* (a fresh start: nothing from before) */
        memset(vd_key, 0, sizeof vd_key);
        fm1_an.master = fm1_an.batt = -1;       /* (until the scan's first readings) */
    }
    fm1_an.drive = 2u;
    fm1_an.want = 1;
    fm1_an.on = 1;
}
