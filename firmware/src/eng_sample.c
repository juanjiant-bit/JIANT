/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (ports/, FELUCCA_HOST_POWER only) SAMPLE: a slot (smp_slots.c) played one shot, with experimental modes, a feedback
 * comb, DESTROY and resampling. Engine 4 (the FM-1's retired SAMPLE: there a sound of it plays as ANALOG, core.h).
 *
 * EDIT 1 SAMPLE  SLOT 1..16, START, LENGTH (of what follows START), MODE:
 *   ONE    plays START..END once, the release ignored          GATE   .. stops on the release (the ADSR's)
 *   LOOP   loops the region while held                         PING   .. forth and back
 *   REV    END..START once                                     SLICE  the region in SLICES equal parts: C4 slice 1,
 *                                                                     C#4 slice 2 .. (at their own speed, TUNE on it)
 *   SCRUB  the playhead follows the velocity and the part's SHAPE (LFO, ENV, the matrix's SHP): it plays a short
 *          loop where it stands and slides to where it is sent (SMOOTH: how slowly)
 *   TAPE   wow and flutter (WOW), and on the release a slow stop (STOP: its time) down to silence
 * EDIT 2 WARP    TUNE -24..24 (C4 = the original speed; the keys play it chromatically), FEEDBK a comb tuned to the
 *   note inside the voice (to self-oscillation at the top, held by a soft limit), TONE its damping (dark .. bright),
 *   DESTROY the voice's output written back into the slot's working copy where it reads: each hit wears it (the
 *   original the host loaded is never touched; a reload brings it back)
 * TRIG (P_LN0..3)  TRIG: NORMAL; CHOKE one voice, each hit restarts; RATCH repeats at RATE of the tempo, AMOUNT their
 *   count (2..16), DECAY their fall; RAND a random start (SLICE: slice) within AMOUNT of the region; ROUND the next of
 *   AMOUNT slots from SLOT each hit (round robin); VEL the velocity picks the start (SLICE: slice) within AMOUNT; LEGATO
 *   a new note goes on from where the last one was; CHANCE each hit plays with AMOUNT's probability. SLICES 2..64
 * REC (P_LN4..7)  REC: OFF, or a source (IN the device's input, MST JIANT's output, T1..T6 a track's) and its bars
 *   (1 2 4 8, H until set back to OFF): armed, it records from the next bar into SLOT (the slot then plays it; the
 *   host saves it), then goes back to OFF. WOW (TAPE; SCRUB: SMOOTH), STOP (TAPE's), DECAY (RATCH's)
 *
 * State beyond voice_t: smp_v[part][voice] (the playhead, the comb, the ratchet). voice_t.s[0] marks a voice ended by
 * the engine. Stereo slots: the side through smp_side (fx.c mix_part) */
enum { SM_ONE, SM_GATE, SM_LOOP, SM_PING, SM_REV, SM_SLICE, SM_SCRUB, SM_TAPE, SM_N };
enum { ST_NORMAL, ST_CHOKE, ST_RATCH, ST_RAND, ST_ROUND, ST_VEL, ST_LEGATO, ST_CHANCE, STR_N };
static const char *const N_SMP_MODE[] = {"ONE", "GATE", "LOOP", "PING", "REV", "SLICE", "SCRUB", "TAPE", 0};
static const char *const N_SMP_TRIG[] = {"NORM", "CHOKE", "RATCH", "RAND", "ROUND", "VEL", "LEGAT", "CHNCE", 0};
static const char *const N_SMP_SLOT[] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14",
                                         "15", "16", 0};
static const char *const N_SMP_RATE[] = {"1/4", "1/8", "1/8T", "1/16", "16T", "1/32", 0};
static const uint16_t SMP_RATE_Q[6] = {96, 48, 32, 24, 16, 12};   /* the repeat's distance in 1/96 of a bar's quarter x 4 */
#define SMP_RECN 41                       /* REC: OFF, then 8 sources x {1 2 4 8 H} bars */
static const char *const N_SMP_REC[SMP_RECN + 1] = {
    "OFF",
    "IN 1", "IN 2", "IN 4", "IN 8", "IN H", "MST 1", "MST 2", "MST 4", "MST 8", "MST H",
    "T1 1", "T1 2", "T1 4", "T1 8", "T1 H", "T2 1", "T2 2", "T2 4", "T2 8", "T2 H",
    "T3 1", "T3 2", "T3 4", "T3 8", "T3 H", "T4 1", "T4 2", "T4 4", "T4 8", "T4 H",
    "T5 1", "T5 2", "T5 4", "T5 8", "T5 H", "T6 1", "T6 2", "T6 4", "T6 8", "T6 H", 0};
static const char *const N_SMP_RECING[SMP_RECN + 1] = {   /* (while it records: the screen says so) */
    "OFF", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC",
    "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC",
    "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", "REC", 0};
static const char *const N_SMP_ARMED[SMP_RECN + 1] = {
    "OFF", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED",
    "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED",
    "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED", "ARMED",
    "ARMED", "ARMED", "ARMED", "ARMED", 0};
static const uint8_t SMP_REC_BARS[5] = {1, 2, 4, 8, 0};

/* the extra parameters, in slots no other engine's on these tracks (core.h: DRUM's lane levels) */
#define P_SXTRIG P_LN0
#define P_SXAMT P_LN1
#define P_SXRATE P_LN2
#define P_SXSLC P_LN3
#define P_SXREC P_LN4
#define P_SXWOW P_LN5
#define P_SXSTOP P_LN6
#define P_SXDEC P_LN7

#define SMP_COMB 2048u                    /* the comb's line: a period of 21.5 Hz */
typedef struct {
    double pos;                           /* the playhead, frames of the slot */
    float dir;                            /* +1 / -1 (PING, REV) */
    uint32_t ver;                         /* the slot's version it plays */
    uint8_t slot, live, ended;
    uint32_t a, b;                        /* the region [a, b) */
    uint32_t r_left, r_due, r_gap;        /* RATCH: hits to come, samples to the next, between them */
    float r_g, r_k;                       /* .. its level now, the fall a hit */
    float tape;                           /* TAPE: the speed after the release (1 .. 0) */
    float wph, fph;                       /* wow's and flutter's phases */
    float c[2][SMP_COMB];                 /* the comb */
    uint32_t cw;
    float clp[2], tail;                   /* its damping states; the level ringing on after the sample */
    float tgt, base;                      /* SCRUB: the place it is sent, where its short loop starts (frames) */
    uint32_t rng;
} smp_v_t;
static smp_v_t smp_v[NPART][NVOICE];
static uint8_t smp_round[NPART];          /* ROUND: the next slot */
static int8_t smp_recv[NPART];            /* REC as last seen (an edge arms, -1: not yet seen) */
static uint8_t smp_seen;

static inline smp_v_t *smp_vs(const track_t *t, const voice_t *v)
{
    return &smp_v[(uint32_t)(t - trk) % NPART][(uint32_t)(v - t->v) % NVOICE];
}
static inline uint32_t smp_rnd(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s >> 8;
}
/* the speed of a note: C4 the original, semis more (TUNE), the voice's fine residual */
static inline float smp_ratio(const vmod_t *m, int32_t semis)
{
    return powf(2.0f, (float)(m->pitch16 - 960 + semis * 16) / 192.0f) * (1.0f + (float)m->fine / 4096.0f);
}
static inline uint32_t smp_slices(const track_t *t) { return (uint32_t)clamp(t->p[P_SXSLC], 2, 64); }

/* the region of slot o: START, LENGTH of what follows */
static void smp_region(const track_t *t, const smp_obj_t *o, uint32_t *a, uint32_t *b)
{
    uint32_t n = o->frames, s = (uint32_t)((uint64_t)n * (uint32_t)clamp(t->p[P_E1], 0, 127) / 128u);
    uint32_t len = (uint32_t)((uint64_t)(n - s) * (uint32_t)clamp(t->p[P_E2], 1, 127) / 127u);
    if (len < 64u)
        len = n - s < 64u ? n - s : 64u;
    *a = s;
    *b = s + (len ? len : 1u);
}

static const param_desc_t SMP_EMPTY = {"EMPTY", F_ENUM, 0, 15, 0, N_SMP_SLOT, 0};
static const param_desc_t SMP_AMT[STR_N] = {
    {"-", F_PCT, 0, 127, 64, 0, 0},       {"-", F_PCT, 0, 127, 64, 0, 0},
    {"COUNT", F_PCT, 0, 127, 64, 0, 0},   {"RANGE", F_PCT, 0, 127, 64, 0, 0},
    {"SLOTS", F_PCT, 0, 127, 64, 0, 0},   {"RANGE", F_PCT, 0, 127, 64, 0, 0},
    {"-", F_PCT, 0, 127, 64, 0, 0},       {"CHANCE", F_PCT, 0, 127, 64, 0, 0}};
static const param_desc_t SMP_X[8] = {
    {"TRIG", F_ENUM, 0, STR_N - 1, 0, N_SMP_TRIG, 0},
    {"AMOUNT", F_PCT, 0, 127, 64, 0, 0},
    {"RATE", F_ENUM, 0, 5, 3, N_SMP_RATE, 0},
    {"SLICES", F_INT, 2, 64, 16, 0, 0},
    {"REC", F_ENUM, 0, SMP_RECN - 1, 0, N_SMP_REC, 0},
    {"WOW", F_PCT, 0, 127, 40, 0, 0},
    {"STOP", F_PCT, 0, 127, 64, 0, 0},
    {"DECAY", F_PCT, 0, 127, 64, 0, 0},
};
static const param_desc_t SMP_SMOOTH = {"SMOOTH", F_PCT, 0, 127, 40, 0, 0};
static const param_desc_t SMP_RECD[2] = {{"REC", F_ENUM, 0, SMP_RECN - 1, 0, N_SMP_ARMED, 0},
                                         {"REC", F_ENUM, 0, SMP_RECN - 1, 0, N_SMP_RECING, 0}};
static const param_desc_t *smp_desc(const track_t *t, uint32_t k)
{
    if (k == 0u && !smp_get((uint32_t)clamp(t->p[P_E0], 0, 15)))
        return &SMP_EMPTY;
    return 0;
}
/* the descriptor of an extra (k 0..7: P_LN0..7) on a SAMPLE track (params.c track_desc) */
static const param_desc_t *smp_xdesc(const track_t *t, uint32_t k)
{
    k &= 7u;
    if (k == 1u)
        return &SMP_AMT[(uint32_t)clamp(t->p[P_SXTRIG], 0, STR_N - 1)];
    if (k == 4u && smp_rec.owner == (uint8_t)(t - trk) && smp_rec.st != SR_IDLE)
        return &SMP_RECD[smp_rec.st == SR_REC];
    if (k == 5u && t->p[P_E3] == SM_SCRUB)
        return &SMP_SMOOTH;
    return &SMP_X[k];
}

/* REC (P_SXREC) and the recorder, a block: a change of it arms (the slot SLOT; slot of GRAIN's too) or stops; done,
 * it goes back to OFF */
static void smp_rec_param(track_t *t, uint32_t pid, uint32_t slot)
{
    uint32_t ti = (uint32_t)(t - trk) % NPART;
    int32_t r = clamp(t->p[pid], 0, SMP_RECN - 1);
    if (!((smp_seen >> ti) & 1u)) {                   /* (a sound loaded with REC on does not record by itself) */
        smp_seen |= (uint8_t)(1u << ti);
        smp_recv[ti] = (int8_t)r;
    }
    if (r != smp_recv[ti]) {
        if (smp_rec.owner == ti && smp_rec.st != SR_IDLE)
            smp_rec_stop();
        if (r && smp_rec_arm(slot, (uint32_t)(r - 1) / 5u, SMP_REC_BARS[(uint32_t)(r - 1) % 5u], ti))
            r = 0;                                    /* (busy, or no room: back to OFF) */
        t->p[pid] = (int16_t)r;
        smp_recv[ti] = (int8_t)r;
    } else if (r && !(smp_rec.owner == ti && smp_rec.st != SR_IDLE)) {
        t->p[pid] = 0;                                /* recorded: OFF again */
        smp_recv[ti] = 0;
    }
}

static void smp_block(track_t *t)
{
    smp_side_begin(t);
    smp_rec_param(t, P_SXREC, (uint32_t)clamp(t->p[P_E0], 0, 15));
}

static void smp_end(voice_t *v)                       /* the engine ends the voice (smp_amp: at the next tick) */
{
    v->s[0] = 1;
}

static void smp_note_on(track_t *t, voice_t *v)
{
    smp_v_t *S = smp_vs(t, v);
    uint32_t ti = (uint32_t)(t - trk) % NPART, trig = (uint32_t)clamp(t->p[P_SXTRIG], 0, STR_N - 1);
    uint32_t mode = (uint32_t)clamp(t->p[P_E3], 0, SM_N - 1), slot = (uint32_t)clamp(t->p[P_E0], 0, 15), k;
    int32_t amt = clamp(t->p[P_SXAMT], 0, 127);
    double from = -1.0;
    const smp_obj_t *o;
    v->s[0] = 0;
    S->rng = S->rng * 69069u + v->note * 2654435761u + v->vel + 1u;
    if (trig == ST_CHOKE || trig == ST_LEGATO) {      /* one voice: the others go (LEGATO: from where it was) */
        uint32_t best = 0;
        for (k = 0; k < NVOICE; k++) {
            voice_t *w = &t->v[k];
            if (w == v || !w->active || w->stage == 4u)
                continue;
            if (w->age >= best && smp_v[ti][k].live) {
                best = w->age;
                from = smp_v[ti][k].pos;
            }
            voice_kill(w);
        }
    }
    if (trig == ST_ROUND) {
        uint32_t n = 1u + (uint32_t)amt * 15u / 127u;
        slot = (slot + smp_round[ti] % n) % SMP_NSLOT;
        smp_round[ti] = (uint8_t)((smp_round[ti] + 1u) % n);
    }
    o = smp_get(slot);
    S->slot = (uint8_t)slot;
    S->ver = smp_slot[slot].ver;
    S->live = o != 0;
    S->ended = 0;
    S->tape = 1.0f;
    S->tail = 0.0f;
    S->cw = 0;
    S->clp[0] = S->clp[1] = 0.0f;
    for (k = 0; k < SMP_COMB; k++)
        S->c[0][k] = S->c[1][k] = 0.0f;
    S->r_left = 0;
    S->r_g = 1.0f;
    if (trig == ST_CHANCE && (int32_t)(smp_rnd(&S->rng) % 127u) >= amt)
        S->live = 0;                                  /* (this hit does not play) */
    if (!S->live) {
        smp_end(v);
        return;
    }
    smp_region(t, o, &S->a, &S->b);
    if (mode == SM_SLICE) {                           /* the note's slice of the region */
        uint32_t ns = smp_slices(t), len = (S->b - S->a) / ns, sl = (uint32_t)((int32_t)v->note - 60 + 64 * (int32_t)ns) % ns;
        if (trig == ST_RAND)
            sl = (sl + smp_rnd(&S->rng) % (1u + (ns - 1u) * (uint32_t)amt / 127u)) % ns;
        else if (trig == ST_VEL)
            sl = (sl + (uint32_t)v->vel * (ns - 1u) * (uint32_t)amt / (127u * 127u)) % ns;
        S->a += sl * (len ? len : 1u);
        S->b = S->a + (len ? len : 1u);
    }
    {
        double span = (double)(S->b - S->a), off = 0.0;
        if (mode != SM_SLICE && trig == ST_RAND)
            off = span * amt / 127.0 * (double)(smp_rnd(&S->rng) & 0xFFFFu) / 65536.0;
        else if (mode != SM_SLICE && trig == ST_VEL)
            off = span * amt / 127.0 * (double)v->vel / 128.0;
        S->dir = mode == SM_REV ? -1.0f : 1.0f;
        S->pos = mode == SM_REV ? (double)S->b - 1.0 - off : (double)S->a + off;
        if (from >= 0.0 && trig == ST_LEGATO)
            S->pos = from < (double)o->frames ? from : (double)S->a;
        S->tgt = S->base = (float)S->pos;
    }
    if (trig == ST_RATCH) {                           /* the repeats, at RATE of the tempo */
        uint32_t q = beat_samples();
        S->r_left = 1u + (uint32_t)amt * 14u / 127u;   /* (2 .. 16 hits) */
        S->r_gap = q * SMP_RATE_Q[(uint32_t)clamp(t->p[P_SXRATE], 0, 5)] / 96u;
        S->r_due = S->r_gap ? S->r_gap : 1u;
        S->r_k = 0.35f + 0.65f * (float)clamp(t->p[P_SXDEC], 0, 127) / 127.0f;
    }
}

/* the voice's level: the one-shot modes ignore the release (they end themselves), ATK still shapes the start */
static int32_t smp_amp(track_t *t, voice_t *v, int32_t adsr)
{
    uint32_t mode = (uint32_t)clamp(t->p[P_E3], 0, SM_N - 1);
    if (!v->active)
        return 0;
    if (v->s[0]) {                                    /* ended by the engine */
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    if (v->stage == 4u || v->stage == 1u)
        return adsr;
    if (mode == SM_ONE || mode == SM_REV || mode == SM_SLICE || mode == SM_TAPE ||
        smp_vs(t, v)->r_left) {                       /* (RATCH: its hits after the release too) */
        v->env = 1 << 24;
        return 32767;
    }
    return adsr;
}

static void smp_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    smp_v_t *S = smp_vs(t, v);
    const int16_t *p = t->p;
    uint32_t mode = (uint32_t)clamp(p[P_E3], 0, SM_N - 1), i, ti = (uint32_t)(t - trk) % NPART;
    smp_obj_t *o = smp_slot[S->slot % SMP_NSLOT].o;
    float spd, fb, damp, dg, wow = (float)clamp(p[P_SXWOW], 0, 127) / 127.0f, del;
    float stop = (float)clamp(p[P_SXSTOP], 0, 127) / 127.0f, sm, len;
    int fade_tail = 0;
    if (!S->live || S->ended || !o || !o->frames || S->ver != smp_slot[S->slot % SMP_NSLOT].ver) {
        smp_end(v);                                   /* (empty, or the slot changed under it) */
        S->live = 0;
        return;
    }
    if (S->b > o->frames)
        S->b = o->frames;
    if (S->a >= S->b)
        S->a = S->b ? S->b - 1u : 0u;
    /* the speed: the note's (C4 the original); SLICE: the slice's own, TUNE, the bend, the LFO on it */
    spd = smp_ratio(m, mode == SM_SLICE ? 60 - (int32_t)v->note + p[P_E4] : p[P_E4]);
    spd = spd > 16.0f ? 16.0f : spd < 0.01f ? 0.01f : spd;
    fb = (float)clamp(p[P_E5], 0, 127) / 127.0f;
    fb = fb * fb * (2.0f - fb) * 1.03f;               /* (the top past 1: it rings by itself, the limit holds it) */
    damp = 0.08f + 0.9f * (float)clamp(p[P_E6], 0, 127) / 127.0f;
    dg = (float)clamp(p[P_E7], 0, 127) / 127.0f * 0.6f;
    del = (float)FS / (440.0f * powf(2.0f, (float)(m->pitch16 - 69 * 16) / 192.0f));   /* the comb: the note's period */
    del = del < 2.0f ? 2.0f : del > (float)(SMP_COMB - 2u) ? (float)(SMP_COMB - 2u) : del;
    sm = 0.00002f + 0.002f * (1.0f - (float)clamp(p[P_SXWOW], 0, 127) / 127.0f);   /* SCRUB: SMOOTH */
    len = (float)(S->b - S->a);
    if (mode == SM_SCRUB) {
        int32_t x = (int32_t)v->vel * 256 + (m->shape - (64 << 8)) * 2;
        S->tgt = (float)S->a + len * (float)clamp(x, 0, 127 << 8) / (float)(127 << 8);
    }
    if (mode == SM_TAPE && !v->gate && v->stage != 4u) {   /* the slow stop: STOP 0 .. 127 = 50 ms .. 4 s */
        float tau = 0.05f + 3.95f * stop * stop;
        S->tape -= (float)n / ((float)FS * tau);
        if (S->tape <= 0.0f) {
            S->tape = 0.0f;
            smp_end(v);
        }
    }
    for (i = 0; i < n; i++) {
        float xl = 0.0f, xr = 0.0f, yl, yr, g, s, rate = spd;
        int playing = 1;
        if (mode == SM_TAPE) {                        /* wow (~0.6 Hz) and flutter (~9 Hz) */
            S->wph += 0.6f / (float)FS;
            S->fph += 9.1f / (float)FS;
            if (S->wph >= 1.0f) S->wph -= 1.0f;
            if (S->fph >= 1.0f) S->fph -= 1.0f;
            rate *= (1.0f + wow * (0.012f * sinf(6.2831853f * S->wph) + 0.003f * sinf(6.2831853f * S->fph))) * S->tape;
        }
        if (S->r_left || S->r_g < 1.0f) {             /* RATCH: the next hit */
            if (S->r_left && !--S->r_due) {
                S->r_left--;
                S->r_due = S->r_gap ? S->r_gap : 1u;
                S->r_g *= S->r_k;
                S->pos = mode == SM_REV ? (double)S->b - 1.0 : (double)S->a;
                S->ended = 0;
                fade_tail = 0;
            }
        }
        if (S->pos < (double)S->a || S->pos >= (double)S->b) {
            switch (mode) {
            case SM_LOOP:
                S->pos = S->pos >= (double)S->b ? S->pos - (S->b - S->a) : (double)S->a;
                break;
            case SM_PING:
                S->dir = -S->dir;
                S->pos = S->pos >= (double)S->b ? (double)S->b - 1.0 : (double)S->a;
                break;
            case SM_SCRUB:
                S->pos = (double)S->a;
                break;
            default:
                playing = 0;                          /* the end: the comb rings on, then the voice ends */
                break;
            }
        }
        if (playing) {
            xl = smp_read(o, 0, S->pos);
            xr = o->ch > 1u ? smp_read(o, 1, S->pos) : xl;
            if (mode == SM_SCRUB) {                   /* a 50 ms loop where it stands, slid to where it is sent: the
                                                       * slide moves the playhead with it (the scratch) */
                float nb = S->base + (S->tgt - S->base) * sm, w = 2205.0f * spd;
                S->pos += (double)(nb - S->base + rate);
                S->base = nb;
                if (S->pos >= (double)(S->base + w))
                    S->pos -= (double)w;
                if (S->pos < (double)S->base)
                    S->pos = (double)S->base;
            } else {
                S->pos += (double)(S->dir * rate);
            }
        } else if (!S->r_left) {
            fade_tail = 1;
        }
        if (S->r_g < 1.0f) {
            xl *= S->r_g;
            xr *= S->r_g;
        }
        if (fb > 0.0f) {                              /* the comb: the note's period, damped, softly limited */
            float dp = (float)S->cw - del + (float)SMP_COMB;
            uint32_t j = (uint32_t)dp, j1;
            float f = dp - (float)j, dl, dr;
            j %= SMP_COMB;
            j1 = (j + 1u) % SMP_COMB;
            dl = S->c[0][j] + (S->c[0][j1] - S->c[0][j]) * f;
            dr = S->c[1][j] + (S->c[1][j1] - S->c[1][j]) * f;
            S->clp[0] += (dl - S->clp[0]) * damp;
            S->clp[1] += (dr - S->clp[1]) * damp;
            yl = xl + fb * S->clp[0];
            yr = xr + fb * S->clp[1];
            yl = 24000.0f * tanhf(yl / 24000.0f);     /* (the limit: self-oscillation stays at full scale) */
            yr = 24000.0f * tanhf(yr / 24000.0f);
            if (fb > 1.0f && fabsf(yl) < 1.0f)        /* (a breath for it to start from silence) */
                yl += (float)((int32_t)(smp_rnd(&S->rng) & 63u) - 32);
            S->c[0][S->cw] = smp_finite(yl);
            S->c[1][S->cw] = smp_finite(yr);
            S->cw = (S->cw + 1u) % SMP_COMB;
            S->tail = S->tail * 0.9995f + fabsf(yl);
        } else {
            yl = xl;
            yr = xr;
        }
        if (dg > 0.0f && playing) {                   /* DESTROY: back into the working copy where it read */
            uint32_t at = (uint32_t)(S->pos - (double)(S->dir * rate));
            smp_scar(o, at, yl, yr, dg);
        }
        g = (float)amp_at(m, i) * (float)VOICE_FS * (1.0f / (32768.0f * 32768.0f));
        if (mode == SM_TAPE && S->tape < 0.25f)
            g *= S->tape * 4.0f;
        s = smp_finite(0.5f * (yl + yr) * g);
        out[i] += (int32_t)s;
        if (o->ch > 1u)
            smp_side_add(i, (int32_t)smp_finite(0.5f * (yl - yr) * g));
    }
    if (dg > 0.0f)
        smp_changed(S->slot);
    if (fade_tail && !S->r_left && (fb <= 0.0f || S->tail < 64.0f || (fb > 1.0f && !v->gate))) {
        S->ended = 1;                                 /* (played, rung out: the voice goes) */
        smp_end(v);
    }
    (void)ti;
}

/* {SLOT, START, LENGTH, MODE, TUNE, FEEDBK, TONE, DESTROY}; the extras below, by preset (smp_preset_x) */
static const preset_t SMP_PRESETS[] = {
    {"ONE SHOT", {0, 0, 127, SM_ONE, 0, 0, 64, 0}, {0, 100, 127, 40}, 0, TAG(FX), FX(0, 0, 0, 10)},
    {"GATE", {0, 0, 127, SM_GATE, 0, 0, 64, 0}, {0, 100, 127, 30}, 0, TAG(FX), FX(0, 0, 0, 10)},
    {"LOOP", {0, 0, 127, SM_LOOP, 0, 0, 64, 0}, {4, 100, 127, 60}, 0, TAG(PAD), FX(0, 10, 0, 30)},
    {"PING PONG", {0, 20, 60, SM_PING, 0, 0, 64, 0}, {10, 100, 127, 70}, 0, TAG(PAD), FX(0, 20, 0, 30)},
    {"REVERSE", {0, 0, 127, SM_REV, 0, 0, 64, 0}, {0, 100, 127, 40}, 0, TAG(FX), FX(0, 0, 10, 30)},
    {"SLICE 16", {0, 0, 127, SM_SLICE, 0, 0, 64, 0}, {0, 100, 127, 20}, 0, TAG(DRUM), FX(0, 0, 0, 10), PAT(11)},
    {"SLICE 8", {0, 0, 127, SM_SLICE, 0, 0, 64, 0}, {0, 100, 127, 20}, 0, TAG(DRUM), FX(0, 0, 0, 10), PAT(10)},
    {"RANDOM SLC", {0, 0, 127, SM_SLICE, 0, 0, 64, 0}, {0, 100, 127, 20}, 0, TAG(DRUM), FX(0, 0, 10, 10), PAT(11)},
    {"VEL SLICE", {0, 0, 127, SM_SLICE, 0, 0, 64, 0}, {0, 100, 127, 20}, 0, TAG(DRUM), FX(0, 0, 0, 10), PAT(9)},
    {"SCRUB", {0, 0, 127, SM_SCRUB, 0, 0, 64, 0}, {10, 100, 127, 50}, 0, TAG(FX), FX(0, 10, 10, 30)},
    {"TAPE STOP", {0, 0, 127, SM_TAPE, 0, 0, 64, 0}, {0, 100, 127, 100}, 0, TAG(FX), FX(0, 0, 0, 20)},
    {"RATCHET", {0, 0, 127, SM_ONE, 0, 0, 64, 0}, {0, 100, 127, 30}, 0, TAG(DRUM), FX(0, 0, 10, 10), PAT(12)},
    {"CHOKE", {0, 0, 127, SM_ONE, 0, 0, 64, 0}, {0, 100, 127, 20}, 0, TAG(DRUM), FX(0, 0, 0, 10), PAT(12)},
    {"ROUND ROBIN", {0, 0, 127, SM_ONE, 0, 0, 64, 0}, {0, 100, 127, 30}, 0, TAG(DRUM), FX(0, 0, 0, 10), PAT(12)},
    {"LEGATO", {0, 0, 127, SM_GATE, 0, 0, 64, 0}, {4, 100, 127, 40}, 1, TAG(LEAD), FX(0, 10, 10, 20), PAT(4)},
    {"CHANCE", {0, 0, 127, SM_ONE, 0, 0, 64, 0}, {0, 100, 127, 30}, 0, TAG(DRUM), FX(0, 0, 10, 10), PAT(12)},
    {"FEEDBACK", {0, 0, 127, SM_ONE, 0, 90, 60, 0}, {0, 100, 127, 50}, 0, TAG(FX), FX(0, 0, 10, 30), PAT(3)},
    {"METAL COMB", {0, 0, 127, SM_GATE, 0, 118, 110, 0}, {0, 100, 127, 60}, 0, TAG(BELL), FX(0, 0, 10, 30), PAT(7)},
    {"DESTROY", {0, 0, 127, SM_ONE, 0, 40, 50, 70}, {0, 100, 127, 40}, 0, TAG(FX), FX(10, 0, 10, 20)},
    {"RESAMPLE", {0, 0, 127, SM_LOOP, 0, 0, 64, 0}, {0, 100, 127, 40}, 0, TAG(FX), FX(0, 0, 0, 10)},
};
/* {TRIG, AMOUNT, RATE, SLICES, REC, WOW, STOP, DECAY} of each preset */
static const uint8_t SMP_PRESET_X[NELEM(SMP_PRESETS)][8] = {
    {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64}, {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64}, {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64},
    {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64}, {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64}, {ST_CHOKE, 64, 3, 16, 0, 40, 64, 64},
    {ST_CHOKE, 64, 3, 8, 0, 40, 64, 64},   {ST_RAND, 127, 3, 16, 0, 40, 64, 64},  {ST_VEL, 127, 3, 16, 0, 40, 64, 64},
    {ST_NORMAL, 64, 3, 16, 0, 60, 64, 64}, {ST_CHOKE, 64, 3, 16, 0, 70, 70, 64},  {ST_RATCH, 30, 3, 16, 0, 40, 64, 80},
    {ST_CHOKE, 64, 3, 16, 0, 40, 64, 64},  {ST_ROUND, 25, 3, 16, 0, 40, 64, 64},  {ST_LEGATO, 64, 3, 16, 0, 40, 64, 64},
    {ST_CHANCE, 80, 3, 16, 0, 40, 64, 64}, {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64}, {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64},
    {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64}, {ST_NORMAL, 64, 3, 16, 0, 40, 64, 64},
};

static const engine_t ENG_SAMPLE = {
    .name = "SAMPLE",
    .page_title = {"SAMPLE", "WARP"},
    .edit = {
        {"SLOT", F_ENUM, 0, 15, 0, N_SMP_SLOT, 0},
        {"START", F_PCT, 0, 127, 0, 0, 0},
        {"LENGTH", F_PCT, 0, 127, 127, 0, 0},
        {"MODE", F_ENUM, 0, SM_N - 1, 0, N_SMP_MODE, 0},
        {"TUNE", F_SEMI, -24, 24, 0, 0, 0},
        {"FEEDBK", F_PCT, 0, 127, 0, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
        {"DSTROY", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = SMP_PRESETS,
    .npresets = NELEM(SMP_PRESETS),
    .note_on = smp_note_on,
    .render = smp_render,
    .amp = smp_amp,
    .desc = smp_desc,
    .block = smp_block,
    .knob = {P_E1, P_E2, P_E5, P_E7},
    .sampled = 1,
};
