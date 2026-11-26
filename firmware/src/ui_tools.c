/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Felucca 1.2: the SEQ TOOLS layer's actions (ui_layer.c: SEQ held on STEP / the DRUM grid, PATTERN, CHANCE,
 * AUTOMATION). Included by ui_layer.c. The white keys from F3 (TL_*):
 *   F3 CLEAR  G3 REVERSE  A3 SHIFT <  B3 SHIFT >       the whole sequence
 *   F4 CLR AUTO                                        (JIANT 0.6.8) the track's automation (V10.6: C4 RANDOM D4 COOK
 *                                                      E4 BEAT and C5 the lane's RANDOM gone: SEQ > EUCLID generates)
 *   G4 CLEAR  A4 REVERSE  B4 FILL                      the selected lane (DRUM: ui.lane, black keys 1..8 pick it)
 * (V10.8.52: the SEQ EDIT page, se_do below, does them on the steps selected; the keys F3 CLEAR, F4 CLR AUTO, G4-B4 the
 * lane's, C5 DUP, E5 COPY, F5 PASTE, G5 CLR STEP stay; G3 ALL, A3 KNOB 1 / 2 MOVE, B3-E4 BAR 1-4, D5 DO IT.)
 * Each works on the selected track's steps 1..LEN (the steps past LEN stay); the track's automation stays (but
 * CLR AUTO's: it clears the track's automation and locks, every step, the steps stay). No undo
 * (TONIC): the song layer's RECALL brings the section back as it was stored.
 * A step that held something and is empty after loses its locks (as EDIT's step clear); REVERSE and SHIFT move the
 * steps' automation and locks with them. RANDOM and COOK draw from libc.c rng(): no seed is kept, each press
 * differs. A sequence of ties reversed keeps its note first: N T T -> N T T at the mirrored place */
enum { TL_CLEAR, TL_REV, TL_LEFT, TL_RIGHT, TL_RANDOM, TL_COOK, TL_BEAT, TL_CAUTO, TL_LCLR, TL_LREV, TL_LFILL, TL_LRND,
       TL_N };
#define TL_DUP 11u                                      /* (V10.8.52 SEQ EDIT's white keys: C5 DUP, E5 COPY, F5 PASTE) */
#define TL_COPY 13u
#define TL_PASTE 14u

/* the steps as they were just before an action (tl_take): what REVERSE and SHIFT read; (seq2d) in .bss again (it was
 * the pool, RAM's .bss tight): the pool's headroom paid for step_t's four bytes of components v2 (proj_scratch) */
#define tl_src (mscr.tl_src)          /* (seq2h) core.h mscr: one action's, the main loop's scratch */

static uint32_t tl_rnd(uint32_t n) { return n ? rng() % n : 0u; }
static int tl_gate(const step_t *s) { return s->time == ST_NOTE && s->n; }
static uint32_t tl_lane(const step_t *s, uint32_t l) { return (step_lanes_of(TSEL,s) >> l) & 1u; }
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

/* REVERSE: steps lo .. lo+n-1 in the mirror order; a note's ties (after it) would come before it, so the note goes
 * first again */
static void tl_reverse(track_t *t, uint32_t lo, uint32_t n, uint32_t len)
{
    uint8_t src[NSTEP], h;
    uint32_t j, e, x;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)(j < lo || j >= lo + n ? j : 2u * lo + n - 1u - j);
    for (j = lo; j < lo + n; j++) {
        for (e = j; e < lo + n && tl_src[src[e]].time == ST_TIE; e++)
            ;
        if (e == j)
            continue;
        if (e < lo + n && tl_src[src[e]].time == ST_NOTE) {   /* ties j .. e - 1, their note at e: the note to j */
            for (h = src[e], x = e; x > j; x--)
                src[x] = src[x - 1u];
            src[j] = h;
        }
        j = e;                                          /* (past the run and its note) */
    }
    tl_order(t, len, src);
}

/* (V10.8.52) ROTATE: steps a .. b-1 turned by d (d > 0 later), inside them; the rest stay */
static void tl_rot(track_t *t, uint32_t a, uint32_t b, int32_t d, uint32_t len)
{
    uint8_t src[NSTEP];
    uint32_t j, w = b - a;
    for (j = 0; j < len; j++)
        src[j] = (uint8_t)(j < a || j >= b ? j : a + (j - a + w * 64u - (uint32_t)d) % w);
    tl_order(t, len, src);
}
static void tl_shift(track_t *t, uint32_t len, uint32_t d) { tl_take(t); tl_rot(t, 0, len, (int32_t)d, len); }

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

/* ------------------------------------------------- (V10.8.52) SEQ EDIT --- */
/* The owner's SEQ EDIT (SEQ held on a SEQ page): the roll (DRUM: the lanes' grid) over KNOB 1 START, 2 END (A3: MOVE
 * and WIDTH), 3 the ACTION, 4 its amount; OCT+ (or D5) does it on the steps selected, or on the whole sequence while
 * none is (OCT- or G3: none). One UNDO point each (GLO + G4). The selection is the track's own (sel_of: its first | its
 * last << 8, bit 15 on; step keys held on the roll set it too, ui_input.c sel_poll): any SEQ page can read it (seq_sel) */
enum { SE_STRETCH, SE_SQUEEZE, SE_REPEAT, SE_NUDGE, SE_ROTATE, SE_SHIFT, SE_REVERSE, SE_DUP, SE_COPY, SE_PASTE, SE_CLEAR,
       SE_CAUTO, SE_CSTEP, SE_VEL, SE_NOTES, SE_HUMAN, SE_TRANSP, SE_OCT, SE_THIN, SE_INVERT, SE_STUT, SE_ARP, SE_MUTATE,
       SE_GEN, SE_RAND, SE_N };   /* (V10.8.53, the owner) GENERATE in the selection (KNOB 4 the genre), RANDOM (its grid) */
typedef struct { char nm[9]; int8_t lo, hi, def; char f; } se_t;
static const se_t SE[SE_N] = {
    {"STRETCH", 2, 4, 2, 'x'}, {"SQUEEZE", 2, 4, 2, '/'}, {"REPEAT", 2, 9, 2, 'x'}, {"NUDGE", -32, 31, 4, 'n'},
    {"ROTATE", -15, 15, 1, 's'}, {"SHIFT", -16, 16, 1, 's'}, {"REVERSE", 0, 0, 0, 0}, {"DUP", 0, 0, 0, 0},
    {"COPY", 0, 0, 0, 0}, {"PASTE", 0, 1, 0, 'p'}, {"CLEAR", 0, 0, 0, 0}, {"CLR AUTO", 0, 0, 0, 0},
    {"CLR STEP", 0, 0, 0, 0}, {"VEL RND", 5, 100, 25, '%'}, {"NOTE RND", 1, 7, 2, 's'}, {"HUMANIZE", 1, 16, 4, 'n'},
    {"TRANSP", -12, 12, 1, 's'}, {"OCTAVE", -2, 2, 1, 's'}, {"THIN", 5, 100, 25, '%'},
    {"INVERT", 0, 0, 0, 0}, {"STUTTER", 2, 4, 2, 'x'}, {"ARP", 0, 3, 0, 'a'}, {"MUTATE", 5, 100, 25, '%'},
    {"GEN", 0, 0, 0, 'g'}, {"RANDOM", 0, 3, 2, 'r'}};
#define sel_of(k) (clay_sp[2u + (k) % NTRK])
#define se_st (clay_sp[8])                              /* the ACTION | KNOB 1 / 2 MOVE and WIDTH << 7 */
#define se_md (clay_sp[9])                              /* its amount */

/* track t's steps selected (lo, n): bit 15, inside LEN; else the whole sequence (0, len). 1 = a selection */
static int seq_sel(const track_t *t, uint32_t *lo, uint32_t *n, uint32_t len)
{
    uint32_t s = (uint16_t)sel_of(trk_index(t)), a = s & 63u, b = (s >> 8) & 63u;
    if (!(s >> 15) || b >= len || a > b) { *lo = 0; *n = len; return 0; }
    *lo = a; *n = b - a + 1u;
    return 1;
}
static void sel_set(uint32_t a, uint32_t b) { cp_sel = (int16_t)(0x8000u | b << 8 | a); ui.bank = (uint8_t)(a / 16u); }

/* the amount's words: "x2", "/3", "+4", "25%", "FILL", "INSERT"; "-" none (V10.8.53: in the Bluetooth segment, as
 * cp_say: the room) */
BTAPP_FN static void se_amt(uint32_t a, char *b)
{
    int32_t m = se_md;
    char f = SE[a].f;
    b[0] = f == 'x' || f == '/' ? f : 0; b[1] = 0;
    if (!f) str_cpy(b, "-", 4);
    else if (f == 'p') str_cpy(b, m ? "INSERT" : "OVER", 8);
    else if (f == 'a') str_cpy(b, m == 0 ? "UP" : m == 1 ? "DOWN" : m == 2 ? "UP+DN" : "RANDOM", 8);
    else if (f == 'g') str_cpy(b, m > 0 ? GENRE[(uint32_t)(m - 1) % NGENRE].name : "ANY", 12);
    else if (f == 'r') str_cpy(b, m == 0 ? "1/4" : m == 1 ? "1/8" : m == 2 ? "1/16" : "1/32", 8);
    else if (a == SE_REPEAT && m > 8) str_cpy(b, "FILL", 8);
    else {
        if ((f == 'n' || f == 's') && m > 0) b[0] = '+';
        fmt_int(b + str_len(b), m);
        if (f == '%') str_cpy(b + str_len(b), "%", 2);
    }
}

/* (V10.8.49) NUDGE: every note step's MICRO in lo .. lo+n-1 d 64ths of a step later (d < 0: earlier) together, each
 * held at MICRO's range; the first note's MICRO said. (V10.8.52) HUMANIZE (h): each its own, up to d either way */
static __attribute__((noinline)) const char *tl_nudge(track_t *t, int32_t d, uint32_t lo, uint32_t n, int h)
{
    static char b[16];
    uint32_t i, c = 0, f = 0;
    int32_t m = 0, v;
    for (i = lo; i < lo + n; i++) {
        step_t *s = &t->step[i];
        if (s->time != ST_NOTE)
            continue;
        v = clamp(ST_MICRO(s) + (h ? (int32_t)tl_rnd(2u * (uint32_t)d + 1u) - d : d), MICRO_MIN, MICRO_MAX);
        if (!f++) m = v;
        if (v != ST_MICRO(s)) { ST_MICRO_SET(s, v); c++; }
    }
    if (!c) { ui_message(f ? "NUDGE: AT ITS LIMIT" : "NO NOTES"); return 0; }
    str_cpy(b, m > 0 ? "NUDGE +" : "NUDGE ", sizeof b);
    fmt_int(b + str_len(b), m);
    str_cpy(b + str_len(b), "/64", 4);
    return h ? "HUMANIZED" : b;
}

static uint32_t tl_reshape(track_t *t, int32_t k, uint32_t f, uint32_t lo, uint32_t n, uint32_t len);
/* action a (SE_*) with amount m on track t's selection (or all of it): 0 nothing done (said why) */
static __attribute__((noinline)) void se_do(uint32_t a, int32_t m)
{
    track_t *t = TSEL;
    char b[24];
    uint32_t len = tl_len(t), lo, n, i, j, sel = (uint32_t)seq_sel(t, &lo, &n, len), dr = (uint32_t)drum_track(t), nl;
    uint64_t was = tl_used(t, len);
    const char *msg = 0;
    if (chain_busy()) { ui_message("SONG PLAYS: STOP IT TO EDIT"); return; }
    if (a == SE_CSTEP) { seq_clr_step(); return; }
    if (a >= SE_NOTES && a != SE_HUMAN && a != SE_THIN && a != SE_STUT && a < SE_MUTATE && dr) { ui_message("NOTES ONLY"); return; }
    clay_sp[10] = (int16_t)((a + 1u) | (uint32_t)(uint8_t)m << 8);   /* (the UNDO key's words: the last action) */
    clay_sp[11] = (int16_t)(lo | n << 6);
    if (a == SE_COPY || a == SE_DUP) {
        cp_add = cp_add_acc = 0;
        cp_copy((~(uint64_t)0 >> (NSTEP - n)) << lo);   /* ("COPY 5-8") */
        if (a == SE_COPY) return;
        if (lo + n >= NSTEP) { ui_message("NO ROOM"); return; }
        cp_put(t, lo + n, 2u);                          /* (DUP: right after them, LEN grown; one UNDO) */
        if (sel) sel_set(lo + n, lo + 2u * n - 1u < NSTEP ? lo + 2u * n - 1u : NSTEP - 1u);
        return;
    }
    if (a == SE_PASTE) {
        if (!clip.n) { ui_message("NOTHING COPIED"); return; }
        if (!sel) { cp_put(t, 0, 1u); return; }        /* (the whole sequence: PASTE SEQ, as before) */
        if (m) {                                        /* INSERT: the steps from lo on moved along, LEN grown */
            nl = len + clip.n < NSTEP ? len + clip.n : NSTEP;
            undo_mark();
            for (i = len; i < nl; i++) { step_clear(&t->step[i]); (void)motion_clear_locks(t, i); }
            tl_take(t);
            tl_rot(t, lo, nl, (int32_t)(nl - len), nl);
            t->p[P_SLEN] = (int16_t)nl;
        }
        cp_put(t, lo, 0);
        return;
    }
    if (a == SE_GEN) {                                  /* (V10.8.53) the track's generator inside the selection: the genre
                                                         * KNOB 4's, a new seed each time ("GEN TECHNO 5-8 #1791") */
        tg_config_t *c = &tg_config[trk_index(t)];
        c->genre = (uint8_t)m;
        c->seed = (uint16_t)tl_rnd(10000u);
        trackgen_make(lo, n);
        str_cpy(b, "GEN ", sizeof b);
        se_amt(a, b + 4);
        str_cpy(b + str_len(b), " ", 2);
        cp_say(b, lo, n);
        str_cpy(b, ui.msg, sizeof b - 6);
        str_cpy(b + str_len(b), " #", 3);
        fmt_int(b + str_len(b), c->seed);
        ui_message(b);
        return;
    }
    undo_mark();                                        /* (one UNDO point each) */
    tl_take(t);
    switch (a) {
    case SE_RAND: {                                     /* (V10.8.53) new notes on the grid KNOB 4 picks (1/4 .. 1/32: a
                                                         * step in 4, 2, every one, every one doubled), in the scale */
        uint32_t g = m == 0 ? 4u : m == 1 ? 2u : 1u, root = 48u + (uint32_t)clamp(t->p[P_ROOT], 0, 11);
        for (i = lo; i < lo + n; i++) {
            step_t *x = &t->step[i];
            step_clear(x);
            if ((i - lo) % g || tl_rnd(100u) >= 60u) continue;
            x->time = ST_NOTE;
            x->vel = (uint8_t)(64u + tl_rnd(64u));
            if (dr) x->hit = (uint8_t)(1u << tl_rnd(4u));
            else { x->n = 1; x->note[0] = (uint8_t)clamp(scale_steps(t, (int32_t)root, (int32_t)tl_rnd(15u) - 7), 0, 127); }
            if (m == 3) step_set_ratchet(x, 2u);
        }
        break;
    }
    case SE_STRETCH: case SE_SQUEEZE: case SE_REPEAT:
        if (a == SE_REPEAT && m > 8) m = (int32_t)((NSTEP - len + n) / n);   /* (FILL: as many as fit in 64) */
        nl = m < 2 ? 0u : tl_reshape(t, a == SE_STRETCH ? 1 : a == SE_SQUEEZE ? -1 : 0, (uint32_t)m, lo, n, len);
        if (!nl) { ui_message(a == SE_SQUEEZE ? "TOO SHORT" : "64 STEPS ALREADY"); return; }
        if (sel) sel_set(lo, lo + n - 1u + nl - len < NSTEP ? lo + n - 1u + nl - len : NSTEP - 1u);
        len = nl;                                       /* (the emptied: of the new LEN) */
        break;
    case SE_NUDGE: case SE_HUMAN:
        if (!(msg = tl_nudge(t, m, lo, n, a == SE_HUMAN))) return;
        break;
    case SE_ROTATE: tl_rot(t, lo, lo + n, m, len); break;
    case SE_SHIFT:                                      /* the steps slid m along, the ones there round them */
        i = m < 0 ? (uint32_t)clamp((int32_t)lo + m, 0, (int32_t)lo) : lo;
        j = m > 0 ? (uint32_t)clamp((int32_t)(lo + n) + m, (int32_t)(lo + n), (int32_t)len) : lo + n;
        if (j - i <= n) { ui_message("AT THE EDGE"); return; }
        tl_rot(t, i, j, m < 0 ? (int32_t)(i - lo) : (int32_t)(j - lo - n), len);
        if (sel) sel_set(m < 0 ? i : j - n, (m < 0 ? i : j - n) + n - 1u);
        break;
    case SE_REVERSE: tl_reverse(t, lo, n, len); break;
    case SE_CAUTO: {                                    /* the automation and locks of the steps (none selected: all) */
        uint32_t f = motion_guard(), k = trk_index(t), c = 0;
        for (i = 0; i < motion.count; i++) {
            motion_event_t e = motion.event[i];
            if (MOTION_TRK(&e) != k || MOTION_STEP(&e) < lo || MOTION_STEP(&e) >= lo + n) motion.event[c++] = e;
        }
        j = motion.count - c;
        memset(motion.event + c, 0, (MOTION_MAX - c) * sizeof motion.event[0]);
        motion.count = (uint8_t)c;
        motion_unguard(f);
        if (!sel) motion_clear(t);
        if (!j && !sel) { ui_message("NOTHING TO CLEAR"); return; }
        break;
    }
    case SE_ARP: {                                      /* a chord's notes one a step, over the TIEs it holds on */
        uint32_t c = 0, k = 0, v = 100, q;
        uint8_t nt[4];
        for (i = lo; i < lo + n; i++) {
            step_t *x = &t->step[i];
            if (x->time == ST_NOTE && x->n > 1u) {
                c = x->n; v = x->vel; k = 0;
                memcpy(nt, x->note, 4);
                for (j = 1; j < c; j++) for (q = j; q && nt[q - 1u] > nt[q]; q--) { uint8_t e = nt[q]; nt[q] = nt[q - 1u]; nt[q - 1u] = e; }
            } else if (!c || x->time != ST_TIE) { c = 0; continue; }
            x->time = ST_NOTE; x->vel = (uint8_t)v;
            q = k % (2u * c - 2u);
            j = m == 3 ? tl_rnd(c) : m == 2 ? (q < c ? q : 2u * c - 2u - q) : m ? c - 1u - k % c : k % c;
            x->n = 1; x->note[0] = nt[j]; k++;
        }
        break;
    }
    default:                                            /* CLEAR, and each step of them on its own */
        if (a == SE_INVERT) {                           /* round the middle of the lowest and the highest note */
            uint32_t lo2 = 127, hi = 0;
            for (i = lo; i < lo + n; i++)
                for (j = 0; t->step[i].time == ST_NOTE && j < t->step[i].n; j++) {
                    if (t->step[i].note[j] < lo2) lo2 = t->step[i].note[j];
                    if (t->step[i].note[j] > hi) hi = t->step[i].note[j];
                }
            m = (int32_t)(lo2 + hi);
        }
        for (i = lo; i < lo + n; i++) {
            step_t *x = &t->step[i];
            if (a == SE_CLEAR) { step_clear(x); continue; }
            if (x->time != ST_NOTE || (!x->n && !x->hit)) continue;
            if (a == SE_VEL) x->vel = (uint8_t)clamp((int32_t)x->vel + (int32_t)tl_rnd(254u * (uint32_t)m / 100u + 1u) -
                                                     127 * m / 100, 1, 127);
            else if (a == SE_THIN) { if (tl_rnd(100u) < (uint32_t)m) step_clear(x); }
            else if (a == SE_STUT) x->flags = (uint8_t)((x->flags & ~SF_RATCH) | (uint32_t)(m - 1) << SF_RATCH_SH);
            else if (a == SE_MUTATE) {                  /* each step, m %: a degree or two, its velocity, its place */
                if (tl_rnd(100u) >= (uint32_t)m) continue;
                if (!dr) x->note[0] = (uint8_t)clamp(scale_steps(t, x->note[0], (int32_t)tl_rnd(5u) - 2), 0, 127);
                x->vel = (uint8_t)clamp((int32_t)x->vel + (int32_t)tl_rnd(41u) - 20, 1, 127);
                ST_MICRO_SET(x, clamp(ST_MICRO(x) + (int32_t)tl_rnd(9u) - 4, MICRO_MIN, MICRO_MAX));
            }
            else
                for (j = 0; j < x->n; j++)
                    x->note[j] = (uint8_t)clamp(a == SE_INVERT ? m - x->note[j] : a == SE_TRANSP ? x->note[j] + m : a == SE_OCT ? x->note[j] + 12 * m :
                                                scale_steps(t, x->note[j], (int32_t)tl_rnd(2u * (uint32_t)m + 1u) - m), 0, 127);
        }
        if (a == SE_CLEAR && !sel) latch_drop_req |= (uint8_t)(1u << trk_index(t));   /* (its held notes end) */
        break;
    }
    tl_unlock(t, was, len);
    if (!msg) {
        str_cpy(b, SE[a].nm, sizeof b);
        if (SE[a].f) { str_cpy(b + str_len(b), " ", 2); se_amt(a, b + str_len(b)); }
        str_cpy(b + str_len(b), " ", 2);
        cp_say(b, lo, n);                               /* "STRETCH x2 5-8" */
    } else
        ui_message(msg);
    ui.force = 1;
}

/* (V10.8.52) SEQ EDIT's UNDO key (F#5): the last action back, again: forward ("UNDO STRETCH x2 5-8", "REDO ..") */
static void se_undo(void)
{
    char b[24];
    uint32_t a = ((uint32_t)clay_sp[10] & 255u) - 1u, r = (uint16_t)clay_sp[11];
    int16_t md = se_md;
    if (chain_busy() || undo_do()) { ui_message("NOTHING TO UNDO"); return; }
    clay_sp[11] = (int16_t)(r ^ 0x8000u);
    if (a >= SE_N) { ui_message(r >> 15 ? "REDONE" : "UNDONE"); return; }
    str_cpy(b, r >> 15 ? "REDO " : "UNDO ", sizeof b);
    str_cpy(b + 5, SE[a].nm, sizeof b - 5);
    se_md = (int16_t)(int8_t)(clay_sp[10] >> 8);
    if (SE[a].f) { str_cpy(b + str_len(b), " ", 2); se_amt(a, b + str_len(b)); }
    se_md = md;
    str_cpy(b + str_len(b), " ", 2);
    cp_say(b, r & 63u, (r >> 6) & 127u);
    ui.force = 1;
}

/* the lane's tools (DRUM: G4 CLR, A4 REV, B4 FILL the lane ui.lane): its message; 0 = nothing done */
static __attribute__((noinline)) const char *tl_do(track_t *t, uint32_t p, uint32_t l)
{
    static char b[24];
    uint32_t len = tl_len(t), i, n;
    const char *msg;
    l %= NLANE;
    undo_mark();
    tl_take(t);
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
    return b;
}

/* ------------------------------------------------- (0.6.85) SEQ > STRETCH --- */
/* (V10.8.51) steps lo .. lo+n-1 of the selected track (tl_src: the steps before), the steps after them moved along, LEN
 * with them (64 at most: what falls past is cut), their automation and locks with their steps; (V10.8.52) f times:
 *   k 1, STRETCH xf: step i to step fi; a note held into the next step (a TIE after it) holds over the new ones
 *   k -1, SQUEEZE /f: step j takes the first that plays of fj .. fj+f-1 (a TIE if the note before holds on)
 *   k 0, REPEAT xf: the steps f times, their automation and locks copied while there is room
 * Returns the new LEN (0: nothing to do) */
BTAPP_FN static uint32_t tl_reshape(track_t *t, int32_t k, uint32_t q, uint32_t lo, uint32_t n, uint32_t len)
{
    uint32_t nn = k < 0 ? (n + q - 1u) / q : n * q, nlen = len - n + nn, i, j, c, f, tk = trk_index(t);
    if (nlen > NSTEP) nlen = NSTEP;
    if (nlen == len) return 0;
    for (j = lo; j < (len > nlen ? len : nlen); j++) step_clear(&t->step[j]);   /* (past both LENs: as they were) */
    for (i = lo + n; i < len; i++)                      /* the steps after the range */
        if (i - n + nn < NSTEP) t->step[i - n + nn] = tl_src[i];
    for (i = 0; i < nn && lo + i < NSTEP; i++) {
        step_t *d = &t->step[lo + i];
        if (k > 0) {                                    /* STRETCH: every q-th */
            const step_t *s = &tl_src[lo + i / q];
            if (!(i % q)) *d = *s;
            else if (s->time == ST_TIE || (s->time == ST_NOTE && s->n && lo + i / q + 1u < len &&
                                           tl_src[lo + i / q + 1u].time == ST_TIE))
                d->time = ST_TIE, d->vel = s->vel;
        } else if (!k)                                  /* REPEAT */
            *d = tl_src[lo + i % n];
        else {                                          /* SQUEEZE */
            uint32_t a = lo + q * i, tie = 0;
            for (j = a; j < a + q && j < lo + n; j++) {
                if (tl_src[j].time == ST_NOTE && (tl_src[j].n || tl_src[j].hit)) break;
                tie |= tl_src[j].time == ST_TIE;
            }
            if (j < a + q && j < lo + n) *d = tl_src[j];
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
            else if (st >= lo) st = k < 0 ? lo + (st - lo) / q : k ? lo + (st - lo) * q : st;
            if (st >= nlen) continue;                   /* (cut with its step) */
            e.place = (uint8_t)((e.place & 0xC0u) | st);
        }
        motion.event[c++] = e;
    }
    memset(motion.event + c, 0, (MOTION_MAX - c) * sizeof motion.event[0]);
    motion.count = (uint8_t)c;
    for (i = 0; !k && i < c; i++) {                     /* REPEAT: the range's on the copies */
        motion_event_t e = motion.event[i];
        uint32_t st = MOTION_STEP(&e);
        if (MOTION_TRK(&e) != tk || st < lo || st >= lo + n) continue;
        for (j = st + n; j < lo + nn && j < nlen && motion.count < MOTION_MAX; j += n) {
            e.place = (uint8_t)((e.place & 0xC0u) | j);
            motion.event[motion.count++] = e;
        }
    }
    RING_PUBLISH();
    motion_unguard(f);
    t->p[P_SLEN] = (int16_t)nlen;
    return nlen;
}

