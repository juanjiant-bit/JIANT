/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.34, the owner: "organic, self-modulating voices") LIFE. (V10.7.43, the owner: "LIFE: one parameter, FM
 * intermodulation between the tracks, cheap; a logarithmic curve, fine at the bottom, extreme noise at the top")
 * Included by mod.c (macro_master calls life_master); voice.c (track_render) and fx.c (mix_part, track_follow_peak)
 * call it. All its state is in the pool (life, params.c life_v: LIFE_AMT alone is read).
 *
 *   The ring: the tracks that sound (not muted, their sound over a floor), in their order; each is moved by the one
 *     before it in the ring (two: a duet, each the other's). A kit moves the next one, it is never the one moved.
 *   CROSS-FM: up to LPM_N pairs at a time (one up to LIFE 50, two past it: the CPU's), the moving tracks the loudest:
 *     the moved track's sum goes through a short line (LPM_L) whose delay the other track's samples of the block before
 *     move: a phase modulation of every partial by its frequency, as FM. Its depth LIFE's curve (life_depth: 2^(LIFE /
 *     12.5) - 1 of 255, ~1 % at 25, ~6 % at 50, ~25 % at 75, all at 100: +-LPM_D samples at the other's peak), ramped (no
 *     click). The pairs chosen every 8th block; one let go fades out before its slot takes another.
 *   Nothing else: no voice's pitch, filter or shape, no send, no parameter, no matrix source or destination.
 * LIFE 0: none of it runs on the sound; every render bit for bit as without LIFE (tests/life_test.c). */
#define LIFE_POOL __attribute__((section(".pool")))
#define LIFE_SZ __attribute__((noinline, minsize))     /* (once a block: its size, not its pace, counts) */
#define LPM_L 32u                                       /* the cross-FM line (a power of 2) */
#define LPM_N 2u                                        /* the pairs at a time */
#define LPM_D (14 << 8)                                 /* the deepest swing, Q8 samples (the line's delay 0 .. 2 x) */
#define LPM_ST 48                                       /* a pair's depth ramp a block, Q8 */
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
    int16_t en[NTRK];                                   /* each track's energy, slow (0..32767) */
    int16_t fo[NTRK];                                   /* .. its follower (fast up, ~50 ms down) */
    volatile int32_t pk[NTRK];                          /* each track's peak this block (fx.c mix_part) */
    uint8_t pres, on, init, kit, blk, tapm, pmc;        /* (tapm: the tracks whose samples are kept; pmc: the ones moved) */
    lpm_t pm[LPM_N];
    uint32_t tap_blk[2][LPM_N];                         /* the block each pair's samples were kept in (by its parity) */
    int32_t tlv[2][LPM_N];                              /* .. the one moving's LEVEL then (Q12) */
    int32_t tap[2][LPM_N][CTL];                         /* .. its samples */
} life_t;
static life_t life LIFE_POOL;

/* LIFE's curve: the depth (Q10, 0 .. 1024) at LIFE a (0..100): (2^(a / 12.5) - 1) / 255, the fraction by a table of 2^ */
static int32_t life_depth(int32_t a)
{
    static const uint16_t X[8] = {256, 279, 304, 332, 362, 395, 431, 470};   /* 2^(k / 8), Q8 */
    int32_t q = clamp(a, 0, 100) * 16 / 25;             /* (a / 12.5 in eighths: 0 .. 64) */
    int32_t v = ((int32_t)X[q & 7] << (q >> 3)) - 256;   /* (2^(q / 8) - 1, Q8: 0 .. 65280) */
    return v * 1024 / 65280;
}
/* (mod.c macro_master, once a block, before the parts) the followers, the ring, the pairs */
static LIFE_SZ void life_master(void)
{
    uint32_t i, j, k, pres = 0, tapm = 0, pmc = 0, sw = !(++life.blk & 7u);   /* (the ring and the pairs every 8th block) */
    int32_t e[NTRK], best[LPM_N], bi[LPM_N], bj[LPM_N], dep;
    if (!life.init) {
        life.init = 1;
        for (k = 0; k < LPM_N; k++)
            life.pm[k].car = (uint8_t)NTRK;
    }
    for (i = 0; i < NTRK; i++) {                        /* the follower, the energy (the page draws them too) */
        int32_t en = life.en[i];
        life.fo[i] = (int16_t)follow_step(life.fo[i], clamp(life.pk[i], 0, 32767));
        life.pk[i] = 0;
        e[i] = clamp(life.fo[i] * 2, 0, 32767);
        en += e[i] > en ? (e[i] - en) >> 3 : -((en + 1023) >> 10);   /* (up in ~10 ms, down over ~1.5 s) */
        life.en[i] = (int16_t)en;
        if (i < trk_live && !trk[i].p[P_MUTE])
            pres |= (uint32_t)(en > 600) << i;
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
    dep = (LPM_D * life_depth(life_v[LIFE_AMT])) >> 10;
    for (k = 0; k < LPM_N; k++)
        best[k] = 0, bi[k] = bj[k] = NTRK;
    for (i = 0; sw && i < NTRK; i++) {                  /* track i, moved by the one before it in the ring */
        uint32_t pred = i;
        if (!((pres >> i) & 1u) || ((life.kit >> i) & 1u) || !e[i])
            continue;
        for (k = 1; k < NTRK; k++) {
            pred = pred ? pred - 1u : NTRK - 1u;
            if ((pres >> pred) & 1u) break;
        }
        if (pred == i || !e[pred])
            continue;
        for (k = 0; k < LPM_N; k++)                     /* (the loudest movers first) */
            if (e[pred] > best[k]) {
                for (j = LPM_N - 1u; j > k; j--)
                    best[j] = best[j - 1u], bi[j] = bi[j - 1u], bj[j] = bj[j - 1u];
                best[k] = e[pred], bi[k] = (int32_t)i, bj[k] = (int32_t)pred;
                break;
            }
    }
    for (k = 1; k < LPM_N && life_v[LIFE_AMT] <= 50; k++)   /* (one pair up to LIFE 50, more past it: the CPU's) */
        bi[k] = NTRK;
    for (k = 0; k < LPM_N; k++) {                       /* the pairs: each slot's target, who it is */
        lpm_t *q = &life.pm[k];
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
        if (q->last != mns_clk - 1u)                    /* (it did not sound last block: from the start when it does) */
            q->b = q->k = 0;
        q->dt = q->car < NTRK && !(q->src & 0x80u) ? dep : 0;
        q->dc += clamp(q->dt - q->dc, -LPM_ST, LPM_ST);
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

/* (voice.c track_render) the block's LIFE of track ti (0..256: the cross-FM's share on it; V10.7.43, the tracks' own LIFE
 * gone: whole) */
static LIFE_SZ int32_t life_ctx(uint32_t ti) { (void)ti; return 256; }

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
        k1 = life.tap_blk[par][k] == mns_clk - 1u ?     /* (its samples of the block before, against its own follower:
                                                         * its peaks swing the whole depth, however loud it plays) */
             ((b1 * life.tlv[par][k]) / (life.fo[q->src & 7u] > 512 ? life.fo[q->src & 7u] : 512)) << 3 : 0;
        if (q->last != mns_clk - 1u) {                  /* (starting, or after a gap: the line from silence) */
            memset(ln, 0, sizeof q->ln);
            q->b = q->k = 0;
        }
        q->last = mns_clk;
        b0 = q->b; k0 = q->k;
        db = (b1 - b0) >> CTL_LOG2; dk = (k1 - k0) >> CTL_LOG2;
        w = q->wp;
        for (i = 0; i < n; i++, w++, b0 += db, k0 += dk) {
            int32_t d = b0 + (((s[i] >> 7) * (k0 >> 2)) >> 6), f, x0;   /* (V10.7.43: s >> 7: |s| 2^19.7 x k0 / 4
                                                         * (2^17.7 at a follower's floor, a loud attack) within 32 bits) */
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
