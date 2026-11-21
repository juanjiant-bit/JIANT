/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (ports/, FELUCCA_HOST_POWER only) GRAIN: granular synthesis on a slot (smp_slots.c) or on what is playing now (LIVE:
 * the device's input or JIANT's master, its last 8 s). Engine 8 (the FM-1's retired GRAIN: there it plays as ANALOG).
 * Each voice runs its own cloud of up to GR_N grains; the key sets their pitch (C4 the original); grains spread in
 * stereo (WIDTH, through smp_side).
 *
 * EDIT 1 GRAIN   SLOT 1..16, POS (where the playhead's region starts; the part's SHAPE moves it: LFO, ENV, the
 *   matrix), SCAN the playhead's speed (-64 .. 63, 0 frozen; TRAVEL STRETCH: the time-stretch factor 0.1x .. 4x;
 *   RANDOM: how often it jumps), SIZE a grain's length (5 ms .. 500 ms)
 * EDIT 2 CLOUD   DENSITY grains a second (1 .. 200, overlapping), SPRAY the position's random jitter (to +-0.5 s),
 *   SPREAD each grain's random detune (to +-12 semitones; QUANT: octaves or fifths only), REVERSE the share of
 *   grains played backwards
 * TRAVEL (P_LN0..3) TRAVEL the playhead in its region: FWD at SCAN's speed (bipolar), REV the other way, PING forth
 *   and back, RAND jumps, STRETCH real time x SCAN's factor (the pitch the key's: a loop slower or faster, its pitch
 *   kept), FREEZE still at POS, KEY the note picks the place (C2 .. C6 across the region); REGION its length after
 *   POS; FEEDBK the cloud's output fed back as the grains' input with the slot (1 s of it, soft limited); SHIMMR the
 *   fed-back grains shifted (OFF, +OCT, +5TH, -OCT, +2OCT: shimmer)
 * SOURCE (P_LN4..7) WINDOW the grain's shape (HANN, TRI, FLAT, PERC, SWELL), WIDTH the stereo spread, SOURCE SLOT /
 *   IN (LIVE: the input) / MST (LIVE: the master; a granular delay and freeze of what plays), REC as SAMPLE's (into
 *   SLOT, from the next bar)
 * TONE (P_FM3_DEC, P_FM4_SUS) QUANT FREE / OCT / 5TH, DAMP the feedback's tone (127 bright) */
#define GR_N 48                           /* grains a voice at most */
#define GR_FBN 44100u                     /* the feedback's line, a part's (1 s) */
enum { GT_FWD, GT_REV, GT_PING, GT_RAND, GT_STRETCH, GT_FREEZE, GT_KEY, GT_N };
enum { GW_HANN, GW_TRI, GW_FLAT, GW_PERC, GW_SWELL, GW_N };
enum { GS_SLOT, GS_IN, GS_MST, GS_N };
static const char *const N_GR_TRAVEL[] = {"FWD", "REV", "PING", "RAND", "STRCH", "FREEZ", "KEY", 0};
static const char *const N_GR_SHIM[] = {"OFF", "+OCT", "+5TH", "-OCT", "+2OCT", 0};
static const char *const N_GR_WIN[] = {"HANN", "TRI", "FLAT", "PERC", "SWELL", 0};
static const char *const N_GR_SRC[] = {"SLOT", "IN", "MST", 0};
static const char *const N_GR_QUANT[] = {"FREE", "OCT", "5TH", 0};
static const float GR_SHIM_R[5] = {1.0f, 2.0f, 1.4983071f, 0.5f, 4.0f};
#define P_GXTRAV P_LN0
#define P_GXREG P_LN1
#define P_GXFB P_LN2
#define P_GXSHIM P_LN3
#define P_GXWIN P_LN4
#define P_GXWIDTH P_LN5
#define P_GXSRC P_LN6
#define P_GXREC P_LN7
#define P_GXQUANT P_FM3_DEC
#define P_GXDAMP P_FM4_SUS

typedef struct {
    double pos;                           /* in the source: slot frames, or LIVE frames since boot */
    float inc, fpos, finc;                /* its speed (negative: reversed); the fed-back line's place (ago), speed */
    float pan, gain;
    uint32_t len, age;
} gr_grain_t;
typedef struct {
    gr_grain_t g[GR_N];
    uint64_t live;                        /* bit k: g[k] sounds */
    double ph;                            /* the playhead (slot frames; LIVE: frames since boot) */
    float dir, due, jump;                 /* PING's way; samples to the next grain; RAND: to the next jump */
    uint32_t ver, rng;
    uint8_t slot;
} gr_v_t;
static gr_v_t gr_v[NPART][NVOICE];
static float gr_fb[NPART][GR_FBN], gr_acc[NPART][CTL], gr_lp[NPART];
static uint32_t gr_fbw[NPART];
static uint8_t gr_acc_on[NPART];
static float gr_win[GW_N][1025];
static uint8_t gr_win_ok;

static void gr_win_init(void)
{
    uint32_t w, i;
    for (w = 0; w < GW_N; w++)
        for (i = 0; i <= 1024u; i++) {
            float x = (float)i / 1024.0f, y;
            switch (w) {
            case GW_HANN: y = 0.5f - 0.5f * cosf(6.2831853f * x); break;
            case GW_TRI: y = 1.0f - fabsf(2.0f * x - 1.0f); break;
            case GW_FLAT: y = x < 0.15f ? 0.5f - 0.5f * cosf(3.14159265f * x / 0.15f)
                            : x > 0.85f ? 0.5f - 0.5f * cosf(3.14159265f * (1.0f - x) / 0.15f) : 1.0f; break;
            case GW_PERC: y = (x < 0.02f ? x / 0.02f : expf(-5.0f * (x - 0.02f))) * (1.0f - x * x * x * x); break;
            default: y = (x > 0.98f ? (1.0f - x) / 0.02f : expf(-5.0f * (0.98f - x))) * (1.0f - (1.0f - x) * (1.0f - x) * (1.0f - x) * (1.0f - x)); break;
            }
            gr_win[w][i] = y;
        }
    gr_win_ok = 1;
}
static inline gr_v_t *gr_vs(const track_t *t, const voice_t *v)
{
    return &gr_v[(uint32_t)(t - trk) % NPART][(uint32_t)(v - t->v) % NVOICE];
}
static inline float gr_rand(uint32_t *s)              /* -1 .. 1 */
{
    *s = *s * 1664525u + 1013904223u;
    return (float)(int32_t)*s * (1.0f / 2147483648.0f);
}
/* a LIVE frame (frames since boot), mono, linear; 0 outside the 8 s kept */
static inline float gr_live(uint32_t src, double at)
{
    double ago = (double)smp_live_w - 1.0 - at;
    uint32_t i;
    float f;
    const int16_t *a, *b;
    if (ago < 1.0 || ago >= (double)(SMP_LIVEF - 2u))
        return 0.0f;
    i = (uint32_t)ago;
    f = (float)(ago - (double)i);
    a = smp_live_at(src, i);
    b = smp_live_at(src, i + 1u);
    return 0.5f * ((float)(a[0] + a[1]) * (1.0f - f) + (float)(b[0] + b[1]) * f);
}
static inline float gr_slot(const smp_obj_t *o, double at)   /* a slot's frame, mono, linear */
{
    int64_t i = (int64_t)floor(at);
    float f = (float)(at - (double)i), a, b;
    if (i < 0 || i + 1 >= (int64_t)o->frames)
        return 0.0f;
    a = (float)o->work[0][i] + (float)o->work[1][i];
    b = (float)o->work[0][i + 1] + (float)o->work[1][i + 1];
    return 0.5f * (a + (b - a) * f);
}
static inline float gr_fbat(uint32_t ti, float ago)   /* the fed-back line, ago samples back */
{
    float x = (float)gr_fbw[ti] - ago + (float)GR_FBN * 2.0f;
    uint32_t i = (uint32_t)x, j;
    float f = x - (float)i;
    i %= GR_FBN;
    j = (i + 1u) % GR_FBN;
    return gr_fb[ti][i] + (gr_fb[ti][j] - gr_fb[ti][i]) * f;
}

static const param_desc_t GR_STRETCH = {"STRTCH", F_BIPCT, -64, 63, 0, 0, 0};
static const param_desc_t GR_JUMPS = {"JUMPS", F_BIPCT, -64, 63, 0, 0, 0};
static const param_desc_t GR_EMPTY = {"EMPTY", F_ENUM, 0, 15, 0, N_SMP_SLOT, 0};
static const param_desc_t GR_LIVEPOS = {"AGO", F_PCT, 0, 127, 0, 0, 0};
static const param_desc_t *gr_desc(const track_t *t, uint32_t k)
{
    uint32_t tr = (uint32_t)clamp(t->p[P_GXTRAV], 0, GT_N - 1), src = (uint32_t)clamp(t->p[P_GXSRC], 0, GS_N - 1);
    if (k == 0u && src == GS_SLOT && !smp_get((uint32_t)clamp(t->p[P_E0], 0, 15)))
        return &GR_EMPTY;
    if (k == 1u && src != GS_SLOT)
        return &GR_LIVEPOS;
    if (k == 2u)
        return tr == GT_STRETCH ? &GR_STRETCH : tr == GT_RAND ? &GR_JUMPS : 0;
    return 0;
}
static const param_desc_t GR_X[8] = {
    {"TRAVEL", F_ENUM, 0, GT_N - 1, 0, N_GR_TRAVEL, 0},
    {"REGION", F_PCT, 0, 127, 127, 0, 0},
    {"FEEDBK", F_PCT, 0, 127, 0, 0, 0},
    {"SHIMMR", F_ENUM, 0, 4, 0, N_GR_SHIM, 0},
    {"WINDOW", F_ENUM, 0, GW_N - 1, 0, N_GR_WIN, 0},
    {"WIDTH", F_PCT, 0, 127, 64, 0, 0},
    {"SOURCE", F_ENUM, 0, GS_N - 1, 0, N_GR_SRC, 0},
    {"REC", F_ENUM, 0, SMP_RECN - 1, 0, N_SMP_REC, 0},
};
static const param_desc_t GR_QUANTD = {"QUANT", F_ENUM, 0, 2, 0, N_GR_QUANT, 0};
static const param_desc_t GR_DAMPD = {"DAMP", F_PCT, 0, 127, 127, 0, 0};
static const param_desc_t *gr_xdesc(const track_t *t, uint32_t id)
{
    if (id == P_GXQUANT)
        return &GR_QUANTD;
    if (id == P_GXDAMP)
        return &GR_DAMPD;
    if (id == P_GXREC && smp_rec.owner == (uint8_t)(t - trk) && smp_rec.st != SR_IDLE)
        return &SMP_RECD[smp_rec.st == SR_REC];
    return &GR_X[(id - P_LN0) & 7u];
}

/* a block, before the voices: the side cleared, last block's cloud into the fed-back line, REC */
static void gr_block(track_t *t)
{
    uint32_t ti = (uint32_t)(t - trk) % NPART, i;
    float fb = (float)clamp(t->p[P_GXFB], 0, 127) / 127.0f, k = 0.05f + 0.95f * (float)clamp(t->p[P_GXDAMP], 0, 127) / 127.0f;
    smp_side_begin(t);
    if (!gr_win_ok)
        gr_win_init();
    if (gr_acc_on[ti] || fb > 0.0f) {
        for (i = 0; i < CTL; i++) {
            float x = gr_acc[ti][i];
            gr_lp[ti] += (x - gr_lp[ti]) * k;
            x = 24000.0f * tanhf(gr_lp[ti] * 0.97f / 24000.0f);   /* (the limit: it never runs away) */
            gr_fb[ti][gr_fbw[ti]] = smp_finite(x);
            gr_fbw[ti] = (gr_fbw[ti] + 1u) % GR_FBN;
            gr_acc[ti][i] = 0.0f;
        }
        gr_lp[ti] = smp_finite(gr_lp[ti]);
        gr_acc_on[ti] = 0;
    }
    smp_rec_param(t, P_GXREC, (uint32_t)clamp(t->p[P_E0], 0, 15));
}

/* the region [*a, *b) of the source (slot frames), its length n */
static void gr_region(const track_t *t, double n, const vmod_t *m, double *a, double *b)
{
    double pos = (double)clamp(t->p[P_E1] * 256 + (m->shape - (64 << 8)), 0, 127 << 8) / (double)(128 << 8);
    double s = pos * n, r = (double)clamp(t->p[P_GXREG], 1, 127) / 127.0 * (n - s);
    *a = s;
    *b = s + (r > 64.0 ? r : 64.0);
    if (*b > n)
        *b = n;
}

static void gr_note_on(track_t *t, voice_t *v)
{
    gr_v_t *G = gr_vs(t, v);
    uint32_t slot = (uint32_t)clamp(t->p[P_E0], 0, 15);
    G->live = 0;
    G->slot = (uint8_t)slot;
    G->ver = smp_slot[slot].ver;
    G->rng = G->rng * 69069u + v->note * 2654435761u + (uint32_t)(v - t->v) * 40503u + 1u;
    G->dir = 1.0f;
    G->due = 0.0f;
    G->jump = 0.0f;
    G->ph = -1.0;                                     /* (placed by the first render, where its region is) */
}

static void gr_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    gr_v_t *G = gr_vs(t, v);
    const int16_t *p = t->p;
    uint32_t ti = (uint32_t)(t - trk) % NPART, i, k;
    uint32_t src = (uint32_t)clamp(p[P_GXSRC], 0, GS_N - 1), tr = (uint32_t)clamp(p[P_GXTRAV], 0, GT_N - 1);
    uint32_t win = (uint32_t)clamp(p[P_GXWIN], 0, GW_N - 1), quant = (uint32_t)clamp(p[P_GXQUANT], 0, 2);
    const smp_obj_t *o = src == GS_SLOT ? smp_get(G->slot) : 0;
    float scan = (float)clamp(p[P_E2], -64, 63) / 64.0f, ratio = smp_ratio(m, 0), size, dens, spray, spread, rev, width;
    float fb = (float)clamp(p[P_GXFB], 0, 127) / 127.0f, shim = GR_SHIM_R[clamp(p[P_GXSHIM], 0, 4)], norm, g;
    double a, b, len, speed = 0.0;
    if (src == GS_SLOT && (!o || G->ver != smp_slot[G->slot].ver)) {
        if (G->ver != smp_slot[G->slot].ver)
            G->live = 0;                              /* (the slot changed under it: its grains go) */
        if (!o)
            return;
        G->ver = smp_slot[G->slot].ver;
    }
    if (o) {
        gr_region(t, (double)o->frames, m, &a, &b);
    } else {                                          /* LIVE: the place is how long ago (POS: 50 ms .. 5.5 s) */
        double now = (double)smp_live_w, ago = 2205.0 + (double)clamp(p[P_E1], 0, 127) / 127.0 * 5.45 * FS;
        a = now - ago;
        b = now - 2205.0;
    }
    len = b - a;
    size = 0.005f * powf(100.0f, (float)clamp(p[P_E3], 0, 127) / 127.0f) * (float)FS;   /* 5 ms .. 500 ms */
    dens = powf(200.0f, (float)clamp(p[P_E4], 0, 127) / 127.0f);                       /* 1 .. 200 a second */
    spray = (float)clamp(p[P_E5], 0, 127) / 127.0f * 0.5f * (float)FS;
    spread = (float)clamp(p[P_E6], 0, 127) / 127.0f * 12.0f;
    rev = (float)clamp(p[P_E7], 0, 127) / 127.0f;
    width = (float)clamp(p[P_GXWIDTH], 0, 127) / 127.0f;
    norm = 1.0f / sqrtf(1.0f + dens * size / (float)FS);   /* (the overlap: about the same level at any density) */
    switch (tr) {                                     /* the playhead's speed (a sample) */
    case GT_FWD: speed = scan * fabsf(scan) * 4.0f; break;
    case GT_REV: speed = -scan * fabsf(scan) * 4.0f; break;
    case GT_PING: speed = fabsf(scan) * fabsf(scan) * 4.0f * G->dir; break;
    case GT_STRETCH: speed = scan >= 0.0f ? 1.0f + scan * 3.0f : 1.0f + scan * 0.9f; break;   /* 0.1x .. 4x */
    default: speed = 0.0; break;
    }
    if (!o && tr != GT_FREEZE && tr != GT_KEY)        /* LIVE: the playhead rides on now (a granular delay) */
        speed = 0.0;
    if (G->ph < 0.0 || (!o && tr != GT_FREEZE) || tr == GT_KEY) {
        G->ph = tr == GT_KEY ? a + len * (double)clamp((int32_t)v->note - 36, 0, 48) / 48.0 : a;
        if (!o && tr != GT_KEY)
            G->ph = a;
    }
    if (!o && tr == GT_FREEZE && (double)smp_live_w - G->ph > 5.5 * FS)
        G->ph = a;                                    /* (LIVE FREEZE: what it holds about to go: held anew) */
    for (i = 0; i < n; i++) {
        float y = 0.0f, side = 0.0f;
        uint64_t lv = G->live;
        if (o && tr != GT_FREEZE && tr != GT_KEY) {   /* the playhead travels its region */
            if (tr == GT_RAND) {
                G->jump -= 1.0f;
                if (G->jump <= 0.0f) {
                    G->ph = a + len * (0.5 + 0.5 * gr_rand(&G->rng));
                    G->jump = (float)FS / (0.25f + 8.0f * (scan + 1.0f) * (scan + 1.0f));
                }
            } else {
                G->ph += speed;
                if (G->ph >= b || G->ph < a) {
                    if (tr == GT_PING) {
                        G->dir = -G->dir;
                        speed = -speed;
                        G->ph = G->ph >= b ? b - 1.0 : a;
                    } else {
                        G->ph = G->ph >= b ? a + fmod(G->ph - b, len) : b - fmod(a - G->ph, len) - 1.0;
                    }
                }
            }
        }
        G->due -= 1.0f;
        if (G->due <= 0.0f) {                         /* a new grain, if there is room */
            G->due += (float)FS / dens;
            if (G->due < 1.0f)
                G->due = 1.0f;
            if (lv != (1ull << GR_N) - 1u) {
                gr_grain_t *q;
                float st = spread * gr_rand(&G->rng), r;
                for (k = 0; k < GR_N && ((lv >> k) & 1u); k++)
                    ;
                if (k < GR_N) {
                    q = &G->g[k];
                    if (quant == 1u)
                        st = 12.0f * floorf(st / 12.0f + 0.5f);
                    else if (quant == 2u) {
                        static const float Q5[7] = {-12.0f, -5.0f, 0.0f, 7.0f, 12.0f, 19.0f, 24.0f};
                        uint32_t j = (uint32_t)((gr_rand(&G->rng) * 0.5f + 0.5f) * (1.0f + 6.0f * spread / 12.0f));
                        st = spread > 0.0f ? Q5[j > 6u ? 6u : j] : 0.0f;
                    }
                    r = ratio * powf(2.0f, st / 12.0f);
                    q->len = (uint32_t)size;
                    q->age = 0;
                    q->inc = (gr_rand(&G->rng) * 0.5f + 0.5f) < rev ? -r : r;
                    q->pos = G->ph + (double)(spray * gr_rand(&G->rng));
                    if (q->inc < 0.0f)
                        q->pos += (double)((float)q->len * r);
                    if (o) {
                        if (q->pos < 0.0) q->pos = 0.0;
                        if (q->pos >= (double)o->frames) q->pos = (double)o->frames - 1.0;
                    } else if (q->pos > (double)smp_live_w - 2.0 - (double)((float)q->len * r)) {
                        q->pos = (double)smp_live_w - 2.0 - (double)((float)q->len * r);   /* (never past now) */
                    }
                    q->finc = r * shim;
                    q->fpos = (float)CTL + 1.0f + (float)q->len * (q->finc > 1.0f ? q->finc : 1.0f) +
                              (0.5f + 0.5f * gr_rand(&G->rng)) * 0.25f * (float)FS;   /* (a past it can read whole) */
                    if (q->fpos > (float)(GR_FBN - 4u)) q->fpos = (float)(GR_FBN - 4u);
                    q->pan = width * gr_rand(&G->rng);
                    q->gain = norm;
                    lv |= 1ull << k;
                }
            }
        }
        for (k = 0; lv >> k; k++) {                   /* the grains */
            gr_grain_t *q = &G->g[k];
            float w, s, x;
            uint32_t wi;
            if (!((lv >> k) & 1u))
                continue;
            x = (float)q->age * 1024.0f / (float)(q->len ? q->len : 1u);
            wi = (uint32_t)x;
            w = wi >= 1024u ? 0.0f : gr_win[win][wi] + (gr_win[win][wi + 1u] - gr_win[win][wi]) * (x - (float)wi);
            s = o ? gr_slot(o, q->pos) : gr_live(src == GS_IN ? 0u : 1u, q->pos);
            if (fb > 0.0f) {
                s = s * (1.0f - 0.7f * fb) + gr_fbat(ti, q->fpos) * fb;
                q->fpos -= q->finc - 1.0f;
                if (q->fpos < 2.0f) q->fpos = 2.0f;
                if (q->fpos > (float)(GR_FBN - 4u)) q->fpos = (float)(GR_FBN - 4u);
            }
            s *= w * q->gain;
            y += s;
            side -= s * q->pan;
            q->pos += (double)q->inc;
            if (++q->age >= q->len)
                lv &= ~(1ull << k);
        }
        G->live = lv;
        if (fb > 0.0f) {
            gr_acc[ti][i] += y;
            gr_acc_on[ti] = 1;
        }
        g = (float)amp_at(m, i) * (float)VOICE_FS * (1.0f / (32768.0f * 32768.0f));
        out[i] += (int32_t)smp_finite(y * g);
        if (width > 0.0f)
            smp_side_add(i, (int32_t)smp_finite(side * g));
    }
}

/* {SLOT, POS, SCAN, SIZE, DENSITY, SPRAY, SPREAD, REVERSE}; the extras by preset (GR_PRESET_X) */
static const preset_t GR_PRESETS[] = {
    {"CLOUD", {0, 0, 8, 70, 70, 20, 0, 0}, {40, 100, 127, 70}, 0, TAG(PAD), FX(0, 20, 10, 50), PAT(5)},
    {"FROZEN CLOUD", {0, 40, 0, 80, 85, 10, 2, 0}, {60, 100, 127, 90}, 0, TAG(PAD), FX(0, 20, 0, 70), PAT(5)},
    {"SCAN", {0, 0, 20, 60, 70, 5, 0, 0}, {20, 100, 127, 60}, 0, TAG(PAD), FX(0, 10, 10, 40), PAT(5)},
    {"STRETCH 0.5X", {0, 0, -36, 75, 80, 4, 0, 0}, {10, 100, 127, 60}, 0, TAG(PAD), FX(0, 10, 0, 40), PAT(5)},
    {"STRETCH 2X", {0, 0, 21, 65, 80, 4, 0, 0}, {10, 100, 127, 60}, 0, TAG(PAD), FX(0, 10, 0, 40), PAT(5)},
    {"SHIMMER", {0, 30, 4, 90, 80, 30, 0, 0}, {80, 100, 127, 110}, 0, TAG(PAD), FX(0, 20, 20, 90), PAT(5)},
    {"SPRAY", {0, 20, 0, 40, 100, 90, 30, 30}, {20, 100, 127, 60}, 0, TAG(FX), FX(0, 10, 10, 50), PAT(5)},
    {"OCTAVES", {0, 30, 4, 70, 80, 20, 60, 0}, {30, 100, 127, 80}, 0, TAG(PAD), FX(0, 20, 10, 60), PAT(5)},
    {"REVERSE", {0, 0, 10, 80, 60, 10, 0, 127}, {20, 100, 127, 60}, 0, TAG(FX), FX(0, 10, 10, 50), PAT(5)},
    {"GLITCH", {0, 0, 40, 20, 90, 40, 20, 40}, {0, 100, 127, 30}, 0, TAG(FX), FX(0, 0, 20, 30), PAT(3)},
    {"KEY POSITION", {0, 0, 0, 55, 70, 4, 0, 0}, {10, 100, 127, 50}, 0, TAG(KEYS), FX(0, 10, 0, 40), PAT(6)},
    {"LIVE DELAY", {0, 30, 0, 60, 60, 20, 0, 0}, {10, 100, 127, 80}, 0, TAG(FX), FX(0, 0, 0, 40)},
    {"LIVE FREEZE", {0, 10, 0, 85, 90, 15, 0, 0}, {40, 100, 127, 100}, 0, TAG(FX), FX(0, 10, 0, 60)},
    {"FEEDBACK", {0, 10, 6, 70, 75, 20, 4, 10}, {30, 100, 127, 90}, 0, TAG(FX), FX(0, 10, 10, 60), PAT(5)},
};
/* {TRAVEL, REGION, FEEDBK, SHIMMR, WINDOW, WIDTH, SOURCE, REC} and {QUANT, DAMP} of each preset */
static const uint8_t GR_PRESET_X[NELEM(GR_PRESETS)][10] = {
    {GT_FWD, 127, 0, 0, GW_HANN, 64, GS_SLOT, 0, 0, 127},
    {GT_FREEZE, 127, 0, 0, GW_HANN, 90, GS_SLOT, 0, 0, 127},
    {GT_FWD, 127, 0, 0, GW_HANN, 60, GS_SLOT, 0, 0, 127},
    {GT_STRETCH, 127, 0, 0, GW_HANN, 40, GS_SLOT, 0, 0, 127},
    {GT_STRETCH, 127, 0, 0, GW_HANN, 40, GS_SLOT, 0, 0, 127},
    {GT_PING, 100, 70, 1, GW_HANN, 100, GS_SLOT, 0, 0, 100},
    {GT_RAND, 127, 0, 0, GW_TRI, 110, GS_SLOT, 0, 0, 127},
    {GT_FWD, 127, 0, 0, GW_HANN, 90, GS_SLOT, 0, 1, 127},
    {GT_REV, 127, 0, 0, GW_SWELL, 60, GS_SLOT, 0, 0, 127},
    {GT_RAND, 127, 0, 0, GW_PERC, 100, GS_SLOT, 0, 2, 127},
    {GT_KEY, 127, 0, 0, GW_FLAT, 50, GS_SLOT, 0, 0, 127},
    {GT_FWD, 127, 30, 0, GW_HANN, 90, GS_MST, 0, 0, 90},
    {GT_FREEZE, 127, 0, 0, GW_HANN, 100, GS_IN, 0, 0, 127},
    {GT_FWD, 127, 90, 2, GW_HANN, 80, GS_SLOT, 0, 0, 80},
};

static const engine_t ENG_GRAIN = {
    .name = "GRAIN",
    .page_title = {"GRAIN", "CLOUD"},
    .edit = {
        {"SLOT", F_ENUM, 0, 15, 0, N_SMP_SLOT, 0},
        {"POS", F_PCT, 0, 127, 0, 0, 0},
        {"SCAN", F_BIPCT, -64, 63, 0, 0, 0},
        {"SIZE", F_PCT, 0, 127, 64, 0, 0},
        {"DENSE", F_PCT, 0, 127, 70, 0, 0},
        {"SPRAY", F_PCT, 0, 127, 10, 0, 0},
        {"SPREAD", F_PCT, 0, 127, 0, 0, 0},
        {"REVRS", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = GR_PRESETS,
    .npresets = NELEM(GR_PRESETS),
    .note_on = gr_note_on,
    .render = gr_render,
    .desc = gr_desc,
    .block = gr_block,
    .knob = {P_E1, P_E2, P_E3, P_E4},
    .sampled = 1,
};
