/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (SMK-37, BOARD_SMK37 builds only: felucca.c includes it after ui_layer.c) The SMK-37's own controls in the V10.8
 * layout the owner approved (docs/SMK37.md "The control mapping"; the pure rules: smk_map.h):
 *   KEYBED   the 37 keys always play the selected track, at their velocity, C3..C6 moved by OCT- / OCT+ (song.octave):
 *            as MIDI in on the selected track's channel (seq.c midi_event: notes, ARP, recording, DRUM's MIDI map),
 *            from the audio block (smk_kb_block, seq.c keyboard_block), their notes to MIDI OUT as the FM-1's keys'
 *   WHEELS   PITCH: pitch bend, MOD: CC1 (the matrix's MOD WHEEL), PEDAL: CC64 (the notes held), the selected track's
 *   PADS     smk_map.h smk_pads_rule: the FM-1's white keys where the FM-1's keys are the UI's, the steps on the
 *            other SEQ pages, else the tracks, UNDO, TAP and eight page shortcuts
 *   K1..K8   KNOB 1..4, SELECT, PRESETS, ALGORITHM, MASTER (panel.c, hal/smk_input.h); KNOB BANK: the 8 macros
 *   F1..F4   FADER BANK A / B (smk_map.h SMK_FADER), with pick-up
 *   < >      one SELECT detent each (the page before / after)
 * Untested on hardware: see docs/SMK37.md. */
#include "smk_map.h"
_Static_assert(SMK_S_HOME == SEC_HOME && SMK_S_EDIT == SEC_EDIT && SMK_S_SEQ == SEC_SEQ && SMK_S_FX == SEC_FX &&
               SMK_S_GLO == SEC_GLO && SMK_S_ENV == SEC_ENV && SMK_S_LFO == SEC_LFO, "smk_map.h: the sections' order");
_Static_assert(FM1_NKEY == SMK_KEYS, "smk_map.h: the keybed's 37 keys");

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
    uint8_t mode;                        /* the pads now (SMK_PM_*) */
    uint8_t fbank;                       /* FADER BANK: 0 A, 1 B */
    uint8_t ptrk;                        /* STEPS: the track the held pads' steps are on */
    uint8_t fval[4];                     /* the faders as last read */
    uint16_t held;                       /* STEPS: the pads held (bit n - 1) */
    uint16_t used;                       /* .. held for a lock (a knob turned): no toggle when let go */
    uint16_t drop;                       /* .. whose step had something: cleared when let go, unless used */
    uint8_t note;                        /* STEPS: a melodic step's note (the keybed's last) */
    uint8_t init;                        /* the pick-ups reset (the pool is zeroed at boot: last 0 would read as a fader) */
    smk_pick_t pick[4];
} smk __attribute__((section(".pool")));

static int smk_seq_page(void) { return !ui.home && !ui.menu && cur_page()->fam == FAM_SEQ; }
static uint32_t smk_pads_mode(void)
{
    return smk_pads_rule(ui.menu || dlg_on(), ui.ly || ui.layer || keys_own() || keys_mode() != 0u, smk_seq_page());
}

/* target t: its value now, 0..127 over its range, and a turn / a fader's value set; *nm its name for the screen */
static track_t *smk_tgt_trk(smk_tgt_t t) { return &trk[t.trk == SMK_T_SEL ? song.sel % NTRK : t.trk % NTRK]; }
static uint32_t smk_tgt_id(smk_tgt_t t)
{
    track_t *k = smk_tgt_trk(t);
    uint32_t e;
    switch (t.what) {
    case SMK_TG_LEVEL: return P_LEVEL;
    case SMK_TG_DSEND: return P_DLY;
    case SMK_TG_RSEND: return P_REV;
    case SMK_TG_DMIX: return G_DMIX;
    case SMK_TG_RSIZE: return G_RSIZE;
    default:                             /* CUT: as CC74 (midi_control.c midi_cc_map), else the FILTER page's CUT */
        e = MIDI_CC_ENG[eng_idx(k->eng_req)] >> 4 & 15u;
        return e ? P_E0 + e - 1u : P_FCUT;
    }
}
static void smk_tgt_range(smk_tgt_t t, int16_t **vp, int32_t *lo, int32_t *hi)
{
    uint32_t id = smk_tgt_id(t);
    if (t.trk == SMK_T_G) {
        *vp = &song.g[id];
        *lo = GP[id].min, *hi = GP[id].max;
    } else {
        const param_desc_t *d = track_desc(smk_tgt_trk(t), id);
        *vp = &smk_tgt_trk(t)->p[id];
        *lo = d->min, *hi = d->max;
    }
}
static uint32_t smk_tgt_get(smk_tgt_t t)
{
    int16_t *vp;
    int32_t lo, hi;
    smk_tgt_range(t, &vp, &lo, &hi);
    return hi > lo ? (uint32_t)clamp((*vp - lo) * 127 / (hi - lo), 0, 127) : 0u;
}
static void smk_tgt_say(smk_tgt_t t, int32_t v)
{
    static const char *const NM[6] = {"LEVEL", "CUT", "DLY SEND", "REV SEND", "DLY MIX", "REV SIZE"};
    char b[24];
    if (t.trk == SMK_T_G) {
        b[0] = 0;
    } else {
        b[0] = 'T', b[1] = (char)('1' + (smk_tgt_trk(t) - trk)), b[2] = ' ', b[3] = 0;
    }
    str_cpy(b + str_len(b), NM[t.what % 6u], sizeof b - str_len(b));
    str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
    fmt_int(b + str_len(b), v);
    ui_message(b);
}
static void smk_tgt_put(smk_tgt_t t, int32_t v)  /* v: the value itself */
{
    int16_t *vp;
    int32_t lo, hi;
    smk_tgt_range(t, &vp, &lo, &hi);
    v = clamp(v, lo, hi);
    if (*vp == v)
        return;
    *vp = (int16_t)v;
    if (t.trk != SMK_T_G)
        motion_capture(smk_tgt_trk(t), smk_tgt_id(t), (int16_t)v);   /* (recorded as a knob move, armed and playing) */
    smk_tgt_say(t, v);
}
static void smk_macro_turn(uint32_t k, int32_t s)
{
    smk_tgt_t t = SMK_MACRO[k & 7u];
    int16_t *vp;
    int32_t lo, hi;
    smk_tgt_range(t, &vp, &lo, &hi);
    smk_tgt_put(t, *vp + accel(EN_K1 + (k & 3u), s, hi - lo));
}

static void smk_page(uint32_t n)                 /* pad n (9..16): its page */
{
    const ch_t *c;
    uint32_t i = n - SMK_PAD_PAGE0, sec = SMK_PAGES[i % 8u].sec, e;
    c = nav_ch(sec);
    for (e = 0; e < c->n; e++) {
        const char *nm = c->e[e].nm ? c->e[e].nm : c->e[e].t;
        if (nm && str_eq(nm, SMK_PAGES[i % 8u].nm) && nv_ok(sec, e)) {
            nav_go(sec, e, nav_sec() != sec);
            return;
        }
    }
    ui_message("NO SUCH PAGE HERE");
}
static int smk_page_shown(uint32_t n)            /* the page of pad n is the one shown */
{
    uint32_t i = n - SMK_PAD_PAGE0, sec = SMK_PAGES[i % 8u].sec, e;
    const ch_t *c;
    if (ui.menu || nav_sec() != sec)
        return 0;
    c = nav_ch(sec);
    e = nav_cur();
    return e < c->n && str_eq(c->e[e].nm ? c->e[e].nm : c->e[e].t ? c->e[e].t : "", SMK_PAGES[i % 8u].nm);
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

static void smk_tracks_press(uint32_t n)
{
    char b[16];
    if (n <= NTRK) {
        uint32_t i = n - 1u;
        if ((fm1_in.buttons >> panel.btn[B_HOME]) & 1u) {   /* PATCH (HOME) held: the mute; no HOME tap, no MENU */
            ui.home_t0 |= 2u;
            trk[i].p[P_MUTE] = (int16_t)!trk[i].p[P_MUTE];
            b[0] = 'T', b[1] = (char)('1' + i), b[2] = 0;
            ui_say(b, trk[i].p[P_MUTE] ? " MUTED" : " ON");
        } else if (i >= trk_live) {
            ui_message("T5-T6 ASLEEP: TRACKS 6");
        } else {
            track_select(i);
        }
    } else if (n == SMK_PAD_UNDO) {
        undo_ui();
    } else if (n == SMK_PAD_TAP) {
        glo_tap();
        if (!song.g[G_CLOCK]) {
            str_cpy(b, "TAP ", sizeof b);
            fmt_int(b + 4, song.g[G_BPM]);
            ui_message(b);
        }
    } else if (n >= SMK_PAD_PAGE0 && n <= 16u) {
        smk_page(n);
    }
}

/* each UI frame, first (ui_input.c ui_input_frame) */
static void smk_input_frame(uint32_t *pressed)
{
    uint32_t labels = 0, b, mode, f;
    int32_t d = (int32_t)((*pressed >> SMK_BTN_RIGHT) & 1u) - (int32_t)((*pressed >> SMK_BTN_LEFT) & 1u), e;
    for (b = 0; b < NB; b++)
        labels |= 1u << panel.btn[b];
    if (d && !((labels >> SMK_BTN_LEFT) & 1u) && !((labels >> SMK_BTN_RIGHT) & 1u)) {   /* < >: a SELECT detent */
        uint32_t l = fm1__lock();
        fm1_in.enc_steps[panel.enc[EN_SELECT]] = (int16_t)(fm1_in.enc_steps[panel.enc[EN_SELECT]] + d * panel.dir[EN_SELECT]);
        fm1__unlock(l);
    }
    if ((*pressed >> SMK_BTN_KNOB_BANK) & 1u && !((labels >> SMK_BTN_KNOB_BANK) & 1u)) {
        smk_kbank ^= 1u;
        ui_say(smk_kbank ? "KNOBS: MACROS " : "KNOBS: PAGE ", smk_kbank ? "K1-6 CUT K7 DLY K8 REV" : "K5 SEL K8 MASTER");
    }
    if ((*pressed >> SMK_BTN_FADER_BANK) & 1u && !((labels >> SMK_BTN_FADER_BANK) & 1u)) {
        smk.fbank ^= 1u;
        for (f = 0; f < 4u; f++)
            smk_pick_reset(&smk.pick[f]);
        ui_say(smk.fbank ? "FADERS B: " : "FADERS A: ", smk.fbank ? "T5 T6 DLY REV" : "T1 T2 T3 T4");
    }
    *pressed &= labels;
    if (!smk.init) {
        for (f = 0; f < 4u; f++)
            smk_pick_reset(&smk.pick[f]);
        smk.init = 1;
    }
    if (smk_kbank)                               /* KNOB BANK: K1..K8 the macros, before the panel reads them */
        for (f = 0; f < FM1_NENC; f++)
            if ((e = fm1_enc_take(f)) != 0)
                smk_macro_turn(f, e);
    for (f = 0; f < 4u; f++) {                   /* the faders, with pick-up */
        uint32_t v = fm1_in.fader[f], first = smk.pick[f].last == 0xFFu;
        smk_tgt_t t = SMK_FADER[smk.fbank & 1u][f];
        if (fm1_in.frames < 128u)                /* (~0.2 s: the faders' filters settle from 0 first, no false cross) */
            continue;
        if (v == smk.fval[f] && !first)
            continue;
        smk.fval[f] = (uint8_t)v;
        if (smk_pickup(&smk.pick[f], v, smk_tgt_get(t))) {
            int16_t *vp;
            int32_t lo, hi;
            smk_tgt_range(t, &vp, &lo, &hi);
            smk_tgt_put(t, lo + ((int32_t)v * (hi - lo) + 63) / 127);
        } else if (!first) {                     /* (moved, not yet met: which way to go) */
            char m[8] = {'F', (char)('1' + f), ' ', 0};
            ui_say(m, smk_tgt_get(t) > v ? "PICK UP: UP" : "PICK UP: DOWN");
        }
    }
    mode = smk_pads_mode();
    if (mode != smk.mode) {                      /* another kind of page: the pads held let go first */
        while (smk.held)
            smk_step_release((uint32_t)__builtin_ctz(smk.held) + 1u);
        smk.mode = (uint8_t)mode;
    }
    smk_pad_key = mode == SMK_PM_WHITE ? SMK_PK_GRID : SMK_PK_NONE;   /* (the HAL takes it at the next hit) */
    while ((e = smk_pad_take()) >= 0) {
        uint32_t n = smk_pad_num(((uint32_t)e >> 8) & 15u), vel = (uint32_t)e & 127u;
        if (mode == SMK_PM_STEPS) {
            if (vel)
                smk_step_press(n, vel);
            else
                smk_step_release(n);
        } else if (mode == SMK_PM_TRACKS && vel) {
            smk_tracks_press(n);
        }
    }
    if (mode == SMK_PM_STEPS && smk.held && smk.ptrk == song.sel && !smk_kbank) {   /* a step held + K1..K4: its lock */
        uint64_t h = smk_steps_held();
        for (f = 0; f < 4u; f++)
            if ((e = panel_enc(EN_K1 + f)) != 0) {
                lock_turn(f, e, h);
                smk.used = smk.held;
                ui.hot_col = (uint8_t)f;
                ui.hot_t = 40, ui.knob_ms = fm1_ms;
            }
    }
    {   /* the keybed's last note: a new melodic step's */
        static uint8_t last;
        if (last_note != last)
            smk.note = last = last_note;
    }
}

/* ---------------------------------------------------------------- the LEDs --- */
/* The buttons: one on/off LED each (hal/smk_input.h SMK_LEDC; SEQ PLAY's green its own id 16). The FM-1's picture
 * where the SMK can show it: lit = lit; what breathes on the FM-1 blinks (~1 Hz); no idle glow (as LEDS OFF; LEDS
 * INV: the idle ones lit). The 16 RGB pads (lcd.c padled_set) show what they do now: WHITE the FM-1 keys' picture
 * (lit white, breathing blinking, the DRUM grid's glow and beats dim); STEPS the page's steps (the track's colour,
 * brighter with something on it, the cursor white, the playhead bright, past LEN dark); TRACKS 1..6 the tracks in
 * their colours (the selected one bright, a muted one red, an asleep one dark), 7 UNDO and 8 TAP dim white, 9..16 the
 * shortcuts dim blue (the page shown bright). Untested on hardware. */
#define SMK_PAD_LIT 72u                                 /* the pads' levels (0..255 a channel: WS2812s are bright) */
#define SMK_PAD_MID 14u
#define SMK_PAD_DIM 3u
static void smk_pad_rgb(uint32_t hw, uint32_t rgb565, uint32_t lvl)   /* a colour at a level (0..SMK_PAD_LIT) */
{
    uint32_t r = (rgb565 >> 11) & 31u, g = (rgb565 >> 5) & 63u, b = rgb565 & 31u;
    padled_set(hw, (uint8_t)(r * lvl / 31u), (uint8_t)(g * lvl / 63u), (uint8_t)(b * lvl / 31u));
}
static void smk_pads_out(uint32_t mode, uint32_t blink)
{
    uint32_t p;
    if (mode == SMK_PM_WHITE) {
        uint32_t kb, c = key_leds(&kb), g = grid_on() && !ui.layer && !name_on() ? grid_glow() : 0u;
        uint32_t beat = g ? grid_beats() & ~c : 0u, lit = c, br = blink ? kb : 0u, inv = settings_leds == LEDS_INV;
        for (p = 0; p < SMK_NPAD; p++) {
            uint32_t bit = 1u << SMK_PK_GRID[p], v;
            v = lit & bit ? SMK_PAD_LIT : br & bit ? SMK_PAD_MID : beat & bit ? SMK_PAD_MID : g & bit ? SMK_PAD_DIM : 0u;
            if (inv && !(br & bit))
                v = lit & bit ? 0u : v ? v : SMK_PAD_MID;
            padled_set(p, (uint8_t)v, (uint8_t)v, (uint8_t)v);
        }
    } else if (mode == SMK_PM_STEPS) {
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
        for (p = 0; p < SMK_NPAD; p++) {
            uint32_t n = smk_pad_num(p);
            if (mode == SMK_PM_OFF)
                padled_set(p, 0, 0, 0);
            else if (n <= NTRK)
                n - 1u >= trk_live ? padled_set(p, 0, 0, 0) :
                trk[n - 1u].p[P_MUTE] ? padled_set(p, SMK_PAD_MID, 0, 0) :
                smk_pad_rgb(p, trk_col(n - 1u), n - 1u == song.sel ? SMK_PAD_LIT : SMK_PAD_MID);
            else if (n < SMK_PAD_PAGE0)
                padled_set(p, SMK_PAD_DIM, SMK_PAD_DIM, SMK_PAD_DIM);
            else
                padled_set(p, 0, 0, smk_page_shown(n) ? SMK_PAD_LIT : SMK_PAD_DIM * 2u);
        }
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
    fm1_led[SMK_BTN_KNOB_BANK] = smk_kbank;            /* (the banks' own: lit = MACROS, B) */
    fm1_led[SMK_BTN_FADER_BANK] = smk.fbank;
#undef SMK_LED
    smk_pads_out(smk.mode, blink);
}
