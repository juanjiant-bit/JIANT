/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The layout is Rhythmer's SMK-37 Pro layout, adapted to V10.8.48: Rhythmer (a GPL-3.0 fork of JIANT for the FM-1) by
 * Jazzy Rhythmer (its smk_runtime.c: KNOB BANK's PLAY / STEPS pads, FADER BANK's four banks and track window with
 * pick-up); the board layer under it is genkigenki's (github.com/genkigenki/Felucca smk37, the porting reference
 * github.com/genkigenki/fm1-to-smk37). */
/* (SMK-37, BOARD_SMK37 builds only: felucca.c includes it after ui_layer.c) The SMK-37's own controls
 * (docs/SMK37.md "The control mapping"; the pure rules: smk_map.h):
 *   KEYBED   all 37 keys through the FM-1's key path (smk_kb.c, seq.c keyboard_block): the central 27 are the FM-1's
 *            keys, the outer ten extra notes (C3..E3, G#5..C6 at OCT 0), at their velocity
 *   WHEELS   pitch bend and MOD (CC1) of the selected track; PEDAL: sustain (smk_kb.c)
 *   PADS     KNOB BANK: PLAY (the FM-1's 16 white keys) <-> STEPS (the page's 16 steps; KNOB BANK lit)
 *   K1..K8   MASTER, SELECT, ALGORITHM, PRESETS, KNOB 1..4 (hal/smk_input.h, panel.c)
 *   F1..F4   FADER BANK: LEVEL -> PAN -> REVERB -> DELAY of T1..T4 (T5 / T6 selected: F1 F2 = T5 T6), with pick-up
 *   < >      one SELECT detent each (the page before / after)
 * Untested on hardware: see docs/SMK37.md. */
#include "smk_map.h"
_Static_assert(FM1_NKEY == SMK_KEYS && SMK_KEY_F3 == SMK_KB_F3 && KB_NK == SMK_KEYS, "smk_map.h: the keybed's 37 keys");

/* --------------------------------------------------------------- velocity --- */
/* MENU > CONTROL > VELOCITY (vel_kb.h VEL_SET: OFF SOFT NORMAL HARD) as the HAL's curve (hal/smk_input.h smk_vel_curve:
 * NORMAL SOFT HARD FIXED 100); vel.c's AFTERTOUCH and VEL LEARN are the FM-1's carbon pills: nothing here */
static void vel_poll(void)
{
    static const uint8_t C[4] = {3, 1, 0, 2};
    smk_vel_curve = C[(VEL_SET ^ 0x0Au) & 3u];
}

#include "smk_kb.c"                        /* the keybed, the wheels and the pedal (the audio side) */


/* ------------------------------------------------------------- the UI side --- */
static struct {
    smk_pads_t pads;                     /* KNOB BANK: PLAY / STEPS (and a change waiting for the pads' release) */
    uint8_t fbank;                       /* FADER BANK: SMK_FB_* */
    uint8_t fctx;                        /* the pick-ups' context (bank, track window) + 1: 0 none yet */
    uint8_t ptrk;                        /* STEPS: the track the held pads' steps are on */
    uint8_t fval[4];                     /* the faders as last read */
    uint16_t held;                       /* STEPS: the pads held (bit n - 1) */
    uint16_t used;                       /* .. held for a lock (a knob turned): no toggle when let go */
    uint16_t drop;                       /* .. whose step had something: cleared when let go, unless used */
    uint8_t note;                        /* STEPS: a melodic step's note (the keys' last) */
    smk_pick_t pick[4];
} smk __attribute__((section(".pool")));

/* fader f's value: the track's LEVEL, PAN, REVERB or DELAY send; 0..127 over its range */
static const uint8_t SMK_FB_P[SMK_FB_N] = {P_LEVEL, P_PAN, P_REV, P_DLY};
static const char *const SMK_FB_NM[SMK_FB_N] = {"LEVEL", "PAN", "REVERB", "DELAY"};
static uint32_t smk_fv(track_t *t, uint32_t id, int32_t *lo, int32_t *hi)
{
    const param_desc_t *d = track_desc(t, id);
    *lo = d->min, *hi = d->max;
    return *hi > *lo ? (uint32_t)clamp((t->p[id] - *lo) * 127 / (*hi - *lo), 0, 127) : 0u;
}
static void smk_bank_say(void)
{
    char b[20];
    str_cpy(b, "FADERS ", sizeof b);
    str_cpy(b + 7, SMK_FB_NM[smk.fbank % SMK_FB_N], sizeof b - 7);
    ui_say(b, song.sel >= 4u ? " T5 T6" : " T1-T4");
}
static void smk_faders(void)
{
    uint32_t f, ctx = smk_fader_ctx(smk.fbank, song.sel) + 1u, id = SMK_FB_P[smk.fbank % SMK_FB_N];
    if (ctx != smk.fctx) {                       /* another bank or track window: picked up again */
        for (f = 0; f < 4u; f++)
            smk_pick_reset(&smk.pick[f]);
        if (smk.fctx && (smk.fctx - 1u) / 2u == smk.fbank)
            smk_bank_say();                      /* (the window moved: say so; a bank change says it itself) */
        smk.fctx = (uint8_t)ctx;
    }
    if (fm1_in.frames < 128u)                    /* (~0.2 s: the faders' filters settle from 0 first, no false cross) */
        return;
    for (f = 0; f < 4u; f++) {
        uint32_t v = fm1_in.fader[f], first = smk.pick[f].last == 0xFFu, cur;
        int ti = smk_fader_track(f, song.sel);
        int32_t lo, hi;
        track_t *t;
        if (ti < 0 || (v == smk.fval[f] && !first))
            continue;
        smk.fval[f] = (uint8_t)v;
        t = &trk[ti];
        cur = smk_fv(t, id, &lo, &hi);
        if (smk_pickup(&smk.pick[f], v, cur)) {
            int32_t x = clamp(lo + ((int32_t)v * (hi - lo) + 63) / 127, lo, hi);
            char b[24];
            if (t->p[id] == x)
                continue;
            t->p[id] = (int16_t)x;
            motion_capture(t, id, (int16_t)x);   /* (recorded as a knob move, armed and playing) */
            b[0] = 'T', b[1] = (char)('1' + ti), b[2] = ' ', b[3] = 0;
            str_cpy(b + 3, SMK_FB_NM[smk.fbank % SMK_FB_N], sizeof b - 3);
            str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
            fmt_int(b + str_len(b), x);
            ui_message(b);
        } else if (!first) {                     /* (moved, not yet met: which way to go) */
            char m[8] = {'F', (char)('1' + f), ' ', 0};
            ui_say(m, cur > v ? "PICK UP: UP" : "PICK UP: DOWN");
        }
    }
}

/* STEPS: pad n's step on the page shown (the bar of the cursor, ui.bank) */
static uint32_t smk_step_of(uint32_t n) { return ui.bank * 16u + n - 1u; }
static int smk_step_full(const track_t *t, uint32_t i)
{
    const step_t *s = &t->step[i % NSTEP];
    return s->time == ST_NOTE && (s->n || (drum_track(t) && ((step_lanes_of(t, s) >> (ui.lane % NLANE)) & 1u)));
}
static void smk_step_press(uint32_t n, uint32_t vel)
{
    track_t *t = TSEL;
    uint32_t i = smk_step_of(n), len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), b = 1u << (n - 1u);
    if (i >= len)
        return;                                  /* (past LEN: no step there) */
    if (chain_busy()) {
        ui_message("SONG PLAYS: STOP IT TO EDIT");
        return;
    }
    if (!smk.held)
        smk.ptrk = (uint8_t)song.sel;
    smk.held |= (uint16_t)b;
    smk.used &= (uint16_t)~b;
    smk.drop &= (uint16_t)~b;
    if (smk_step_full(t, i)) {
        smk.drop |= (uint16_t)b;                 /* (cleared when let go, unless held for a lock) */
    } else if (drum_track(t)) {
        grid_hit(t, i, ui.lane, 1);
    } else {
        step_t *s = &t->step[i];
        step_clear(s);
        s->note[0] = smk.note ? smk.note : (uint8_t)clamp(60 + t->p[P_ROOT], 0, 127);
        s->n = 1;
        s->time = ST_NOTE;
        s->vel = (uint8_t)(vel ? vel : 96u);
    }
    cursor_set((int32_t)i);
    ui.force = 1;
}
static void smk_step_release(uint32_t n)
{
    uint32_t b = 1u << (n - 1u), i = smk_step_of(n);
    track_t *t = &trk[smk.ptrk % NTRK];
    if (!(smk.held & b))
        return;
    smk.held &= (uint16_t)~b;
    if ((smk.drop & b) && !(smk.used & b) && i < NSTEP && !chain_busy()) {
        if (drum_track(t))
            grid_hit(t, i, ui.lane, 0);
        else
            step_clear(&t->step[i]);
        ui.force = 1;
    }
    smk.drop &= (uint16_t)~b;
    smk.used &= (uint16_t)~b;
}
static uint64_t smk_steps_held(void)
{
    uint64_t m = 0;
    uint32_t n;
    for (n = 1; n <= 16u; n++)
        if ((smk.held >> (n - 1u)) & 1u)
            m |= (uint64_t)1 << smk_step_of(n);
    return m;
}
static int smk_steps_off(void) { return ui.menu || dlg_on() || ui.layer || name_on(); }   /* STEPS: the pads idle */

/* each UI frame, first (ui_input.c ui_input_frame) */
static void smk_input_frame(uint32_t *pressed)
{
    uint32_t labels = 0, b, f, down = 0;
    int32_t d = (int32_t)((*pressed >> SMK_BTN_RIGHT) & 1u) - (int32_t)((*pressed >> SMK_BTN_LEFT) & 1u), e;
    for (b = 0; b < NB; b++)
        labels |= 1u << panel.btn[b];
    if (d && !((labels >> SMK_BTN_LEFT) & 1u) && !((labels >> SMK_BTN_RIGHT) & 1u) && !dlg_on()) {   /* < >: SELECT */
        uint32_t l = fm1__lock();
        fm1_in.enc_steps[panel.enc[EN_SELECT]] = (int16_t)(fm1_in.enc_steps[panel.enc[EN_SELECT]] + d * panel.dir[EN_SELECT]);
        fm1__unlock(l);
    }
    if ((*pressed >> SMK_BTN_KNOB_BANK) & 1u && !((labels >> SMK_BTN_KNOB_BANK) & 1u)) {
        smk_pads_toggle(&smk.pads);
        ui_say("PADS ", (smk.pads.mode == SMK_PADS_STEPS) == !smk.pads.gate ? "STEPS" : "PLAY");
    }
    if ((*pressed >> SMK_BTN_FADER_BANK) & 1u && !((labels >> SMK_BTN_FADER_BANK) & 1u)) {
        smk.fbank = (uint8_t)smk_fb_next(smk.fbank);
        smk_bank_say();
    }
    *pressed &= labels;
    smk_faders();
    for (f = 0; f < SMK_NPAD; f++)
        down |= fm1_in.pad_st[f];
    if (smk_pads_settle(&smk.pads, down != 0u))  /* (KNOB BANK's change, once no pad is down) */
        while (smk.held)
            smk_step_release((uint32_t)__builtin_ctz(smk.held) + 1u);
    smk_pad_key = smk_pads_white(&smk.pads) ? SMK_PK_GRID : SMK_PK_NONE;   /* (the HAL takes it at the next hit) */
    while ((e = smk_pad_take()) >= 0) {          /* STEPS (a change waiting: releases only) */
        uint32_t n = smk_pad_num(((uint32_t)e >> 8) & 15u), vel = (uint32_t)e & 127u;
        if (!vel)
            smk_step_release(n);
        else if (smk.pads.mode == SMK_PADS_STEPS && !smk.pads.gate && !smk_steps_off())
            smk_step_press(n, vel);
    }
    if (smk.held && smk.ptrk == song.sel) {      /* a step held + KNOB 1..4 (K5..K8): its lock */
        uint64_t h = smk_steps_held();
        for (f = 0; f < 4u; f++)
            if ((e = panel_enc(EN_K1 + f)) != 0) {
                lock_turn(f, e, h);
                smk.used = smk.held;
                ui.hot_col = (uint8_t)f;
                ui.hot_t = 40, ui.knob_ms = fm1_ms;
            }
    }
    {   /* the keys' last note: a new melodic step's */
        static uint8_t last;
        if (last_note != last)
            smk.note = last = last_note;
    }
}

/* ---------------------------------------------------------------- the LEDs --- */
/* The buttons: one on/off LED each (hal/smk_input.h SMK_LEDC; SEQ PLAY's green its own id 16). The FM-1's picture
 * where the SMK can show it: lit = lit; what breathes on the FM-1 blinks (~1 Hz); no idle glow (as LEDS OFF; LEDS
 * INV: the idle ones lit). KNOB BANK lit: the pads are STEPS; FADER BANK lit: a bank other than LEVEL. The 16 RGB pads
 * (lcd.c padled_set): PLAY the FM-1 keys' picture (lit white, breathing blinking, the DRUM grid's glow and beats dim);
 * STEPS the page's steps (the track's colour, brighter with something on it, the cursor white, the playhead bright,
 * past LEN dark); dark while a KNOB BANK change waits or (STEPS) in the MENU, a dialog, a layer, NAME. Untested on
 * hardware. */
#define SMK_PAD_LIT 72u                                 /* the pads' levels (0..255 a channel: WS2812s are bright) */
#define SMK_PAD_MID 14u
#define SMK_PAD_DIM 3u
static void smk_pad_rgb(uint32_t hw, uint32_t rgb565, uint32_t lvl)   /* a colour at a level (0..SMK_PAD_LIT) */
{
    uint32_t r = (rgb565 >> 11) & 31u, g = (rgb565 >> 5) & 63u, b = rgb565 & 31u;
    padled_set(hw, (uint8_t)(r * lvl / 31u), (uint8_t)(g * lvl / 63u), (uint8_t)(b * lvl / 31u));
}
static void smk_pads_out(uint32_t blink)
{
    uint32_t p;
    if (smk_pads_white(&smk.pads)) {
        uint32_t kb, c = key_leds(&kb), g = grid_on() && !ui.layer && !name_on() ? grid_glow() : 0u;
        uint32_t beat = g ? grid_beats() & ~c : 0u, lit = c, br = blink ? kb : 0u, inv = settings_leds == LEDS_INV;
        for (p = 0; p < SMK_NPAD; p++) {
            uint32_t bit = 1u << SMK_PK_GRID[p], v;
            v = lit & bit ? SMK_PAD_LIT : br & bit ? SMK_PAD_MID : beat & bit ? SMK_PAD_MID : g & bit ? SMK_PAD_DIM : 0u;
            if (inv && !(br & bit))
                v = lit & bit ? 0u : v ? v : SMK_PAD_MID;
            padled_set(p, (uint8_t)v, (uint8_t)v, (uint8_t)v);
        }
    } else if (!smk.pads.gate && !smk_steps_off()) {
        const track_t *t = TSEL;
        uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), col = trk_col(song.sel);
        for (p = 0; p < SMK_NPAD; p++) {
            uint32_t n = smk_pad_num(p), i = smk_step_of(n);
            if (i >= len)
                padled_set(p, 0, 0, 0);
            else if (song.playing && t->seq_idx == i)
                padled_set(p, SMK_PAD_LIT, SMK_PAD_LIT, SMK_PAD_LIT);
            else if ((smk.held >> (n - 1u)) & 1u)
                smk_pad_rgb(p, col, SMK_PAD_LIT);
            else if (smk_step_full(t, i))
                smk_pad_rgb(p, col, ui.cursor == i ? SMK_PAD_LIT / 2u : SMK_PAD_MID);
            else if (ui.cursor == i)
                padled_set(p, SMK_PAD_MID, SMK_PAD_MID, SMK_PAD_MID);
            else
                padled_set(p, (n - 1u) % 4u ? 0u : SMK_PAD_DIM, (n - 1u) % 4u ? 0u : SMK_PAD_DIM, (n - 1u) % 4u ? 0u : SMK_PAD_DIM);
        }
    } else {
        for (p = 0; p < SMK_NPAD; p++)
            padled_set(p, 0, 0, 0);
    }
    padled_flush();                                     /* (only when a colour changed: ~0.8 ms) */
}
static void ui_leds(void)
{
    uint8_t on[SMK_NLED] = {0}, br[SMK_NLED] = {0};
    uint32_t k, blink = (fm1_ms / 500u) & 1u, fam = cur_fam(), mode = settings_leds;
#define SMK_LED(a, id, v) do { if ((uint32_t)(id) < SMK_NLED && (v)) (a)[(uint32_t)(id)] = 1u; } while (0)
    if (!ui.layer || FAM_BTN[fam] != layer_btn())
        SMK_LED(on, panel.btn[FAM_BTN[fam]], 1);
    if ((k = arp_led()) != 2u && (!ui.layer || layer_btn() != B_ARP))
        SMK_LED(on, panel.btn[B_ARP], FAM_BTN[fam] == B_ARP ? !k : k);
    if (!ui.layer && (perf_latched || fx_mon || perf_k[0] || perf_k[1] || perf_k[2] || perf_k[3]))
        SMK_LED(on, panel.btn[B_FX], 1);
    if (ui.layer)
        SMK_LED(br, panel.btn[layer_btn()], 1);
    SMK_LED(on, panel.btn[B_REC], song.rec != 0u && ui.layer != LAYER_REC);
    k = oct_leds();
    SMK_LED(on, panel.btn[B_OCTDN], k & 1u);
    SMK_LED(on, panel.btn[B_OCTUP], (k >> 1) & 1u);
    SMK_LED(br, panel.btn[B_OCTUP], k & OCT_BREATH);
    SMK_LED(br, panel.btn[B_OCTDN], (k & OCT_BREATH_DN) != 0u);
    if (song.playing || (seq_counting() && cin_pos < click_beat_len() / 4u))   /* SEQ PLAY (green): playing, the count-in */
        SMK_LED(on, panel.btn[B_PLAY], 1);
    for (k = 0; k < NB; k++) {
        uint32_t id = panel.btn[k];
        if (id >= SMK_NLED)
            continue;
        if (br[id])
            fm1_led[id] = (uint8_t)blink;
        else if (mode == LEDS_INV && k != B_PLAY)
            fm1_led[id] = (uint8_t)!on[id];
        else
            fm1_led[id] = on[id];
    }
    fm1_led[SMK_BTN_KNOB_BANK] = smk.pads.gate ? (uint8_t)blink : (uint8_t)(smk.pads.mode == SMK_PADS_STEPS);
    fm1_led[SMK_BTN_FADER_BANK] = smk.fbank != SMK_FB_LEVEL;
#undef SMK_LED
    smk_pads_out(blink);
}
