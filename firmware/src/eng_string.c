/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* STRING (JIANT 0.9.9-snd5): a plucked, struck or bowed string, Karplus-Strong. A delay line a period long, filled
 * with the EXCITE at the note (noise, a plucked noise, a pick's triangle, a hammer's or a mallet's bump, at POS along
 * the string), read back through a two-point average, a one-pole low-pass (BRIGHT: the loss of the highs each pass)
 * and an allpass (STRETCH: a stiff string's partials sharpened, a piano's, a bar's), times the loop's gain (DECAY:
 * its T60, 50 ms .. 13 s, the same whatever the note). BODY / SIZE a resonant band (70 .. 800 Hz: a box under the
 * string) mixed in. (V10.7.26) BOW is a bow: while the key is held it holds the string at the hair's speed and lets
 * it slip once a period, at POS (string_render); FDBK an electric guitar into an amp (below).
 *
 * Memory: none of its own. The lines are the COMB filter's (voice.c comb_buf: 1024 samples a part, in the pool), cut
 * in NVOICE lines of 128; a note lower than ~350 Hz runs its loop at a half, a quarter, ... of the rate (DECIM: 2^d
 * samples a pass of the loop, the line still 128 long: down to ~22 Hz), its output interpolated (a string that low has
 * nothing above a few kHz). So a STRING part has no COMB filter: string_block turns a COMB TYPE into BP (the body's
 * band again). The voice's state: ph[0] the noise (FDBK: MOVE's phase), ph[1] the line's write index (and the loop's
 * sample count << 8, the last block's peak << 16), ph[2] the output's DC blocker; s[0] the low-pass, s[2] s[3] the
 * allpass, s[6] s[7] the body's band (low, band; FDBK: the bloom, the amp's hiss); (V10.7.26) s[1] the bow's clock
 * (FDBK: the cab's one-pole), s[4] s[5] the bridge's hill (FDBK: the cab's resonance); the average's last input and the
 * last two loop outputs are read back from the line. LFO / ENV -> SHAPE move BRIGHT. Cost: ~30 operations a sample at
 * the top rate, fewer below (a pass every 2^d samples); the bow and the amp ~1.2x a plucked string's. */
#define ST_SL 128u                                       /* a voice's line: comb_buf[part] / NVOICE */
#define ST_MAXP ((ST_SL - 6u) << 8)                      /* the longest period a line holds (Q8) */
static int16_t comb_buf[NTRK][1024] __attribute__((section(".pool")));   /* (voice.c: the COMB's lines, COMB_LEN) */
static const char *const N_STR_EXC[] = {"NOISE", "PLUCK", "PICK", "HAMMR", "STICK", "FDBK"};   /* (five letters: a knob's cell) */
enum { SX_NOISE, SX_PLUCK, SX_PICK, SX_HAMMER, SX_MALLET, SX_FDBK, SX_N };
/* (p10) FDBK: a guitar's feedback. (V10.7.26) An electric guitar into an amp: picked (a triangle near the bridge), a
 * steel string, a pickup a fifth along it, a tube (a biased soft clip that sags) and a cab (~55 Hz .. ~6 kHz, a
 * presence at ~4.4 kHz); the amp heard back by the string: the BODY page becomes the amp's. GAIN how close (how much
 * comes back: as much as the string loses at 64), DIST the tube's drive, SPEAKR where along the period the speaker's
 * sound meets the string (which harmonic it blooms into), BLOOM how slowly the feedback grows while the key is held;
 * the feedback holds itself at a level (never runs away). BRIGHT stays the tone. No memory more: the speaker's tap is
 * the line. */
static const param_desc_t STR_AMP[5] = {
    {"MOVE", F_PCT, 0, 127, 28, 0},                /* (par12) POS: the guitar swayed before the amp */
    {"GAIN", F_PCT, 0, 127, 64, 0}, {"DIST", F_PCT, 0, 127, 50, 0},   /* (V10.23: heard at once, as FEEDBACK) */
    {"SPEAKR", F_PCT, 0, 127, 40, 0}, {"BLOOM", F_PCT, 0, 127, 40, 0}};
static const param_desc_t *string_desc(const track_t *t, uint32_t k)
{
    return k >= 3u && t->p[P_E0] == SX_FDBK ? &STR_AMP[k - 3u] : 0;
}

static int16_t *str_line(const track_t *t, const voice_t *v)
{
    return comb_buf[(uint32_t)(t - trk) % NTRK] + ((uint32_t)(v - t->v) % NVOICE) * ST_SL;
}
/* 2^-x, x Q8 (0 .. 16 octaves), Q16 (a quadratic between the octaves: 0.3 % at worst) */
static uint32_t str_exp2n(uint32_t x)
{
    uint32_t i = x >> 8, f = (x & 255u) << 8, y;
    if (i >= 16u)
        return 0;
    y = 65536u - ((f * 43024u) >> 16) + ((((f * f) >> 16) * 10256u) >> 16);
    return y >> i;
}
/* the loop's period (Q8, in the loop's samples) at phase increment inc, and its rate's shift (a pass every 2^d) */
static uint32_t str_period(uint32_t inc, uint32_t *d)
{
    uint32_t per = 0xFFFFFFFFu / ((inc >> 8) | 1u), k = 0;
    while (per > ST_MAXP && k < 5u) {
        per >>= 1;
        k++;
    }
    *d = k;
    return per > ST_MAXP ? ST_MAXP : per;
}
/* the loop's gain (Q15) for a T60 of 50 ms x 2^(DECAY / 16) at a period of real Q8 samples: g = 2^-(9.97 T / T60) */
static int32_t str_gain(uint32_t real, int32_t decay)
{
    uint32_t x = ((real >> 4) * 296u) >> 4;              /* 9.97 / (44100 x 0.05) a sample, Q16 */
    x = ((x >> 4) * (str_exp2n((uint32_t)clamp(decay, 0, 127) * 2048u / 127u) >> 4)) >> 8;
    x >>= 8;                                             /* (Q8 octaves) */
    return clamp((int32_t)(str_exp2n(x) >> 1), 0, 32740);
}

/* the EXCITE's sample i of n (z a noise sample, lp PLUCK's low-pass state, k its coefficient) */
static int32_t str_exc(uint32_t mode, uint32_t i, uint32_t n, uint32_t pp, uint32_t wd, int32_t z, int32_t *lp, int32_t k)
{
    int32_t dx;
    switch (mode) {
    case SX_NOISE: return z;
    case SX_PLUCK: *lp += ((z - *lp) * k) >> 15; return *lp * 2;
    case SX_PICK:                                        /* the string pulled aside at POS, let go: a triangle */
        return (i < pp ? (int32_t)(i * 16384u / pp) : (int32_t)((n - i) * 16384u / (n - pp + 1u))) + (z >> 3);
    default:                                             /* HAMMER MALLET: a raised cosine at POS, its width */
        dx = (int32_t)i - (int32_t)pp;
        dx = dx < 0 ? -dx : dx;
        return dx < (int32_t)wd ? (16384 + sine_i(0x40000000u + (uint32_t)dx * (0x80000000u / wd))) >> 1 : 0;
    }
}
static void string_note_on(track_t *t, voice_t *v)
{
    const int16_t *p = t->p;
    int16_t *b = str_line(t, v);
    uint32_t d, per = str_period(pitch_inc((uint32_t)clamp(v->pitch16, 0, 2047)), &d), n = per >> 8, mode = (uint32_t)p[P_E0] % SX_N;
    uint32_t i, w, pp, wd;
    int32_t pos = clamp(p[P_E3], 0, 127), fbk = mode == SX_FDBK, bowing = !fbk && p[P_E7] > 0, lp3 = 0, kf = 0;
    if (fbk) {                                           /* (p10) FDBK: an electric guitar's string, picked (V10.7.26: a
                                                         * pick's triangle near the bridge, as hard and as bright as the
                                                         * velocity); the feedback grows again from 0 */
        mode = SX_PICK;
        v->s[6] = 0;
        pos = 20;                                        /* (par12: POS is MOVE) */
        kf = 4000 + v->vel * 190 + clamp(p[P_E2], 0, 127) * 60;
    }
    int32_t nz = (int32_t)(v->note * 2654435761u + 0x9E37u) | 1, nz2, lp = 0, lp2 = 0, amp = 9000 + v->vel * 90, sum = 0, k, mean;
    int32_t br = clamp(p[P_E2] + v->vel / 4 - 16, 0, 127);
    if (!v->env) {                                       /* (from silence: the line and the loop at rest) */
        for (i = 0; i < ST_SL; i++)
            b[i] = 0;
        for (i = 0; i < 8u; i++)
            v->s[i] = 0;
        v->ph[1] = 0;
        v->ph[2] = 0;
    } else
        nz = (int32_t)v->ph[0] | 1;
    if (bowing) {                                        /* (V10.7.26) bowed: no EXCITE, the bow starts the string from
                                                         * rest (or takes it as it rings: a new stroke); the ADSR is its
                                                         * speed, from 0 */
        v->env = 0;
        v->ph[0] = (uint32_t)nz;
        return;
    }
    if (n < 2u)
        n = 2;
    pp = 2u + (n * (uint32_t)pos) / 256u;
    wd = mode == SX_HAMMER ? n / 8u + 1u : n / 3u + 1u;
    k = 4000 + br * 225;                                 /* (PLUCK's noise low-passed, brighter with BRIGHT and the velocity) */
    nz2 = nz;
    w = v->ph[1] % ST_SL;
    for (i = 0; i < n; i++) {                            /* (the noises at POS: less what a pick there leaves, the same
                                                         * noise POS samples behind: a comb, without a buffer) */
        uint32_t q = (w + ST_SL - n + i) % ST_SL;
        int32_t e = str_exc(mode, i, n, pp, wd, (int32_t)(noise32(&nz) >> 17) - 16384, &lp, k);
        if (mode <= SX_PLUCK && pos && i >= pp)
            e -= str_exc(mode, i - pp, n, pp, wd, (int32_t)(noise32(&nz2) >> 17) - 16384, &lp2, k);
        if (kf)                                          /* (FDBK: the pick's edge, softer played: darker) */
            e = lp3 += ((e - lp3) * kf) >> 15;
        e = (e * amp) >> 14;
        sum += e;
        b[q] = (int16_t)clamp((b[q] >> 1) + e, -32767, 32767);   /* (struck again: what rings halved) */
    }
    mean = sum / (int32_t)n;                             /* (no DC in the loop) */
    for (i = 0; i < n; i++) {
        uint32_t q = (w + ST_SL - n + i) % ST_SL;
        b[q] = (int16_t)clamp(b[q] - mean, -32767, 32767);
    }
    v->ph[0] = (uint32_t)nz;
}

#define ST_M (ST_SL - 1u)
static void string_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    int16_t *b = str_line(t, v);
    uint32_t d, per = str_period(m->inc, &d), D = 1u << d, i, w = v->ph[1] % ST_SL, cnt = (v->ph[1] >> 8) & 63u;
    int32_t lp = v->s[0], c1 = v->s[1], apx = v->s[2], apy = v->s[3], f1 = v->s[4], f2 = v->s[5], bl = v->s[6], bb = v->s[7];
    int32_t dc = (int32_t)v->ph[2], nst = (int32_t)v->ph[0] | 1, lvl = (int32_t)(v->ph[1] >> 16), body = clamp(p[P_E5], 0, 127), bf;
    int32_t fb = p[P_E0] == SX_FDBK, bw = !fb && p[P_E7] > 0, og = 256, fk = 0, fdo = 0, fol = 0, fz0 = 0, mvo = 0, r1, y0, y1;
    uint32_t ft = 0, mv = v->ph[0], na = 0, pu = 0;
    int32_t br = clamp(p[P_E2] + ((m->shape - (64 << 8)) >> 8), 0, 127), a = 5000 + br * 218;
    int32_t s15 = clamp(p[P_E4], 0, 127) * 193, comp, dly, di, fr, g, pk = 0, vb = 0, vbn = 0, hf = 0, dsh = 10, fbi = 0, sw = 0, gk = 0, sj = 0;
    if (fb) {                                            /* (p10) FDBK: the BODY page is the amp's */
        int32_t mk = clamp(p[P_E3], 0, 127), dv = clamp(p[P_E5], 0, 127), ag, am, ft0;
        if (v->gate)                                     /* BLOOM: at once .. ~12 s up (par11: a harder hit blooms sooner) */
            bl += 4096 >> clamp((p[P_E7] - v->vel / 4) / 10, 0, 12);
        else                                             /* (V10.7.26) let go: the fingers off the string, no feedback */
            bl -= bl >> 3;
        bl = clamp(bl, 0, 32767);
        am = clamp(p[P_E4], 0, 127);                     /* (V10.7.29, the owner: "a feedback amount, to reach a high
                                                         * level, the same in the lows and the highs") GAIN the amount:
                                                         * the level the feedback holds, ~-34 .. -1 dB at the pickup */
        ft0 = 600 + am * 225;
        og = 256 + (am > 64 ? (am - 64) * 4 : 0);
        ag = am && lvl < ft0 ? (ft0 - lvl) * 32767 / ft0 : 0;   /* (the feedback's own limit: none at it) */
        fk = ((127 * (bl >> 7)) >> 8) * ag >> 15;   /* (any amount drives the loop as hard: it
                                                         * blooms at every GAIN, its level the amount's) */
        fdo = 384 + ((dv * dv) >> 1);                      /* the amp's drive: x1.5 .. x33 into the tube */
        fol = 7000 + 19000 * 256 / fdo;                    /* (its level kept) */
        fdo = fdo * 16384 / (16384 + lvl);                 /* (V10.7.26) its sag: a loud string drives it less */
        fbi = 1024 + dv * 40;
        fz0 = (3 * fbi - ((((fbi * fbi) >> 15) * fbi) >> 15)) >> 1;   /* (the tube's bias: its even harmonics) */
        mv += (uint32_t)(mk * mk) << 10;                   /* (par12) MOVE: ~0.07 .. 2.7 Hz, two sines: the speaker's
                                                         * distance swaying (+-48 of SPEAKR at the most) */
        mvo = ((sine_i(mv) + (sine_i(mv * 3u + 0x40000000u) >> 1)) * mk) >> 17;
        s15 = 4 * 193;                                   /* (a steel string's stiffness, a little) */
        body = 0;
        dsh = 7;                                         /* (the cab's low end: ~55 Hz) */
    }
    comp = 128 + ((32768 - a) << 8) / a + ((32768 + s15) << 8) / (32768 - s15);   /* the average's, the low-pass's, the allpass's delays */
    dly = (int32_t)per - comp;
    if (dly < (2 << 8))
        dly = 2 << 8;
    di = dly >> 8;
    fr = dly & 255;
    ft = 1u + (((uint32_t)di * (12u + 2u * (uint32_t)clamp(p[P_E6] + mvo + (fb ? (m->shape - (64 << 8)) >> 8 : 0), 0, 127))) >> 8);
    /* SPEAKR: 0.05 .. 1 period back; (par11) LFO / ENV -> SHAPE move it too: the guitar walked to and from the amp, the
     * feedback jumping from harmonic to harmonic */
    if (ft > ST_SL - 2u)
        ft = ST_SL - 2u;
    g = str_gain(per << d, p[P_E1]);
    if (fk)                                              /* (V10.7.26) as much back as the string loses a pass, x0 .. x2
                                                         * (GAIN 64: what it loses; the pickup's comb up to x1.9 more), and
                                                         * a little more: a long string blooms slowly, a short one at once */
        fk = ((32768 - g + 400 + 20000 / (di + 1)) * fk) / 64;   /* (V10.7.29: a short string loses more to the loop's low-pass a pass: as much more back, the highs held as the lows) */
    if (!v->gate) {                                      /* (p5) let go: RELEASE the damper (low: a palm mute; high: it
                                                         * rings its DECAY out; V10.7.26: FDBK's too) */
        g = (g * str_gain(per << d, p[P_REL])) >> 15;   /* (both < 2^15: in 32 bits) */
        if (lvl < 512)                                   /* (under ~-36 dB: damped out, no limit cycle left ringing) */
            g = (g * 29000) >> 15;
    }
    bf = 2048 - clamp(p[P_E6], 0, 127) * 896 / 127;     /* (SIZE: 70 .. 790 Hz, as 2^-x) */
    if (bw) {                                            /* (V10.7.26) BOW: the bow's speed the ADSR's (string_amp), as
                                                         * fast as the velocity; its grip the BOW (the pressure); where it
                                                         * bows POS (near the bridge .. over the fingerboard) */
        int32_t pr = clamp(p[P_E7], 0, 127), e8 = ((int32_t)v->note - 60) * 128 / 10, h8;
        vb = v->gate ? (clamp(((v->env >> 9) * (96 + v->vel)) >> 7, 0, 32767) * (400 + v->vel * 11)) >> 15 : 0;   /* (a
                                                         * harder stroke up to speed sooner: ATTACK the softest's) */
        vbn = (vb * (12 + pr / 3)) >> 9;                 /* (the hair's rosin: a noise in the bow's speed, more
                                                         * pressed) */
        na = 2u + (((uint32_t)di * (10u + (uint32_t)clamp(p[P_E3], 0, 127) * 51u / 127u)) >> 8);
        sw = (int32_t)((per * (na + 1)) / (uint32_t)(di + 2));   /* (it slips about as long as the wave takes
                                                         * to the bridge and back) */
        gk = 8000 + pr * 190;                            /* (the pressure: how firmly it holds while it sticks) */
        sj = (int32_t)(per >> 3) * (4 + ((32767 - clamp(v->env >> 9, 0, 32767)) >> 10) * (v->vel > 110 ? pr : 0) / 127) >> 7;
        bf = clamp(bf - e8 * 6 / 10, 0, 4095);           /* the body follows the register: a violin's high, a cello's
                                                         * low (~0.6 octave an octave) */
        h8 = clamp((e8 * 35) / 100 + (br - 64) * 8 / 3, -768, 512);   /* its bridge's hill: ~2.4 kHz at C4, higher and
                                                         * lower with the key, BRIGHT */
        hf = (int32_t)((59000 * str_exp2n((uint32_t)(512 - h8))) >> 16);
        if (hf > 24000)
            hf = 24000;
    }
    bf = (int32_t)((327u * str_exp2n((uint32_t)bf)) >> 8);   /* (2 sin(pi f / FS), Q15) */
    if (fb) {
        pu = 1u + (((uint32_t)di * 51u) >> 8);           /* (V10.7.26) the pickup: a fifth of the string from the bridge */
        if (ft + pu > ST_SL - 2u)
            ft = ST_SL - 2u - pu;
    }
    r1 = b[(w - 1u - (uint32_t)di) & ST_M];              /* (the loop's last two outputs and the average's last input:
                                                         * read back from the line, no state) */
    r1 += ((b[(w - 2u - (uint32_t)di) & ST_M] - r1) * fr) >> 8;
    y1 = b[(w - 1u) & ST_M] - (fb ? b[(w - 1u - pu) & ST_M] : 0);
    y0 = b[(w - 2u) & ST_M] - (fb ? b[(w - 2u - pu) & ST_M] : 0);
    if (n > CTL)
        n = CTL;
    for (i = 0; i < n; i++) {
        int32_t s;
        if (++cnt >= D) {                                /* a pass of the loop */
            int32_t x0 = b[(w - (uint32_t)di) & ST_M], x1 = b[(w - (uint32_t)di - 1u) & ST_M];
            int32_t r = x0 + (((x1 - x0) * fr) >> 8), x, yy;
            x = (r + r1) >> 1;
            r1 = r;
            if (fk) {                                    /* the amp: the string heard SPEAKR back, driven into a soft
                                                         * clip (3x - x^3) / 2, played back into the string */
                int32_t c = b[(w - ft) & ST_M] - b[(w - ft - pu) & ST_M];   /* (V10.7.26: what the pickup heard
                                                         * then: no DC, nothing builds up in the loop) */
                c = clamp(c, -32767, 32767);
                c = (3 * c - ((((c * c) >> 15) * c) >> 15)) >> 1;
                bb = (int32_t)(((uint32_t)bb * 25173u + 13849u) & 0xFFFFu);   /* (and the amp's hiss: a string let
                                                         * die blooms again) */
                x += ((c + ((bb - 32768) >> 8)) * fk) >> 15;
            }
            lp += ((x - lp) * a) >> 15;
            yy = ((-s15 * (lp - apy)) >> 15) + apx;      /* (the allpass, its coefficient -STRETCH) */
            apx = lp;
            apy = clamp(yy, -32767, 32767);
            yy = apy * g / 32768;                        /* (towards 0: no limit cycle left ringing) */
            if (vb) {                                    /* (V10.7.26) the bow: the string under it (the wave coming in,
                                                         * less the one going out, POS back) held to the hair's speed
                                                         * while it sticks, let go once a period while it slips (a
                                                         * Helmholtz motion, clocked by the string's own period: on the
                                                         * note, never an octave up nor stuck) */
                uint32_t q = (w - na) & ST_M;
                int32_t mid = b[q], f;
                c1 += 256;
                nst = (int32_t)((uint32_t)nst * 1664525u + 1013904223u);   /* (a cheap noise: the rosin's, the
                                                         * slips' jitter) */
                if (c1 >= (int32_t)per)
                    c1 -= (int32_t)per + (((nst >> 17) * sj) >> 15);
                if (c1 >= sw) {                          /* (stuck: the hair's pull, a rosin's noise in it) */
                    f = vb + (((nst >> 17) * vbn) >> 14) - (yy - mid);
                    f = (f * gk) >> 16;                  /* (|f| < 2^17, gk < 2^15) */
                    yy += f;
                    b[q] = (int16_t)clamp(mid - f, -32767, 32767);
                }
            }
            yy = clamp(yy, -32767, 32767);
            b[w] = (int16_t)yy;
            y0 = y1;
            y1 = fb ? yy - b[(w - pu) & ST_M] : yy;      /* (FDBK: the pickup's comb, a fifth along) */
            x = fb ? y1 : y1 - ((dc + 2048) >> 12);      /* (the loop's peak, its DC out as the output's: string_done,
                                                         * FDBK's limit) */
            x = x < 0 ? -x : x;
            pk = x > pk ? x : pk;
            w = (w + 1u) & ST_M;
            cnt = 0;
        }
        s = y0 + (((y1 - y0) * (int32_t)cnt) >> d);
        if (hf) {                                        /* (V10.7.26) the bridge's hill: a resonant low-pass, the
                                                         * highs a bow's rasp left above it taken down */
            int32_t h;
            f1 += (hf * f2) >> 15;
            h = s - f1 - ((22000 * f2) >> 15);
            f2 += (hf * h) >> 15;
            s = f1 + ((f2 * 13) >> 4);                   /* (its low-pass and band: ~+5 dB at it, -12 dB at 8 kHz) */
        }
        if (body) {                                      /* BODY: a band at SIZE under the string */
            int32_t hi;
            bl += (bf * bb) >> 15;
            hi = s - bl - ((10900 * bb) >> 15);
            bb += (bf * hi) >> 15;
            bb = clamp(bb, -65536, 65536);
            s += (bb * body) >> 8;
        }
        if (fb) {                                        /* (V10.7.26) the amp: the pickup into a tube (tanh, biased:
                                                         * its even harmonics), then the cab: a resonant low-pass at
                                                         * ~4.4 kHz (its presence) and a one-pole after (~6.5 kHz) */
            int32_t c, h;
            c = clamp(((s * fdo) >> 8) + fbi, -32767, 32767);   /* ((3x - x^3) / 2: flat at the clip, no corner) */
            c = ((3 * c - ((((c * c) >> 15) * c) >> 15)) >> 1) - fz0;
            c = (c * fol) >> 15;
            f1 += (20205 * f2) >> 15;
            h = c - f1 - ((22938 * f2) >> 15);
            f2 += (20205 * h) >> 15;
            c1 += ((f1 - c1) * 19800) >> 15;
            c = clamp((c1 * og) >> 8, -32767, 32767);   /* (V10.7.29) the amount louder past 64 (up to +6 dB), into a
                                                         * soft limit: (3x - x^3) / 2 at two thirds, flat below
                                                         * ~-6 dB, never past full scale: no spike */
            c = (c * 21845) >> 15;
            s = (3 * c - ((((c * c) >> 15) * c) >> 15)) >> 1;
        }
        dc += ((s << 12) - dc) >> dsh;                   /* (the output's DC blocked: ~7 Hz (FDBK ~55 Hz, the cab's
                                                         * low end); Q12, what sticks under a quarter of a step: rounded
                                                         * away) */
        s -= (dc + 2048) >> 12;
        out[i] += voice_amp(s, m, i);
    }
    v->ph[0] = fb ? mv : (uint32_t)nst;
    v->ph[1] = w | cnt << 8 | (uint32_t)(pk > 65535 ? 65535 : pk) << 16;   /* (p5: the block's peak, string_done; FDBK
                                                         * the pickup's, the feedback's limit) */
    v->ph[2] = (uint32_t)dc;
    v->s[0] = lp; v->s[1] = c1; v->s[2] = apx; v->s[3] = apy; v->s[4] = f1; v->s[5] = f2; v->s[6] = bl; v->s[7] = bb;
}
/* (par12) the amplitude: FDBK the ADSR (ATTACK fades it in, RELEASE the feedback's), the other EXCITEs their own (the
 * ADSR held at full: the string rings past the step, RELEASE its damper) */
static int32_t string_amp(track_t *t, voice_t *v, int32_t adsr)
{
    if (t->p[P_E0] == SX_FDBK || v->stage == 4u)
        return adsr;
    if (v->gate && t->p[P_E7] > 0)                       /* (V10.7.26) bowed: the ADSR runs on as the bow's speed
                                                         * (string_render reads it), the string its own amplitude */
        return 32767;
    v->env = 1 << 24;
    return 32767;
}
/* (p5) the string its own amplitude (ownenv): the voice ends once let go and silent (its last block's peak ~-66 dB) */
static int string_done(track_t *t, voice_t *v)
{
    (void)t;
    return !v->gate && (v->ph[1] >> 16) < 16u;
}
/* the part's FILTER: its COMB lines are the strings, so a COMB TYPE plays as BP (the body's band) */
static void string_block(track_t *t)
{
    if (FT_IS_COMB(ft_of(t->p)))
        t->p[P_FTYPE] = FT_BP;
}

/* {EXCITE, DECAY, BRIGHT, POS, STRETCH, BODY, SIZE, BOW (FDBK: GAIN DIST SPEAKR BLOOM)}, {A D S R}, fenv, mono */
static const preset_t STRING_PRESETS[] = {
    /* (V10.6i, reworked in place: the stored numbers kept, PROGRAM CHANGE too; browsed in STRING_ORDER) the string's
     * own envelope: RELEASE its damper (FDBK: the whole ADSR); SHP moves BRIGHT (FDBK: SPEAKR), CUT the part's FILTER;
     * ENV 2 -> AMP swells a bowed note in */
    {"NYLON", {SX_PLUCK, 72, 52, 28, 0, 60, 40, 0}, {0, 64, 127, 104}, 0, TAG(PLUCK), FX(0, 0, 0, 30), PAT(3),
     MODS(MMOD(1, PS_VEL, PD_SHP, 20))},                                                  /* softer is darker */
    {"STEEL GTR", {SX_PICK, 86, 88, 24, 6, 40, 52, 0}, {0, 64, 127, 104}, 0, TAG(PLUCK), FX(0, 15, 0, 30), PAT(3),
     MODS(MMOD(1, PS_VEL, PD_SHP, 24))},
    {"HARP", {SX_PLUCK, 96, 62, 64, 0, 30, 30, 0}, {0, 64, 127, 112}, 0, TAG(PLUCK), FX(0, 0, 10, 55), PAT(6),
     MODS(MMOD(1, PS_RAND, PD_PAN, 20), MMOD(2, PS_VEL, PD_SHP, 16))},                   /* the strings across the harp */
    {"KOTO", {SX_PICK, 78, 98, 14, 16, 30, 62, 0}, {0, 64, 127, 104}, 0, TAG(PLUCK), FX(0, 0, 15, 40), PAT(5),
     MODS(MMOD(1, PS_AT, PD_PITCH, 11), MMOD(2, PS_VEL, PD_SHP, 20))},                   /* AT presses it up ~2 st */
    {"BANJO", {SX_PICK, 52, 112, 10, 4, 90, 92, 0}, {0, 64, 127, 96}, 0, TAG(PLUCK), FX(0, 0, 0, 20), PAT(4),
     MODS(MMOD(1, PS_VEL, PD_SHP, 30))},
    {"PIANO", {SX_HAMMER, 104, 76, 18, 42, 20, 22, 0}, {0, 64, 127, 92}, 0, TAG(KEYS), FX(0, 0, 0, 35), PAT(6),
     MODS(MMOD(1, PS_VEL, PD_SHP, 30), MMOD(2, PS_KEY, PD_E(1), -20))},                  /* the low notes ring longer */
    {"DULCIMER", {SX_HAMMER, 92, 100, 20, 26, 36, 60, 0}, {0, 64, 127, 108}, 0, TAG(PLUCK), FX(0, 10, 10, 40), PAT(5),
     MODS(MMOD(1, PS_VEL, PD_SHP, 20))},
    {"MARIMBA", {SX_MALLET, 56, 40, 64, 72, 80, 70, 0}, {0, 64, 127, 80}, 0, TAG(BELL), FX(0, 0, 0, 30), PAT(5),
     MODS(MMOD(1, PS_VEL, PD_SHP, 30))},
    {"PLUCK BASS", {SX_PLUCK, 62, 36, 20, 0, 50, 10, 0}, {0, 64, 127, 80}, 0, 1 | TAG(BASS), FX(0, 0, 0, 5), PAT(2),
     MODS(P_GLIDE, 8, MMOD(1, PS_VEL, PD_SHP, 30))},                                      /* legato: a slide */
    {"BOWED", {SX_NOISE, 100, 64, 26, 0, 56, 60, 64}, {70, 64, 118, 84}, 0, TAG(PAD), FX(0, 30, 10, 60), PAT(5),
     MODS(P_LRATE, 88, P_LD_PIT, 1, P_LFADE, 84, MMOD(2, PS_AT, PD_E(7), 30))},          /* (V10.7.26) a bow: ATTACK its
                                                         * stroke, the body by the register (a violin's high, a cello's
                                                         * low), the vibrato late; AT: the bow's pressure */
    {"INIT STRING", {SX_PLUCK, 72, 60, 28, 0, 40, 40, 0}, {0, 64, 127, 90}, 0, TAG(PLUCK), FX(0, 0, 0, 0)},
    {"METAL STR", {SX_NOISE, 100, 120, 10, 112, 0, 0, 0}, {0, 64, 127, 104}, 0, TAG(PLUCK), FX(0, 0, 20, 45), PAT(3),
     MODS(MENV2(0, 30, 0, 20), MMOD(1, PS_ENV2, PD_PITCH, 8), MMOD(2, PS_RAND, PD_PAN, 35))},   /* each hit falls */
    {"FEEDBACK", {SX_FDBK, 90, 96, 10, 28, 72, 64, 84}, {0, 64, 127, 72}, 0, TAG(LEAD), FX(10, 0, 20, 35), PAT(5),
     MODS(MMOD(1, PS_MODW, PD_E(4), 50), MMOD(2, PS_AT, PD_E(6), 16))},   /* (V10.7.26) MODW: closer to the amp (GAIN),
                                                         * a held note swells into its octave; AT SPEAKR: a step aside,
                                                         * the octave to the twelfth */
    {"SHOEGAZE", {SX_FDBK, 110, 50, 30, 55, 100, 90, 70}, {40, 64, 127, 112}, 0, TAG(PAD), FX(20, 30, 30, 70), PAT(5),
     MODS(P_LWAVE, LW_DRIFT, P_LRATE, 30, P_LD_SHP, 20)},   /* the amp wanders */
    {"SQUEAL", {SX_FDBK, 80, 96, 16, 70, 110, 12, 30}, {0, 64, 127, 80}, 0, TAG(LEAD), FX(40, 0, 25, 30), PAT(4),
     MODS(P_LWAVE, LW_SH, P_LRATE, -11, P_LD_SHP, 30, MMOD(1, PS_MODW, PD_E(5), 40))},   /* harmonics jump */
    {"EBOW", {SX_FDBK, 127, 60, 40, 70, 20, 120, 90}, {20, 64, 67, 100}, 0, TAG(LEAD), FX(0, 20, 20, 50), PAT(5),
     MODS(P_LRATE, 84, P_LD_PIT, 1, P_LFADE, 80, MMOD(1, PS_AT, PD_E(4), 25))},
    /* (V10.7.26) the violin family, bowed (appended: every number before kept; PROGRAM CHANGE reaches the first 16):
     * ATTACK the bow's stroke, BOW its pressure (AT presses), POS where it bows, SIZE the body, BRIGHT the bridge */
    {"VIOLIN", {SX_NOISE, 104, 76, 22, 0, 60, 72, 60}, {68, 64, 120, 76}, 0, 1 | TAG(LEAD), FX(0, 0, 10, 45), PAT(5),
     MODS(P_LRATE, 90, P_LD_PIT, 1, P_LFADE, 84, MMOD(1, PS_AT, PD_E(7), 30), MMOD(2, PS_MODW, PD_VIB, 40))},
    {"VIOLA", {SX_NOISE, 100, 66, 24, 0, 64, 62, 64}, {70, 64, 120, 80}, 0, 1 | TAG(LEAD), FX(0, 0, 10, 45), PAT(5),
     MODS(P_LRATE, 88, P_LD_PIT, 1, P_LFADE, 86, MMOD(1, PS_AT, PD_E(7), 30), MMOD(2, PS_MODW, PD_VIB, 40))},
    {"CELLO", {SX_NOISE, 104, 58, 26, 0, 70, 54, 70}, {72, 64, 122, 84}, 0, 1 | TAG(BASS), FX(0, 0, 10, 50), PAT(2),
     MODS(P_LRATE, 86, P_LD_PIT, 1, P_LFADE, 88, MMOD(1, PS_AT, PD_E(7), 30), MMOD(2, PS_MODW, PD_VIB, 40))},
};
/* (V10.6i) the browsing order: to play, then the experiments, then INIT */
static const uint8_t STRING_ORDER[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 16, 17, 18,   /* to play (V10.7.26: the bowed
                                                                        * three after BOWED) */
                                       15, 12, 13, 14, 11,             /* experiments */
                                       10};                            /* INIT */
_Static_assert(sizeof STRING_ORDER == NELEM(STRING_PRESETS), "STRING's browsing order: every preset once");

static const engine_t ENG_STRING = {
    .name = "STRING",
    .page_title = {"STRING", "BODY"},
    .edit = {
        {"EXCITE", F_ENUM, 0, SX_N - 1, SX_PLUCK, N_STR_EXC},
        {"DECAY", F_PCT, 0, 127, 72, 0},
        {"BRIGHT", F_PCT, 0, 127, 60, 0},
        {"POS", F_PCT, 0, 127, 28, 0},
        {"STRETCH", F_PCT, 0, 127, 0, 0},
        {"BODY", F_PCT, 0, 127, 40, 0},
        {"SIZE", F_PCT, 0, 127, 40, 0},
        {"BOW", F_PCT, 0, 127, 0, 0},
    },
    .presets = STRING_PRESETS,
    .npresets = NELEM(STRING_PRESETS),
    .keep = 0xF3u,                                       /* (s[0] s[1] s[4..7]: struck again, the loop rings on) */
    .note_on = string_note_on,
    .render = string_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .block = string_block,
    .ownenv = 1,                                         /* (p5: RELEASE its damper; the sequencer's short gates let it ring) */
    .done = string_done,
    .desc = string_desc,
    .amp = string_amp,
};

