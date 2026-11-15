/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* PERFORM: the effects of the FX hold layer, on the master. A key pressed while FX is held belongs to the
 * layer (seq.c keyboard_block): it plays no note, sends no MIDI, records nothing, and holds its effect until
 * it is let go (ui_input.c opens the layer and shows the map). (V10.7.36, the owner) The keys:
 *   white F3 G3 A3  REPEAT 1/8 1/16 1/32: the last 1/8 .. 1/32 looped
 *         B3        REVERSE: each 1/16 (1/8 from ~167 BPM, 1/32 under ~82) the one before it, backwards, on and on
 *         C4        TAPE STOP: the mix slows to a stop (pitch and speed falling) over a beat (half a beat under ~86 BPM),
 *                   then silence while held
 *         D4 E4 F4  the punch-in MIDI effects (pfx.c) OCT- OCT+ 1/2 TEMPO
 *         G4        WOBBLE: a resonant low-pass on the master opening and closing once every 1/8 of the transport
 *         A4        DLY 1/16: the delay bus at 1/16, FEEDBK 95, MIX at least 100, the whole dry mix sent into it (the dry
 *                   kept): echoes building while held; let go, a bar at FEEDBK 30 (they die out), then the user's delay
 *         B4 .. F5  the punch-in MIDI effects (pfx.c) STUT 1/8 STUT 1/16 ATK+ ARP RANDOM (the punch-in lane stores the
 *                   MIDI ones by these indices, D4 = 1; 4 and 5, G4 A4, DEC- DEC+ before V10.7.36, play nothing)
 *         G5        COMB: a resonant comb on the master at the root (C3 .. B3), its tuning and feedback swaying by
 *                   themselves (a buffer effect: its line in sl_buf). The lane is cleared by REC + FX or FX + PRESETS,
 *                   KNOB 4 CLEAR (CLR AUTO, G5's before, is gone)
 *   black F#3 G#3   PITCH UP / DOWN: the whole mix's pitch ramps up / down while held, 12 semitones over a bar, then
 *                   stays; in time (a crossfaded delay-line shifter on the buffer)
 *         A#3       FREEZE: the master held as it is, a drone (V10.7.37: a buffer effect; perf_frz): the next 1/4 beat
 *                   (100 .. 280 ms) captured, then sustained as overlapping windowed grains reading it forward and
 *                   wrapping (to a random place), their places jittered ~1.5 ms; the live mix muted while it holds
 *         C#4 D#4   REV ALL / DLY ALL: full wet, every track into the reverb / the delay, the dry muted; let go, the
 *                   dry back, the tail rings on
 *         F#4       RING: the master ring-modulated by a sine two octaves over the scale's root (C5 .. B5), its
 *                   depth wobbling once a bar
 *         G#4 C#5 D#5 LOOP 3 / 5 / 7 (pfx.c, seq.c seq_tick): every track its last 3 / 5 / 7 steps again, on the 1/16
 *         A#4       LIFE MAX: HOME > LIFE at 100 % (LIFE FILTER PITCH FX) while held (pfx.c: put back after each block,
 *                   never saved)
 *         F#5       GLITCH: the master cut on the transport's 1/16s (ratchets, reversed and octave slices, dropouts,
 *                   crush bursts, tape-stop slivers; perf_glt_step)
 * The MIDI effects act on every track (the TRK ALL / SYN / DRM selector is gone: a lane saved with SYN or DRM: ALL).
 * REPEAT, REVERSE, TAPE STOP and GLITCH start on the next 1/16 (at once while stopped); the others at once; all end
 * when let go, with a 2.9 ms ramp. Effects of different kinds stack; of the buffer effects (REPEAT REVERSE TAPE STOP
 * PITCH GLITCH COMB FREEZE) the last pressed plays, and letting it go returns to the one held before.
 * The buffer: the SLICER's recordings (sl_buf, 32 KB) borrowed as one stereo loop of 8192 frames at 22.05 kHz
 * (371 ms). While borrowed, STUT tracks play live; their recordings are dropped afterwards. A REPEAT 1/8 longer
 * than the loop at the tempo (below 81 BPM) records at 11 kHz (V10.59: lo-fi, down to ~41 BPM; slower: dimmed).
 * KNOB 1..4 with FX: the macros FILTER (a low-pass left, a high-pass right), CRUSH, THROW (the dry mix into the delay
 * and reverb sends), DEPTH (the buffer effects' level).
 * MENU > FX LATCH ON (#40, a hand that cannot hold FX, a key and a knob together): a key pressed with FX turns its
 * effect on, the next press turns it off (perf_latched; letting the key or FX go does nothing), the macros keep
 * their values when FX is let go, FX + OCT- turns everything off (ui_layer.c), so does the menu or a dialog.
 * Chain: the buffer effect -> FILTER -> CRUSH -> RING -> WOBBLE, after the master level and before master_out (the limiter);
 * THROW, REV ALL, DLY ALL, DLY 1/16's send and GLO's solo (the others muted) act before the buses (fx.c mix_block /
 * mix_part). Idle (no key, no knob, no ramp left) every stage is skipped: the output is bit-identical. */
enum { PF_R8, PF_R16, PF_R32, PF_REV, PF_TAPE,
       PF_OCTD, PF_OCTU, PF_HALF, PF_WOB, PF_D16, PF_S8, PF_S16, PF_ATK, PF_ARP, PF_RND,   /* the MIDI ones (pfx.c), but
                                                           * G4 WOBBLE and A4 DLY 1/16 (V10.7.36; were DEC- DEC+) */
       PF_NW,                                              /* the white keys' effects: F3 .. F5 (G5: PF_COMB) */
       PF_PUP = PF_NW, PF_PDN, PF_FRZ, PF_RVA, PF_DLA, PF_RING,   /* black keys 1..6, F#3 .. F#4 */
       PF_GLT,                                             /* F#5 GLITCH */
       PF_LP3, PF_LP5, PF_LP7,                             /* .. G#4 LOOP 3, C#5 LOOP 5, D#5 LOOP 7: the last steps again (pfx.c) */
       PF_LMAX,                                            /* A#4 LIFE MAX (pfx.c) */
       PF_COMB,                                            /* G5 COMB */
       PF_N };
/* the black keys F#3 .. F#5 (seq.c perf_key, ui_layer.c's map) */
static const uint8_t PF_BK[11] = {PF_PUP, PF_PDN, PF_FRZ, PF_RVA, PF_DLA, PF_RING, PF_LP3, PF_LMAX, PF_LP5, PF_LP7, PF_GLT};
#define PF_BIT(e) (1u << (e))
#define PF_REPEAT 0x7u                                         /* the three REPEAT rates */
#define PF_PITCH (PF_BIT(PF_PUP) | PF_BIT(PF_PDN))
#define PF_Q (PF_REPEAT | PF_BIT(PF_REV) | PF_BIT(PF_TAPE) | PF_BIT(PF_GLT))   /* start on the 1/16 */
#define PF_BUF (PF_Q | PF_PITCH | PF_BIT(PF_COMB) | PF_BIT(PF_FRZ))   /* the buffer effects (the last pressed plays) */
#define PF_LOOPS (PF_BIT(PF_LP3) | PF_BIT(PF_LP5) | PF_BIT(PF_LP7))             /* (snd2) on the sequencer (pfx.c), as the MIDI ones */
#define PF_MIDI ((((1u << (PF_RND + 1)) - 1u) & ~((1u << PF_OCTD) - 1u)) & ~(PF_BIT(PF_WOB) | PF_BIT(PF_D16)))   /* PF_OCTD ..
                                                               * PF_RND, on the notes (pfx.c), less WOBBLE and DLY 1/16 */
#define PF_NOTES (PF_MIDI | PF_LOOPS | PF_BIT(PF_LMAX))        /* pfx.c's, not perform's */
_Static_assert(PF_N <= 32 && PF_OCTD == 5 && PF_RND == 14 && PF_GLT == 21 && PF_LP7 == 24, "PF bits: the lane, the scenes, the PUNCH push");
#define PB_FRAMES SL_FR                       /* stereo frames in sl_buf (a power of 2: the mask), 371 ms at 22.05 kHz */
_Static_assert(PB_FRAMES <= SL_FR && !(PB_FRAMES & (PB_FRAMES - 1u)), "the REPEAT loop in sl_buf");
#define PB_MAX (2u * PB_FRAMES)               /* the longest loop, 44.1 kHz samples */
#define PF_TOP (63 << 8)                      /* filter cutoff index, Q8 (PF_SVF): the low-pass's open end */
#define PF_LOW (14 << 8)                      /* .. KNOB 1's ends */
#define PF_CSTEP (192 * (int32_t)CTL / 32)    /* (ctl64) a cutoff's step a block: 192 a block of 32, the range in ~45 ms */
#define PT_G 2048u                            /* (V10.7.36) PITCH: the delay's span read (46 ms), its least, the jump's fade */
#define PT_LO 320u
#define PT_X 256u
#define GL_SPAN 7680u                         /* (snd2) GLITCH: the longest slice it reads back (the loop holds two and a margin) */
enum { BM_NONE, BM_LOOP, BM_GLT, BM_REV, BM_TAPE, BM_PIT, BM_COMB, BM_FRZ };
enum { GA_PASS, GA_RPT, GA_REV, GA_UP, GA_DN, GA_GATE, GA_SKIP, GA_CRU, GA_ROLL, GA_TST };
/* (0.9.9-snd2) GLITCH's knobs of 0.9.18 (ui_layer.c GX_K there), here fixed at its defaults: DENS 50 PITCH 0 SIZE 0 VARI 1 */
enum { GX_DENS = 50, GX_PITCH = 0, GX_SIZE = 0, GX_VARI = 1 };

static volatile uint32_t perf_mask;   /* main: the FX button's bit while its layer may own keys, 0 = none */
static volatile uint32_t kb_mask;     /* main: the button bits whose hold makes keys a layer's (ui_layer.c), 0 = none */
static volatile uint8_t perf_solo;    /* main: tracks soloed in the GLO layer (the others muted: perf_mute) */
static volatile uint32_t scene_pfx;  /* (JIANT) the punch-in MIDI effects a song row's scene holds (song_play.c), PF bits */
static volatile uint8_t perf_kill;    /* main: every effect off (the menu, a dialog, UBOOT) */
static volatile int8_t perf_k[4];     /* main: the knob macros, 0 = untouched: FILTER -100..100 (- low-pass, + high-pass),
                                       * CRUSH 0..100, THROW 0..100, DEPTH cut 0..100 (the buffer effects' level) */
static volatile uint32_t kb_layer;    /* ISR: the keys held that are the layer's, a bit per key */
static volatile uint32_t perf_held;   /* ISR: the effects held (PF_*) */
static volatile uint8_t perf_latch_on;   /* main: MENU > FX LATCH ON (#40): a key with FX toggles its effect */
static volatile uint32_t perf_latched;   /* ISR: the effects latched (FX LATCH; main clears it: OCT-, the menu) */
static volatile uint32_t perf_act;    /* ISR: .. running (a 1/16 one from its start on) */
static uint32_t perf_ord[PF_N], perf_seq;   /* press order: the last pressed buffer effect plays */

/* the ZDF state-variable filter at cutoff index i (30 Hz * 2^(i / 7), 0..63: up to 15.4 kHz), k = 1:
 * a1 a2 a3 in Q14 (tools: g = tan(pi fc / fs), a1 = 1 / (1 + g (g + k)), a2 = g a1, a3 = g a2) */
static const int16_t PF_SVF[64][3] = {
    {16349, 35, 0}, {16345, 39, 0}, {16341, 43, 0}, {16337, 47, 0}, {16332, 52, 0}, {16327, 57, 0}, {16321, 63, 0},
    {16314, 70, 0}, {16307, 77, 0}, {16299, 85, 0}, {16290, 94, 1}, {16280, 103, 1}, {16269, 114, 1},
    {16257, 126, 1}, {16244, 139, 1}, {16229, 153, 1}, {16213, 169, 2}, {16196, 186, 2}, {16176, 205, 3},
    {16154, 227, 3}, {16130, 250, 4}, {16104, 275, 5}, {16075, 303, 6}, {16043, 334, 7}, {16007, 368, 8},
    {15968, 406, 10}, {15925, 447, 13}, {15877, 492, 15}, {15824, 541, 19}, {15766, 596, 22}, {15702, 655, 27},
    {15631, 720, 33}, {15553, 791, 40}, {15467, 869, 49}, {15372, 953, 59}, {15267, 1046, 72}, {15151, 1146, 87},
    {15024, 1255, 105}, {14883, 1374, 127}, {14729, 1502, 153}, {14559, 1640, 185}, {14372, 1790, 223},
    {14166, 1950, 268}, {13940, 2121, 323}, {13692, 2304, 388}, {13420, 2499, 465}, {13122, 2704, 557},
    {12797, 2921, 667}, {12441, 3147, 796}, {12053, 3382, 949}, {11631, 3624, 1129}, {11173, 3870, 1341},
    {10677, 4119, 1589}, {10140, 4365, 1879}, {9562, 4605, 2218}, {8940, 4832, 2612}, {8274, 5040, 3070},
    {7563, 5219, 3602}, {6807, 5359, 4219}, {6006, 5444, 4934}, {5163, 5456, 5766}, {4282, 5369, 6733},
    {3372, 5149, 7863}, {2449, 4745, 9190},
};

static struct {
    uint32_t ph, P, split;             /* samples since play; the 1/16; this block: where a 1/16 starts */
    uint32_t act, act0;                /* running after / before split */
    uint8_t busy, sync, mute, mon;     /* sync: a 1/16 starts with the next block; mute: tracks to ramp; mon: muted (GLO's
                                        * solo: the others) */
    /* the buffer */
    uint8_t mode, src, next;           /* BM_*, the effect it plays (PF_N none), one waiting for the fade */
    uint8_t state, odd;                /* LOOP: 0 recording, 1 playing; the sample of a frame's (1 << q) */
    uint8_t q;                         /* (V10.59) samples a frame, log2: 1 (22.05 kHz), 2 (11 kHz: a REPEAT longer than the
                                        * loop at 22.05, a slow tempo's 1/8: lo-fi, the effect's) */
    uint32_t len, wr, rp;              /* loop length (samples), frames written, read (samples into the loop) */
    uint32_t xrp, xlen, xn;            /* the loop it switched from, faded over xn samples */
    int32_t w, tw, hl, hr;             /* the buffer's share and its target, Q15; the first of a pair */
    int32_t yl, yr;                    /* (snd2) what GLITCH plays this sample */
    uint32_t gs, gph;                  /* .. GLITCH: the transport's 1/16 (a count), samples into it */
    struct {                           /* (snd2, 0.9.18's) GLITCH */
        struct {                       /* GLITCH: the action, the crush's count, hold and shift; segment length, samples
                                        * into it, segments past; the loop's sample at the slice's start, the span read;
                                        * the read place at the segment's start (Q16) and its rate (Q16, signed) */
            uint8_t a, n, h, sh;
            uint32_t l, u, k, g0, sp, src;
            int32_t rate, cl, cr;
        } g;
        struct {                       /* (V10.7.36) REVERSE, TAPE STOP, PITCH: samples held (REVERSE: into its window), the
                                        * window (TAPE: the stop's length); REVERSE: the window's start (the loop's sample),
                                        * windows past; TAPE: the read place (Q16), the speed (Q24); PITCH: the delay read
                                        * (Q16), the one it fades from and the fade's samples left, the speed (Q16);
                                        * (V10.7.37) FREEZE: samples into the hop, the hop (half a grain), the grain in its
                                        * first half's start (the window's samples), grains so far, the one in its second
                                        * half's start (t len s0 k d) */
            uint32_t t, len, s0, k;
            uint32_t d, xd, xn;
            int32_t v;
        } r;
        struct {                       /* (V10.7.36) COMB: samples written, the delay (Q16), the feedback (Q15), the loop's
                                        * low-pass states */
            uint32_t t, d;
            int32_t g, zl, zr;
        } c;
    } u;
    /* the filters, CRUSH, THROW, mutes */
    int32_t lc, hc, la, ha;            /* KNOB 1's: cutoffs (Q8 index) and shares (Q15) of the low-pass and high-pass */
    int32_t lz[4], hz[4];              /* their states: L ic1 ic2, R ic1 ic2 */
    int16_t lk[3], hk[3];              /* their coefficients this block */
    int32_t cw, cc, chl, chr;          /* CRUSH share, amount, held sample */
    uint32_t cn;
    int32_t td;                        /* THROW: the share of the dry mix sent, Q15 */
    int32_t mg[NTRK];                  /* mute gains, Q15 (32768 = open) */
} pf = {.src = PF_N, .next = PF_N, .lc = PF_TOP, .mg = {[0 ... NTRK - 1] = 32768}};   /* (every track open: 0.6, six) */
/* (V10.7.36) REV ALL, DLY ALL, RING, WOBBLE, DLY 1/16: in the pool (the RAM's .bss is full; zeroed at
 * boot) */
static struct {
    int32_t rw, dw, rg, ew, wg;        /* shares, Q15: REV ALL's, DLY ALL's (the dry muted by the larger), RING's, DLY
                                        * 1/16's send, WOBBLE's */
    uint32_t rph, wph;                 /* RING: the sine's phase, its depth's wobble */
    uint32_t etail;                    /* DLY 1/16 let go: samples its time and a low feedback still hold the delay */
    int32_t wl[2], wb[2];              /* WOBBLE's filter: low-pass and band states, L R */
} pfw __attribute__((section(".pool")));

/* the loop length of a REPEAT at the tempo, 44.1 kHz samples; 0 = not one */
static const uint8_t PF_DEN[PF_R32 + 1] = {2, 4, 8};
static uint32_t perf_len(uint32_t e) { return e <= PF_R32 ? beat_samples() / PF_DEN[e] : 0u; }
/* the effects that can run at the tempo: a REPEAT longer than the loop cannot */
static uint32_t perf_avail(void)
{
    uint32_t e, m = ~0u, b = beat_samples();
    for (e = PF_R8; e <= PF_R32; e++)
        if (b > 2u * PB_MAX * PF_DEN[e])   /* (V10.59: longer than the loop: at 11 kHz, pf.q 2) */
            m &= ~PF_BIT(e);
    return m;
}

/* seq.c keyboard_block: a layer key down / up (e: PF_*, PF_N = no effect) */
static uint32_t pfx_lph;              /* samples into the punch-in lane (pfx.c): from the transport's start, a section's */
static uint8_t pfx_lane[32], pfx_ltgt;   /* the section's punch-in lane (pfx.c); its tracks (V10.7.36: always ALL, 0;
                                          * a lane saved with SYN 1 or DRM 2 plays on every track too) */
static void perf_press(uint32_t e, int down)
{
    if (e >= PF_N)
        return;
    if (perf_latch_on) {                  /* FX LATCH: a press turns it on or off, letting go does nothing */
        if (down) {
            perf_latched ^= PF_BIT(e);
            perf_ord[e] = ++perf_seq;
        }
        return;
    }
    if (down) {
        perf_held |= PF_BIT(e);
        perf_ord[e] = ++perf_seq;
    } else {
        perf_held &= ~PF_BIT(e);
    }
}

static void perf_start(void) { pf.ph = 0; pf.sync = 1; pfx_lph = 0; }   /* seq_start: a 1/16 starts with the transport */

/* the SLICER's recordings dropped (when the buffer is taken and given back) */
static void perf_drop_slicer(void) { slicer_drop(); }

/* the buffer effect to play: the last pressed of those running */
static uint32_t perf_pick(uint32_t a)
{
    uint32_t e, best = PF_N;
    a &= PF_BUF;
    for (e = 0; e < PF_N; e++)
        if (((a >> e) & 1u) && (best == PF_N || perf_ord[e] - perf_ord[best] < 0x80000000u))
            best = e;
    return best;
}

static void perf_glt_step(void);
static __attribute__((noinline)) void perf_buf_start(uint32_t e)
{
    static const uint8_t BM[PF_N] = {[PF_R8] = BM_LOOP, [PF_R16] = BM_LOOP, [PF_R32] = BM_LOOP, [PF_REV] = BM_REV,
                                           [PF_TAPE] = BM_TAPE, [PF_PUP] = BM_PIT, [PF_PDN] = BM_PIT, [PF_GLT] = BM_GLT,
                                           [PF_COMB] = BM_COMB, [PF_FRZ] = BM_FRZ};
    if (!sl_lent) {
        sl_lent = 1;
        perf_drop_slicer();
    }
    pf.src = (uint8_t)e;
    pf.next = PF_N;
    pf.wr = pf.rp = 0;
    pf.odd = 0;
    pf.hl = pf.hr = 0;
    pf.xn = 0;
    pf.mode = BM[e];
    pf.state = 0;
    pf.len = perf_len(e);
    pf.q = pf.mode == BM_LOOP && pf.len > PB_MAX ? 2u : 1u;
    memset(&pf.u, 0, sizeof pf.u);
    memset(sl_buf[PB_FRAMES - 2u], 0, 2u * sizeof sl_buf[0]);   /* (what GLITCH reads before the start: quiet) */
    if (e == PF_GLT) {                    /* its first 1/16: the transport's (this block's split starts the next one) */
        pf.gs = song.playing && clk_pos != CLK_START ? clk_n + (pf.split ? 1u : 0u) : 0u;
        perf_glt_step();
    } else if (pf.mode == BM_REV) {       /* REVERSE: a 1/16 window (1/8 when two fit the loop, 1/32 when one does not) */
        uint32_t w = pf.P ? pf.P : 1u;
        pf.u.r.len = 4u * w + 1024u <= PB_MAX ? 2u * w : 2u * w + 1024u <= PB_MAX ? w : w / 2u;
        pf.u.r.t = ~0u;                           /* (its first sample: 0) */
    } else if (pf.mode == BM_TAPE) {      /* TAPE STOP: over a beat (the read falls behind by half of it), else half */
        uint32_t b = beat_samples();
        b = b / 2u + 1024u <= PB_MAX ? b : b / 4u + 1024u <= PB_MAX ? b / 2u : 2u * (PB_MAX - 1024u);
        pf.u.r.len = b > 64u ? b : 64u;
        pf.u.r.v = 1 << 24;
    } else if (pf.mode == BM_PIT) {
        pf.u.r.d = (e == PF_PUP ? PT_LO + 512u : PT_LO) << 16;
        pf.u.r.v = 65536;
        pf.u.r.k = e == PF_PDN;                   /* (down) */
    } else if (pf.mode == BM_COMB) {              /* COMB: its own line, the first 1024 frames (44.1 kHz), from silence */
        memset(sl_buf, 0, 1024u * sizeof sl_buf[0]);
    } else if (pf.mode == BM_FRZ) {               /* FREEZE: a window of a 1/4 beat (100 .. 280 ms), grains of half of it */
        uint32_t b = beat_samples() / 4u;
        pf.len = b < 4410u ? 4410u : b > 12348u ? 12348u : b;
        pf.u.r.len = pf.len / 4u;
        pf.u.r.d = pf.len / 2u;                   /* (first heard: the grain in its second half, the window's last
                                                   * quarter; the next from its start) */
    }
}

/* the buffer effect to play becomes e (at a 1/16): a shorter REPEAT of what is
 * recorded switches with a cross-fade, anything else fades the old one out first */
static void perf_buf_select(uint32_t e)
{
    if (e == pf.src && pf.next == PF_N)
        return;
    if (e != PF_N && pf.mode == BM_LOOP && pf.state && e <= PF_R32 && pf.src != PF_N &&
        perf_len(e) <= pf.wr << pf.q) {
        pf.xrp = pf.rp;
        pf.xlen = pf.len;
        pf.xn = SL_RAMP;
        pf.src = (uint8_t)e;
        pf.next = PF_N;
        pf.len = perf_len(e);
        pf.rp = 0;
        return;
    }
    if (pf.mode == BM_NONE || !pf.w) {
        if (e == PF_N) {
            pf.src = PF_N;
            pf.next = PF_N;
        } else {
            perf_buf_start(e);
        }
        return;
    }
    pf.src = PF_N;                                  /* fade out, then the next one (perf_seg) */
    pf.next = (uint8_t)e;
}

/* each block, after the keys (seq.c) and before the parts: what runs, the 1/16, the filters, the mutes.
 * Returns 1 when a stage has something to do */
/* (0.6.19) a track let go from its mute, silent (its voices ended at 0, perf_hush_end): nothing to ramp back, open at
 * once; else mix_part never reached perf_mute again (no signal) and the FX layer stayed busy every block */
static __attribute__((noinline)) void perf_unhush(void)   /* (out of perf_begin: target_budget.py) */
{
    uint32_t k, i, on;
    for (k = 0; k < NTRK; k++)
        if (!pf.mg[k] && !((pf.mon >> k) & 1u)) {
            for (i = on = 0; i < NVOICE; i++)
                on |= trk[k].v[i].active;
            if (!on)
                pf.mg[k] = 32768;
        }
}
static void perf_gfx(uint32_t n);
static __attribute__((noinline)) int perf_begin(uint32_t n)
{
    uint32_t held = perf_kill ? 0u : (perf_held | perf_latched) & perf_avail() & ~PF_NOTES;
    uint32_t q, k, ph0, bnd, mon = perf_kill || !perf_solo ? 0u : ~(uint32_t)perf_solo & ((1u << NTRK) - 1u);
    int32_t m;
    if (!held && !mon && !pf.busy && !(perf_k[0] | perf_k[1] | perf_k[2])) {   /* idle: the clock only */
        pf.ph += n;
        pf.sync = 0;
        perf_act = 0;
        trk_hush = 0;
        return 0;
    }
    pf.P = beat_samples() >> 2;
    if (pf.sync) {
        pf.ph = 0;
        pf.sync = 0;
    }
    if (song.playing && clk_pos != CLK_START)       /* (JIANT) playing: the transport's 1/16 (seq.c click_tick, through
                                                     * tempo changes and an external clock), so a REPEAT starts on the grid */
        pf.P = div_samples(2);
    ph0 = (song.playing && clk_pos != CLK_START ? clk_pos : pf.ph) % pf.P;   /* the 1/16 in this block (n: none) */
    bnd = ph0 ? (pf.P - ph0 < n ? pf.P - ph0 : n) : 0u;
    pf.ph += n;
    pf.act &= held;                                 /* let go: at once */
    pf.act |= held & ~PF_Q;
    pf.act0 = pf.act;
    q = held & PF_Q & ~pf.act;
    pf.split = n;
    if (q && (!song.playing || bnd < n)) {
        pf.split = song.playing ? bnd : 0u;
        pf.act |= q;
    }
    perf_act = pf.act;
    if ((pf.act & (PF_BIT(PF_DLA) | PF_BIT(PF_D16))) || pfw.etail) {   /* (V10.7.36) the delay's (perf_gfx: through a
                                                     * register, out of the RAM code's reach) */
        void (*f)(uint32_t) = perf_gfx;
        FM1_OPAQUE(f);
        f(n);
    }
    /* KNOB 1 FILTER: left a low-pass, right a high-pass */
    m = perf_k[0];
    {
        int32_t lt = m < 0 ? PF_TOP + m * (PF_TOP - PF_LOW) / 100 : PF_TOP, ht = m > 0 ? m * (PF_TOP - PF_LOW) / 100 : 0;
        pf.lc += clamp(lt - pf.lc, -PF_CSTEP, PF_CSTEP);   /* ~45 ms over the range */
        pf.hc += clamp(ht - pf.hc, -PF_CSTEP, PF_CSTEP);
        for (k = 0; k < 3u; k++) {                  /* the coefficients, between two table rows */
            int32_t i = pf.lc >> 8, f = pf.lc & 255, j = pf.hc >> 8, g = pf.hc & 255;
            pf.lk[k] = (int16_t)(PF_SVF[i][k] + (((PF_SVF[i < 63 ? i + 1 : 63][k] - PF_SVF[i][k]) * f) >> 8));
            pf.hk[k] = (int16_t)(PF_SVF[j][k] + (((PF_SVF[j < 63 ? j + 1 : 63][k] - PF_SVF[j][k]) * g) >> 8));
        }
    }
    pf.mute = 0;
    pf.mon = (uint8_t)mon;
    perf_unhush();
    for (k = 0; k < NTRK; k++)
        if (((mon >> k) & 1u) || pf.mg[k] != 32768)
            pf.mute |= (uint8_t)(1u << k);
    trk_hush = (uint8_t)mon;                        /* (0.6.15) muted: no new notes (voice.c) */
    pf.busy = 1;
    return 1;
}

/* a track muted (GLO's solo: the others): its signal ramps to 0 and back (fx.c mix_part, after the SLICER) */
static __attribute__((noinline)) void perf_hush_end(uint32_t k)   /* (out of perf_mute's loop: target_budget.py) */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[k].v[i];
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = v->env_out = 0;
    }
}
static __attribute__((noinline)) void perf_mute(uint32_t k, int32_t *b, uint32_t n)
{
    uint32_t i;
    int32_t g = pf.mg[k], t = (pf.mon >> k) & 1u ? 0 : 32768;
    for (i = 0; i < n; i++) {
        g += clamp(t - g, -SL_SLOPE, SL_SLOPE);
        b[i] = mulq16(b[i], (uint32_t)g << 1);
    }
    pf.mg[k] = g;
    if (!g && !t)                                       /* (0.6.15) silent: its voices end, the track costs nothing
                                                         * while muted (a MIDI mute: no new notes, trk_hush) */
        perf_hush_end(k);
}

/* (V10.7.36) REV ALL, DLY ALL: the dry mix (and the SLICER's bus) all into the reverb / the delay, the dry muted. Each
 * ramps over 2.9 ms */
static __attribute__((noinline)) void perf_wet(int32_t *ml, int32_t *mr, int32_t *bl, int32_t *br, int32_t *sd, int32_t *sr,
                                               uint32_t n)
{
    uint32_t i, a = pf.act;
    int32_t rt = (a >> PF_RVA) & 1u ? 32768 : 0, dt = (a >> PF_DLA) & 1u ? 32768 : 0;
    int32_t et = (a >> PF_D16) & 1u ? 32768 : 0;
    for (i = 0; i < n; i++) {
        int32_t x = (ml[i] + mr[i] + bl[i] + br[i]) >> 1, g;
        pfw.ew += clamp(et - pfw.ew, -SL_SLOPE, SL_SLOPE);   /* (DLY 1/16: the dry kept, all of it into the delay too) */
        if (pfw.ew)
            sd[i] += mulq16(x, (uint32_t)pfw.ew << 1);
        pfw.rw += clamp(rt - pfw.rw, -SL_SLOPE, SL_SLOPE);
        pfw.dw += clamp(dt - pfw.dw, -SL_SLOPE, SL_SLOPE);
        if (pfw.rw)
            sr[i] += mulq16(x, (uint32_t)pfw.rw << 1);
        if (pfw.dw)
            sd[i] += mulq16(x, (uint32_t)pfw.dw << 1);
        g = pfw.rw > pfw.dw ? pfw.rw : pfw.dw;
        if (g) {
            uint32_t k = (uint32_t)(32768 - g) << 1;
            ml[i] = mulq16(ml[i], k);
            mr[i] = mulq16(mr[i], k);
            bl[i] = mulq16(bl[i], k);
            br[i] = mulq16(br[i], k);
        }
    }
}
/* .. DLY ALL: the delay's MIX at
 * least 100; DLY 1/16: the delay's TIME 1/16 and FEEDBK 95 (fx.c dly_fb: under self-oscillation, its loop's saturation
 * bounds it), MIX at least 100; let go, a bar of TIME 1/16 at FEEDBK 30: the echoes die out quickly, then the user's
 * delay (the matrix's offset, gfx_off, written again by macro_master every block) */
static void pf_goff(uint32_t k, int32_t v) { gfx_off[k] = (int16_t)(gfx_off[k] + v - gfx_val(k)); }
static __attribute__((noinline)) void perf_gfx(uint32_t n)
{
    uint32_t e = (pf.act >> PF_D16) & 1u;
    if (e)
        pfw.etail = 16u * (pf.P ? pf.P : 1u);
    else if (pfw.etail)
        pfw.etail = pfw.etail > n ? pfw.etail - n : 0u;
    if (e || pfw.etail) {
        pf_goff(GF_DTIME, 2);                           /* (N_DTIME: 2 = 1/16) */
        pf_goff(GF_DFDBK, e ? 95 : 30);
    }
    if ((pf.act & (PF_BIT(PF_DLA) | PF_BIT(PF_D16))) && gfx_val(GF_DMIX) < 100)
        pf_goff(GF_DMIX, 100);
}
#define PF_WET (PF_BIT(PF_RVA) | PF_BIT(PF_DLA) | PF_BIT(PF_D16))

/* before the buses: THROW (KNOB 3) adds the dry mix (and the SLICER's bus, bl / br) to the delay and reverb sends */
static __attribute__((noinline)) void perf_pre(int32_t *ml, int32_t *mr, int32_t *bl, int32_t *br, int32_t *sd, int32_t *sr,
                                               uint32_t n)
{
    uint32_t i;
    int32_t m = perf_k[2] * 327;
    for (i = 0; i < n; i++) {
        pf.td += clamp(m - pf.td, -SL_SLOPE, SL_SLOPE);
        if (pf.td) {
            int32_t x = mulq16((ml[i] + mr[i] + bl[i] + br[i]) >> 1, (uint32_t)pf.td << 1);
            sd[i] += x;
            sr[i] += x;
        }
    }
    if ((pf.act & PF_WET) || pfw.rw || pfw.dw || pfw.ew) {   /* (V10.7.36, as perf_gfx: through a register) */
        void (*f)(int32_t *, int32_t *, int32_t *, int32_t *, int32_t *, int32_t *, uint32_t) = perf_wet;
        FM1_OPAQUE(f);
        f(ml, mr, bl, br, sd, sr, n);
    }
}

/* x towards y by w (Q15, 32768 = y) */
static inline int32_t pf_mix(int32_t x, int32_t y, int32_t w)
{
    return w == 32768 ? y : x + mulq16(y - x, (uint32_t)w << 1);
}

/* the loop: frame f of it, both channels (Q15) */
static inline void pb_put(uint32_t f, int32_t l, int32_t r)
{
    int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    p[0] = (int16_t)clamp(l, -32768, 32767);
    p[1] = (int16_t)clamp(r, -32768, 32767);
}
/* sample s of the frames recorded from 0 (n of them): between two frames on an odd one */
static inline void pb_at(uint32_t s, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 1 < n ? s >> 1 : n - 1u;    /* (an odd loop's last sample: the frame before) */
    const int16_t *p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
    const int16_t *q;                               /* a frame is the mean of samples 2f, 2f + 1: sample s */
    if (pf.q > 1u) {                                /* (V10.59) 11 kHz: four samples a frame, linear between frames */
        int32_t fr = (int32_t)(s & 3u);
        f = s >> 2 < n ? s >> 2 : n - 1u;
        p = &sl_buf[0][0] + 2u * (f & (PB_FRAMES - 1u));
        q = f + 1u < n ? p + 2 : p;
        *l = 2 * (p[0] + (((q[0] - p[0]) * fr) >> 2));
        *r = 2 * (p[1] + (((q[1] - p[1]) * fr) >> 2));
        return;
    }
    if (s & 1u)                                     /* lies a quarter frame from it, towards f + 1 or f - 1 */
        q = f + 1u < n ? p + 2 : p;
    else
        q = f ? p - 2 : p;
    *l = (3 * p[0] + q[0]) >> 1;
    *r = (3 * p[1] + q[1]) >> 1;
}
/* REPEAT at sample rp of a loop of len, windowed at both ends */
static inline void pb_loop(uint32_t rp, uint32_t len, uint32_t n, int32_t *l, int32_t *r)
{
    uint32_t e = len - 1u - rp;
    pb_at(rp, n, l, r);
    if (rp < e)
        e = rp;
    if (e < (uint32_t)SL_RAMP) {
        *l = (*l * (int32_t)e) >> SL_RAMP_LOG2;
        *r = (*r * (int32_t)e) >> SL_RAMP_LOG2;
    }
}

/* record the pair into frame f (the mean, >> 1: headroom) */
static inline uint32_t pb_rec(uint32_t f, int32_t l, int32_t r)
{
    pf.hl += l;                                     /* (V10.59: 1 << q samples a frame, their mean >> 1) */
    pf.hr += r;
    if (++pf.odd < (1u << pf.q))
        return f;
    pf.odd = 0;
    pb_put(f, pf.hl >> (pf.q + 1u), pf.hr >> (pf.q + 1u));
    pf.hl = pf.hr = 0;
    return f + 1u;
}

/* (0.9.18's) the loop as a ring (GLITCH: frames recorded on and on) at the place s (Q16 of a 44.1 kHz sample),
 * between its two frames (linear): any speed, backwards too */
static __attribute__((noinline)) void pb_ring_f(uint32_t s, int32_t *l, int32_t *r)
{
    uint32_t f = s >> 17;
    int32_t fr = (int32_t)((s >> 2) & 0x7FFFu);
    const int16_t *p = &sl_buf[f & (PB_FRAMES - 1u)][0], *q = &sl_buf[(f + 1u) & (PB_FRAMES - 1u)][0];
    *l = 2 * (p[0] + (((q[0] - p[0]) * fr) >> 15));
    *r = 2 * (p[1] + (((q[1] - p[1]) * fr) >> 15));
}
/* a slice's place in its 2-bar cycle (and its variation) to 32 random bits */
static uint32_t pf_hash(uint32_t h)
{
    h *= 0x9E3779B1u;
    h = (h ^ (h >> 15)) * 0x2C1B3C6Du;
    return h ^ (h >> 13);
}
static uint32_t semi_q16(int32_t st);              /* fx.c: 2^(st / 12), Q16 */

/* ------------------------------------------------------------------ GLITCH --- */
/* (0.9.9-snd2, ported from 0.9.18; V10.7.36 more of it) the master cut on the transport's 1/16s: each one, by a hash of
 * its place in a 2-bar cycle (two of them, a bar pair each in turn: a 4-bar phrase, the same every time, so it grooves;
 * half of the 1/16s glitched, the beats a quarter, the phrase's last beat nearly all: a fill), played live or: a ratchet
 * (the last 1/16, 1/32 or 1/64 again and again), a ROLL (1/32 or 1/64 repeats, each 3 semitones lower), the last 1/16
 * reversed, an octave up or down, gated (dropouts of a 1/32 or 1/64), SKIP (a CD skipping: tiny fragments, 0.7 ms edges),
 * CRUSH (live, 6 .. 9 bits off, held 2 .. 12 samples: the rate down) or a tape-stop sliver (the 1/16 slowing to a halt).
 * Every slice windowed (SL_RAMP). Below ~82 BPM only its gates, crushes and live slices (the loop holds 371 ms). An
 * action by 5 bits of the hash: ratchets most */
static const uint8_t GA_OF[32] = {GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_RPT, GA_ROLL, GA_ROLL,
                                  GA_ROLL, GA_REV, GA_REV, GA_REV, GA_REV, GA_UP, GA_UP, GA_UP, GA_DN, GA_DN, GA_DN,
                                  GA_GATE, GA_GATE, GA_GATE, GA_SKIP, GA_SKIP, GA_CRU, GA_CRU, GA_CRU, GA_TST, GA_TST, GA_TST};
/* a segment's start: where it reads (Q16) and how fast (SKIP: a place in the 1/16, two segments at each) */
static __attribute__((noinline)) void perf_glt_seg(void)
{
    uint32_t a = pf.u.g.a, L = pf.u.g.l, g0 = pf.u.g.g0, s = g0 - L;
    pf.u.g.rate = 65536;
    if (a == GA_RPT) {
        pf.u.g.rate = (int32_t)semi_q16(clamp((int32_t)(pf.u.g.k < 24u ? pf.u.g.k : 24u) * GX_PITCH, -24, 24));
    } else if (a == GA_ROLL) {
        pf.u.g.rate = (int32_t)semi_q16(-3 * (int32_t)(pf.u.g.k < 4u ? pf.u.g.k : 4u));
    } else if (a == GA_REV) {
        s = g0 - 1u;
        pf.u.g.rate = -65536;
    } else if (a == GA_UP) {
        s = g0 - 2u * L;
        pf.u.g.rate = 131072;
    } else if (a == GA_DN) {
        pf.u.g.rate = 32768;
    } else if (a == GA_TST) {
        s = g0 - 8u;                                /* (from the live place, falling behind: perf_glt) */
    } else if (a == GA_SKIP) {
        s = g0 - pf.u.g.sp + pf_hash(pf.gs * 64u + (pf.u.g.k >> 1)) % (pf.u.g.sp / L) * L;   /* (p9c: on its own grid,
                                                         * a whole number of fragments into the 1/16, not any sample) */
    }
    pf.u.g.src = s << 16;
}
/* the next 1/16's slice */
static __attribute__((noinline)) void perf_glt_step(void)
{
    uint32_t c = pf.gs & 63u, h = pf_hash((pf.gs & 31u) + 0x51u + ((uint32_t)(GX_VARI - 1) << 5) + ((pf.gs >> 5) & 1u) * 0x3A7u),
             s16 = pf.P ? pf.P : 1u, sp = s16 < GL_SPAN ? s16 : GL_SPAN, d = GX_DENS, z = GX_SIZE, j = (h >> 12) % 3u, a, L;
    d = c >= 60u ? 90u : c & 3u ? d : d * d / 100u;   /* (the beats fewer; the phrase's last beat: a fill) */
    a = ((h >> 24) * 100u >> 8) < d ? GA_OF[(h >> 7) & 31u] : GA_PASS;
    pf.u.g.g0 = 2u * pf.wr + pf.odd;                /* (the loop's sample now) */
    if ((s16 + sp > PB_MAX - 512u || pf.u.g.g0 < sp) && a != GA_GATE && a != GA_CRU)
        a = GA_PASS;                                /* (too slow: what it would read is gone; or not recorded yet) */
    L = a == GA_RPT ? sp >> (z + j) : a == GA_ROLL ? sp >> (1u + z + j % 2u)
      : a == GA_GATE ? (s16 / 2u) >> (z / 2u + 1u + j % 2u) : a == GA_SKIP ? sp >> (3u + z / 2u + j % 2u)
      : a == GA_UP ? sp / 2u : a == GA_CRU || a == GA_TST ? s16 : sp;
    pf.u.g.a = (uint8_t)a;
    pf.u.g.l = L < 2u * SL_RAMP ? 2u * SL_RAMP : L;
    pf.u.g.sp = sp;
    pf.u.g.h = (uint8_t)(2u + (h >> 14) % 11u);
    pf.u.g.sh = (uint8_t)(6u + j + (h >> 20) % 2u);
    pf.u.g.n = 255u;
    pf.u.g.u = pf.u.g.k = pf.gph = 0;
    perf_glt_seg();
}
/* GLITCH on one sample: 1 = it plays (pf.yl yr), 0 = live */
static __attribute__((noinline)) int perf_glt(int32_t l, int32_t r)
{
    uint32_t u = pf.u.g.u, L = pf.u.g.l, a = pf.u.g.a, e = L - 1u - u < u ? L - 1u - u : u, g = 32768u;
    pf.wr = pb_rec(pf.wr, l, r);
    if (a != GA_PASS) {
        if (a == GA_SKIP) {
            if (e < SL_RAMP / 4u)
                g = e << (17 - SL_RAMP_LOG2);
        } else if (e < (uint32_t)SL_RAMP) {
            g = e << (15 - SL_RAMP_LOG2);
        }
        if (a == GA_GATE) {
            g = pf.u.g.k & 1u ? 0u : g;
            pf.yl = mulq16(l, g << 1);
            pf.yr = mulq16(r, g << 1);
        } else if (a == GA_CRU) {
            if (++pf.u.g.n >= pf.u.g.h) {
                int32_t b = 1 << (pf.u.g.sh - 1u);
                pf.u.g.n = 0;
                pf.u.g.cl = ((l + b) >> pf.u.g.sh) << pf.u.g.sh;
                pf.u.g.cr = ((r + b) >> pf.u.g.sh) << pf.u.g.sh;
            }
            pf.yl = mulq16(pf.u.g.cl, g << 1);
            pf.yr = mulq16(pf.u.g.cr, g << 1);
        } else {
            uint32_t at = pf.u.g.src + (uint32_t)((int32_t)u * (pf.u.g.rate >> 2)) * 4u;
            if (a == GA_TST) {                      /* (V10.7.36) a tape stop in the 1/16: the speed falling to 0, the
                                                     * read u - u^2 / 2L behind; quieter in its last quarter */
                uint32_t v = L - u;
                at = pf.u.g.src + ((u - u * u / (2u * L)) << 16);
                if (4u * v < L && (v << 17) / L < g)
                    g = (v << 17) / L;
            }
            pb_ring_f(at, &pf.yl, &pf.yr);
            pf.yl = mulq16(pf.yl, g << 1);
            pf.yr = mulq16(pf.yr, g << 1);
        }
    }
    if (++pf.u.g.u >= L) {
        pf.u.g.u = 0;
        pf.u.g.k++;
        perf_glt_seg();
    }
    if (++pf.gph >= pf.P) {
        pf.gs = song.playing && clk_pos != CLK_START ? clk_n + 1u : pf.gs + 1u;   /* (p9c: the transport's own 1/16, so its
                                                         * 2-bar cycle stays on the bar) */
        perf_glt_step();
    }
    return a != GA_PASS;
}

/* ------------------------------------------- REVERSE, TAPE STOP, PITCH --- */
/* (V10.7.36) recorded on and on into the loop as GLITCH (a ring), read back between its frames (pb_ring_f).
 * REVERSE: window after window (perf_buf_start: a 1/16 mostly) each plays the one before it backwards (the read
 *   2 windows behind at most), windowed at both ends (SL_RAMP); the first window only records (live).
 * TAPE STOP: from the live place, the speed falling straight to 0 over len (the read len / 2 behind at the end), quieter
 *   in its last quarter of speed; then silence while held.
 * PITCH UP / DOWN: a delay read at the speed v (Q16): the delay shrinks (up) or grows (down) by 1 - v a sample; at its
 *   end it jumps by PT_G (46 ms), the old read faded out over PT_X samples (one read otherwise: no comb). v from 1 to 2
 *   (or 1/2) over a bar: 12 semitones, then held (2^x ~ 1 + x (0.6565 + 0.3435 x)). 1 = it plays (pf.yl yr) */
static int perf_frz(int32_t l, int32_t r);
static __attribute__((noinline)) int perf_rtp(int32_t l, int32_t r)
{
    uint32_t g0, t = pf.u.r.t, L = pf.u.r.len, g = 32768u;
    if (pf.mode == BM_FRZ)                          /* (V10.7.37: from here, perf_buf as it was) */
        return perf_frz(l, r);
    pf.wr = pb_rec(pf.wr, l, r);
    g0 = 2u * pf.wr + pf.odd;
    if (pf.mode == BM_REV) {
        uint32_t e;
        if (++t >= L) {
            t = 0;
            pf.u.r.s0 = g0;
            pf.u.r.k++;
        }
        pf.u.r.t = t;
        if (!pf.u.r.k)
            return 0;
        e = L - 1u - t < t ? L - 1u - t : t;
        if (e < (uint32_t)SL_RAMP)
            g = e << (15 - SL_RAMP_LOG2);
        pb_ring_f((pf.u.r.s0 - 4u - t) << 16, &pf.yl, &pf.yr);
    } else if (pf.mode == BM_TAPE) {
        uint32_t v = (uint32_t)pf.u.r.v;
        if (!t)
            pf.u.r.d = (g0 - 8u) << 16;
        if (t < L) {
            pf.u.r.t = ++t;
            pf.u.r.d += v >> 8;
            pf.u.r.v = (int32_t)(v > (1u << 24) / L && t < L ? v - (1u << 24) / L : 0u);
        }
        if (t < 16u)
            return 0;
        if (!v) {
            pf.yl = pf.yr = 0;
            return 1;
        }
        if (v < 1u << 22)
            g = v >> 7;
        pb_ring_f(pf.u.r.d, &pf.yl, &pf.yr);
    } else {
        int32_t dv;
        if (!(t & 31u)) {                           /* the speed, every 32 samples */
            uint32_t bar = 16u * (pf.P ? pf.P : 1u), x = (t << 8) / ((bar >> 8) | 1u), up;
            x = x < 65535u ? x : 65535u;
            up = 65536u + ((x * (43024u + ((22512u * x) >> 16))) >> 16);
            pf.u.r.v = (int32_t)(pf.u.r.k ? 0xFFFFFFFFu / up : up);
        }
        pf.u.r.t = t < 0xFFFFFF00u ? t + 1u : t;
        dv = 65536 - pf.u.r.v;
        pf.u.r.d += (uint32_t)dv;
        if (pf.u.r.xn) {
            pf.u.r.xd += (uint32_t)dv;
            pf.u.r.xn--;
        } else if (pf.u.r.d < PT_LO << 16 || pf.u.r.d > (PT_LO + PT_G) << 16) {
            pf.u.r.xd = pf.u.r.d;
            pf.u.r.d = dv < 0 ? pf.u.r.d + (PT_G << 16) : pf.u.r.d - (PT_G << 16);
            pf.u.r.xn = PT_X;
        }
        if (t < (pf.u.r.d >> 16) + 16u)            /* (not recorded yet) */
            return 0;
        pb_ring_f((g0 << 16) - pf.u.r.d, &pf.yl, &pf.yr);
        if (pf.u.r.xn) {
            int32_t ol, orr, k = (int32_t)pf.u.r.xn;
            pb_ring_f((g0 << 16) - pf.u.r.xd, &ol, &orr);
            pf.yl += ((ol - pf.yl) * k) >> 8;
            pf.yr += ((orr - pf.yr) * k) >> 8;
        }
        return 1;
    }
    pf.yl = mulq16(pf.yl, g << 1);
    pf.yr = mulq16(pf.yr, g << 1);
    return 1;
}

/* ------------------------------------------------------------------- RING --- */
/* (V10.7.36) the master times a sine two octaves over the root of the selected track's scale (C5 523 Hz .. B5), its
 * depth wobbling 12 % .. 98 % once a bar: x (1 - d + d sin), never louder than x; in and out over 2.9 ms */
#define RING_C5 50960238u                   /* 523.25 Hz as a phase step a sample (2^32 / 44100) */
static __attribute__((noinline)) void perf_ring(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, bar = 16u * (pf.P ? pf.P : 1u);
    uint32_t inc = (uint32_t)(umul64(RING_C5, semi_q16(clamp(trk[song.sel % NTRK].p[P_ROOT], 0, 11))) >> 16);
    int32_t t = (pf.act >> PF_RING) & 1u ? 32768 : 0, d;
    pfw.wph += 0xFFFFFFFFu / bar * n;
    d = 18000 + ((sine_i(pfw.wph) * 14000) >> 15);
    for (i = 0; i < n; i++) {
        int32_t gd, k;
        pfw.rg += clamp(t - pfw.rg, -SL_SLOPE, SL_SLOPE);
        gd = (pfw.rg * d) >> 15;
        k = 65536 - gd + ((gd * sine_i(pfw.rph)) >> 15);   /* ((1 - d + d sin) + 1) / 2, Q16: 0 .. 1 */
        pfw.rph += inc;
        bl[i] = 2 * mulq16(bl[i], (uint32_t)k) - bl[i];
        br[i] = 2 * mulq16(br[i], (uint32_t)k) - br[i];
    }
}
/* ----------------------------------------------------------------- WOBBLE --- */
/* (V10.7.36) a resonant low-pass on the master (a state-variable filter, Q 2, out at 3/4), its cutoff opening and closing once every
 * 1/8 of the transport (80 Hz shut on the 1/8s, ~1.8 kHz open between them: 54 semitones on a raised cosine); the
 * states held to +-2^24 (bounded); in and out over 2.9 ms */
static __attribute__((noinline)) void perf_wobble(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, P = pf.P ? pf.P : 1u, inc = 0xFFFFFFFFu / (2u * P);
    uint32_t pos = song.playing && clk_pos != CLK_START ? (clk_n & 1u) * P + clk_pos : pf.ph - n;   /* (the block's start
                                                     * in its 1/8) */
    int32_t t = (pf.act >> PF_WOB) & 1u ? 32768 : 0, f = 0;
    for (i = 0; i < n; i++) {
        int32_t x[2] = {bl[i], br[i]}, c;
        uint32_t k;
        if (!(i & 7u)) {                            /* the cutoff every 8 samples: 80 Hz x 2^(st / 12), f = 2 pi fc / fs */
            int32_t st = (54 * (32768 - sine_i((((pos + i) % (2u * P)) * inc) + 0x40000000u))) >> 8;   /* (Q8, 0 .. 54) */
            uint32_t a = semi_q16(st >> 8), b = semi_q16((st >> 8) + 1);
            f = (int32_t)((373u * (a + (((b - a) * (uint32_t)(st & 255)) >> 8))) >> 16);
        }
        pfw.wg += clamp(t - pfw.wg, -SL_SLOPE, SL_SLOPE);
        for (k = 0; k < 2u; k++) {
            int32_t hi = (x[k] >> 1) - pfw.wl[k] - mulq16(pfw.wb[k], 32768u);   /* (q 0.5: a peak of ~2 at the cutoff) */
            pfw.wb[k] = clamp(pfw.wb[k] + mulq16(hi, (uint32_t)f << 1), -(1 << 24), 1 << 24);
            pfw.wl[k] = clamp(pfw.wl[k] + mulq16(pfw.wb[k], (uint32_t)f << 1), -(1 << 24), 1 << 24);
            c = pf_mix(x[k], pfw.wl[k] + (pfw.wl[k] >> 1), pfw.wg);   /* (x 3/4: the peak ~1.5) */
            x[k] = c;
        }
        bl[i] = x[0];
        br[i] = x[1];
    }
    if (!pfw.wg)
        pfw.wl[0] = pfw.wl[1] = pfw.wb[0] = pfw.wb[1] = 0;
}

/* ------------------------------------------------------------------- COMB --- */
/* (V10.7.36) a buffer effect: sl_buf's first 1024 frames a stereo line at 44.1 kHz (int16, half the level), y = (1 - g) x
 * + g lp(y[n - D]): peaks of gain 1 at the root's harmonics (C3 .. B3, the selected track's scale) and gaps between, its
 * loop a little darker each pass (lp: a one-pole); D swaying a quarter tone up and down over 2 bars, g swaying over
 * a bar (0.86 .. 0.96, never 1: bounded, |y| <= the input's peak); out 1.5 y and an eighth of the dry (<= 1.625 x the
 * input's peak). 1 = it plays (pf.yl yr) */
#define COMB_C3 22093949u                   /* 44100 / 130.81 Hz in samples, Q16 */
static __attribute__((noinline)) int perf_comb(int32_t l, int32_t r)
{
    uint32_t t = pf.u.c.t++, P = pf.P ? pf.P : 1u, D;
    int32_t g, y[2], x[2] = {l, r}, *z[2] = {&pf.u.c.zl, &pf.u.c.zr};
    uint32_t k;
    if (!(t & 31u)) {                               /* the delay and the feedback, every 32 samples */
        D = (uint32_t)(umul64(COMB_C3, semi_q16(-clamp(trk[song.sel % NTRK].p[P_ROOT], 0, 11))) >> 16);
        D += (uint32_t)(((int32_t)(D >> 6) * sine_i(t * (0xFFFFFFFFu / (32u * P)))) >> 15);   /* (+-1.6 %: a quarter tone) */
        pf.u.c.d = D;
        pf.u.c.g = 29819 + ((1638 * sine_i(t * (0xFFFFFFFFu / (16u * P)) + 0x40000000u)) >> 15);   /* (0.91 +- 0.05) */
    }
    D = pf.u.c.d;
    g = pf.u.c.g;
    for (k = 0; k < 2u; k++) {
        uint32_t at = t - (D >> 16);
        int32_t fr = (int32_t)((D >> 1) & 0x7FFFu), a = sl_buf[at & 1023u][k] * 2, b = sl_buf[(at - 1u) & 1023u][k] * 2;
        int32_t d = a + (((b - a) * fr) >> 15);
        *z[k] += ((d - *z[k]) * 3) >> 2;            /* (the loop's low-pass, gentle) */
        y[k] = mulq16(x[k], (uint32_t)(32768 - g) << 1) + mulq16(*z[k], (uint32_t)g << 1);
        sl_buf[t & 1023u][k] = (int16_t)clamp(y[k] >> 1, -32768, 32767);
    }
    pf.yl = y[0] + (y[0] >> 1) + (l >> 3);         /* (x 1.5: what the gaps between the peaks take, back) */
    pf.yr = y[1] + (y[1] >> 1) + (r >> 3);
    return 1;
}

/* ----------------------------------------------------------------- FREEZE --- */
/* (V10.7.37) a buffer effect: len samples (a 1/4 beat) recorded (live meanwhile), then held: two grains of 2 h samples
 * (h = len / 4) overlapping by half, each windowed by sin^2 (their windows sum to 1: no seam, no level step), reading the
 * window forward: each new grain starts h after the one before (where it reads now: in phase with it) and ~1.5 ms
 * jittered, back to a random place past the window's first eighth when the next would pass its end. 1 = it plays (pf.yl yr) */
static __attribute__((noinline)) int perf_frz(int32_t l, int32_t r)
{
    uint32_t t = pf.u.r.t, h = pf.u.r.len;
    int32_t w, al, ar;
    if (!pf.state) {
        pf.wr = pb_rec(pf.wr, l, r);
        if (++pf.rp >= pf.len)
            pf.state = 1;
        return 0;
    }
    if (t >= h) {                                  /* the next grain: the one in its first half to its second */
        uint32_t q = pf_hash(++pf.u.r.k), s = pf.u.r.s0 + h + q % 129u - 64u;
        pf.u.r.d = pf.u.r.s0;
        pf.u.r.s0 = s + 2u * h > pf.len || (int32_t)s < 0 ? h / 2u + (q >> 8) % (3u * h / 2u) : s;   /* (back: anywhere
                                                   * past the window's first eighth, its attack, so no tick repeats) */
        t = 0;
    }
    pf.u.r.t = t + 1u;
    w = (32768 - sine_i(t * (0x80000000u / h) + 0x40000000u)) >> 1;   /* sin^2: the first half's rising window */
    pb_ring_f((pf.u.r.s0 + t) << 16, &al, &ar);
    pb_ring_f((pf.u.r.d + h + t) << 16, &pf.yl, &pf.yr);
    pf.yl += mulq16(al - pf.yl, (uint32_t)w << 1);
    pf.yr += mulq16(ar - pf.yr, (uint32_t)w << 1);
    return 1;
}

/* the buffer effect faded out: the next one, or the buffer back. Once per effect, out of the per-sample loop
 * (noinline: its calls stay out of perf_block's loop code) */
static __attribute__((noinline)) void perf_buf_done(void)
{
    if (pf.next != PF_N) {
        perf_buf_start(pf.next);
    } else {
        pf.mode = BM_NONE;
        if (sl_lent) {
            sl_lent = 0;
            perf_drop_slicer();
        }
    }
}

/* the buffer effect on one sample: the share pf.w of what it plays, the rest live (snd2: out of perf_block's loop, as
 * 0.9.18's: the RAM code's room) */
static __attribute__((noinline)) void perf_buf(int32_t *pl, int32_t *pr)
{
    int32_t l = *pl, r = *pr, yl = 0, yr = 0, on = 0;
    if (pf.mode == BM_LOOP) {
        if (!pf.state) {
            pf.wr = pb_rec(pf.wr, l, r);
            if (++pf.rp >= pf.len) {                /* (rp: samples recorded) */
                pf.state = 1;
                pf.rp = 0;
            }
        } else {
            pb_loop(pf.rp, pf.len, pf.wr, &yl, &yr);
            if (pf.xn) {                            /* the loop it switched from, fading out */
                int32_t ol, orr, g = (int32_t)pf.xn;
                pb_loop(pf.xrp, pf.xlen, pf.wr, &ol, &orr);
                yl += (ol * g) >> SL_RAMP_LOG2;
                yr += (orr * g) >> SL_RAMP_LOG2;
                pf.xn--;
                if (++pf.xrp >= pf.xlen)
                    pf.xrp = 0;
            }
            if (++pf.rp >= pf.len)
                pf.rp = 0;
            on = 1;
        }
    } else if ((on = pf.mode == BM_GLT ? perf_glt(l, r) : pf.mode == BM_COMB ? perf_comb(l, r) : perf_rtp(l, r)) != 0) {   /* (snd2) GLITCH; (V10.7.36) REVERSE,
                                                     * TAPE STOP, PITCH; (V10.7.37) FREEZE (perf_rtp's) */
        yl = pf.yl;
        yr = pf.yr;
    }
    pf.tw = on && pf.src != PF_N ? 32768 - perf_k[3] * 327 : 0;
    pf.w += clamp(pf.tw - pf.w, -SL_SLOPE, SL_SLOPE);
    if (pf.w) {
        *pl = pf_mix(l, yl, pf.w);
        *pr = pf_mix(r, yr, pf.w);
    } else if (pf.src == PF_N) {                    /* faded out: the next one, or the buffer back */
        perf_buf_done();
    }
}

/* one channel of a state-variable filter (k = 1): x in Q15 >> 2 */
static inline int32_t pf_svf(int32_t x, int32_t *z, const int16_t *c, int hp)
{
    int32_t v3 = x - z[1], v1 = (c[0] * z[0] + c[1] * v3) >> 14, v2 = z[1] + ((c[1] * z[0] + c[2] * v3) >> 14);
    z[0] = 2 * v1 - z[0];
    z[1] = 2 * v2 - z[1];
    return hp ? x - v1 - v2 : v2;
}

static __attribute__((noinline)) void perf_switch(uint32_t a) { perf_buf_select(perf_pick(a)); }

/* after the master level: the buffer effect, KNOB 1's low-pass and high-pass, CRUSH, RING on l / r (stereo, Q15) */
static __attribute__((noinline)) void perf_block(int32_t *bl, int32_t *br, uint32_t n)
{
    uint32_t i, a = pf.act0, sh, hold;
    int32_t c = perf_k[1], ct, lt = perf_k[0] < 0 || pf.lc < PF_TOP ? 32768 : 0, ht = perf_k[0] > 0 || pf.hc > 0 ? 32768 : 0;
    if (c)
        pf.cc = c;
    ct = c ? 32768 : 0;
    sh = 4u + (uint32_t)pf.cc * 7u / 100u;
    hold = sh - 3u;
    frz_on = pf.mode == BM_FRZ && pf.state;            /* (V10.7.41: voice.c voice_soft) */
    if (pf.split)
        perf_switch(a);
    for (i = 0; i < n; i++) {
        int32_t l = bl[i], r = br[i];
        if (i == pf.split) {
            a = pf.act;
            perf_switch(a);
        }
        if (pf.mode != BM_NONE)
            perf_buf(&l, &r);
        pf.la += clamp(lt - pf.la, -SL_SLOPE, SL_SLOPE);
        if (pf.la) {
            int32_t fl = pf_svf(clamp(l >> 2, -32767, 32767), &pf.lz[0], pf.lk, 0) << 2;
            int32_t fr = pf_svf(clamp(r >> 2, -32767, 32767), &pf.lz[2], pf.lk, 0) << 2;
            l = pf_mix(l, fl, pf.la);
            r = pf_mix(r, fr, pf.la);
        } else {
            pf.lz[0] = pf.lz[1] = pf.lz[2] = pf.lz[3] = 0;
        }
        pf.ha += clamp(ht - pf.ha, -SL_SLOPE, SL_SLOPE);
        if (pf.ha) {
            int32_t fl = pf_svf(clamp(l >> 2, -32767, 32767), &pf.hz[0], pf.hk, 1) << 2;
            int32_t fr = pf_svf(clamp(r >> 2, -32767, 32767), &pf.hz[2], pf.hk, 1) << 2;
            l = pf_mix(l, fl, pf.ha);
            r = pf_mix(r, fr, pf.ha);
        } else {
            pf.hz[0] = pf.hz[1] = pf.hz[2] = pf.hz[3] = 0;
        }
        {   /* CRUSH: fewer bits, a held sample (KNOB 2) */
            pf.cw += clamp(ct - pf.cw, -SL_SLOPE, SL_SLOPE);
            if (pf.cw) {
                if (++pf.cn >= hold) {
                    pf.cn = 0;
                    pf.chl = ((l >> sh) << sh) + (1 << (sh - 1u));
                    pf.chr = ((r >> sh) << sh) + (1 << (sh - 1u));
                }
                l = pf_mix(l, pf.chl, pf.cw);
                r = pf_mix(r, pf.chr, pf.cw);
            }
        }
        bl[i] = l;
        br[i] = r;
    }
    if ((pf.act & PF_BIT(PF_RING)) || pfw.rg) {    /* (V10.7.36, as perf_gfx: through a register) */
        void (*f)(int32_t *, int32_t *, uint32_t) = perf_ring;
        FM1_OPAQUE(f);
        f(bl, br, n);
    }
    if ((pf.act & PF_BIT(PF_WOB)) || pfw.wg) {
        void (*f)(int32_t *, int32_t *, uint32_t) = perf_wobble;
        FM1_OPAQUE(f);
        f(bl, br, n);
    }
    pf.busy = pf.act || pf.mode != BM_NONE || pf.w || pf.la || pf.ha || pf.cw || pf.td || pf.mute || perf_k[0] ||
              perf_k[1] || perf_k[2] || pfw.rw || pfw.dw || pfw.rg ||
              pfw.ew || pfw.wg || pfw.etail;
}
