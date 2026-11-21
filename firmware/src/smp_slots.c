/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (ports/, FELUCCA_HOST_POWER only) the sample slots of the big CPUs: 16 slots, each a sample object of up to 60 s at
 * 44.1 kHz, mono or stereo, as int16 planes; what the SAMPLE and GRAIN engines (eng_sample.c, eng_grain.c) and DRUM-X's
 * sample layer (eng_drum.c dxs_*) play, the recorder (REC: the device's input, the master, a track) and the rolling
 * LIVE buffers GRAIN reads. Included by engines.c, never on the FM-1.
 *
 * Memory: a sample object (jiant_sample, ports/core) is one block the host side makes on any thread (resampled to
 * 44.1 kHz there) and hands to the audio thread whole: jiant_slot_put swaps a pointer, never copies. Each holds the
 * original PCM and a working copy of it (what plays: DESTROY writes into it, never into the original). An object a
 * slot lets go of goes to a retired ring that jiant_service (ports/core, any one other thread) frees; the recorder
 * records into a spare object jiant_service keeps ready. So the audio path allocates nothing, locks nothing, and the
 * only shared words are the ring's indices and the spare's pointer (atomics).
 *
 * The voices hold a slot number and a version, never an object across blocks: a slot changed under a voice (a load,
 * a recording) ends it. */
#include <math.h>
#define SMP_NSLOT 16u
#define SMP_MAXF (60u * FS)               /* frames a slot holds at most (60 s) */
#define SMP_NRET 64u                      /* the retired ring */
#define SMP_LIVEF (8u * FS)               /* the LIVE buffers: the last 8 s of the input and of the master */
#define SMP_INF 8192u                     /* the input's ring (jiant_input), frames */

typedef struct jiant_sample {             /* a sample object (ports/core makes and frees it; one allocation) */
    uint32_t frames, cap, ch;             /* frames held, frames room for, channels 1 / 2 */
    int16_t *orig[2], *work[2];           /* the planes (mono: [1] is [0]); work: what plays */
    char name[32];
} smp_obj_t;

typedef struct {
    smp_obj_t *o;                         /* 0: empty */
    uint32_t ver;                         /* every swap (a voice playing it stops) */
    uint32_t gen, saved;                  /* changed (recorded, destroyed) since the host saved it: gen != saved */
} smp_slot_t;
static smp_slot_t smp_slot[SMP_NSLOT];

static smp_obj_t *smp_ret[SMP_NRET];
static uint32_t smp_ret_w, smp_ret_r, smp_ret_lost;   /* (atomics: the audio thread writes w, jiant_service r) */
static smp_obj_t *smp_spare;                          /* (atomic: jiant_service sets it, the recorder takes it) */

static void smp_retire(smp_obj_t *o)
{
    uint32_t w, r;
    if (!o)
        return;
    w = __atomic_load_n(&smp_ret_w, __ATOMIC_RELAXED);
    r = __atomic_load_n(&smp_ret_r, __ATOMIC_ACQUIRE);
    if (w - r >= SMP_NRET) {                          /* (full: no service running; it leaks, the sound goes on) */
        smp_ret_lost++;
        return;
    }
    smp_ret[w % SMP_NRET] = o;
    __atomic_store_n(&smp_ret_w, w + 1u, __ATOMIC_RELEASE);
}

/* slot s now holds o (0: empty); the one it held retired */
static void smp_put(uint32_t s, smp_obj_t *o)
{
    smp_slot_t *S = &smp_slot[s % SMP_NSLOT];
    smp_obj_t *old = S->o;
    S->o = o;
    S->ver++;
    S->gen = S->saved = 0;                            /* (a load is what the host has: nothing to save) */
    if (old != o)
        smp_retire(old);
}
static inline const smp_obj_t *smp_get(uint32_t s)
{
    const smp_obj_t *o = smp_slot[s % SMP_NSLOT].o;
    return o && o->frames ? o : 0;
}
static inline void smp_changed(uint32_t s)            /* recorded or destroyed: the host saves it */
{
    smp_slot_t *S = &smp_slot[s % SMP_NSLOT];
    if (S->gen == S->saved)
        S->gen++;
}

/* a cubic (Catmull-Rom) read of plane c at pos (frames, any real), 0 outside */
static inline float smp_px(const int16_t *d, int64_t i, uint32_t n) { return i >= 0 && i < (int64_t)n ? (float)d[i] : 0.0f; }
static inline float smp_read(const smp_obj_t *o, uint32_t c, double pos)
{
    const int16_t *d = o->work[c & 1u];
    int64_t i = (int64_t)floor(pos);
    float f = (float)(pos - (double)i), a, b, cc, e;
    if (i >= 1 && i + 2 < (int64_t)o->frames) {
        a = d[i - 1]; b = d[i]; cc = d[i + 1]; e = d[i + 2];
    } else {
        a = smp_px(d, i - 1, o->frames); b = smp_px(d, i, o->frames);
        cc = smp_px(d, i + 1, o->frames); e = smp_px(d, i + 2, o->frames);
    }
    return b + 0.5f * f * (cc - a + f * (2.0f * a - 5.0f * b + 4.0f * cc - e + f * (3.0f * (b - cc) + e - a)));
}
/* DESTROY: x (int16 scale) mixed into the working copy at frame i by amount g (0..1) */
static inline void smp_scar(smp_obj_t *o, uint32_t i, float xl, float xr, float g)
{
    float a, b;
    if (i >= o->frames)
        return;
    a = (float)o->work[0][i] * (1.0f - g) + xl * g;
    o->work[0][i] = (int16_t)(a > 32767.0f ? 32767.0f : a < -32768.0f ? -32768.0f : a);
    if (o->ch > 1u) {
        b = (float)o->work[1][i] * (1.0f - g) + xr * g;
        o->work[1][i] = (int16_t)(b > 32767.0f ? 32767.0f : b < -32768.0f ? -32768.0f : b);
    }
}
static inline int16_t smp_i16(float x)                /* rounded, held to int16 */
{
    return (int16_t)(x >= 32767.0f ? 32767.0f : x <= -32768.0f ? -32768.0f : x >= 0.0f ? x + 0.5f : x - 0.5f);
}
static inline float smp_finite(float x) { return x == x && x < 1e9f && x > -1e9f ? x : 0.0f; }

/* ---------------------------------------------------------------- the stereo side (a part's) --- *
 * A part renders one mono sum (voice.c); a stereo sample or GRAIN's spread adds a side signal here (the part's scale:
 * L = mid + side, R = mid - side), fx.c mix_part takes it at the part's LEVEL and PAN. The engine's block hook clears
 * it; a part with DIST or a FILTER on its sum mixes none (they shape the mid only: mono then) */
static int32_t smp_side[CTL];
static uint8_t smp_side_on, smp_side_trk;
static void smp_side_begin(const track_t *t)
{
    uint32_t i;
    for (i = 0; i < CTL; i++)
        smp_side[i] = 0;
    smp_side_on = 0;
    smp_side_trk = (uint8_t)(t - trk);
}
static inline void smp_side_add(uint32_t i, int32_t s)
{
    smp_side[i] += s;
    smp_side_on = 1;
}

/* ------------------------------------------------------------- input, taps, the LIVE buffers --- */
static int16_t smp_in[SMP_INF][2];
static uint32_t smp_in_w, smp_in_r;
static int16_t smp_live[2][SMP_LIVEF][2];             /* [0] the input, [1] the master: the last 8 s */
static uint32_t smp_live_w;                           /* frames written since boot */
static int16_t smp_tap[NPART][CTL];                   /* each part's output this block (fx.c mix_part) */
static uint32_t smp_tap_blk[NPART], smp_blk = 1;

/* the device's audio input, stereo at FS (the host's, each block: jiant_input) */
static void smp_input(const float *l, const float *r, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        float a = l ? smp_finite(l[i]) : 0.0f, b = r ? smp_finite(r[i]) : a;
        if (smp_in_w - smp_in_r >= SMP_INF)
            smp_in_r++;                               /* (full: the oldest goes) */
        smp_in[smp_in_w % SMP_INF][0] = smp_i16(a * 32768.0f);
        smp_in[smp_in_w % SMP_INF][1] = smp_i16(b * 32768.0f);
        smp_in_w++;
    }
}
/* a part's output (its mono sum after DIST, unity level), fx.c mix_part */
static void smp_track_tap(uint32_t ti, const int32_t *b, uint32_t n)
{
    uint32_t i;
    if (ti >= NPART || n > CTL)
        return;
    for (i = 0; i < n; i++)
        smp_tap[ti][i] = (int16_t)clamp(b[i], -32768, 32767);
    smp_tap_blk[ti] = smp_blk;
}
static inline const int16_t *smp_live_at(uint32_t src, uint32_t ago)   /* frame ago frames back (< SMP_LIVEF) */
{
    return smp_live[src & 1u][(smp_live_w - 1u - ago) % SMP_LIVEF];
}

/* ---------------------------------------------------------------------------- the recorder --- *
 * One at a time, from a source (SR_IN the input, SR_MST the master, SR_T1 + k track k), into a slot: armed, it starts
 * on the next bar (at once with the transport stopped) and records bars bars at the tempo (0: until stopped, or the
 * 60 s), then the slot holds it (changed: the host saves it). The host (jiant_rec_*) or an engine's REC (owner: the
 * track) starts it */
enum { SR_IDLE, SR_ARMED, SR_REC };
enum { SR_IN, SR_MST, SR_T1 };
#define SR_NSRC (SR_T1 + NPART)
#define SR_HOST 0xFFu
static struct {
    uint8_t st, slot, src, owner, err;
    uint32_t bars, len, pos, bar;
    smp_obj_t *b;                                     /* the object it records into (kept when cancelled) */
} smp_rec;
static uint32_t beat_samples(void);                   /* (fx.c) */
static uint32_t ms_clock;                             /* (mod.c: samples since PLAY on the grid; its definition) */
static uint32_t smp_bar_len(void) { uint32_t q = beat_samples(); return (q ? q : FS / 2u) * 4u; }

/* 0 armed; 1 busy, 2 no room to record into (no spare yet: jiant_service), 3 bad arguments */
static int smp_rec_arm(uint32_t slot, uint32_t src, uint32_t bars, uint32_t owner)
{
    if (slot >= SMP_NSLOT || src >= SR_NSRC)
        return 3;
    if (smp_rec.st != SR_IDLE)
        return 1;
    if (!smp_rec.b)
        smp_rec.b = __atomic_exchange_n(&smp_spare, (smp_obj_t *)0, __ATOMIC_ACQ_REL);
    if (!smp_rec.b) {
        smp_rec.err = 2;
        return 2;
    }
    smp_rec.slot = (uint8_t)slot;
    smp_rec.src = (uint8_t)src;
    smp_rec.owner = (uint8_t)owner;
    smp_rec.bars = bars;
    smp_rec.pos = 0;
    smp_rec.err = 0;
    smp_rec.bar = song.playing ? ms_clock / smp_bar_len() : 0u;
    smp_rec.st = SR_ARMED;
    return 0;
}
static void smp_rec_finish(void)
{
    smp_obj_t *b = smp_rec.b;
    static const char *const SRC[SR_NSRC] = {"REC IN", "REC MST", "REC T1", "REC T2", "REC T3", "REC T4", "REC T5",
                                             "REC T6"};
    smp_rec.st = SR_IDLE;
    if (!b || !smp_rec.pos)
        return;                                       /* (nothing recorded: the object stays ours, for the next) */
    b->frames = smp_rec.pos;
    b->ch = smp_rec.src >= SR_T1 ? 1u : 2u;
    if (b->ch == 1u) {
        b->orig[1] = b->orig[0];
        b->work[1] = b->work[0];
    }
    str_cpy(b->name, SRC[smp_rec.src % SR_NSRC], sizeof b->name);
    smp_rec.b = 0;
    smp_put(smp_rec.slot, b);
    smp_slot[smp_rec.slot].gen = 1;                   /* (recorded: the host saves it) */
    smp_slot[smp_rec.slot].saved = 0;
}
static void smp_rec_stop(void)
{
    if (smp_rec.st == SR_REC)
        smp_rec_finish();
    smp_rec.st = SR_IDLE;
}

/* every audio block after the mix (audio.c audio_block): out the master, stereo Q15 interleaved. The input taken from
 * its ring, the LIVE buffers written, the recorder run */
static void smp_audio_block(const int32_t *out, uint32_t n)
{
    uint32_t i, ti = (uint32_t)(smp_rec.src - SR_T1);
    int16_t in[CTL][2];
    if (n > CTL)
        n = CTL;
    for (i = 0; i < n; i++) {
        if (smp_in_r != smp_in_w) {
            in[i][0] = smp_in[smp_in_r % SMP_INF][0];
            in[i][1] = smp_in[smp_in_r % SMP_INF][1];
            smp_in_r++;
        } else {
            in[i][0] = in[i][1] = 0;
        }
        smp_live[0][(smp_live_w + i) % SMP_LIVEF][0] = in[i][0];
        smp_live[0][(smp_live_w + i) % SMP_LIVEF][1] = in[i][1];
        smp_live[1][(smp_live_w + i) % SMP_LIVEF][0] = (int16_t)clamp(out[2u * i], -32768, 32767);
        smp_live[1][(smp_live_w + i) % SMP_LIVEF][1] = (int16_t)clamp(out[2u * i + 1u], -32768, 32767);
    }
    smp_live_w += n;
    if (smp_rec.st == SR_ARMED) {                     /* the next bar (stopped: now) */
        uint32_t bl = smp_bar_len(), b = song.playing ? ms_clock / bl : 0u;
        if (!song.playing || b != smp_rec.bar) {
            smp_rec.len = smp_rec.bars ? smp_rec.bars * bl : SMP_MAXF;
            if (smp_rec.len > smp_rec.b->cap)
                smp_rec.len = smp_rec.b->cap;
            smp_rec.pos = 0;
            smp_rec.st = SR_REC;
        }
        smp_rec.bar = b;
    }
    if (smp_rec.st == SR_REC) {
        smp_obj_t *B = smp_rec.b;
        for (i = 0; i < n && smp_rec.pos < smp_rec.len; i++) {
            int16_t l, r;
            if (smp_rec.src == SR_IN) {
                l = in[i][0];
                r = in[i][1];
            } else if (smp_rec.src == SR_MST) {
                l = (int16_t)clamp(out[2u * i], -32768, 32767);
                r = (int16_t)clamp(out[2u * i + 1u], -32768, 32767);
            } else {
                l = r = ti < NPART && smp_tap_blk[ti] == smp_blk ? smp_tap[ti][i] : 0;
            }
            B->orig[0][smp_rec.pos] = B->work[0][smp_rec.pos] = l;
            B->orig[1][smp_rec.pos] = B->work[1][smp_rec.pos] = r;
            smp_rec.pos++;
        }
        if (smp_rec.pos >= smp_rec.len)
            smp_rec_finish();
    }
    smp_blk++;
}
