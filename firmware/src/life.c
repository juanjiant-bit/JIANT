/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.34, the owner: "organic, self-modulating voices") LIFE. (V10.7.43, the owner: "cut it as much as possible: what
 * matters is the tracks really FM-ing each other, and a little of the followers on each engine's own parameters") Included
 * by mod.c (its sources, its block in macro_master); voice.c (track_render, voice_block) and fx.c (mix_part,
 * track_follow_peak) call it. All its state is in the pool (life, params.c life_v).
 *
 *   The beings (HOME > LIFE draws them): each track's swims in the water at a pace its sound sets (agitated loud, calm in
 *     silence; SWIM the pace) and breathes; their places are the MATRIX's BEING, the breath its LIFE. Who moves whom: each
 *     track is held most by the one before it in the ring of tracks that sound (two: a duet, each the other's), a little
 *     by the rest, more when their beings swim close (the weights slewed). Once a block, from each track's peak of the
 *     block before (fx.c mix_part's own loop: no scan of its own).
 *   CROSS-FM (X-FM): up to LPM_N pairs at a time, the strongest (a weight x the one moving's sound): the moved track's sum
 *     goes through a short line (LPM_L) whose delay the other track's samples of the block before move, a phase modulation
 *     of every partial by its frequency, as FM: on every engine (a kit is never the one moved: it moves). Depth: LIFE x X-FM
 *     x the track's LIFE x the weight, up to +-LPM_D samples at a full-scale sample; ramped (no click), the line's delay
 *     inside it. The pairs chosen every 8th block; one let go fades out before its slot takes another.
 *   FOLLOW: each track's follower (its peak, followed) onto its engine's own parameters, per voice, once a block: the
 *     others' sound on its CUTOFF (a kit's ducks it, any other opens it), its own on its SHAPE (each engine's timbre:
 *     WEST's FOLD, FM6's modulators, PHASE's depth, STRING's brightness ..; not ANALOG's and WHEEL's, whose plain sound
 *     a SHAPE would make dearer).
 * LIFE 0 (or a track's LIFE 0): none of it runs on the sound; every render bit for bit as before (tests/life_test.c). */
#define LIFE_POOL __attribute__((section(".pool")))
#define LIFE_SZ __attribute__((noinline, minsize))     /* (once a block, a part: its size, not its pace, counts) */
#define LPM_L 32u                                       /* the cross-FM line (a power of 2) */
#define LPM_N 2u                                        /* the pairs at a time */
#define LPM_D (13 << 8)                                 /* the deepest swing, Q8 samples (the line's delay 0 .. 2 x) */
#define LPM_ST 48                                       /* a pair's depth ramp a block, Q8 (~0.2 s to the deepest) */
typedef struct {
    int32_t ln[LPM_L];                                  /* the line */
    int32_t b, k;                                       /* its centre (Q8 samples) and gain (Q12 x Q8) played last */
    int32_t dt;                                         /* the depth it ramps to (Q8 samples), dc the one it plays now */
    int32_t dc;
    uint32_t last;                                      /* (the block it last ran: a gap, the line from silence) */
    uint8_t car, src, wp;                               /* the one moved, the one moving (NTRK: free; | 0x80 let go); the
                                                         * line's write point */
} lpm_t;
typedef struct {
    int32_t bx[NTRK], by[NTRK];                         /* the beings' places in the water, 0..65535 (y down) */
    uint32_t bclk[NTRK];                                /* each one's clock: its swim, its breath (quicker loud) */
    int16_t en[NTRK];                                   /* each track's energy, slow (0..32767) */
    int16_t fo[NTRK];                                   /* .. its follower (fast up, ~50 ms down) */
    int16_t breath[NTRK];                               /* the LIFE source (bipolar) */
    int16_t w[NTRK][NTRK];                              /* w[i][j]: how much track j moves track i, Q15 (slewed) */
    int16_t xc[NTRK], xh[NTRK];                         /* track i's this block, at its LIFE 100 %: cutoff, shape (<< 8) */
    volatile int32_t pk[NTRK];                          /* each track's peak this block (fx.c mix_part) */
    uint8_t dir[NTRK];                                  /* the swim's bounces (bit 0 x, bit 1 y) */
    uint8_t pres, on, init, kit, blk, tapm, pmc;        /* (tapm: the tracks whose samples are kept; pmc: the ones moved) */
    lpm_t pm[LPM_N];
    uint32_t tap_blk[2][LPM_N];                         /* the block each pair's samples were kept in (by its parity) */
    int32_t tlv[2][LPM_N];                              /* .. the one moving's LEVEL then (Q12) */
    int32_t tap[2][LPM_N][CTL];                         /* .. its samples */
} life_t;
static life_t life LIFE_POOL;

/* drift_noise (mod.c), out of line */
static __attribute__((noinline)) int32_t life_noise(uint32_t seed, uint32_t clk, uint32_t lg) { return drift_noise(seed, clk, lg); }
/* track ti's LIFE as it receives it this block (0..256): the global's gate, then its own with the matrix's (lof) */
static int32_t life_pt(uint32_t ti, int32_t lof)
{
    int32_t v = clamp(life_v[LIFE_N + ti % NTRK] + lof, 0, 100);
    return life_v[LIFE_AMT] > 0 ? (v * 655) >> 8 : 0;
}
/* the sources: LIFE the breath, BEING its height (up +) */
static LIFE_SZ int32_t life_src(const track_t *t, uint32_t s)
{
    uint32_t i = (uint32_t)(t - trk) % NTRK;
    return s == MS_LIFE ? life.breath[i] : clamp(32767 - life.by[i], -32768, 32767);
}

/* being i's swim and breath (life_master, every 8th block): a smooth wander at the pace of its sound, off the walls */
static LIFE_SZ void life_swim(uint32_t i, int32_t en)
{
    uint32_t k, c = life.bclk[i] += ((8u * (24u + ((uint32_t)en >> 8))) * (uint32_t)(life_v[LIFE_SWM] + 10)) / 40u;   /* (loud:
                                                         * ~6 x the pace; SWIM 30 x1, 0 a quarter, 100 x2.75) */
    for (k = 0; k < 2u; k++) {                          /* x, then y */
        int32_t *q = k ? &life.by[i] : &life.bx[i];
        int32_t v = (life_noise(i * 977u + k * 0x3b0u + 11u, c >> 4, 11u) * (96 + (en >> 7))) >> 17;
        *q += (life.dir[i] >> k) & 1u ? -v : v;
        if (*q < 4000 || *q > 61535) {
            life.dir[i] ^= (uint8_t)(1u << k);
            *q = clamp(*q, 4000, 61535);
        }
    }
    life.breath[i] = (int16_t)life_noise(i * 31u + 7u, c >> 3, 9u);
}
/* (mod.c macro_master, once a block, before the parts) the beings, the weights, the pairs, each track's followers */
static LIFE_SZ void life_master(void)
{
    uint32_t i, j, k, pres = 0, tapm = 0, pmc = 0, ph = ++life.blk & 7u, sw = ph == 7u;   /* (each being's swim every 8th
                                                         * block, one a block in turn; the weights and the pairs in the 8th) */
    int32_t e[NTRK], best[LPM_N], bi[LPM_N], bj[LPM_N];
    int32_t fol = (life_v[LIFE_AMT] * life_v[LIFE_FOL] * 105) >> 10;   /* (FOLLOW at this LIFE, Q10: 0..1025) */
    int32_t xfm = (life_v[LIFE_AMT] * life_v[LIFE_XFM] * 105) >> 10;   /* (X-FM at this LIFE, Q10) */
    if (!life.init) {                                   /* (where they start: two rows of three) */
        static const uint16_t X0[NTRK] = {12000, 32000, 52000, 22000, 42000, 60000};
        life.init = 1;
        for (i = 0; i < NTRK; i++) {
            life.bx[i] = X0[i];
            life.by[i] = i < 3u ? 16000 : 46000;
        }
        for (k = 0; k < LPM_N; k++)
            life.pm[k].car = (uint8_t)NTRK;
    }
    for (i = 0; i < NTRK; i++) {                        /* the follower, the energy, the swim (always: the page, the sources) */
        int32_t en = life.en[i];
        life.fo[i] = (int16_t)follow_step(life.fo[i], clamp(life.pk[i], 0, 32767));
        life.pk[i] = 0;
        e[i] = clamp(life.fo[i] * 2, 0, 32767);
        en += e[i] > en ? (e[i] - en) >> 3 : -((en + 1023) >> 10);   /* (up in ~10 ms, down over ~1.5 s) */
        life.en[i] = (int16_t)en;
        if (i < trk_live && !trk[i].p[P_MUTE])
            pres |= (uint32_t)(en > 600) << i;
        if (ph == i)
            life_swim(i, en);
    }
    if (sw) {
        life.pres = (uint8_t)pres;
        for (i = 0, k = 0; i < NTRK; i++)               /* (the kits) */
            k |= (uint32_t)(trk[i].engine == ENGI_DRUM) << i;
        life.kit = (uint8_t)k;
    }
    pres = life.pres;
    life.on = life_v[LIFE_AMT] > 0;
    if (!life.on) {                                     /* (LIFE 0: nothing on the sound) */
        life.tapm = life.pmc = 0;
        for (k = 0; k < LPM_N; k++)
            life.pm[k].car = (uint8_t)NTRK, life.pm[k].dc = 0;
        return;
    }
    for (k = 0; k < LPM_N; k++)
        best[k] = 0, bi[k] = bj[k] = NTRK;
    for (i = 0; i < NTRK; i++) {                        /* track i, moved by the others */
        uint32_t pred = i, mj = NTRK;
        int32_t cs = 0, mx = 0;
        if ((ph & 3u) != 3u)                            /* (the followers on the voices every 4th block, ~6 ms: they
                                                         * move slowly; the weights and the pairs in the 8th) */
            break;
        life.xc[i] = life.xh[i] = 0;
        if (sw) {
            for (k = 1; k < NTRK; k++) {                /* the one before it in the ring of those that sound */
                pred = pred ? pred - 1u : NTRK - 1u;
                if ((pres >> pred) & 1u) break;
            }
            for (j = 0; j < NTRK; j++) {
                int32_t tg = 0, w = life.w[i][j], d;
                if (j != i && ((pres >> j) & 1u)) {     /* (the nearer their beings, the more: touching 1, far 0.4) */
                    d = life.bx[i] - life.bx[j];
                    tg = d < 0 ? -d : d;
                    d = life.by[i] - life.by[j];
                    tg = 32767 - clamp((tg + (d < 0 ? -d : d)) * 5 / 8, 0, 32767);
                    tg = ((j == pred ? 32767 : 9830) * (13107 + ((19660 * tg) >> 15))) >> 15;
                }
                w += (tg - w) >> 3;                     /* (~90 ms) */
                w += tg && !w;
                life.w[i][j] = (int16_t)w;
            }
        }
        if (!e[i])                                      /* (it silent: nothing to move) */
            continue;
        for (j = 0; j < NTRK; j++) {
            int32_t d = (life.w[i][j] * e[j]) >> 15;
            cs += (life.kit >> j) & 1u ? -d : d;        /* (a kit ducks the filter, a voice opens it) */
            if (j != i && d > mx)
                mx = d, mj = j;
        }
        life.xc[i] = (int16_t)((clamp(cs, -32767, 32767) * fol) >> 12);   /* (+-32 steps << 8 at 100 %) */
        life.xh[i] = (int16_t)((e[i] * fol * 3) >> 14);  /* (its own sound on its SHAPE: up to +24 steps << 8) */
        if (sw && xfm && !((life.kit >> i) & 1u) && mj < NTRK)   /* (a pair candidate: the strongest of its movers) */
            for (k = 0; k < LPM_N; k++)
                if (mx > best[k]) {
                    for (j = LPM_N - 1u; j > k; j--)
                        best[j] = best[j - 1u], bi[j] = bi[j - 1u], bj[j] = bj[j - 1u];
                    best[k] = mx, bi[k] = (int32_t)i, bj[k] = (int32_t)mj;
                    break;
                }
    }
    for (k = 1; k < LPM_N && life_v[LIFE_AMT] <= 50; k++)   /* (one pair up to LIFE 50, more past it: the CPU's) */
        bi[k] = NTRK;
    for (k = 0; k < LPM_N; k++) {                       /* the pairs: each slot's target, who it is */
        lpm_t *q = &life.pm[k];
        int32_t dt = 0;
        if (sw) {
            for (j = 0; j < LPM_N && !(bi[j] == q->car && bj[j] == q->src); j++) {}
            if (j < LPM_N)
                bi[j] = NTRK;                           /* (kept: the same pair) */
            else if (!q->dc && !q->b) {                 /* (faded out: free for the next) */
                q->car = (uint8_t)NTRK;
                for (j = 0; j < LPM_N && bi[j] == NTRK; j++) {}
                if (j < LPM_N) {
                    q->car = (uint8_t)bi[j], q->src = (uint8_t)bj[j];
                    bi[j] = NTRK;
                }
            } else
                q->src |= 0x80u;                        /* (let go: fading out) */
        }
        if (q->car < NTRK && !(q->src & 0x80u))
            dt = (((LPM_D * xfm) >> 10) * life.w[q->car][q->src]) >> 15;
        if (q->last != mns_clk - 1u)                    /* (it did not sound last block: from the start when it does) */
            q->b = q->k = 0;
        q->dt = dt;
        q->dc += clamp(dt - q->dc, -LPM_ST, LPM_ST);
        if (q->car < NTRK && (q->dc || q->b)) {
            pmc |= 1u << q->car;
            tapm |= 1u << (q->src & 7u);
        } else if (q->src & 0x80u)
            q->car = (uint8_t)NTRK, q->src = 0;
    }
    life.tapm = (uint8_t)tapm;
    life.pmc = (uint8_t)pmc;
}

/* (fx.c track_follow_peak) track ti's samples of this block, kept for the cross-FM of the next */
static __attribute__((noinline)) void life_tap(uint32_t ti, const int32_t *b, uint32_t n, int32_t level)
{
    uint32_t k, par = mns_clk & 1u;
    if (n > CTL) n = CTL;
    for (k = 0; k < LPM_N; k++)
        if ((life.pm[k].src & 7u) == ti && life.pm[k].car < NTRK) {
            int32_t *d = life.tap[par][k];
            memcpy(d, b, n * sizeof *b);
            memset(d + n, 0, (CTL - n) * sizeof *b);
            life.tap_blk[par][k] = mns_clk;
            life.tlv[par][k] = level;
        }
}

/* (voice.c track_render) the block's LIFE of track ti: the followers on its voices (c: vctx_t's lcut lshp); its LIFE
 * (0..256) returned */
static LIFE_SZ int32_t life_ctx(uint32_t ti, int32_t *c)
{
    static const uint16_t SHP = (1u << ENGI_FM6) | (1u << ENGI_PHASE) | (1u << ENGI_WEST) | (1u << ENGI_STRING) |
                                (1u << ENGI_ADD) | (1u << ENGI_VOICE) | (1u << ENGI_LOFI) | (1u << ENGI_ANIMAL);   /* (its SHAPE
                                                         * free: not ANALOG's, WHEEL's cheap plain sound) */
    int32_t lpt = life_pt(ti, MOD.on ? MOD.lof : 0);
    uint32_t e = trk[ti % NTRK].engine;
    ti %= NTRK;
    c[0] = (life.xc[ti] * lpt) >> 8;
    c[1] = e < 16u && ((SHP >> e) & 1u) ? (life.xh[ti] * lpt) >> 8 : 0;
    return lpt;
}

/* (voice.c track_render) the cross-FM: track ti's sum through its pair's line, the delay moved by the other track's
 * samples of the block before (a phase modulation: every partial by its frequency, as FM); centre and gain ramped */
static __attribute__((noinline)) void life_pm(uint32_t ti, int32_t *out, uint32_t n, int32_t lpt)
{
    uint32_t i, k, w, par = (mns_clk - 1u) & 1u;
    for (k = 0; k < LPM_N; k++) {
        lpm_t *q = &life.pm[k];
        int32_t b0, k0, b1, k1, db, dk, *ln = q->ln;
        const int32_t *s = life.tap[par][k];
        if (q->car != ti)
            continue;
        if (n > CTL) n = CTL;
        b1 = (q->dc * lpt) >> 8;                        /* (the centre: the swing's depth, so 0 .. 2 x) */
        k1 = life.tap_blk[par][k] == mns_clk - 1u ? (b1 * life.tlv[par][k]) >> 12 : 0;   /* (its samples of the block
                                                         * before, at their LEVEL: a full-scale one swings the depth) */
        if (q->last != mns_clk - 1u) {                  /* (starting, or after a gap: the line from silence) */
            memset(ln, 0, sizeof q->ln);
            q->b = q->k = 0;
        }
        q->last = mns_clk;
        b0 = q->b; k0 = q->k;
        db = (b1 - b0) >> CTL_LOG2; dk = (k1 - k0) >> CTL_LOG2;
        w = q->wp;
        for (i = 0; i < n; i++, w++, b0 += db, k0 += dk) {
            int32_t d = b0 + (((s[i] >> 3) * (k0 >> 2)) >> 10), f, x0;
            uint32_t r;
            ln[w & (LPM_L - 1u)] = out[i];
            if ((uint32_t)d > (uint32_t)(LPM_L - 2u) << 8)
                d = d < 0 ? 0 : (int32_t)(LPM_L - 2u) << 8;
            r = w + LPM_L - (uint32_t)(d >> 8);
            f = d & 255;
            x0 = ln[r & (LPM_L - 1u)];
            out[i] = x0 + (((ln[(r - 1u) & (LPM_L - 1u)] - x0) * f) >> 8);
        }
        q->b = b1; q->k = k1;
        q->wp = (uint8_t)(w & (LPM_L - 1u));
    }
}

/* the state back to its start (the tests' power-on; a project's LIFE is in life_v) */
static void life_reset(void) { memset(&life, 0, sizeof life); }
