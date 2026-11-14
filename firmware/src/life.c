/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.34, the owner: "organic, self-modulating voices") LIFE. Included by mod.c (its sources and destination, its
 * block in macro_master); voice.c (track_render, voice_block) and fx.c (track_follow_peak) call it. All its state is in
 * the pool (life, params.c life_v): the RAM's .bss is full and the sound's addresses stay where they were.
 *
 *   Per voice (every engine): each voice its own slow wandering, a few control-rate value noises (drift_noise, mod.c) on
 *     a clock of its own: its pitch (a few cents) and a detune of the note's own (its RAND), its cutoff, its shape (when
 *     something moves it already: a plain wave stays plain), its level. A held note has an arc: it starts with energy
 *     (the wander quicker and wider, by its velocity), blooms (the filter opens a little over ~1.5 s) and tires (from
 *     ~3 s a little darker, its breath slower). How much: LIFE (HOME > LIFE KNOB 1) x the track's LIFE (EDIT > VOICES 3
 *     KNOB 2; the matrix's LIFE on top).
 *   Between tracks (Lyra-like, before the effects): each track's dry sound of the block before (its envelope follower,
 *     mod.c track_env, and, at the AUDIO end, its samples: life_tap) moves the others: FILTER (a drum's ducks the cutoff,
 *     any other opens it), PITCH (an envelope's sag of up to ~30 ct up to 50 %; past it, audio-rate cross-FM, a phase
 *     modulation of the track's sum through a short line: life_pm) and FX (the sends breathe). Who moves whom: each track
 *     is held most by the one before it in the ring of tracks that sound (two: a duet, each the other's), a little by the
 *     rest, more when their beings swim close (the weights slewed: no step, no click). One block late: no order between
 *     the parts, no recursion (and the instability is a Lyra's). Bounded: every offset clamped, the line's delay inside it.
 *   The beings (HOME > LIFE draws them where they are here): each track's swims in the water at a pace its sound sets
 *     (agitated loud, calm in silence) and breathes; their places are the MATRIX's BEING, the breath its LIFE.
 * LIFE 0 (or a track's LIFE 0): none of it runs on the sound; every render bit for bit as before (tests/life_test.c). */
#define LIFE_POOL __attribute__((section(".pool")))
#define LIFE_SZ __attribute__((noinline, minsize))     /* (once a block, a part: its size, not its pace, counts) */
#define LPM_L 32u                                       /* the cross-FM line (a power of 2): delays 0 .. ~28 samples
                                                         * (past 25 % up to 30: a longer line, the pool's headroom) */
typedef struct {
    int32_t bx[NTRK], by[NTRK];                         /* the beings' places in the water, 0..65535 (y down) */
    uint32_t bclk[NTRK];                                /* each one's clock: its swim, its breath (quicker loud) */
    int16_t en[NTRK];                                   /* each track's energy, slow (0..32767) */
    int16_t breath[NTRK];                               /* the LIFE source (bipolar) */
    int16_t w[NTRK][NTRK];                              /* w[i][j]: how much track j moves track i, Q15 (slewed) */
    int16_t xc[NTRK], xp[NTRK], xs[NTRK], pa[NTRK];     /* track i's this block, at its LIFE 100 %: cutoff (<< 8), pitch
                                                         * (1/4096 of the increment), sends (Q15), cross-FM depth (Q15) */
    int16_t pg[NTRK];                                   /* .. the cross-FM depth it plays now (Q15, ramped a block) */
    uint8_t dir[NTRK];                                  /* the swim's bounces (bit 0 x, bit 1 y) */
    uint8_t tapm, pres, on, init, kit;                  /* (kit: the DRUM tracks) */
    uint8_t fol;                                        /* the followers LIFE alone asks for (a scan of every 4th sample) */
    uint8_t wany;                                       /* a weight not 0 (the last swim's) */                       /* the tracks whose samples are kept; that sound; LIFE on */
    uint32_t tap_blk[2][NTRK];                          /* the block each kept its samples in (by the block's parity) */
    int16_t tap[2][NTRK][CTL];                          /* .. its samples (as the follower scales them) */
    int32_t line[NTRK][LPM_L];                          /* the cross-FM lines */
    uint8_t lw[NTRK];
    uint16_t vtag[NTRK * NVOICE], vblk[NTRK * NVOICE];  /* each voice: its note (voice_t.age), its blocks since */
    uint32_t vph[NTRK * NVOICE];                        /* .. its wander's clock */
    int16_t vn[NTRK * NVOICE][3];                       /* .. its three noises (one worked out a block, in turn) */
    uint8_t blk;                                        /* (the blocks: the swim every 8th) */
    uint16_t bst;                                       /* (V10.7.35, the owner: "at 100 % absolutely extreme") LIFE past
                                                         * 25 % on everything, Q8: x1 at 25 % and under, x4 at 100 % (a
                                                         * square: the knob's top the wild end) */
} life_t;
static life_t life LIFE_POOL;

/* drift_noise (mod.c), out of line: LIFE asks it five times, each a copy inlined otherwise */
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
static int life_snd_on(const track_t *t) { return life.on && life.xs[(uint32_t)(t - trk) % NTRK]; }

/* being i's swim and breath (life_master, every 8th block): a smooth wander at the pace of its sound, off the walls */
static LIFE_SZ void life_swim(uint32_t i, int32_t en)
{
    uint32_t k, c = life.bclk[i] += 8u * (24u + ((uint32_t)en >> 8));   /* (loud: ~6 x the pace) */
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
/* (mod.c macro_master, once a block, before the parts) the beings, the weights, each track's offsets for the block */
static LIFE_SZ void life_master(void)
{
    uint32_t i, j, k, pres = 0, tapm = 0, live = 0, ph = ++life.blk & 7u, sw = ph == 7u, run;   /* (each being's swim
                                                         * every 8th block, ~12 ms, one a block in turn; the weights in the 8th) */
    int32_t gate = clamp(life_v[LIFE_AMT] * 4, 0, 100), pit = life_v[LIFE_PIT] * 2, e[NTRK], sc[4];
    const int32_t kn[4] = {life_v[LIFE_FILT], pit < 100 ? pit : 100, life_v[LIFE_FX], pit > 100 ? pit - 100 : 0};
    for (k = 0; k < 4u; k++)                            /* the knobs at this LIFE, Q15 (x gate / 100 / 100: x 838 >> 8):
                                                         * FILTER, the sag's PITCH (up to 50 %), FX, the cross-FM's (past it) */
        sc[k] = (kn[k] * gate * 838) >> 8;
    k = life_v[LIFE_AMT] > 25 ? (uint32_t)(life_v[LIFE_AMT] - 25) : 0u;
    life.bst = (uint16_t)(256u + k * k * 768u / 5625u);
    if (!life.init) {                                   /* (where they start: two rows of three) */
        static const uint16_t X0[NTRK] = {12000, 32000, 52000, 22000, 42000, 60000};
        life.init = 1;
        for (i = 0; i < NTRK; i++) {
            life.bx[i] = X0[i];
            life.by[i] = i < 3u ? 16000 : 46000;
        }
    }
    for (i = 0; i < NTRK; i++) {                        /* the energy, the swim, the breath (always: the page, the sources) */
        int32_t en = life.en[i];
        e[i] = clamp(track_env[i] * 2, 0, 32767);
        en += e[i] > en ? (e[i] - en) >> 3 : -((en + 1023) >> 10);   /* (up in ~10 ms, down over ~1.5 s) */
        life.en[i] = (int16_t)en;
        life.kit = (uint8_t)((life.kit & ~(1u << i)) | (uint32_t)(trk[i].engine == ENGI_DRUM) << i);
        if (i < trk_live && !trk[i].p[P_MUTE]) {
            live |= 1u << i;
            pres |= (uint32_t)(en > 600) << i;
        }
        if (ph == i)
            life_swim(i, en);
    }
    if (sw)
        life.pres = (uint8_t)pres;
    pres = life.pres;
    life.on = life_v[LIFE_AMT] > 0;
    life.fol = (uint8_t)(life.on ? live & ~track_follow_mask : 0u);
    track_follow_mask |= life.fol;                      /* (every live track's follower: the envelopes) */
    run = life.on && (pres || life.wany);
    if (sw)
        life.wany = 0;
    for (i = 0; i < NTRK; i++) {                        /* track i, moved by the others */
        uint32_t pred = i;
        int32_t cs = 0, es = 0;
        life.xc[i] = life.xp[i] = life.xs[i] = life.pa[i] = 0;
        if (!life.pg[i] && (!run || !e[i])) {           /* (LIFE 0, nothing sounding, or it silent: nothing to move; its
                                                         * weights wait, as they were) */
            for (j = 0; j < NTRK; j++) life.wany |= life.w[i][j] != 0;
            continue;
        }
        for (k = 1; k < NTRK; k++) {                    /* the one before it in the ring of those that sound */
            pred = pred ? pred - 1u : NTRK - 1u;
            if ((pres >> pred) & 1u) break;
        }
        for (j = 0; j < NTRK; j++) {
            int32_t tg = 0, w = life.w[i][j], d;
            if (sw) {
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
                life.wany |= w != 0;
            }
            d = (w * e[j]) >> 15;
            cs += (life.kit >> j) & 1u ? -d : d;        /* (a kit ducks the filter, a voice opens it) */
            es += d;
            if (j != i && w && (sc[3] || life.pg[i]))   /* (whose samples the cross-FM reads next block) */
                tapm |= 1u << j;
        }
        cs = clamp(cs, -32767, 32767);
        es = clamp(es, 0, 32767);
        life.xc[i] = (int16_t)clamp(((((cs * sc[0]) >> 15) >> 2) * life.bst) >> 8, -32767, 32767);   /* (+-32 steps, << 8;
                                                         * x4 at 100 %) */
        life.xp[i] = (int16_t)clamp((-(((es * sc[1]) >> 15) * 71 >> 15) * life.bst) >> 8, -32767, 32767);   /* (a sag, ~30 ct;
                                                         * ~2 semitones at 100 %) */
        life.xs[i] = (int16_t)((es * sc[2]) >> 15);
        life.pa[i] = (int16_t)sc[3];
    }
    life.tapm = (uint8_t)tapm;
}

/* (fx.c track_follow_peak) track ti's samples of this block, kept for the cross-FM of the next */
static __attribute__((noinline)) void life_tap(uint32_t ti, const int32_t *b, uint32_t n, int32_t level)
{
    uint32_t i, par = mns_clk & 1u;
    int16_t *d = life.tap[par][ti % NTRK];
    if (n > CTL) n = CTL;
    for (i = 0; i < n; i++)
        d[i] = (int16_t)clamp(((b[i] >> 2) * level) >> 10, -32767, 32767);
    for (; i < CTL; i++)
        d[i] = 0;
    life.tap_blk[par][ti % NTRK] = mns_clk;
}

/* (voice.c track_render) the block's LIFE of track ti: each voice's depth, the others' cutoff and pitch on it (c: three,
 * vctx_t's lv lcut lfine); its LIFE (0..256) returned */
static LIFE_SZ int32_t life_ctx(uint32_t ti, int32_t *c)
{
    int32_t lpt = life_pt(ti, MOD.on ? MOD.lof : 0);
    ti %= NTRK;
    c[0] = ((life_v[LIFE_AMT] * lpt / 100) * life.bst) >> 8;   /* (0..1024: x4 at 100 %) */
    c[1] = (life.xc[ti] * lpt) >> 8;
    c[2] = (life.xp[ti] * lpt) >> 8;
    return lpt;
}

/* (voice.c voice_block, after the matrix) voice vi's life this block, lc its track's (vctx_t lv lcut lfine: each voice's
 * depth 0..256, the other tracks' cutoff and pitch): onto its increment, cutoff, shape and level (m->amp1, v->env_out).
 * step: its first part of the block (its clock goes on) */
static __attribute__((noinline)) void life_voice(uint32_t vi, voice_t *v, vmod_t *m, const int32_t *lc, uint32_t step)
{
    uint32_t blk, k, ph;
    int32_t a = lc[0], en = 48 + v->mvel, bloom, exc, tire, n1, n2, n3, fine = lc[2], cut = lc[1];
    vi %= NTRK * NVOICE;
    if (life.vtag[vi] != (uint16_t)v->age) {            /* a new note: its arc from the start, its own wander */
        life.vtag[vi] = (uint16_t)v->age;
        life.vblk[vi] = 0;
        life.vph[vi] = v->age * 2654435761u;
        life.vn[vi][1] = life.vn[vi][2] = 0;            /* (its noises in in turn: the first now, below; a note's onset
                                                         * hides the others' first step, and the worst block stays light) */
    }
    blk = life.vblk[vi];
    k = blk >> 5;                                       /* (~46 ms) */
    bloom = k * 8u < 256u ? (int32_t)k * 8 : 256;       /* (~1.5 s) */
    exc = 256 - (k * 12u < 192u ? (int32_t)k * 12 : 192);   /* (fresh 256 .. 64 in ~0.7 s) */
    tire = k < 64u ? 0 : (k - 64u) * 2u < 256u ? (int32_t)(k - 64u) * 2 : 256;   /* (~3 .. ~9 s) */
    if (step) {
        if (blk < 65535u) life.vblk[vi] = (uint16_t)(blk + 1u);
        life.vph[vi] += (uint32_t)(160 + ((exc * en) >> 7) - (tire >> 2));   /* (fresh quick, tired slow) */
    }
    if (a) {
        int16_t *vn = life.vn[vi];
        ph = life.vph[vi];
        k = blk % 3u;                                   /* (one of the three a block, in turn: they move slowly) */
        vn[k] = (int16_t)life_noise(vi * 0x9E37u + k * 0x5bd1u + 1u, (ph >> (8u + (k != 0u))) + k * 77u, 8u);
        n1 = vn[0];                                     /* (pitch ~0.37 s a point settled; cutoff, shape and level slower) */
        n2 = vn[1];
        n3 = vn[2];
        fine += (n1 * ((28 * a * (256 + exc)) >> 17)) >> 15;   /* (+-12 ct fresh at 100 %) */
        fine += (v->mrnd * ((19 * a) >> 8)) >> 15;     /* (the note's own detune, +-8 ct) */
        cut += (((n2 * a) >> 8) * 10) >> 7;             /* (+-10 steps) */
        cut += (((bloom * 6 * en) >> 7) * a >> 8) - ((tire * 12 * a) >> 8);   /* (blooms ~+6 x energy, tires -12) */
        if (m->shape != 64 << 8)                        /* (+-8 steps of shape, when something moves it already: at
                                                         * its centre an engine's plain wave, ANALOG's cheap one, stays) */
            m->shape = clamp(m->shape + ((((n3 * a) >> 8) * 8) >> 7), -62 * 256, 190 * 256);
        n3 = (((n3 + 32768) >> 1) * (((31 + (tire >> 4)) * a) >> 8)) >> 8;
        m->amp1 = mulq15(m->amp1, 32767 - (n3 < 16384 ? n3 : 16384));   /* (its level breathing: down ~12 %, ~18 % tired;
                                                         * at most half, LIFE 100 %) */
        v->env_out = m->amp1;
    }
    m->inc += (uint32_t)((int32_t)(m->inc >> 12) * fine);
    m->cutoff = clamp(m->cutoff + cut, -150 * 256, 150 * 256);
}

/* (voice.c track_render) the cross-FM: track ti's sum through a short line whose delay the other tracks' samples of the
 * block before move (a phase modulation: every partial by its frequency, as FM), depth ramped from pg to the target */
static __attribute__((noinline)) void life_pm(uint32_t ti, int32_t *out, uint32_t n, int32_t lpt)
{
    uint32_t i, j, ns = 0, par = (mns_clk - 1u) & 1u, w;
    int32_t a0 = life.pg[ti], a1 = (life.pa[ti] * lpt) >> 8, g0[NTRK];
    const int16_t *src[NTRK];
    int32_t *ln = life.line[ti];
    if (n > CTL) n = CTL;
    a1 = a0 + clamp(a1 - a0, -2048, 2048);              /* (the depth slewed: ~16 blocks to full) */
    life.pg[ti] = (int16_t)a1;
    if (!a0 && !a1) return;
    if (!a0)                                            /* (starting: the line from silence) */
        memset(ln, 0, sizeof life.line[0]);
    for (j = 0; j < NTRK; j++)
        if (j != ti && life.w[ti][j] && life.tap_blk[par][j] == mns_clk - 1u) {
            src[ns] = life.tap[par][j];
            g0[ns++] = life.w[ti][j];
        }
    w = life.lw[ti];
    for (i = 0; i < n; i++) {
        int32_t m = 0, A = a0 + (((a1 - a0) * (int32_t)i) >> CTL_LOG2), d, f, x0, x1;
        uint32_t s, r;
        for (s = 0; s < ns; s++)
            m += (g0[s] * src[s][i]) >> 15;
        m = clamp(m * 3, -32767, 32767);
        ln[w & (LPM_L - 1u)] = out[i];
        d = (((((A >> 7) * ((32768 + m) >> 7)) * 14) >> 8) * life.bst) >> 8;   /* (Q8 samples: 0 .. ~27.8; past 25 % deeper, up to 30) */
        d = d < (int32_t)(LPM_L - 2u) << 8 ? d : (int32_t)(LPM_L - 2u) << 8;
        r = w - (uint32_t)(d >> 8);
        f = d & 255;
        x0 = ln[r & (LPM_L - 1u)];
        x1 = ln[(r - 1u) & (LPM_L - 1u)];
        out[i] = x0 + (((x1 - x0) * f) >> 8);
        w++;
    }
    life.lw[ti] = (uint8_t)(w & (LPM_L - 1u));
}

/* (mod.c mod_begin) the sends as the other tracks move them: CHO DLY REV up from where they are (a dry one stays dry),
 * by track i's sum of them this block, at its LIFE; through the matrix's keep (mod_end puts them back) */
static LIFE_SZ void life_snd(track_t *t, mod_t *mp, int32_t *off)
{
    static const uint8_t ID[3] = {P_CHOR, P_DLY, P_REV};
    uint32_t ti = (uint32_t)(t - trk) % NTRK, k, j;
    int32_t x = (((life.xs[ti] * life_pt(ti, mp->lof)) >> 8) * life.bst) >> 8;   /* (x4 at 100 %: the sends up to x3) */
    x = x < 65534 ? x : 65534;
    for (k = 0; k < 3u; k++) {
        uint32_t id = ID[k];
        for (j = 0; j < mp->nk && mp->kid[j] != id; j++)
            ;
        if (j == mp->nk) {
            mp->kid[j] = (uint8_t)id;
            mp->keep[j] = t->p[id];
            off[j] = 0;
            mp->nk++;
        }
        off[j] += (mp->keep[j] * x >> 15) >> (k ? 0 : 1);   /* (up to twice; CHO half that. A dry track stays dry: its
                                                         * buses asleep) */
    }
}

/* the state back to its start (the tests' power-on; a project's LIFE is in life_v) */
static void life_reset(void) { memset(&life, 0, sizeof life); }
