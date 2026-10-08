/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* SLICE: a sample slicer, played from the keys and the sequencer. Felucca's own design.
 *
 * Material (SRC): the built-in BREAK (tools/gen_samples.py: one bar of 16ths at 120 BPM arranged
 * from Felucca's generated drums, stored after the SAMPLE sets) or PIANO (SRC 4, since 1.0.4: one note, the SAMPLE
 * engine's PIANO zone of middle C, played from its data in SMP_DATA: no copy). SRC 1..3, once the user slots
 * USR1..3 (JIANT has none), are kept as aliases of BREAK (params.c enum_orig). A source with no material (PIANO in a
 * build without the CC0 samples) plays a plain sine at the key's pitch (+ PTCH) instead, through DCAY, TONE and the
 * ADSR (MODE ONE acts as GATE there: no slice end), and the UI says so (ui_input.c sample_notice).
 *
 * Slices (DIV): 4 / 8 / 16 / 32 equal ones or AUTO: one per onset (a stored MAN, Felucca's slices set by hand on
 * user slots, plays AUTO). IMA ADPCM can only be entered at a known decoder state, so each source keeps a table
 * (slc_src_t), written at build time: the states at the 128 grid points k * len / 128 (every equal slice starts on
 * one, and they are the reverse checkpoints) and up to 32 AUTO starts with their states (BREAK's hits are its AUTO
 * slices; PIANO has one, the attack).
 *
 * Playing: note n plays slice (n - 60 - ROOT + START) mod the slice count (the keys: the lowest
 * key is slice 0, seq.c kb_map). Slices play at the source rate times PITCH (varispeed: faster is
 * higher). MODE: ONE plays the slice to its end (a note-off does not stop it), GATE until the
 * note-off (then the ADSR release) or the slice end, LOOP repeats the slice while the note is held.
 * REV plays a slice backwards: windows of 64 samples are decoded forwards from the nearest
 * checkpoint (grid point or the slice start) into the voice's own buffer and read from the top
 * down; it costs about (len / 128) / 128 extra decodes per source sample (BREAK: ~3). DECAY: an exponential fade per slice (127 = none). TONE: a gentle low-pass
 * (SAMPLE's CUT). The last ~1.5 ms of a slice fade out (no click at the cut), a new slice fades in
 * over one block (0.7 ms).
 * Voice state: ph[0] position (reverse: position + 1), ph[1] fraction Q16, ph[2] slice end (reverse:
 * its start), s[0] predictor (reverse: window start), s[1] step index, s[2] / s[3] previous / current
 * sample, s[4] source | segment << 3 | reverse << 7 | slice << 8 | DIV << 16 (source SLC_SINE: the sine, its
 * phase in ph[0]), s[5] DECAY level Q15, s[6] 0 to prime, 1 playing, 3 fading out, 2 ended, s[7] low-pass. */
#define SLC_GRID 128u                /* grid points: decoder states at k * len / SLC_GRID */
#define SLC_GRID_LOG2 7
#define SLC_AUTO 32u                 /* AUTO slices at most */
#define SLC_SEGS 16u                 /* zones of a source (as the generator writes them) */
#define SLC_RB 64u                   /* reverse: samples decoded per window */
#define SLC_BASE 60                  /* note of slice 0 (+ the track's ROOT) */
#define SLC_HOP 32u                  /* AUTO detector: samples per hop */

typedef struct {
    uint32_t off, at, n;             /* data (SMP_DATA index), material position of its first sample, samples */
} slc_seg_t;
typedef struct {
    uint32_t len, rate, nseg, nauto; /* material samples (0 = none), rate (as smp_zone_t), segments, AUTO slices */
    slc_seg_t seg[SLC_SEGS];
    uint32_t grid[SLC_GRID];         /* decoder state before sample k * len / SLC_GRID: */
                                     /* (uint16_t)predictor | step index << 16 | segment << 24 */
    uint32_t apos[SLC_AUTO], ast[SLC_AUTO];   /* AUTO slice starts (apos[0] = 0) and their states */
} slc_src_t;

static const slc_src_t SLC_BREAK = SLC_BREAK_INIT;
static const slc_src_t SLC_PIANO = SLC_PIANO_INIT;
static int16_t slc_rbuf[NPART][NVOICE][SLC_RB];       /* reverse windows, one per part voice */
/* append-only: stored sounds keep their SRC numbers (1.0.4 added PIANO); 1..3, once USR1..3: BREAK (params.c) */
static const char *const N_SLC_SRC[] = {"BREAK", "BREAK", "BREAK", "BREAK", "PIANO"};
#define SLC_SRC_PIANO 4u
#define SLC_NSRC 5
#define SLC_SINE 7u                  /* a voice's source when it has no material: the sine */
static const char *const N_SLC_DIV[] = {"4", "8", "16", "32", "AUTO"};
static const char *const N_SLC_MODE[] = {"ONE", "GATE", "LOOP"};
static const char *const N_SLC_REV[] = {"OFF", "ON"};
enum { SLC_ONE, SLC_GATE, SLC_LOOP };
#define SLC_DIV_AUTO 4u
/* the SRC of a part's sound (0..4: a stored value out of range reads as the nearest) */
static uint32_t slc_src_of(const int16_t *p) { return (uint32_t)clamp(p[P_E0], 0, SLC_NSRC - 1); }
/* source 4 = PIANO, else BREAK; 0 = no material (no PIANO in the build) */
static const slc_src_t *slc_get(uint32_t src)
{
    if (src == SLC_SRC_PIANO)
        return SLC_PIANO.len ? &SLC_PIANO : 0;
    return SLC_BREAK.len ? &SLC_BREAK : 0;
}

/* ---- the ADPCM decoder over the material (segments one after the other, each from 0 / 0) */
typedef struct {
    int32_t pred, idx;
    uint32_t pos, seg;
} slc_dec_t;
static inline void slc_dec_at(slc_dec_t *d, uint32_t pos, uint32_t st)
{
    d->pos = pos;
    d->pred = (int16_t)(st & 0xFFFFu);
    d->idx = (int32_t)((st >> 16) & 127u);
    d->seg = st >> 24;
}
static inline uint32_t slc_dec_st(const slc_dec_t *d)
{
    return (uint32_t)(uint16_t)d->pred | (uint32_t)d->idx << 16 | d->seg << 24;
}
/* the sample at d->pos (< len), then the next position */
static inline int32_t slc_dec_next(const slc_src_t *s, slc_dec_t *d)
{
    const slc_seg_t *g = &s->seg[d->seg];
    uint32_t rel = d->pos - g->at, b, code;
    int32_t step, vd;
    if (rel >= g->n && d->seg + 1u < s->nseg) {         /* into the next zone: its data starts from 0 / 0 */
        d->seg++;
        g++;
        rel = d->pos - g->at;
        d->pred = 0;
        d->idx = 0;
    }
    b = SMP_DATA[g->off + (rel >> 1)];
    code = (rel & 1u) ? (b >> 4) : (b & 15u);
    step = IMA_STEP[d->idx];
    vd = step >> 3;
    if (code & 4u)
        vd += step;
    if (code & 2u)
        vd += step >> 1;
    if (code & 1u)
        vd += step >> 2;
    d->pred = clamp(d->pred + ((code & 8u) ? -vd : vd), -32768, 32767);
    d->idx = clamp(d->idx + IMA_IDX[code & 7u], 0, 88);
    d->pos++;
    return d->pred;
}

static inline uint32_t slc_gpos(const slc_src_t *s, uint32_t k) { return (k * s->len) >> SLC_GRID_LOG2; }
static uint32_t slc_count(const slc_src_t *s, uint32_t div) { return div < SLC_DIV_AUTO ? 4u << div : s->nauto; }
/* the slice a note plays (n slices): note - C4 - ROOT + START, mod n */
static uint32_t slc_note_slice(const int16_t *p, uint32_t note, uint32_t n)
{
    int32_t j = ((int32_t)note - SLC_BASE - p[P_ROOT] + p[P_E2]) % (int32_t)n;
    return (uint32_t)(j < 0 ? j + (int32_t)n : j);
}

/* slice j of DIV div: [*a, *b), *st the state at *a */
static void slc_bounds(const slc_src_t *s, uint32_t div, uint32_t j, uint32_t *a, uint32_t *b, uint32_t *st)
{
    if (div < SLC_DIV_AUTO) {
        uint32_t w = SLC_GRID >> (2u + div);
        *a = slc_gpos(s, j * w);
        *b = slc_gpos(s, (j + 1u) * w);
        *st = s->grid[j * w];
    } else {
        j = j < s->nauto ? j : s->nauto - 1u;
        *a = s->apos[j];
        *b = j + 1u < s->nauto ? s->apos[j + 1u] : s->len;
        *st = s->ast[j];
    }
}
static void slc_bounds_v(const slc_src_t *s, const voice_t *v, uint32_t *a, uint32_t *b, uint32_t *st)
{
    uint32_t pk = (uint32_t)v->s[4];
    slc_bounds(s, (pk >> 16) & 7u, (pk >> 8) & 63u, a, b, st);
}

/* ---- voices */
static int16_t *slc_rb(track_t *t, voice_t *v)
{
    uint32_t p = (uint32_t)(t - trk), i = (uint32_t)(v - t->v);
    return p < NPART && i < NVOICE ? slc_rbuf[p][i] : 0;
}

static void slice_note_on(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    uint32_t src = slc_src_of(p), div = (uint32_t)clamp(p[P_E1], 0, SLC_DIV_AUTO), rev = p[P_E5] != 0;
    uint32_t a, b, st, j;
    const slc_src_t *s = slc_get(src);
    v->ph[1] = 0;
    v->s[2] = v->s[3] = 0;
    v->s[5] = 32767;
    v->s[6] = 2;
    v->s[7] = 0;
    v->env_out = 0;                                     /* a new slice fades in over one block */
    if (!s) {                                           /* no material: the sine (from phase 0: no click) */
        v->s[4] = (int32_t)SLC_SINE;
        v->s[6] = 1;
        v->ph[0] = 0;
        return;
    }
    j = slc_note_slice(p, v->note, slc_count(s, div));
    slc_bounds(s, div, j, &a, &b, &st);
    if (b <= a || (rev && !slc_rb(t, v)))
        return;
    v->s[4] = (int32_t)(src | (st >> 24) << 3 | rev << 7 | j << 8 | div << 16);
    v->s[6] = 0;
    if (rev) {
        v->ph[0] = b;
        v->ph[2] = a;
        v->s[0] = 0x7FFFFFFF;                           /* no window yet */
    } else {
        v->ph[0] = a;
        v->ph[2] = b;
        v->s[0] = (int16_t)(st & 0xFFFFu);
        v->s[1] = (int32_t)((st >> 16) & 127u);
    }
}

/* the reverse window [ws, ws + n): decoded from the nearest checkpoint at or below ws (a grid point,
 * or a = the slice start with its state sa) */
static void slc_fill(const slc_src_t *s, int16_t *rb, uint32_t ws, uint32_t n, uint32_t a, uint32_t sa)
{
    slc_dec_t d;
    uint32_t k = (ws << SLC_GRID_LOG2) / s->len, i;
    while (k + 1u < SLC_GRID && slc_gpos(s, k + 1u) <= ws)
        k++;
    if (a <= ws && a > slc_gpos(s, k))
        slc_dec_at(&d, a, sa);
    else
        slc_dec_at(&d, slc_gpos(s, k), s->grid[k]);
    while (d.pos < ws)
        slc_dec_next(s, &d);
    for (i = 0; i < n; i++)
        rb[i] = (int16_t)slc_dec_next(s, &d);
}

/* LOOP: back to the slice's start. Out of line: once
 * per loop, it keeps slc_fwd inlined in slice_render */
static __attribute__((noinline)) void slc_loop_fwd(const slc_src_t *s, voice_t *v, slc_dec_t *d)
{
    uint32_t a, b, st;
    slc_bounds_v(s, v, &a, &b, &st);
    slc_dec_at(d, a, st);
    v->ph[2] = b;
}
/* the next source sample into *x; 0 = the slice ended */
static inline int slc_fwd(const slc_src_t *s, voice_t *v, slc_dec_t *d, int loop, int32_t *x)
{
    if (d->pos >= v->ph[2]) {
        if (!loop)
            return 0;
        slc_loop_fwd(s, v, d);
    }
    *x = slc_dec_next(s, d);
    return 1;
}
static inline int slc_rev(const slc_src_t *s, voice_t *v, int16_t *rb, int loop, int32_t *x)
{
    uint32_t q = v->ph[0], ws = (uint32_t)v->s[0], a, b, st;
    if (q <= v->ph[2]) {
        if (!loop)
            return 0;
        slc_bounds_v(s, v, &a, &b, &st);
        q = b;
        v->ph[2] = a;
    }
    q--;
    if (q < ws || q >= ws + SLC_RB) {
        slc_bounds_v(s, v, &a, &b, &st);
        ws = q + 1u >= a + SLC_RB ? q + 1u - SLC_RB : a;
        slc_fill(s, rb, ws, q + 1u - ws, a, st);
        v->s[0] = (int32_t)ws;
    }
    *x = rb[q - ws];
    v->ph[0] = q;
    return 1;
}

/* the ADSR (ONE: no release), DECAY, the fade at the slice end */
static int32_t slice_amp(track_t *t, voice_t *v, int32_t adsr)
{
    const int16_t *p = t->p;
    if (p[P_E4] == SLC_ONE && v->stage == 3u && ((uint32_t)v->s[4] & 7u) != SLC_SINE)
        v->stage = 2;                                   /* ONE: the slice plays to its end (the sine has none) */
    if (p[P_E6] < 127) {
        v->s[5] -= mulq16(v->s[5], ENV_EXP[p[P_E6] & 127]);
        if (v->s[5] < 8)
            v->s[6] = 2;                                /* faded out: the voice ends */
    }
    return v->s[6] == 3 ? 0 : mulq15(adsr, v->s[5]);
}

static void slice_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    uint32_t pk = (uint32_t)v->s[4], i, frac = v->ph[1], rev = (pk >> 7) & 1u, stepq, rem;
    int32_t lp = 4000 + ((clamp((p[P_E7] << 8) + m->cutoff, 0, 127 << 8) * 28767) >> 15), x;
    int loop = p[P_E4] == SLC_LOOP && v->gate;
    const slc_src_t *s = v->s[6] == 2 || (pk & 7u) == SLC_SINE ? 0 : slc_get(pk & 7u);
    int16_t *rb = slc_rb(t, v);
    slc_dec_t d;
    if ((pk & 7u) == SLC_SINE && v->s[6] != 2) {        /* no material: the sine at the key's pitch + PTCH */
        smp_sine(out, n, m, p[P_E3] * 16, lp, &v->ph[0], &v->s[7]);
        return;
    }
    if (!s) {                                           /* ended, or its slot was erased */
        v->active = 0;
        return;
    }
    stepq = (pow2_q16(clamp(m->pitch16 - v->pitch_cur + p[P_E3] * 16, -1536, 576)) >> 8) * (s->rate >> 8);
    d.pos = v->ph[0];
    d.pred = v->s[0];
    d.idx = v->s[1];
    d.seg = (pk >> 3) & 15u;
    if (!v->s[6]) {                                     /* prime the interpolator */
        if (!(rev ? slc_rev(s, v, rb, 0, &x) : slc_fwd(s, v, &d, 0, &x))) {
            v->active = 0;
            return;
        }
        v->s[3] = x;
        v->s[6] = 1;
    }
    rem = rev ? v->ph[0] - v->ph[2] : v->ph[2] - d.pos;
    if (v->s[6] == 1 && !loop && rem < ((stepq * 3u * n) >> 16))
        v->s[6] = 3;                                    /* the next block fades to 0 (slice_amp) */
    for (i = 0; i < n; i++) {
        int32_t y;
        frac += stepq;
        while (frac >= 65536u) {
            frac -= 65536u;
            v->s[2] = v->s[3];
            if (!(rev ? slc_rev(s, v, rb, loop, &x) : slc_fwd(s, v, &d, loop, &x))) {
                v->s[6] = 2;                            /* the slice ended */
                v->s[3] = 0;
                break;
            }
            v->s[3] = x;
        }
        y = v->s[2] + (((v->s[3] - v->s[2]) * (int32_t)(frac >> 1)) >> 15);
        v->s[7] += mulq15(y - v->s[7], lp);             /* TONE */
        out[i] += voice_amp(v->s[7], m, i) << 1;
        if (v->s[6] == 2)
            break;
    }
    v->ph[1] = frac;
    if (!rev) {
        v->ph[0] = d.pos;
        v->s[0] = d.pred;
        v->s[1] = d.idx;
        v->s[4] = (int32_t)((pk & ~(15u << 3)) | d.seg << 3);
    }
}

/* two factory sounds, both on BREAK (always there). PATTERNS (engines.c): CHOP suggests
 * 9 CHOP (16 slices re-ordered), STUTTER 10 STUTTER (8 slices, repeats); 11 SLICES plays 0..15 in order */
static const preset_t SLICE_PRESETS[] = {
    {"CHOP", {0, 2, 0, 0, SLC_ONE, 0, 127, 127}, {0, 127, 127, 30}, 0, 0, FX(0, 0, 0, 12), PAT(9)},
    {"STUTTER", {0, 1, 0, 0, SLC_GATE, 0, 90, 110}, {0, 127, 127, 12}, 0, 0, FX(10, 0, 20, 10), PAT(10)},
};

/* the lowest key is slice 0 (C4 + ROOT), no scale */
static int32_t slice_keys(const track_t *t, uint32_t k)
{
    return clamp(SLC_BASE + t->p[P_ROOT] + 12 * song.octave + (int32_t)k, 0, 127);
}

static const engine_t ENG_SLICE = {
    .name = "SLICE",
    .page_title = {"SLCE", "PLAY"},
    .edit = {
        {"SRC", F_ENUM, 0, SLC_NSRC - 1, 0, N_SLC_SRC, 0},
        {"DIV", F_ENUM, 0, SLC_DIV_AUTO, 2, N_SLC_DIV, 0},
        {"START", F_INT, 0, 31, 0, 0, 0},
        {"PTCH", F_SEMI, -24, 24, 0, 0, 0},
        {"MODE", F_ENUM, 0, 2, 0, N_SLC_MODE, 0},
        {"REV", F_ENUM, 0, 1, 0, N_SLC_REV, 0},
        {"DCAY", F_TIME, 0, 127, 127, 0, 0},
        {"TONE", F_INT, 0, 127, 127, 0, 0},
    },
    .presets = SLICE_PRESETS,
    .npresets = NELEM(SLICE_PRESETS),
    .note_on = slice_note_on,
    .render = slice_render,
    .knob = {P_E0, P_E1, P_E3, P_E6},
    .sampled = 1,
    .keys = slice_keys,
    .amp = slice_amp,
};
