/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Felucca 1.2: the SEQ TOOLS layer's actions (ui_layer.c: SEQ held on STEP / the DRUM grid, PATTERN, CHANCE,
 * AUTOMATION). Included by ui_layer.c. The white keys from F3 (TL_*):
 *   F3 CLEAR  G3 REVERSE  A3 SHIFT <  B3 SHIFT >       the whole sequence
 *   F4 CLR AUTO                                        (JIANT 0.6.8) the track's automation (V10.6: C4 RANDOM D4 COOK
 *                                                      E4 BEAT and C5 the lane's RANDOM gone: SEQ > EUCLID generates)
 *   G4 CLEAR  A4 REVERSE  B4 FILL                      the selected lane (DRUM: ui.lane, black keys 1..8 pick it)
 * Each works on the selected track's steps 1..LEN (the steps past LEN stay); the track's automation stays (but
 * CLR AUTO's: it clears the track's automation and locks, every step, the steps stay). No undo
 * (TONIC): the song layer's RECALL brings the section back as it was stored.
 * A step that held something and is empty after loses its locks (as EDIT's step clear); REVERSE and SHIFT move the
 * steps' automation and locks with them. RANDOM and COOK draw from libc.c rng(): no seed is kept, each press
 * differs. A sequence of ties reversed keeps its note first: N T T -> N T T at the mirrored place */
enum { TL_CLEAR, TL_REV, TL_LEFT, TL_RIGHT, TL_RANDOM, TL_COOK, TL_BEAT, TL_CAUTO, TL_LCLR, TL_LREV, TL_LFILL, TL_LRND,
       TL_N };

/* the steps as they were just before an action (tl_take): what REVERSE and SHIFT read; (seq2d) in .bss again (it was
 * the pool, RAM's .bss tight): the pool's headroom paid for step_t's four bytes of components v2 (proj_scratch) */
#define tl_src (mscr.tl_src)          /* (seq2h) core.h mscr: one action's, the main loop's scratch */

static uint32_t tl_rnd(uint32_t n) { return n ? rng() % n : 0u; }
static int tl_gate(const step_t *s) { return s->time == ST_NOTE && s->n; }
static uint32_t tl_lane(const step_t *s, uint32_t l) { return (step_lanes_of(TSEL,s) >> l) & 1u; }
/* place p's action is there for this track (the lane row and BEAT: DRUM only) */
static int tl_cell(const track_t *t, uint32_t p)    /* (V10.6) RANDOM COOK BEAT and the lane's RANDOM gone: EUCLID */
{
    return p <= TL_CAUTO || (drum_track(t) && p >= TL_LCLR && p < TL_LRND);
}

/* the steps of t as they are now, before an action */
static void tl_take(const track_t *t) { memcpy(tl_src, t->step, sizeof tl_src); }

/* steps 1..len of t that are not empty (a REST); after the action the ones emptied lose their locks */
static uint64_t tl_used(const track_t *t, uint32_t len)
{
    uint64_t m = 0;
    uint32_t i;
    for (i = 0; i < len; i++)
        if (t->step[i].time != ST_REST)
            m |= (uint64_t)1 << i;
    return m;
}
static void tl_unlock(track_t *t, uint64_t was, uint32_t len)
{
    uint32_t i;
    for (i = 0; i < len; i++)
        if (((was >> i) & 1u) && t->step[i].time == ST_REST)
            (void)motion_clear_locks(t, i);
}

/* step j of t becomes the step src[j] as it was (tl_src), its automation and locks with it */
static void tl_order(track_t *t, uint32_t len, const uint8_t *src)
{
    uint8_t to[NSTEP];
    uint32_t j, k = trk_index(t), f;
    for (j = 0; j < len; j++) {
        t->step[j] = tl_src[src[j]];
        to[src[j]] = (uint8_t)j;
    }
    f = motion_guard();
    for (j = 0; j < motion.count; j++) {
        motion_event_t *e = &motion.event[j];
        if (MOTION_TRK(e) == k && MOTION_STEP(e) < len)
            e->place = (uint8_t)((e->place & 0xC0u) | to[MOTION_STEP(e)]);   /* (the track's bits kept, MOTION_TRK) */
    }
    motion_unguard(f);
}

/* REVERSE: the mirror order; a note's ties (after it) would come before it, so the note goes first again */
static void tl_reverse(track_t *t, uint32_t len)
{
    uint8_t src[NSTEP], h;
    uint32_t j, e, x;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)(len - 1u - j);
    for (j = 0; j < len; j++) {
        for (e = j; e < len && tl_src[src[e]].time == ST_TIE; e++)
            ;
        if (e == j)
            continue;
        if (e < len && tl_src[src[e]].time == ST_NOTE) {   /* ties j .. e - 1, their note at e: the note to j */
            for (h = src[e], x = e; x > j; x--)
                src[x] = src[x - 1u];
            src[j] = h;
        }
        j = e;                                          /* (past the run and its note) */
    }
    tl_order(t, len, src);
}

/* SHIFT: rotated by one step inside LEN, d = 1 later (>), len - 1 earlier (<) */
static void tl_shift(track_t *t, uint32_t len, uint32_t d)
{
    uint8_t src[NSTEP];
    uint32_t j;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)((j + len - d) % len);
    tl_order(t, len, src);
}

/* ------------------------------------------------------------- drums --- */
static void tl_lane_clear(track_t *t, uint32_t len, uint32_t l)
{
    uint32_t i;
    for (i = 0; i < len; i++)
        grid_hit(t, i, l, 0);
}
static uint32_t tl_fill(track_t *t, uint32_t len, uint32_t l)
{
    static const uint8_t E[3] = {1, 2, 4};
    uint32_t e, i, now = 0;
    for (e = 0; e < 3u && !now; e++) {
        for (i = 0; i < len && tl_lane(&t->step[i], l) == (i % E[e] == 0u); i++)
            ;
        if (i == len)
            now = e + 1u;
    }
    e = now == 3u ? 0u : E[now % 3u];
    for (i = 0; i < len; i++)
        grid_hit(t, i, l, e && i % e == 0u);
    return e;
}

/* ----------------------------------------------------------- melodic --- */
/* ------------------------------------------------------------ actions --- */
/* (V10.8.49) SEQ TOOLS' STRETCH x2 / SQUEEZE /2 / REPEAT x2; (V10.8.51) on the range held on SEQ's roll (SEL 5-8: cp_sel,
 * taken once) or else the whole track */
static uint32_t tl_reshape(track_t *t, int32_t k, uint32_t lo, uint32_t n, uint32_t len);
static const char *tl_shape(track_t *t, uint32_t p, uint32_t len, char *b)
{
    uint32_t s = (uint16_t)cp_sel, sel = s >> 15, lo = sel ? s & 63u : 0u, n = sel ? ((s >> 8) & 63u) - lo + 1u : len, nl;
    cp_sel = 0;
    if (lo + n > len) lo = 0, n = len, sel = 0;          /* (LEN shortened since: the whole track) */
    nl = tl_reshape(t, p == TL_BEAT ? 0 : p == TL_RANDOM ? 2 : -2, lo, n, len);
    if (!nl) { ui_message(p == TL_COOK ? "TOO SHORT" : "64 STEPS ALREADY"); return 0; }
    if (sel) {
        cp_say(p == TL_BEAT ? "REPEAT " : p == TL_RANDOM ? "STRETCH " : "SQUEEZE ", lo, n);
        return ui.msg;
    }
    str_cpy(b, "LEN ", 24); fmt_int(b + 4, (int32_t)len); str_cpy(b + str_len(b), " > ", 4);
    fmt_int(b + str_len(b), (int32_t)nl);
    return b;
}
/* (V10.8.49, the owner) SEQ TOOLS' C#5 / D#5: NUDGE, every note step's MICRO d 64ths of a step later (d < 0: earlier)
 * together, each held at MICRO's range (no step passes another); the first note's MICRO said */
static __attribute__((noinline)) const char *tl_nudge(track_t *t, int32_t d)
{
    static char b[16];
    uint32_t len = tl_len(t), i, n = 0, f = 0;
    int32_t m = 0, v;
    for (i = 0; i < len; i++) {
        step_t *s = &t->step[i];
        if (s->time != ST_NOTE)
            continue;
        v = clamp(ST_MICRO(s) + d, MICRO_MIN, MICRO_MAX);
        if (!f++) m = v;
        if (v != ST_MICRO(s)) { ST_MICRO_SET(s, v); n++; }
    }
    if (!n) { ui_message(f ? "NUDGE: AT ITS LIMIT" : "NO NOTES"); return 0; }
    str_cpy(b, m > 0 ? "NUDGE +" : "NUDGE ", sizeof b);
    fmt_int(b + str_len(b), m);
    str_cpy(b + str_len(b), "/64", 4);
    return b;
}
/* the action of place p on track t (lane l): its message; 0 = nothing done (said why) */
static __attribute__((noinline)) const char *tl_do(track_t *t, uint32_t p, uint32_t l)
{
    static char b[24];
    uint32_t len = tl_len(t), i, n = 0;
    uint64_t was = tl_used(t, len);
    const char *msg = 0;
    l %= NLANE;
    if (p == TL_CAUTO) {                                /* (JIANT 0.6.8) the automation and locks, every step */
        if (!motion_count(t)) {
            ui_message("NOTHING TO CLEAR");
            return 0;
        }
        motion_clear(t);
        return "AUTOMATION CLEARED";
    }
    tl_take(t);
    switch (p) {
    case TL_CLEAR: {                                     /* (0.6.28: names the track, ends its held / repeated notes) */
        static char cb[20];
        for (i = 0; i < len; i++)
            step_clear(&t->step[i]);
        latch_drop_req |= (uint8_t)(1u << trk_index(t));
        str_cpy(cb, "T1 CLEARED", sizeof cb);
        cb[1] = (char)('1' + trk_index(t));
        msg = cb;
        break;
    }
    case TL_REV: tl_reverse(t, len); msg = "REVERSED"; break;
    case TL_LEFT: tl_shift(t, len, len - 1u); msg = "SHIFTED LEFT"; break;
    case TL_RIGHT: tl_shift(t, len, 1u); msg = "SHIFTED RIGHT"; break;
    case TL_RANDOM: case TL_COOK: case TL_BEAT:         /* (V10.8.49, the owner) SEQ TOOLS: STRETCH x2, SQUEEZE /2, REPEAT x2 */
        return tl_shape(t, p, len, b);
    default:                                            /* the lane's */
        str_cpy(b, drum_lane_name(t, l), sizeof b);
        if (p == TL_LCLR) {
            tl_lane_clear(t, len, l);
            msg = " CLEARED";
        } else if (p == TL_LREV) {
            for (i = 0; i < len; i++) {
                const step_t *s = &tl_src[len - 1u - i];
                grid_hit(t, i, l, tl_lane(s, l));
                if (tl_lane(s, l))
                    grid_acc(t, i, l, (step_accents_of(TSEL,s) >> l) & 1u);
            }
            msg = " REVERSED";
        } else {
            n = tl_fill(t, len, l);
            msg = n == 1u ? " EVERY STEP" : n == 2u ? " EVERY 2ND" : n ? " EVERY 4TH" : " CLEARED";
        }
        str_cpy(b + str_len(b), msg, sizeof b - str_len(b));
        msg = b;
        break;
    }
    tl_unlock(t, was, len);
    return msg;
}

/* ------------------------------------------------- (0.6.85) SEQ > STRETCH --- */
/* (V10.8.51) steps lo .. lo+n-1 of the selected track (tl_src: the steps before), the steps after them moved along, LEN
 * with them (64 at most: what falls past is cut), their automation and locks with their steps:
 *   k 2, STRETCH x2: step i to step 2i; a note held into the next step (a TIE after it) holds over the new one
 *   k -2, SQUEEZE /2: step j takes the first that plays of 2j, 2j+1 (a TIE if the note before holds on)
 *   k 0, REPEAT x2: the steps again right after them, their automation and locks copied while there is room
 * Returns the new LEN (0: nothing to do) */
BTAPP_FN static uint32_t tl_reshape(track_t *t, int32_t k, uint32_t lo, uint32_t n, uint32_t len)
{
    uint32_t nn = k < 0 ? (n + 1u) / 2u : n * 2u, nlen = len - n + nn, i, j, c, f, tk = trk_index(t);
    if (nlen > NSTEP) nlen = NSTEP;
    if (nlen == len) return 0;
    undo_mark();                                        /* (V10.8.51: one UNDO point) */
    for (j = lo; j < (len > nlen ? len : nlen); j++) step_clear(&t->step[j]);   /* (past both LENs: as they were) */
    for (i = lo + n; i < len; i++)                      /* the steps after the range */
        if (i - n + nn < NSTEP) t->step[i - n + nn] = tl_src[i];
    for (i = 0; i < nn && lo + i < NSTEP; i++) {
        step_t *d = &t->step[lo + i];
        if (k > 0) {                                    /* STRETCH: the even ones */
            const step_t *s = &tl_src[lo + i / 2u];
            if (!(i & 1u)) *d = *s;
            else if (s->time == ST_TIE || (s->time == ST_NOTE && s->n && lo + i / 2u + 1u < len &&
                                           tl_src[lo + i / 2u + 1u].time == ST_TIE))
                d->time = ST_TIE, d->vel = s->vel;
        } else if (!k)                                  /* REPEAT */
            *d = tl_src[lo + i % n];
        else {                                          /* SQUEEZE */
            uint32_t a = lo + 2u * i, tie = 0;
            for (j = a; j < a + 2u && j < lo + n; j++) {
                if (tl_src[j].time == ST_NOTE && (tl_src[j].n || tl_src[j].hit)) break;
                tie |= tl_src[j].time == ST_TIE;
            }
            if (j < a + 2u && j < lo + n) *d = tl_src[j];
            else if (tie && lo + i) d->time = ST_TIE, d->vel = tl_src[a].vel;
        }
    }
    f = motion_guard();
    for (i = c = 0; i < motion.count; i++) {
        motion_event_t e = motion.event[i];
        uint32_t st = MOTION_STEP(&e);
        if (MOTION_TRK(&e) == tk) {
            if (st >= len) { if (st < nlen) continue; }  /* (its step covered now) */
            else if (st >= lo + n) st = st - n + nn;
            else if (st >= lo) st = k < 0 ? lo + (st - lo) / 2u : k ? lo + (st - lo) * 2u : st;
            if (st >= nlen) continue;                   /* (cut with its step) */
            e.place = (uint8_t)((e.place & 0xC0u) | st);
        }
        motion.event[c++] = e;
    }
    memset(motion.event + c, 0, (MOTION_MAX - c) * sizeof motion.event[0]);
    motion.count = (uint8_t)c;
    for (i = 0; !k && i < c; i++) {                     /* REPEAT: the range's on the copy */
        motion_event_t e = motion.event[i];
        uint32_t st = MOTION_STEP(&e);
        if (MOTION_TRK(&e) != tk || st < lo || st >= lo + n || st + n >= nlen || motion.count >= MOTION_MAX) continue;
        e.place = (uint8_t)((e.place & 0xC0u) | (st + n));
        motion.event[motion.count++] = e;
    }
    RING_PUBLISH();
    motion_unguard(f);
    t->p[P_SLEN] = (int16_t)nlen;
    return nlen;
}

