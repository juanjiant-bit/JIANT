/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLICER: a per-track insert, a tempo-synced 16-step gate / stutter. It works on the track's dry
 * mono signal after DIST and before LEVEL / PAN / the sends (fx.c mix_part), so the sends follow
 * the chopped sound.
 *   P_SLCR    OFF / GATE / STUT
 *   P_SLPAT   one of SL_NPAT patterns of 16 steps ('x' live / open, '.' gated / repeated)
 *   P_SLRATE  the step: 1/8, 1/16, 1/32, 8T, 16T, 32T
 *   P_SLDEPTH GATE: how far a '.' step closes (100 % = silent); STUT: the level of the repeat
 * GATE: the gain moves at most 1 / SL_RAMP per sample (2.9 ms from open to closed): a closing
 *   ramp ends on the step boundary, an opening one starts on it.
 * STUT: an 'x' step plays live and is recorded (22.05 kHz, 16 bit, SL_LEN samples a track; JIANT 0.6: SL_SLOTS
 *   recordings shared by the six tracks, a STUT track takes a free one at a step's start and gives it back when it
 *   leaves STUT; a fifth STUT track plays live, records nothing, and the screen says so); a '.'
 *   step plays that recording from its start, looped at the step length (halved until it fits the
 *   recording), cross-faded with the live sound by DEPTH. Every pass of the loop is windowed
 *   (SL_RAMP at both ends and at the step end), the cross-fade moves as the gate does.
 * The step clock: per track, always running (also with the SLICER OFF, so switching it on lands
 * in time), restarted with the transport (seq_start -> slicer_start: step 0 starts with the
 * sequencer's step 0), BPM and the track's + the global SWING as the sequencer has them (seq.c
 * step_samples); sample exact. With the SLICER OFF and no ramp left, the signal is not touched. */
#define SL_NPAT 16
#define SL_LEN 4096u                    /* recording, 22.05 kHz samples a track: 186 ms, 8 KB */
#define SL_RAMP_LOG2 7
#define SL_RAMP (1 << SL_RAMP_LOG2)     /* 128 samples, 2.9 ms */
#define SL_SLOPE (32768 / SL_RAMP)
enum { SL_OFF, SL_GATE, SL_STUT };      /* P_SLCR (params.c N_SLCR) */

/* bit k = step k: 1 = 'x' (open / live), 0 = '.' (closed / repeat) */
static const uint16_t SL_PAT[SL_NPAT] = {
    0x5555,   /*  1 x.x.x.x.x.x.x.x. */
    0xB6DB,   /*  2 xx.xx.xx.xx.xx.x */
    0x5249,   /*  3 x..x..x..x..x.x. */
    0xB777,   /*  4 xxx.xxx.xxx.xx.x */
    0x6D6D,   /*  5 x.xx.xx.x.xx.xx. */
    0x0F0F,   /*  6 xxxx....xxxx.... */
    0x1111,   /*  7 x...x...x...x... */
    0x54A5,   /*  8 x.x..x.x..x.x.x. */
    0x5333,   /*  9 xx..xx..xx..x.x. */
    0xA929,   /* 10 x..x.x..x..x.x.x */
    0xAAFF,   /* 11 xxxxxxxx.x.x.x.x */
    0xD501,   /* 12 x.......x.x.x.xx */
    0xAAAA,   /* 13 .x.x.x.x.x.x.x.x */
    0xAB6B,   /* 14 xx.x.xx.xx.x.x.x */
    0xBB5D,   /* 15 x.xxx.x.xx.xxx.x */
    0x7597,   /* 16 xxx.x..xx.x.xxx. */
};
static const uint8_t SL_DEN[6] = {2, 4, 8, 3, 6, 12};   /* P_SLRATE (N_SLDIV): a step = 1 / DEN beats */

#define SL_SLOTS 4u                     /* (JIANT 0.6) STUT recordings at once (GATE needs none): 32 KB, as with four
                                         * tracks; perform.c borrows all of them as the REPEAT loop */
#define SL_NONE 0xFFu
static int16_t sl_buf[SL_SLOTS][SL_LEN] __attribute__((section(".pool")));
typedef struct {
    uint32_t pos, len;           /* samples into the step, its length */
    uint32_t base;               /* the step without swing */
    uint32_t rp, loop;           /* repeat: read position, loop length (44.1 kHz samples), 0 = none */
    uint32_t rec;                /* recorded (22.05 kHz samples) */
    int32_t gc;                  /* gate closure, Q15: 0 = open */
    int32_t w;                   /* STUT cross-fade, Q15: 0 = live */
    int32_t half;                /* recording: the first sample of a pair */
    uint8_t idx;                 /* step 0..15 */
    uint8_t bit;                 /* this step's pattern bit (latched at its start) */
    uint8_t rec_on;              /* recording this step */
    uint8_t slot;                /* its sl_buf (STUT), SL_NONE: none */
} sl_t;
static sl_t sl[NTRK] = {[0 ... NTRK - 1] = {.slot = SL_NONE}};
static uint8_t sl_lent;          /* perform.c has borrowed sl_buf: STUT plays live, records nothing */

static void slicer_reset(void)   /* every track's clock and state cleared, no recording held (tests, power-on) */
{
    uint32_t k;
    memset(sl, 0, sizeof sl);
    for (k = 0; k < NTRK; k++)
        sl[k].slot = SL_NONE;
}
static void slicer_start(void)   /* seq_start: the next block starts step 0 of every track */
{
    uint32_t k;
    for (k = 0; k < NTRK; k++) {
        sl[k].idx = 15;
        sl[k].pos = sl[k].len = 0;
    }
}

static uint32_t sl_pattern(const track_t *t) { return SL_PAT[(uint32_t)(t->p[P_SLPAT] - 1) % SL_NPAT]; }

/* the step clock enters the next step, at sample at of the block. (JIANT) While the transport runs it is the
 * transport's grid (mod.c clk_n / clk_pos: the 1/16 steps since PLAY, through tempo changes and an external clock):
 * the step and the place in it from there, so a SLICER switched on, its RATE changed or the tempo moved lands on
 * the bar at once, not only after the next PLAY. Swing pairs two steps: their pair from the grid, the step in it */
static __attribute__((noinline)) void sl_enter(const track_t *t, sl_t *s, uint32_t at)
{
    uint32_t mode = (uint32_t)t->p[P_SLCR], den = SL_DEN[(uint32_t)t->p[P_SLRATE] % 6u];
    s->idx = (uint8_t)((s->idx + 1u) & 15u);
    s->base = (uint32_t)FS * 60u / (uint32_t)song.g[G_BPM] / den;
    s->pos = 0;
    if (song.playing && clk_pos != CLK_START) {
        uint32_t p = div_samples(2), q = 4u * p, a, k, r, b, rp, l0;
        a = ((clk_n % 192u) * p + clk_pos + at) * den;  /* (192 1/16s: a whole number of every RATE's 16 steps) */
        k = a / q;                                      /* the step (no swing), exact in 1/(4p den) */
        r = (a % q) / den;                              /* .. samples into it */
        b = q / den;
        s->base = b;
        l0 = swing_step_len(t, b, 0);
        rp = (k & 1u ? b : 0u) + r;                     /* into the swung pair */
        k &= ~1u;
        if (2u * b - rp <= 16u) {                       /* (a hair before a step's start: that start) */
            k += 2u;
            rp = 0;
        } else if (rp < l0 && l0 - rp <= 16u) {
            rp = l0;
        }
        s->idx = (uint8_t)((k + (rp >= l0)) & 15u);
        s->pos = rp >= l0 ? rp - l0 : rp;
    }
    s->len = swing_step_len(t, s->base, s->idx);   /* core.h, as seq.c step_samples */
    if (s->pos >= s->len)
        s->pos = s->len - 1u;
    s->bit = (uint8_t)((sl_pattern(t) >> s->idx) & 1u);
    s->rp = 0;
    s->loop = 0;
    s->rec_on = 0;
    if (mode != SL_STUT && s->slot != SL_NONE) {
        s->slot = SL_NONE;                          /* (out of STUT: its recording back) */
    } else if (mode == SL_STUT && s->slot == SL_NONE) {
        uint32_t k, used = 0;                       /* a free recording, if any */
        for (k = 0; k < NTRK; k++)
            if (sl[k].slot != SL_NONE)
                used |= 1u << sl[k].slot;
        for (k = 0; k < SL_SLOTS && ((used >> k) & 1u); k++)
            ;
        if (k < SL_SLOTS) {
            s->slot = (uint8_t)k;
            s->rec = 0;
        }
    }
    if (mode != SL_STUT || sl_lent || s->slot == SL_NONE) {
        s->rec = 0;                                 /* nothing old to repeat when STUT comes on */
    } else if (s->bit) {
        s->rec = 0;                                 /* a live step: record it */
        s->rec_on = 1;
    } else if (s->rec >= 2u * SL_RAMP) {            /* a repeat: the last live step, looped */
        uint32_t l = s->base;
        while (l > 2u * SL_LEN)
            l >>= 1;                                /* (a long step: a half, a quarter .. of it) */
        s->loop = l < 2u * s->rec ? l : 2u * s->rec;
    }
}

/* m samples of one step (no boundary inside) */
static void sl_seg(const track_t *t, sl_t *s, int16_t *buf, int32_t *b, uint32_t m)
{
    uint32_t mode = (uint32_t)t->p[P_SLCR], j, nbit = (sl_pattern(t) >> ((s->idx + 1u) & 15u)) & 1u;
    int32_t depth = t->p[P_SLDEPTH] * 258;          /* Q15, 0..32766 */
    int32_t tc = mode == SL_GATE && !s->bit ? depth : 0;           /* gate closure of this step */
    int32_t tn = mode == SL_GATE && !nbit ? depth : 0;             /* .. of the next one */
    int32_t tw = mode == SL_STUT && s->loop ? depth : 0;           /* repeat level */
    for (j = 0; j < m; j++) {
        int32_t x = b[j], y, tg = tc;
        uint32_t left = s->len - s->pos - j;        /* samples to the step end, >= 1 */
        if (left <= (uint32_t)SL_RAMP && tn > tg)
            tg = tn;                                /* close by the boundary */
        s->gc += clamp(tg - s->gc, -SL_SLOPE, SL_SLOPE);
        s->w += clamp(tw - s->w, -SL_SLOPE, SL_SLOPE);
        y = s->gc ? x - mulq16(x, (uint32_t)s->gc << 1) : x;
        if (s->rec_on) {                            /* 2:1, the pair's mean; Q15 >> 2 (the mix's headroom) */
            if ((s->pos + j) & 1u) {
                buf[s->rec] = (int16_t)clamp((s->half + x) >> 3, -32768, 32767);
                if (++s->rec >= SL_LEN)
                    s->rec_on = 0;
            } else {
                s->half = x;
            }
        }
        if (s->loop) {                              /* the repeat, windowed, read between samples */
            uint32_t rp = s->rp, e = s->loop - 1u - rp;
            int32_t r = buf[rp >> 1];
            if (rp & 1u)
                r = (r + buf[((rp >> 1) + 1u) & (SL_LEN - 1u)]) >> 1;
            r <<= 2;
            if (rp < e)
                e = rp;
            if (left - 1u < e)
                e = left - 1u;
            if (e < (uint32_t)SL_RAMP)
                r = (r * (int32_t)e) >> SL_RAMP_LOG2;
            if (++s->rp >= s->loop)
                s->rp = 0;
            y += mulq16(r - y, (uint32_t)s->w << 1);
        } else if (s->w) {
            y -= mulq16(y, (uint32_t)s->w << 1);    /* (the repeat ended on a boundary: it is 0 there) */
        }
        b[j] = y;
    }
}

/* the step clock over n samples and the SLICER on b (0: the clock only, the track is silent) */
static void slicer_track(const track_t *t, int32_t *b, uint32_t n)
{
    uint32_t k = (uint32_t)(t - trk), i = 0;
    sl_t *s = &sl[k];
    int act = b && (t->p[P_SLCR] != SL_OFF || s->gc || s->w);
    while (i < n) {
        uint32_t m;
        if (s->pos >= s->len)
            sl_enter(t, s, i);
        m = s->len - s->pos;
        if (m > n - i)
            m = n - i;
        if (act)
            sl_seg(t, s, sl_buf[s->slot < SL_SLOTS ? s->slot : 0u], b + i, m);   /* (no slot: no recording, no repeat) */
        s->pos += m;
        i += m;
    }
}

/* (JIANT 0.6) track t wants STUT but every recording is taken: it plays live (the screen says why) */
static int slicer_no_slot(const track_t *t)
{
    return t->p[P_SLCR] == SL_STUT && sl[t - trk].slot == SL_NONE;
}
/* a silent part must still be rendered: a repeat is playing or fading (fx.c mix_part) */
static int slicer_busy(const track_t *t)
{
    const sl_t *s = &sl[t - trk];
    return s->w || (t->p[P_SLCR] == SL_STUT && s->loop);
}
