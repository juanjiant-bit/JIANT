/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 JIANT port: Copyright (C) 2026 Israel Peña (pedeplei1281) */
/* SMK-37 (smk37-port.yaml pads, faders): what the 16 pads are, frame by frame, and the four faders' banks. Included by
 * ui_input.c (BOARD_SMK37 only). The pads, in reading order (1-8 the top row, 9-16 the bottom row):
 *  - on every SEQ page (smk_pad_raw SMK_PADS_STEPS): the 16 steps of the bar shown (< > pick the bar). On a DRUM track a
 *    tap toggles the selected lane's hit there (its accent while ACC is held) as the FM-1 grid's white keys do, and plays
 *    the lane; on another track a tap on an empty step writes the last note played there, a tap on a filled step clears
 *    it when let go, and the step's notes play either way. A step held + KNOB 1..4 (K5..K8) locks, as a held step on the
 *    FM-1 (ui_input.c lock_held), and a step held for a lock keeps what it had.
 *  - KNOB BANK held on a SEQ page, and every other page (SMK_PADS_DRUMS): T1's drum pads (seq.c smk_drum_pads_block).
 *  - a quick layer, NAME, LIVE MANO (SMK_PADS_KEYS): the white keys F3..G5, as before (their tiles).
 * The LEDs (smk_pad_leds): the screen's colour; SEQ: a step with a hit / a note lit, the playhead full, the rest dark; the
 * drum pads dim, full while hit. */

/* a step pad held: its step (+ 1), its track and the notes it plays */
static struct { uint8_t i, t, n, note[4]; } smk_sp[SMK_NPAD];

static void smk_steps_release_all(void);
/* each frame (main.c smk_grid, before the LEDs): the pads' mode */
static void smk_pads_mode(void)
{
    uint32_t m;
    int kb = ((fm1_in.buttons >> SMK_BTN_KNOB_BANK) & 1u) && smk_btn_free(SMK_BTN_KNOB_BANK);
    if (ui.layer || (name_on() && !ui.menu) || song.grid >= 2u || live_mano())
        m = SMK_PADS_KEYS;
    else if (song.seq_mode && !ui.menu && !ui.confirm && !kb)
        m = SMK_PADS_STEPS;
    else
        m = SMK_PADS_DRUMS;
    if (m != smk_pad_raw) {
        if (smk_pad_raw == SMK_PADS_STEPS)
            smk_steps_release_all();                    /* (their releases still come: nothing left to do then) */
        smk_pad_raw = (uint8_t)m;
    }
}

static void smk_sp_note(uint32_t p, uint32_t note, uint32_t vel)
{
    if (smk_sp[p].n < 4u) {
        smk_sp[p].note[smk_sp[p].n++] = (uint8_t)note;
        smk_play(smk_sp[p].t, note, vel);
    }
}

/* step pad p pressed at velocity v: the step of the bar shown */
static void smk_step_press(uint32_t p, uint32_t v)
{
    track_t *t = TSEL;
    uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i = ui.bank * 16u + p, j;
    uint64_t bit;
    step_t *st;
    if (i >= len)
        return;                                         /* past LEN: no step there */
    if (chain_busy()) {
        ui_message("STOP TO EDIT");
        return;
    }
    st = &t->step[i];
    bit = (uint64_t)1 << i;
    smk_sp[p].i = (uint8_t)(i + 1u);
    smk_sp[p].t = (uint8_t)(song.sel % NTRK);
    smk_sp[p].n = 0;
    smk_steps_held |= bit;
    if (drum_track(t)) {                                /* the FM-1 grid's white key (ui_input.c grid_keys) */
        uint32_t acc = (uint32_t)black_held(GK_ACC);
        if (((acc ? step_accents(st) : step_lanes(st)) >> ui.lane) & 1u) {
            ui.plk_rm |= bit;                           /* (off when let go, unless held for a lock: lock_keys) */
            ui.plk_acc = acc ? ui.plk_acc | bit : ui.plk_acc & ~bit;
            ui.plk_lane = ui.lane;
            ui.plk_trk = song.sel;
        } else if (acc) {
            grid_acc(t, i, ui.lane, 1);
        } else {
            grid_hit(t, i, ui.lane, 1);
        }
        smk_sp_note(p, DRUM_LANE_NOTE[ui.lane % NLANE], v);
    } else if (st->time == ST_NOTE && st->n) {          /* a filled step: it plays, and goes when let go */
        smk_step_rm |= bit;
        for (j = 0; j < st->n && j < 4u; j++)
            smk_sp_note(p, st->note[j], v);
    } else {                                            /* an empty step (a REST, a TIE): the last note played */
        step_clear(st);
        st->time = ST_NOTE;
        st->n = 1;
        st->note[0] = last_note;
        st->vel = (uint8_t)v;
        smk_sp_note(p, last_note, v);
    }
    cursor_set((int32_t)i);
    ui.force = 1;
}

static void smk_step_release(uint32_t p)
{
    uint32_t i, j;
    uint64_t bit;
    if (!smk_sp[p].i)
        return;
    i = smk_sp[p].i - 1u;
    bit = (uint64_t)1 << i;
    for (j = 0; j < smk_sp[p].n; j++)
        smk_play(smk_sp[p].t, smk_sp[p].note[j], 0);
    smk_steps_held &= ~bit;
    if (smk_step_rm & bit) {                            /* a filled step tapped (not held for a lock): cleared */
        track_t *t = &trk[smk_sp[p].t % NTRK];
        smk_step_rm &= ~bit;
        if (!drum_track(t) && !chain_busy()) {
            step_clear(&t->step[i]);
            ui.force = 1;
        }
    }
    smk_sp[p].i = 0;
    smk_sp[p].n = 0;
}

static void smk_steps_release_all(void)
{
    uint32_t p;
    for (p = 0; p < SMK_NPAD; p++)
        if (smk_sp[p].i) {
            smk_step_rm &= ~((uint64_t)1 << (smk_sp[p].i - 1u));   /* (left as it was) */
            smk_step_release(p);
        }
    smk_steps_held = 0;
}

/* each frame (ui_input_frame): the step pads' presses and releases */
static void smk_steps_frame(void)
{
    int32_t e;
    while ((e = smk_pad_take(0)) >= 0) {
        uint32_t p = ((uint32_t)e >> 8) & 15u, v = (uint32_t)e & 0x7Fu;
        if (v && smk_pad_raw == SMK_PADS_STEPS)
            smk_step_press(p, smk_vel(v));
        else if (!v)
            smk_step_release(p);
    }
}

/* the pads' LEDs in the STEPS and DRUMS modes, colour r g b (0..255) at level 0..256 */
static void smk_pad_leds(uint32_t r, uint32_t g, uint32_t b)
{
    uint32_t p;
    for (p = 0; p < SMK_NPAD; p++) {
        uint32_t lv = 0;
        if (smk_pad_raw == SMK_PADS_STEPS) {
            const track_t *t = TSEL;
            uint32_t len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), i = ui.bank * 16u + p, src;
            if (i < len) {
                const step_t *s = &seq_steps(t)[i];
                uint32_t acc = (uint32_t)black_held(GK_ACC), on;
                src = seq_src(t, t->seq_idx);
                on = drum_track(t) ? (((acc ? step_accents(s) : step_lanes(s)) >> ui.lane) & 1u)
                                   : s->time == ST_NOTE ? s->n != 0u : 0u;
                lv = song.playing && src == i ? 256u : smk_sp[p].i ? 256u : on ? 96u : s->time == ST_TIE ? 24u : 0u;
            }
        } else {
            lv = smk_dp_note[p] ? 256u : 24u;           /* the drum pads: dim, full while hit */
        }
        padled_set(p, (uint8_t)(r * lv >> 8), (uint8_t)(g * lv >> 8), (uint8_t)(b * lv >> 8));
    }
}

/* each frame (ui_input_frame): MENU > MIDI > KEYBOARD's settings to the audio side and the TRS jack */
static void smk_cfg_poll(void)
{
    uint32_t kv = smk_cfg & 7u;
    smk_kvel_mode = (uint8_t)(kv <= 5u ? kv : 0u);
    smk_bend_semis = (uint8_t)(smk_bendb && smk_bendb <= 12u ? smk_bendb : 2u);
    smk_trs_set(!(smk_cfg & SMK_CFG_TRS_OFF));
    {
        uint32_t t;
        for (t = 0; t < NTRK; t++)                      /* MENU > MIDI > CHANNELS (seq.c smk_play_block takes them) */
            smk_mch_req[t] = smk_mch_set(t);
    }
}

/* ---------------------------------------------------------------- faders --- */
/* FADER BANK (smk37-port.yaml faders): A the levels of T1..T4; B T5's and T6's levels, the reverb's return (fx.c
 * smk_rev_ret, SMK only, full at power-on) and the delay's (GLO > DLY MIX); C the selected track's filter cutoff (ANALOG's CUT, the
 * other engines' part filter), DIST, REV send and DLY send. A fader acts only once it moves (also after a bank change):
 * nothing jumps. A move names what it set and its value on the top bar */
enum { SMK_FB_A, SMK_FB_B, SMK_FB_C, SMK_FB_N };
static int16_t *smk_fader_target(uint32_t f, const param_desc_t **d, const char **name)
{
    static const char *const LV[NTRK] = {"T1 LEVEL ", "T2 LEVEL ", "T3 LEVEL ", "T4 LEVEL ", "T5 LEVEL ", "T6 LEVEL "};
    track_t *t;
    uint32_t id;
    if (smk_fbank == SMK_FB_A || (smk_fbank == SMK_FB_B && f < 2u)) {
        uint32_t ti = smk_fbank == SMK_FB_A ? f : 4u + f;
        if (ti >= NTRK)
            return 0;
        *d = track_desc(&trk[ti], P_LEVEL);
        *name = LV[ti];
        return &trk[ti].p[P_LEVEL];
    }
    if (smk_fbank == SMK_FB_B) {
        if (f == 2u) {                                  /* the reverb's return (fx.c smk_rev_ret: SMK only, not stored) */
            static const param_desc_t REVRET = {"REV RET", F_PCT, 0, 127, 127, 0, 0};
            *d = &REVRET;
            *name = "REV RETURN ";
            return &smk_rev_ret;
        }
        *d = &GP[G_DMIX];
        *name = "DLY MIX ";
        return &song.g[G_DMIX];
    }
    t = TSEL;
    id = f == 0u ? (ENGINES[eng_idx(t->eng_req)] == &ENG_ANALOG ? P_E4 : P_FCUT) : f == 1u ? P_DIST : f == 2u ? P_REV : P_DLY;
    *d = track_desc(t, id);
    *name = f == 0u ? "CUT " : f == 1u ? "DIST " : f == 2u ? "REV " : "DLY ";
    return &t->p[id];
}

/* each frame (main.c) */
static void smk_faders(void)
{
    static uint8_t last[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint32_t i;
    if (smk_fbank_new) {                                /* a new bank: wait for each fader to move */
        smk_fbank_new = 0;
        for (i = 0; i < 4u; i++)
            last[i] = 0xFF;
    }
    for (i = 0; i < 4u; i++) {
        uint32_t v = fm1_in.fader[i];
        const param_desc_t *d = 0;
        const char *name = "";
        int16_t *pv;
        char num[8];
        if (last[i] == 0xFF || fm1_in.frames < 200u) {  /* (the first readings settle) */
            last[i] = (uint8_t)v;
            continue;
        }
        if (v == last[i])
            continue;
        last[i] = (uint8_t)v;
        pv = smk_fader_target(i, &d, &name);
        if (!pv || !d)
            continue;
        *pv = (int16_t)(d->min + ((int32_t)v * (d->max - d->min) + 63) / 127);
        fmt_int(num, *pv);
        ui_say(name, num);
        ui.force = 1;
    }
}
