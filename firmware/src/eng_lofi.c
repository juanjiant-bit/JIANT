/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Chip voice: pulse/triangle/saw/noise/wave RAM, quantised amplitude and a held
 * (downsampled) output; SWEEP bends the pitch down after note-on.
 *
 * WAVE = WRAM plays a 32-sample, 4-bit wave table, read step-wise (no interpolation);
 * DUTY then picks one of 16 built-in tables (DUTY / 8, shown as "WAV#" with its name).
 * CHIP = STEP: a 4-bit stepped volume envelope with a frame-clock divider replaces the
 * ADSR curve (lofi_amp), the triangle becomes a 32-step 4-bit staircase, the pulse
 * duties are 12.5 / 25 / 50 / 75 %; CRSH then sets the envelope (shown as "DCY"). */
static const char *const N_CHIP[] = {"4BIT", "4B/2", "8BIT", "1BIT", "STEP"};
static const char *const N_RWAVE[] = {"PLS", "TRI", "SAW", "NOIS", "WRAM", "BYTE"};
static const char *const N_RARP[] = {"OFF", "OCT", "MAJ", "MIN"};
enum { CHIP_4BIT, CHIP_4B2, CHIP_8BIT, CHIP_1BIT, CHIP_STEP };
enum { RW_PLS, RW_TRI, RW_SAW, RW_NOIS, RW_WRAM, RW_BYTE };

/* wave RAM: 16 tables x 32 samples x 4 bits, two samples a byte (high nibble first);
 * our own shapes, from simple formulas (squares, sine, triangle, saws, harmonic sums,
 * one formant peak for the vowels). DC is taken out per table in lofi_render. */
static const char *const N_WRAM[] = {"SQ50", "SQ25", "SQ12", "SINE", "TRI", "SAW", "SAW2", "BAS1",
                                     "BAS2", "ORGN", "HOLW", "VOXA", "VOXO", "VOXE", "BUZZ", "STAIR", 0};
static const uint8_t WRAM[16][16] = {
    {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ50 */
    {0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ25 */
    {0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},   /* SQ12 */
    {0x89, 0xAC, 0xDE, 0xEF, 0xFF, 0xEE, 0xDC, 0xA9, 0x86, 0x53, 0x21, 0x10, 0x00, 0x11, 0x23, 0x56},   /* SINE */
    {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10},   /* TRI */
    {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF},   /* SAW */
    {0x01, 0x12, 0x33, 0x45, 0x56, 0x77, 0x89, 0x9A, 0x56, 0x67, 0x88, 0x9A, 0xAB, 0xCC, 0xDE, 0xEF},   /* SAW2: + its octave */
    {0xEE, 0xEF, 0xFF, 0xFF, 0xFF, 0xEE, 0xED, 0xDC, 0x11, 0x10, 0x00, 0x00, 0x00, 0x11, 0x12, 0x23},   /* BAS1: sloped square */
    {0xCE, 0xFF, 0xED, 0xBA, 0x9A, 0xBD, 0xEF, 0xFE, 0xC9, 0x64, 0x21, 0x00, 0x00, 0x01, 0x24, 0x69},   /* BAS2: 1 + 2 + 3 */
    {0x8B, 0xEF, 0xED, 0xBA, 0x9A, 0xAA, 0x98, 0x77, 0x88, 0x87, 0x65, 0x55, 0x65, 0x42, 0x10, 0x14},   /* ORGN: 1..4 */
    {0x8D, 0xFE, 0xDD, 0xDD, 0xCD, 0xDD, 0xDE, 0xFD, 0x82, 0x01, 0x22, 0x22, 0x32, 0x22, 0x21, 0x02},   /* HOLW: odd */
    {0x8F, 0xFC, 0xA7, 0x67, 0x89, 0x88, 0x88, 0x87, 0x88, 0x77, 0x77, 0x76, 0x78, 0x98, 0x53, 0x00},   /* VOXA: "ah" */
    {0x8F, 0xEE, 0xFD, 0xDB, 0x98, 0x66, 0x66, 0x77, 0x88, 0x89, 0x99, 0x97, 0x64, 0x22, 0x01, 0x10},   /* VOXO: "oh" */
    {0x7F, 0x96, 0xCF, 0xBB, 0xCC, 0xBB, 0xAA, 0x98, 0x77, 0x65, 0x54, 0x43, 0x34, 0x40, 0x39, 0x60},   /* VOXE: "ee" */
    {0xF3, 0x00, 0x12, 0x22, 0x11, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22},   /* BUZZ: spike + ring */
    {0xFF, 0xFF, 0xFF, 0xFF, 0xAA, 0xAA, 0xAA, 0xAA, 0x55, 0x55, 0x55, 0x55, 0x00, 0x00, 0x00, 0x00},   /* STAIR: 4 levels */
};
#define Q4 2184                  /* one 4-bit step (2 * 2184 * 7.5 = 32760) */

/* CHIP = STEP: CRSH as the envelope, CRSH / 4 = b: 0 constant volume, 1..16 a one-shot decay
 * (divider period 0..15), 17..31 a looping one (period 1..15) */
static const char *const N_DCY[] = {"CONST", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7", "D8", "D9", "D10",
                                    "D11", "D12", "D13", "D14", "D15", "L1", "L2", "L3", "L4", "L5", "L6", "L7",
                                    "L8", "L9", "L10", "L11", "L12", "L13", "L14", "L15", 0};
static const param_desc_t LOFI_WAVNUM = {"WAV#", F_INT, 0, 127, 64, N_WRAM, 0};   /* = edit[2] but the names */
static const param_desc_t LOFI_DCY = {"DCY", F_INT, 0, 127, 0, N_DCY, 0};        /* = edit[3] but the names */

/* (JIANT 0.4) WAVE = BYTE: bytebeat, an 8-bit formula of the time t played as the wave (after viznut's 2011
 * one-liners: the music is in the formula). DUTY picks one of 32 (ALGO, DUTY / 4), CRSH is the formulas' variable
 * a (VAR: 1 .. 16); t runs with the note played (C4: ~8 kHz, the classic rate; an octave up twice as fast), restarts
 * at each note-on; the hold (4B/2), the bits (CHIP), TONE and the rest as for the other waves. Our own picks and
 * variations; each a few operations a sample */
static const char *const N_BYTE[] = {"B01", "B02", "B03", "B04", "B05", "B06", "B07", "B08", "B09", "B10", "B11",
                                     "B12", "B13", "B14", "B15", "B16", "B17", "B18", "B19", "B20", "B21", "B22",
                                     "B23", "B24", "B25", "B26", "B27", "B28", "B29", "B30", "B31", "B32", 0};
static const param_desc_t LOFI_ALGO = {"ALGO", F_INT, 0, 127, 64, N_BYTE, 0};    /* = edit[2] (BYTE) */
static const param_desc_t LOFI_VAR = {"VAR", F_PCT, 0, 127, 0, 0, 0};            /* = edit[3] (BYTE) */
static __attribute__((noinline)) uint32_t lofi_byte(uint32_t f, uint32_t t, uint32_t a)
{
    switch (f & 31u) {
    case 0: return t * (42u & t >> 10);
    case 1: return t * ((t >> 12 | t >> 8) & 63u & t >> 4);
    case 2: return t * (t >> 5 | t >> 8) >> (t >> 16 & 7u);
    case 3: return t * (t >> 11 & t >> 8 & 123u & t >> 3);
    case 4: return (t >> 6 | t | t >> (t >> 16 & 15u)) * 10u + (t >> 11 & 7u);
    case 5: return (t * 5u & t >> 7) | (t * 3u & t * 4u >> 10);
    case 6: return (t * 9u & t >> 4) | (t * 5u & t >> 7) | (t * 3u & t >> 10);
    case 7: return (t * (t >> 9 | t >> 13) & 16u) * 15u;
    case 8: return t & t >> 8;
    case 9: return (t >> 7 | t | t >> 6) * 10u + 4u * ((t & t >> 13) | t >> 6);
    case 10: return (t * (0xCA98u >> (t >> 9 & 14u) & 15u)) | t >> 8;
    case 11: return (t * (t >> 8 | t >> 9) & 46u & t >> 8) ^ ((t & t >> 13) | t >> 6);
    case 12: return (t * 3u & t >> 8) | (t >> 4 & t * 7u);
    case 13: return t * (t >> (t >> 9 & 7u) & 63u & t >> 4);
    case 14: return (t >> 4) * (13u & 0x8898A989u >> (t >> 11 & 30u));
    case 15: return t * ((t >> 3 | t >> 9) & 82u & t >> 9);
    case 16: return t * (a & t >> 10);                                  /* (a: VAR) */
    case 17: return t * ((t >> (a & 15u) | t >> 8) & 63u & t >> 4);
    case 18: return (t * a & t >> 7) | (t * 3u & t >> 10);
    case 19: return (t >> a | t) * (t >> 9 & 7u);
    case 20: return t * (t >> 8 & a) ^ t >> 4;
    case 21: return (t * a & t >> 8) * (t >> 12 & 3u);
    case 22: return t * (0x9AD5u >> (t >> (8u + (a & 3u)) & 14u) & 15u);
    case 23: return (t ^ t >> a) * (t >> 10 & 5u);
    case 24: return t >> 2 & (t * a >> 6 | t >> 9);
    case 25: return (t | t >> a | t >> 7) * (t >> 13 & 7u);
    case 26: return t * ((t >> 11 & 3u) + a) & t >> 6;
    case 27: return (t * (a | 1u) % (257u - (t >> 10 & 127u))) & 255u;
    case 28: return t * (t >> 10 & 7u & a) | t >> 5;
    case 29: return (t >> 5 & t >> 7) * a + (t >> 9 & 32u);
    case 30: return t * (t >> 12 & a ? 3u : 2u) & t >> 6;
    default: return (t * a >> 4 | t >> 3) ^ (t >> 7 & t >> 11);
    }
}
/* BYTE's samples (lofi_render's set-up done: inc the note's, the hold, the bits): t in v->s[2], its fraction (Q24) in
 * v->ph[0] */
static __attribute__((noinline)) void lofi_byte_render(track_t *t, voice_t *v, int32_t *out, uint32_t n,
                                                      const vmod_t *m, uint32_t inc, int32_t hold, int32_t bits, int32_t lpk)
{
    const int16_t *p = t->p;
    uint32_t f = (uint32_t)p[P_E2] >> 2, a = 1u + ((uint32_t)p[P_E3] >> 3), tt = (uint32_t)v->s[2], tf = v->ph[0], i;
    uint32_t ti = (inc >> 8) * 31u;                     /* t a sample, Q24: C4 (inc ~2.55e7) ~0.18 = 8 kHz */
    int32_t held = v->s[0], cnt = v->s[1], lp = v->s[4];
    for (i = 0; i < n; i++) {
        if (--cnt <= 0) {
            int32_t s = (int32_t)(lofi_byte(f, tt, a) & 255u) * 256 - 32640;
            cnt = hold;
            if (bits < 8)
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            held = s;
        }
        tf += ti;
        tt += tf >> 24;
        tf &= 0xFFFFFFu;
        lp += mulq15(held - lp, lpk);
        out[i] += voice_amp(lp, m, i);
    }
    v->s[0] = held;
    v->s[1] = cnt;
    v->s[2] = (int32_t)tt;
    v->s[4] = lp;
    v->ph[0] = tf;
}

static const param_desc_t *lofi_desc(const track_t *t, uint32_t k)
{
    if (t->p[P_E1] == RW_BYTE && (k == 2u || k == 3u))
        return k == 2u ? &LOFI_ALGO : &LOFI_VAR;
    if (k == 2u && t->p[P_E1] == RW_WRAM)
        return &LOFI_WAVNUM;
    if (k == 3u && t->p[P_E0] == CHIP_STEP)
        return &LOFI_DCY;
    return 0;
}

/* the stepped envelope: volume 15..0 in v->s[6] bits 0..3, its divider in bits 4..7, a 240 Hz
 * frame clock (Q16 phase in v->s[7]); each frame the divider counts down and, at 0, reloads the
 * period and takes one step off the volume (a looping one starts again at 15 after 0) */
#define FRAME_Q16 ((240u * CTL * 65536u) / FS)       /* frames per control tick, Q16 */
static int32_t step_period(int32_t b) { return b <= 16 ? b - 1 : b - 16; }

static void lofi_note_on(track_t *t, voice_t *v)
{
    v->s[0] = 0;                 /* held sample */
    v->s[1] = 0;                 /* hold counter */
    v->s[2] = t->p[P_E1] == RW_BYTE ? 0 : 0x7FFF;   /* LFSR (BYTE: its time t, from 0) */
    v->s[3] = 0;                 /* sweep, 1/16 st */
    v->s[4] = 0;                 /* 1-pole lp state */
    v->s[5] = 0;                 /* arp counter */
    v->s[6] = 15 | (step_period(t->p[P_E3] >> 2) & 15) << 4;   /* stepped env: every note-on restarts it */
    v->s[7] = 0;
}

/* CHIP = STEP: the voice amplitude is the stepped envelope, not the ADSR curve. The ADSR only
 * gates: held, it is kept at full; after note-off the staircase keeps running (as the chip's
 * envelope never sees a note-off) and the voice is cut when the ADSR release has fallen to 1 %
 * (about the REL time; REL 0 cuts at once) or a one-shot decay has reached 0. */
static int32_t lofi_amp(track_t *t, voice_t *v, int32_t adsr)
{
    int32_t b = t->p[P_E3] >> 2, vol = v->s[6] & 15, div = (v->s[6] >> 4) & 15;
    if (t->p[P_E0] != CHIP_STEP)
        return adsr;
    if (!v->active)                                     /* the ADSR ended it (or the voice was given up) */
        return 0;
    if (v->stage == 3u) {
        if (v->env < (1 << 24) / 100 || (b >= 1 && b <= 16 && !vol)) {
            v->env = 0;
            v->stage = 0;
            v->active = 0;
            return 0;
        }
    } else {
        v->env = 1 << 24;                               /* the release starts from full */
    }
    if (!b)
        return 15 * Q4;
    v->s[7] += (int32_t)FRAME_Q16;
    while (v->s[7] >= 65536) {
        v->s[7] -= 65536;
        if (div) {
            div--;
        } else {
            div = step_period(b);
            if (vol)
                vol--;
            else if (b > 16)
                vol = 15;
        }
    }
    v->s[6] = vol | div << 4;
    return vol * Q4;
}

static void lofi_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    static const uint8_t STEP_DUTY[4] = {1, 2, 4, 6};   /* eighths: 12.5 / 25 / 50 / 75 % */
    const int16_t *p = t->p;
    uint32_t chip = (uint32_t)p[P_E0], wave = (uint32_t)p[P_E1], i;
    uint32_t stepc = chip == CHIP_STEP;
    uint32_t duty = stepc ? 0x20000000u * STEP_DUTY[(p[P_E2] / 32) & 3]
                          : 0x20000000u * (1u + (uint32_t)p[P_E2] / 32u);   /* 12.5 / 25 / 37.5 / 50 % */
    int32_t crush = stepc ? 0 : p[P_E3];                /* STEP: CRSH is the envelope */
    int32_t bits = chip == CHIP_8BIT ? 8 : chip == CHIP_1BIT ? 1 : 4;
    int32_t hold = 1 + crush / 8 + (chip == 1u ? 1 : 0);
    int32_t lpk = 3000 + (p[P_E7] << 8) + (m->cutoff > 0 ? m->cutoff : 0);
    static const int8_t ARPS[4][3] = {{0, 0, 0}, {0, 12, 0}, {0, 4, 7}, {0, 3, 7}};
    int32_t ar = p[P_E6];
    int32_t pitch, held = v->s[0], cnt = v->s[1], lp = v->s[4];   /* state in locals: out[] may alias v->s[] */
    uint32_t inc, ph0 = v->ph[0], lfsr = (uint32_t)v->s[2];
    const uint8_t *wr = WRAM[((uint32_t)p[P_E2] >> 3) & 15u];
    int32_t wdc = 0;
    if (wave == RW_WRAM) {                              /* the table's mean: no DC from lopsided shapes */
        int32_t sum = 0;
        for (i = 0; i < 16u; i++)
            sum += bits == 1 ? ((wr[i] >> 7) + ((wr[i] >> 3) & 1)) * 2 - 2 : (wr[i] >> 4) + (wr[i] & 15) - 15;
        wdc = bits == 1 ? sum * 32767 / 32 : sum * 2 * Q4 / 32;   /* (2 v - 15) Q4, or +-32767 at 1 bit */
    }
    /* sweep down, vibrato (uses the track LFO through m->pitch16 already) */
    if (v->s[3] < 4096)                                 /* past 2047 + an arp step the pitch is 0 anyway */
        v->s[3] += p[P_E4] / 8;
    v->s[5]++;
    pitch = m->pitch16 - v->s[3] + ARPS[ar & 3][(v->s[5] / 28) % 3] * 16;   /* ~50 Hz, chip-style */
    inc = pitch_inc(clamp(pitch, 0, 2047));
    if (p[P_E5])
        inc += (uint32_t)(((int32_t)(inc >> 12) * (((osc_sine(v->ph[1]) >> 8) * p[P_E5]) >> 4)) >> 4);   /* no overflow */
    v->ph[1] += 0x01000000u;
    if (lpk > 32767)
        lpk = 32767;
    if (wave == RW_BYTE) {                              /* (JIANT 0.4) bytebeat */
        lofi_byte_render(t, v, out, n, m, inc, 1 + (chip == 1u ? 1 : 0), bits, lpk);
        return;
    }
    for (i = 0; i < n; i++) {
        int32_t s;
        if (--cnt <= 0) {
            uint32_t q = bits < 16;                     /* quantise (the 4-bit tables are already) */
            cnt = hold;
            switch (wave) {
            case RW_TRI:
                if (stepc) {                            /* 32 steps, 4 bits: 15..0, 0..15 */
                    uint32_t k = ph0 >> 27;
                    s = (int32_t)(k < 16u ? 15u - k : k - 16u) * (2 * Q4) - 15 * Q4;
                    q = 0;
                } else {
                    s = osc_tri(ph0);
                }
                break;
            case RW_SAW:
                s = (int32_t)(ph0 >> 16) - 32768;
                break;
            case RW_NOIS: {
                uint32_t l = lfsr;
                if ((ph0 + inc * (uint32_t)hold) < ph0 || chip == 3u)
                    l = (l >> 1) | (((l ^ (l >> 1)) & 1u) << 14);
                lfsr = l;
                s = (l & 1u) ? 32767 : -32768;
                break;
            }
            case RW_WRAM: {                             /* step-wise, no interpolation */
                uint32_t k = ph0 >> 27, b = wr[k >> 1];
                b = (k & 1u) ? b & 15u : b >> 4;
                s = (bits == 1 ? (b >= 8u ? 32767 : -32767) : ((int32_t)b * 2 - 15) * Q4) - wdc;
                q = 0;
                break;
            }
            default:                                    /* minus its mean: no DC for narrow pulses */
                s = (ph0 < duty ? 32767 : -32768) - ((int32_t)(duty >> 16) - 32768);
                break;
            }
            if (q)                                      /* rounded, not floored (floor = DC) */
                s = ((s + (1 << (15 - bits))) >> (16 - bits)) << (16 - bits);
            if (crush > 64)
                s = (s >> 12) << 12;
            held = s;
        }
        ph0 += inc;
        lp += mulq15(held - lp, lpk);
        out[i] += voice_amp(lp, m, i);
    }
    v->ph[0] = ph0;
    v->s[0] = held;
    v->s[1] = cnt;
    v->s[2] = (int32_t)lfsr;
    v->s[4] = lp;
}

static const preset_t LOFI_PRESETS[] = {
    {"PULSE LD", {0, 0, 32, 0, 0, 20, 0, 127}, {0, 60, 90, 30}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    {"WAVE BASS", {1, 1, 0, 0, 0, 0, 0, 90}, {0, 50, 70, 20}, 0, 1, FX(0, 0, 10, 0), PAT(2)},
    {"ARP 8BIT", {2, 0, 96, 0, 0, 0, 2, 110}, {0, 60, 80, 40}, 0, 0, FX(0, 0, 30, 20), PAT(13)},
    /* wave RAM VOXA (DUTY 92 / 8 = 11), a little vibrato */
    {"WAVE LEAD", {0, 4, 92, 0, 0, 18, 0, 110}, {0, 70, 90, 30}, 0, 1, FX(0, 0, 40, 25), PAT(3)},
    /* STEP: 25 % pulse, DCY 34 = D7 (a 15-step decay over 0.5 s), REL ~54 ms of staircase after note-off */
    {"STEP LEAD", {4, 0, 40, 34, 0, 16, 0, 127}, {0, 64, 127, 55}, 0, 1, FX(0, 0, 40, 20), PAT(4)},
    /* (JIANT 0.4) BYTE: 8 bits, ALGO B02 (DUTY 4 / 4 = 1); B17 (64 / 4 = 16) with VAR 40 (a = 6) */
    {"BYTEBEAT", {2, 5, 4, 0, 0, 0, 0, 120}, {0, 80, 110, 40}, 0, 1, FX(0, 0, 25, 20), PAT(4)},
    {"BYTE VAR", {2, 5, 64, 40, 0, 0, 0, 100}, {0, 80, 110, 50}, 0, 1, FX(0, 20, 30, 25), PAT(3)},
};

static const engine_t ENG_LOFI = {
    .name = "LOFI",
    .page_title = {"CHIP", "MOTN"},
    .edit = {
        {"CHIP", F_ENUM, 0, 4, 0, N_CHIP, 0},
        {"WAVE", F_ENUM, 0, 5, 0, N_RWAVE, 0},
        {"DUTY", F_INT, 0, 127, 64, 0, 0},              /* WRAM: the table, DUTY / 8 (lofi_desc) */
        {"CRSH", F_PCT, 0, 127, 0, 0, 0},               /* STEP: the envelope (lofi_desc) */
        {"SWP", F_PCT, 0, 127, 0, 0, 0},
        {"VIB", F_PCT, 0, 127, 0, 0, 0},
        {"ARP", F_ENUM, 0, 3, 0, N_RARP, 0},
        {"TONE", F_PCT, 0, 127, 127, 0, 0},
    },
    .presets = LOFI_PRESETS,
    .npresets = NELEM(LOFI_PRESETS),
    .note_on = lofi_note_on,
    .render = lofi_render,
    .knob = {P_E1, P_E2, P_E3, P_REL},
    .keep = 0x11,                /* the held sample and the low-pass */
    .amp = lofi_amp,
    .desc = lofi_desc,
};
