/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.34, the owner: "organic, self-modulating voices") LIFE. (V10.7.44, the owner: "LIFE does too little": JIANT
 * BYTE's design, "the formulas talk") HOME > LIFE's four: LIFE (how alive it all is), TALK (how much each track hears
 * the others: their sound in its phase, FM between the tracks), BREAK (how much the others' sound breaks it: its CRUSH
 * rises while they play), WEB (who hears whom: 0 each the one before it in the ring, 100 all of them each other).
 * Included by mod.c (macro_master calls life_master); voice.c (track_render), fx.c (mix_part, track_follow_peak,
 * crush_block) call it. All its state is in the pool (life; params.c life_v).
 *
 *   The weights: w[i][j], how much track i hears track j (Q8, slewed a block): the tracks that sound (not muted, their
 *     sound over a floor), the one before i in their ring 256, every other WEB's share of it.
 *   TALK: up to LPM_N tracks at a time (the ones that hear the most; a kit is never one) have their sum through a short
 *     line of their own (LPM_L) whose delay the others' samples of the block before move, each by its weight, each
 *     against its own follower (its peaks swing the whole depth, however loud it plays): a phase modulation of every
 *     partial by its frequency, as FM; one let go fades out. The depth LIFE x TALK's curve (life_depth: ~5 % at 12, ~27 % at 50, all at 100: +-LPM_D
 *     samples, three times that where the others pile up, the line's end the limit), ramped (no click).
 *   BREAK: each track's CRUSH (fx.c crush_block) up by what it hears (the others' followers x the weights, block-rate),
 *     up to ~100 at LIFE x BREAK 100; back as they go quiet. Every engine has it.
 * LIFE 0: none of it runs on the sound; every render bit for bit as without LIFE (tests/life_test.c). */
#define LIFE_POOL __attribute__((section(".pool")))
#define LIFE_SZ __attribute__((noinline, minsize))     /* (once a block: its size, not its pace, counts) */
#define LPM_L 64u                                       /* the cross-FM lines (a power of 2) */
#define LPM_N 3u                                        /* the tracks that hear at a time (the CPU's) */
#define LPM_D (24 << 8)                                 /* the depth at LIFE x TALK 100, Q8 samples */
#define LPM_ST 96                                       /* a line's depth ramp a block, Q8 */
typedef struct {
    int16_t en[NTRK];                                   /* each track's energy, slow (0..32767) */
    int16_t fo[NTRK];                                   /* .. its follower (fast up, ~50 ms down) */
    volatile int32_t pk[NTRK];                          /* each track's peak this block (fx.c mix_part) */
    int16_t w[NTRK][NTRK];                              /* w[i][j]: how much i hears j (Q8) */
    uint8_t brk[NTRK];                                  /* BREAK: each track's CRUSH on top (0..100) */
    uint8_t pres, on, kit, blk, tapm, pmc, sel;         /* (tapm: the tracks whose samples are kept; pmc: the ones that
                                                         * hear; sel: the ones chosen to) */
    int32_t dc[NTRK], b[NTRK];                          /* each one's line: its depth (ramped), the one played last (Q8) */
    uint32_t last[NTRK];                                /* .. the block it last ran (a gap: the line from silence) */
    uint8_t wp[NTRK];                                   /* .. its write point */
    int32_t ln[NTRK][LPM_L];                            /* .. the line */
    uint32_t tap_blk[2][NTRK];                          /* the block each one's mix was kept in (by its parity) */
    int16_t tap[2][NTRK][CTL];                          /* .. the mix it hears (Q12: 4096 a source's peak; +-3 x that) */
} life_t;
static life_t life LIFE_POOL;

/* TALK's curve: the depth (Q10, 0 .. 1024) at a = LIFE x TALK (0..100): a (0.4 + 0.6 a^2), heard from the bottom */
static int32_t life_depth(int32_t a)
{
    a = clamp(a, 0, 100);
    return a * (4000 + a * a * 3 / 5) * 64 / 62500;
}
/* (mod.c macro_master, once a block, before the parts) the followers, the weights, BREAK, the lines */
static LIFE_SZ void life_master(void)
{
    uint32_t i, j, pres = 0, tapm = 0, pmc = 0, sw = !(++life.blk & 7u);   /* (the ring and the lines every 8th block) */
    int32_t e[NTRK], hr[NTRK], dep, web, amt, bm;
    for (i = 0; i < NTRK; i++) {                        /* the follower, the energy (the page draws them too) */
        int32_t en = life.en[i];
        life.fo[i] = (int16_t)follow_step(life.fo[i], clamp(life.pk[i], 0, 32767));
        life.pk[i] = 0;
        e[i] = clamp(life.fo[i] * 2, 0, 32767);
        en += e[i] > en ? (e[i] - en) >> 3 : -((en + 1023) >> 10);   /* (up in ~10 ms, down over ~1.5 s) */
        life.en[i] = (int16_t)en;
        if (i < trk_live && !trk[i].p[P_MUTE])
            pres |= (uint32_t)(en > 600) << i;
        if (sw)                                         /* (the kits) */
            life.kit = (uint8_t)((life.kit & ~(1u << i)) | (uint32_t)(trk[i].engine == ENGI_DRUM) << i);
    }
    if (sw)
        life.pres = (uint8_t)pres;
    pres = life.pres;
    life.on = life_v[LIFE_AMT] > 0;
    if (!life.on && !life.pmc) {                        /* (LIFE 0, the lines out: no one hears, nothing on the sound) */
        memset(life.w, 0, sizeof life.w);
        memset(life.brk, 0, sizeof life.brk);
        life.tapm = life.sel = 0;
        return;
    }
    web = life.on ? life_v[LIFE_WEB] * 256 / 100 : 0;
    amt = life_v[LIFE_AMT] * life_v[LIFE_BRK] / 100;    /* BREAK at LIFE: its most (0..100), steeper at the top */
    bm = amt * (amt + 60) / 160;
    for (i = 0; i < NTRK; i++) {                        /* track i: who it hears, how much */
        uint32_t pred = i;
        int32_t h = 0;
        for (j = 1; j < NTRK; j++) {                    /* (the one before it in the ring of those that sound) */
            pred = pred ? pred - 1u : NTRK - 1u;
            if ((pres >> pred) & 1u) break;
        }
        for (j = 0; j < NTRK; j++) {
            int32_t tg = j == i || !life.on || !((pres >> i) & (pres >> j) & 1u) ? 0 : j == pred ? 256 : web,
                    w = life.w[i][j];
            w += tg > w ? (tg - w + 3) >> 2 : -((w - tg + 3) >> 2);   /* (~5 ms; LIFE 0: at once) */
            life.w[i][j] = (int16_t)(life.on ? w : 0);
            h += (w * e[j]) >> 8;
        }
        hr[i] = (pres >> i) & 1u ? clamp(h, 0, 32767) : 0;
        life.brk[i] = (uint8_t)((bm * (hr[i] < 16384 ? hr[i] : 16384)) >> 14);
        if ((life.kit >> i) & 1u)                       /* (a kit never hears by TALK) */
            hr[i] = 0;
    }
    amt = life_v[LIFE_AMT] * life_v[LIFE_TALK] / 100;
    dep = (LPM_D * life_depth(amt)) >> 10;
    for (i = 0; sw && i < NTRK; i++) {                  /* the ones that hear the most, LPM_N */
        uint32_t r = 0;
        for (j = 0; j < NTRK; j++)
            r += hr[j] > hr[i] || (hr[j] == hr[i] && j < i);
        life.sel = (uint8_t)((life.sel & ~(1u << i)) | (uint32_t)(r < LPM_N && hr[i]) << i);
    }
    for (i = 0; i < NTRK; i++) {                        /* each one's line: its depth ramped to the target (out: to 0) */
        if (life.last[i] != mns_clk - 1u)               /* (it did not sound last block: from the start when it does) */
            life.b[i] = 0;
        life.dc[i] += clamp(((life.sel >> i) & 1u ? dep : 0) - life.dc[i], -LPM_ST, LPM_ST);
        if (life.dc[i] || life.b[i]) {
            pmc |= 1u << i;
            for (j = 0; j < NTRK; j++)
                tapm |= (uint32_t)(life.w[i][j] > 0) << j;
        }
    }
    life.tapm = (uint8_t)tapm;
    life.pmc = (uint8_t)pmc;
}

/* (fx.c track_follow_peak) track ti's samples of this block into the mix each one that hears it hears next block, by its
 * weight, against its own follower */
static __attribute__((noinline)) void life_tap(uint32_t ti, const int32_t *b, uint32_t n, int32_t level)
{
    uint32_t i, k, par = mns_clk & 1u;
    if (n > CTL) n = CTL;
    for (k = 0; k < NTRK; k++) {
        int16_t *d = life.tap[par][k];
        int32_t g = life.w[k][ti];
        if (!((life.pmc >> k) & 1u) || g <= 0)
            continue;
        if (life.tap_blk[par][k] != mns_clk) {          /* (the block's first: from 0) */
            memset(d, 0, sizeof life.tap[0][0]);
            life.tap_blk[par][k] = mns_clk;
        }
        g = ((level * g) << 4) / (life.fo[ti] > 512 ? life.fo[ti] : 512);
        for (i = 0; i < n; i++)
            d[i] = (int16_t)clamp(d[i] + (((b[i] >> 7) * g) >> 5), -12288, 12288);
    }
}

/* (voice.c track_render) TALK: track ti's sum through its line, the delay moved by the mix it hears of the block before
 * (a phase modulation: every partial by its frequency, as FM); its depth ramped */
static __attribute__((noinline)) void life_pm(uint32_t ti, int32_t *out, uint32_t n)
{
    uint32_t i, w, par = (mns_clk - 1u) & 1u;
    int32_t b0, b1 = life.dc[ti], db, g, *ln = life.ln[ti];
    const int16_t *s = life.tap[par][ti];
    if (n > CTL) n = CTL;
    g = life.tap_blk[par][ti] == mns_clk - 1u;          /* (no one heard last block: the centre alone) */
    if (life.last[ti] != mns_clk - 1u) {                /* (starting, or after a gap: the line from silence) */
        memset(ln, 0, sizeof life.ln[0]);
        life.b[ti] = 0;
    }
    life.last[ti] = mns_clk;
    b0 = life.b[ti];
    db = (b1 - b0) >> CTL_LOG2;
    w = life.wp[ti];
    for (i = 0; i < n; i++, w++, b0 += db) {
        int32_t d = b0 + (((g ? s[i] : 0) * b0) >> 12), f, x0;
        uint32_t r;
        ln[w & (LPM_L - 1u)] = out[i];
        if ((uint32_t)d > (uint32_t)(LPM_L - 2u) << 8)
            d = d < 0 ? 0 : (int32_t)(LPM_L - 2u) << 8;
        r = w + LPM_L - (uint32_t)(d >> 8);
        f = d & 255;
        x0 = ln[r & (LPM_L - 1u)];
        out[i] = x0 + (((ln[(r - 1u) & (LPM_L - 1u)] - x0) * f) >> 8);
    }
    life.b[ti] = b1;
    life.wp[ti] = (uint8_t)(w & (LPM_L - 1u));
}

/* the state back to its start (the tests' power-on; a project's LIFE is in life_v) */
static void life_reset(void) { memset(&life, 0, sizeof life); }
