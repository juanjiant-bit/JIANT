/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Felucca 1.1.5: SEQ > AUTO LIST, the selected track's parameter locks and automation events as a list (included by
 * ui.c). (V10.7.32, the owner) LOCKS: AUTO LIST and AUTOMATION one page. Its top row is AUTOMATION's: whether the records
 * play (KNOB 2 PLAY ON / OFF), how many LOCK and REC there are, OCT+ CLEAR ALL (the dialog); then a row a record: its
 * kind (LOCK: a parameter lock, MOTION_LOCK; REC: recorded motion, an automation event; RAMP: a COMP RAMP's), its step,
 * its parameter, its value; in the order of the steps, then of the parameters; the last row is + ADD LOCK.
 *   KNOB 1 (or the PRESETS knob) the row (the top first)
 *   KNOB 2 the step: the record moves to the next step where its parameter has no record (1 .. LEN)
 *   KNOB 3 the parameter: any one motion can record (motion.c motion_param) that the track's sound has, by name; it
 *          takes the next one with no record on that step, its value the sound's own (a lock that changes nothing yet)
 *   KNOB 4 the value
 *   OCT+   on + ADD LOCK: a lock on the step and parameter KNOB 2 / 3 set there (the SEQ cursor's step and the first
 *          parameter of the sound page shown last, to begin with), at the sound's own value; on a record: its kind
 *          LOCK <-> REC (an automation event holds its value until the next one, a lock sounds on its step only)
 *   SEQ + G5 deletes the record;  OCT- goes HOME (as on the other action pages)
 * Not while a song plays
 * (STOP TO EDIT). The 64 records are shared by the four tracks: AUTOMATION FULL when they are used */
static int32_t accel(uint32_t role, int32_t s, int32_t range);

/* id has a row's name on track t: motion records it, and the track's sound has it (DRUM's lane levels on a DRUM
 * track, the engine's own E1..E8 that it labels; not DIGITAL's operators, retired) */
static int ev_id_ok(const track_t *t, uint32_t id)
{
    const param_desc_t *d;
    if (id == MOT_DJF)                                  /* (0.9.1) the DJ FILTER's records: listed (GLO > MIXER records them) */
        return 1;
    if (id >= P_COUNT || !motion_param(id) || (id >= P_FM1_ATK && id <= P_FM4_LEVEL && id != P_TCHAN && id != P_MORPH && id != P_CLEN && id != P_DXIMG &&
                                                              id != P_HVEL && id != P_HTIM && id != P_HGAT && id != P_HRQ && id != P_TVEL))
        return 0;
    if (id >= P_LN0 && id <= P_LN7 && !(id >= P_FKEY && id <= P_FDRV && !drum_track(t) &&
                                        ENGINES[eng_idx(t->eng_req)] != &ENG_INPUT))   /* (filters2: a synth's FILTER 2) */
        return drum_track(t);
    d = track_desc(t, id);
    return d && d->label && d->label[0] && d->label[0] != '-' && d->max > d->min;
}
/* its name in the list and on the card (b holds 12): the page's word before the card's label where the label alone
 * would say two things (ENV ATK, ENV FLT: ENV DEST's, LFO FLT: LFO DEST's) */
static void ev_name(const track_t *t, uint32_t id, char *b)
{
    const char *pre = id >= P_ATK && id <= P_ED_SHP ? "ENV " : id >= P_LRATE && id <= P_LD_AMP ? "LFO " : "";
    str_cpy(b, pre, 12);
    str_cpy(b + str_len(b), id == P_LEVEL ? "LEVEL" : id == P_GLIDE ? "GLIDE" : track_desc(t, id)->label, 12 - str_len(b));
}
/* the next parameter after id in direction dir (+1 / -1) that is free on track t's step (a record there: skipped);
 * id itself when there is none */
static uint32_t ev_id_step(const track_t *t, uint32_t step, uint32_t id, int32_t dir)
{
    int32_t i = (int32_t)id;
    for (;;) {
        i += dir;
        if (i < 0 || i >= (int32_t)P_COUNT)
            return id;
        if (ev_id_ok(t, (uint32_t)i) && (step >= NSTEP || motion_find(t, step, (uint32_t)i) < 0))
            return (uint32_t)i;
    }
}
static uint32_t ev_len(const track_t *t) { return (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); }

/* the rows of the selected track (idx[MOTION_MAX]): their count; the row selected, inside 0..count */
static uint32_t ev_rows(uint8_t *idx) { return motion_rows(song.sel, idx); }
static uint32_t ev_row(uint32_t n) { return ui.ev_row < n ? ui.ev_row : n; }

/* the page opened, or the track or its sound changed: + ADD LOCK's step and parameter inside what the track has */
static void ev_fix(void)
{
    const track_t *t = TSEL;
    if (ui.ev_step >= ev_len(t))
        ui.ev_step = (uint8_t)(ev_len(t) - 1u);
    if (!ev_id_ok(t, ui.ev_id)) {
        uint32_t id = lock_id(0);
        ui.ev_id = (uint8_t)(id != 0xFFu && ev_id_ok(t, id) ? id : ev_id_step(t, NSTEP, (uint32_t)-1, 1));
    }
}
static void ev_enter(void)
{
    ui.ev_top = 1;                                      /* (V10.7.32: LOCKS opens on its top row) */
    ui.ev_step = (uint8_t)ui.cursor;
    ui.ev_id = 0xFFu;
    ev_fix();
}

/* the row showing record i (after a move it may sit elsewhere in the order) */
static void ev_follow(uint32_t i)
{
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), r;
    for (r = 0; r < n && idx[r] != i; r++)
        ;
    ui.ev_row = (uint8_t)r;
}

static const char *ev_act_name(void)
{
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), r = ev_row(n);
    if (ui.ev_top)
        return "CLEAR";
    if (r == n)
        return "ADD";
    return (motion.event[idx[r]].param & MOTION_LOCK) ? "REC" : "LOCK";   /* what OCT+ turns it into */
}
static int ev_act_ready(void)
{
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx);
    if (chain_busy())
        return 0;
    if (ui.ev_top)
        return n != 0u;
    return ev_row(n) < n || (motion.count < MOTION_MAX && ev_id_ok(TSEL, ui.ev_id));
}

static int ev_busy(void)
{
    if (!chain_busy())
        return 0;
    ui_message("SONG PLAYS: STOP IT TO EDIT");
    return 1;
}

/* KNOB slot turned s on AUTO LIST */
static __attribute__((noinline)) void ev_knob(uint32_t slot, int32_t s)
{
    track_t *t = TSEL;
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), r = ev_row(n), i, step, id;
    const motion_event_t *e;
    const param_desc_t *d;
    if (slot == 0u) {                                   /* (the top row above the first record) */
        int32_t at = clamp((ui.ev_top ? -1 : (int32_t)r) + s, -1, (int32_t)n);
        ui.ev_top = at < 0;
        if (at >= 0)
            ui.ev_row = (uint8_t)at;
        return;
    }
    if (ev_busy())
        return;
    if (ui.ev_top) {                                    /* the top row: KNOB 2 PLAY (AUTOMATION's) */
        if (slot == 1u)
            motion_set_enabled(t, s > 0);
        return;
    }
    if (r == n) {                                       /* + ADD LOCK: where it goes */
        ev_fix();
        if (slot == 1u)
            ui.ev_step = (uint8_t)clamp((int32_t)ui.ev_step + s, 0, (int32_t)ev_len(t) - 1);
        else if (slot == 2u)
            for (i = (uint32_t)(s > 0 ? s : -s); i; i--)
                ui.ev_id = (uint8_t)ev_id_step(t, NSTEP, ui.ev_id, s > 0 ? 1 : -1);
        return;
    }
    i = idx[r];
    e = &motion.event[i];
    step = MOTION_STEP(e);
    id = MOTION_ID(e);
    if (slot == 1u) {                                   /* the next step where its parameter is free */
        uint32_t top = ev_len(t) > step ? ev_len(t) : step + 1u, m = (uint32_t)(s > 0 ? s : -s);
        int32_t to = (int32_t)step, at = (int32_t)step;
        while (m--) {                                   /* a detent each: the next free step */
            do
                at += s > 0 ? 1 : -1;
            while (at >= 0 && at < (int32_t)top && motion_find(t, (uint32_t)at, id) >= 0);
            if (at < 0 || at >= (int32_t)top)
                break;
            to = at;
        }
        if (to == (int32_t)step)
            return;
        (void)motion_move(t, i, (uint32_t)to, id, MOTION_VAL(e));
    } else if (slot == 2u) {                            /* another parameter, from the sound's own value */
        uint32_t to = id, m = (uint32_t)(s > 0 ? s : -s);
        while (m--)                                     /* a detent each: the next free parameter */
            to = ev_id_step(t, step, to, s > 0 ? 1 : -1);
        if (to == id)
            return;
        (void)motion_move(t, i, step, to, motion_base_value(t, to));
        ui.ev_id = (uint8_t)to;
    } else {                                            /* the value */
        d = track_desc(t, id);
        s = accel(EN_K1 + slot, s, d->fmt == F_ENUM ? 0 : d->max - d->min);
        (void)motion_move(t, i, step, id, (int16_t)param_turn(d, MOTION_VAL(e), s));
    }
    ev_follow(i);
}

/* OCT+: add a lock on + ADD LOCK, else the record's kind LOCK <-> AUTO */
static void ev_oct(void)
{
    track_t *t = TSEL;
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), r = ev_row(n);
    int32_t at;
    if (ev_busy())
        return;
    if (ui.ev_top) {                                    /* the top row: CLEAR ALL (AUTOMATION's dialog) */
        if (n)
            confirm_open(CF_CLEAR_MOTION, song.sel);
        else
            ui_message("NOTHING TO CLEAR");
        return;
    }
    if (r == n) {
        ev_fix();
        if (!ev_id_ok(t, ui.ev_id))
            return;
        if ((at = motion_find(t, ui.ev_step, ui.ev_id)) >= 0) {   /* one is there already: show it */
            ev_follow((uint32_t)at);
            ui_message("ALREADY LISTED");
            return;
        }
        if (motion.count >= MOTION_MAX) {
            ui_message("AUTOMATION FULL");
            return;
        }
        (void)motion_set_lock(t, ui.ev_step, ui.ev_id, motion_base_value(t, ui.ev_id));
        if ((at = motion_find(t, ui.ev_step, ui.ev_id)) >= 0)
            ev_follow((uint32_t)at);
        ui_message("LOCK ADDED");
        return;
    }
    {
        const motion_event_t e = motion.event[idx[r]];
        uint32_t lock = !(e.param & MOTION_LOCK);
        if (motion_put(t, MOTION_STEP(&e), MOTION_ID(&e), MOTION_VAL(&e), lock ? MOTION_LOCK : 0u))
            ui_message("AUTOMATION ONLY");              /* (0.9.1: the DJ FILTER's, no lock) */
        else
            ui_message(lock ? "NOW A LOCK" : "NOW REC");
    }
}

/* SEQ + G5: the record goes (the row below moves up) */
static void ev_delete(void)
{
    track_t *t = TSEL;
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), r = ev_row(n);
    const motion_event_t *e;
    if (r == n || ui.ev_top) {
        ui_message("NOTHING TO DELETE");
        return;
    }
    if (ev_busy())
        return;
    e = &motion.event[idx[r]];
    motion_delete_event(t, MOTION_STEP(e), MOTION_ID(e));
    ui.ev_row = (uint8_t)r;                              /* (ev_row clamps it to the new count) */
    ui_message("DELETED");
}

/* (V10.7.32) LOCKS' panel: the top row (PLAY, the LOCK and REC counts, OCT+ CLEAR ALL) an outline card, then the rows a
 * record each (its kind's tag, its step, its parameter, its value) and + ADD LOCK, eight at a time round the one picked
 * (TINT, framed THEME; the top's frame TEXT) */
static void card_line(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint16_t c, int32_t lw);   /* (ui_draw.c) */
#define LK_ROWS 8u
static void graph_locks(const track_t *t)
{
    uint8_t idx[MOTION_MAX];
    uint32_t n = ev_rows(idx), r = ev_row(n), nl = motion_lock_count(t), i, first;
    char b[32], v[16], nm[12];
    const char *u = "";
    card_line(4, 2, 232, 17, 3, ui.ev_top ? T_TEXT : T_LINE, 1);
    str_cpy(b, motion_enabled(t) ? "PLAY ON \xB7 " : "PLAY OFF \xB7 ", sizeof b);
    fmt_int(b + str_len(b), (int32_t)nl);
    str_cpy(b + str_len(b), " LOCK \xB7 ", sizeof b - str_len(b));
    fmt_int(b + str_len(b), (int32_t)(n - nl));
    str_cpy(b + str_len(b), " REC", sizeof b - str_len(b));
    GFX_HOOK_ALIGN(0, 2, 0, 19, AL_V | AL_PASS, "locks top row");
    cv_text(10, 2 + CAP_IN(X, 17), &AF_X, b, motion_enabled(t) ? T_TEXT : T_MID);
    GFX_HOOK_ALIGN(0, 2, 0, 19, AL_V | AL_PASS, "locks top row");
    cv_text_r(230, 2 + CAP_IN(X, 17), &AF_X, "OCT+ CLEAR", ui.ev_top && n ? T_ACCENT : T_DIM, T_BG);
    first = r + 4u > LK_ROWS ? r + 4u - LK_ROWS : 0u;   /* (the one picked in view, three after it when there are) */
    if (first + LK_ROWS > n + 1u)
        first = n + 1u > LK_ROWS ? n + 1u - LK_ROWS : 0u;
    for (i = first; i <= n && i < first + LK_ROWS; i++) {
        int32_t y = 24 + (int32_t)(i - first) * 14;
        int sel = !ui.ev_top && i == r;
        uint16_t bg = sel ? T_TINT : T_BG, tc;
        uint32_t step, id;
        int16_t val;
        const char *tag;
        if (sel) {
            cv_rect(4, y, 232, 13, T_TINT);
            cv_frame(4, y, 232, 13, T_THEME);
        }
        if (i == n) {                                   /* + ADD LOCK: on the step and parameter KNOB 2 / 3 set */
            ev_fix();
            tag = "+ ADD";
            step = ui.ev_step;
            id = ui.ev_id;
            val = id < P_COUNT ? motion_base_value(t, id) : 0;
            tc = T_THEME;
        } else {
            const motion_event_t *e = &motion.event[idx[i]];
            step = MOTION_STEP(e);
            id = MOTION_ID(e);
            val = MOTION_VAL(e);
            tag = (e->param & MOTION_LOCK) ? "LOCK" : MOTION_RAMP(e) ? "RAMP" : "REC";
            tc = (e->param & MOTION_LOCK) ? T_ACCENT : T_TEXT;
        }
        GFX_HOOK_ALIGN(0, y, 0, y + 13, AL_V | AL_PASS, "locks row");
        cv_text_on(9, y + CAP_IN(X, 13), &AF_X, tag, tc, bg);
        b[0] = 'S';
        fmt_int(b + 1, (int32_t)step + 1);
        GFX_HOOK_ALIGN(0, y, 0, y + 13, AL_V | AL_PASS, "locks row");
        cv_text_on(48, y + CAP_IN(X, 13), &AF_X, b, i == n ? T_MID : T_TEXT, bg);
        if ((id >= P_COUNT && id != MOT_DJF) || !ev_id_ok(t, id))
            continue;
        ev_name(t, id, nm);
        param_format(track_desc(t, id), val, v, &u);
        str_cpy(v + str_len(v), u, sizeof v - str_len(v));
        GFX_HOOK_ALIGN(0, y, 0, y + 13, AL_V | AL_PASS, "locks row");
        cv_free_text(78, y + CAP_IN(X, 13), &AF_X, nm, i == n ? T_MID : T_TEXT, bg, 96);
        GFX_HOOK_ALIGN(0, y, 0, y + 13, AL_V | AL_PASS, "locks row");
        cv_text_r(230, y + CAP_IN(X, 13), &AF_X, v, i == n ? T_MID : T_TEXT, bg);
    }
}
