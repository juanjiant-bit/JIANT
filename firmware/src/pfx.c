/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* PFX (JIANT, FELUCCA-TONIC-SPEC.md fase 7): the punch-in MIDI effects of the FX layer. They act on the notes and
 * the sound of the tracks while their key is held (or latched: FX LATCH), never on what is stored: let go, all is
 * as it was. White keys after the audio ones (perform.c PF_*):
 *   D4 OCT-, E4 OCT+   every voice an octave down / up (both: none), at once, notes held too
 *   F4 1/2 TEMPO       the track's sequencer at half speed; let go, it is back where it would have been
 *   G4 DEC-, A4 DEC+   short / long decays: a synth's DEC and REL, DRUM's DECY (DEC- wins)
 *   B4 C5 D5           STUTTER 1/16, 1/32, 1/16T: the notes the track struck last together (a step's chord, a
 *                      drum step's hits) again at the rate, gated half; the sequencer's own notes wait meanwhile
 *   E5 ARP             those notes one at a time at 1/16, up two octaves (DRUM: its hits in turn)
 *   F5 RANDOM          each new note: an octave, a fifth or a fourth away now and then (DRUM: +-3 semitones)
 * Of the repeats (STUTTER, ARP) the fastest held plays. A#4 (black key 8) steps the tracks they act on: ALL,
 * SYN (the synths), DRM (DRUM tracks) (pfx_tgt; the layer's header says it).
 * The punch-in lane (the automation): a section's 64 steps of 1/16 (4 bars, from the transport's start or the
 * section's), one effect a step (pfx_lane: a nibble, 0 none, 1..10 D4 .. F5) and the tracks it acts on (pfx_ltgt).
 * Recording (REC armed, playing, not a song): the steps that pass while a MIDI effect is held get it (the lowest held),
 * with the tracks A#4 picked; G5 held erases them. Stopped, FX + G5 clears the lane. Played back with the held ones.
 * Saved with the section (project.c FUNA: the 33 spare bytes at PROJ_PFX_OFF), staged by the song (song_chain.c).
 * In the ISR: pfx_block before the events (what acts on each track, the repeats, the decays put in), pfx_end after
 * the parts (the decays back: as mod.c, nothing outside the ISR sees them). Idle: one test a block. */
static uint8_t pfx_in;                /* a repeat's own note-on (pfx_note lets it through) */
static uint8_t pfx_any;               /* a track has something running (or to put back) */
static uint32_t pfx_blk;              /* blocks: the notes of one block were struck together */
static uint32_t pfx_seed = 0x9E3779B9u;

/* voice.c trk_note_on, before it starts a note: 1 = not played (a repeat holds the track). Remembers the notes
 * struck together; RANDOM draws the note's offset */
static int pfx_note(track_t *t, uint32_t note, uint32_t vel)
{
    if (pfx_in)
        return 0;
    if (t->pfx & PFX_REP)
        return 1;
    if (t->pfx_lblk != pfx_blk) {
        t->pfx_lblk = pfx_blk;
        t->pfx_lnn = 0;
    }
    if (t->pfx_lnn < 4u) {
        t->pfx_ln[t->pfx_lnn] = (uint8_t)note;
        t->pfx_lv[t->pfx_lnn++] = (uint8_t)vel;
    }
    t->pfx_rpit = 0;
    if (t->pfx & PFX_RND) {
        static const int8_t R[8] = {0, 0, 12, -12, 7, -5, 0, 12};
        pfx_seed = pfx_seed * 1664525u + 1013904223u;
        t->pfx_rpit = (int16_t)(16 * (t->engine == ENGI_DRUM ? (int32_t)(pfx_seed >> 29) - 3 : R[pfx_seed >> 29]));
    }
    return 0;
}

/* the notes a repeat sounds end */
static void pfx_hush(track_t *t)
{
    uint32_t i;
    for (i = 0; i < t->pfx_snn; i++)
        trk_note_off(t, t->pfx_sn[i]);
    t->pfx_snn = 0;
}

/* a repeat: the notes again (STUTTER), or the next of them (ARP) */
static __attribute__((noinline)) void pfx_fire(track_t *t, int arp)
{
    uint32_t i, n = t->pfx_lnn;
    if (!n)
        return;
    pfx_hush(t);
    pfx_in = 1;
    t->pfx_rpit = 0;
    if (arp) {
        uint32_t k = t->pfx_si++ % (t->engine == ENGI_DRUM ? n : 3u * n);
        uint32_t note = t->pfx_ln[k % n] + 12u * (k / n);
        note = note > 127u ? 127u : note;
        trk_note_on(t, note, t->pfx_lv[k % n]);
        t->pfx_sn[0] = (uint8_t)note;
        t->pfx_snn = 1;
    } else {
        for (i = 0; i < n; i++) {
            trk_note_on(t, t->pfx_ln[i], t->pfx_lv[i]);
            t->pfx_sn[i] = t->pfx_ln[i];
        }
        t->pfx_snn = (uint8_t)n;
    }
    pfx_in = 0;
}

/* the values DEC- / DEC+ took out (pfx_end puts them back) */
static int16_t pfx_keep[NTRK][2];

static int pfx_rec_ok(void);          /* seq.c: REC armed, not a song playing */
static uint8_t pfx_rh, pfx_rs0, pfx_rst, pfx_rold;   /* recording the lane: a press on, its first step, the step being
                                                      * written and what it held before */
/* the lane's step s: 0 none, else the effect (PF_OCTD + v - 1) */
static uint32_t pfx_lane_at(uint32_t s) { return (pfx_lane[(s >> 1) & 31u] >> ((s & 1u) * 4u)) & 15u; }
static void pfx_lane_put(uint32_t s, uint32_t v)
{
    uint8_t *p = &pfx_lane[(s >> 1) & 31u];
    *p = (uint8_t)((*p & (s & 1u ? 0x0Fu : 0xF0u)) | (v & 15u) << ((s & 1u) * 4u));
}

/* each block, before the events (fx.c mix_block). held: the effects held (PF_* bits) */
static __attribute__((noinline)) void pfx_block(uint32_t n, uint32_t held)
{
    uint32_t k, b = beat_samples(), lane = 0;
    pfx_blk++;
    if (song.playing && b >= 4u) {                      /* the lane: record, or play */
        uint32_t s16 = b / 4u, st = (pfx_lph / s16) & 63u, half = pfx_lph % s16 >= s16 / 2u;
        uint32_t w = pfx_rec_ok() && (held || pfx_clr) ? (held ? (uint32_t)__builtin_ctz(held) - PF_OCTD + 1u : 0u) : 16u;
        pfx_lph += n;
        if (w < 16u) {                                  /* recording, to the nearest step: a press in a step's second */
            if (!pfx_rh) {                              /* half starts at the next one */
                pfx_rh = 1;
                pfx_rs0 = (uint8_t)((st + half) & 63u);
                pfx_rst = 0xFF;
            }
            if (st != ((pfx_rs0 - 1u) & 63u) || st == pfx_rs0) {
                if (st != pfx_rst) {                    /* (a step new to this press: what it held, kept) */
                    pfx_rst = (uint8_t)st;
                    pfx_rold = (uint8_t)pfx_lane_at(st);
                }
                pfx_lane_put(st, w);
            }
            if (held)
                pfx_ltgt = pfx_tgt;
        } else {
            if (pfx_rh && st == pfx_rst && !half && st != pfx_rs0)
                pfx_lane_put(st, pfx_rold);             /* let go in a step's first half: that step as it was */
            pfx_rh = 0;
            if ((k = pfx_lane_at(st)) != 0u && k <= 10u)
                lane = PF_BIT(PF_OCTD + k - 1u);
        }
    } else if (pfx_clr) {
        memset(pfx_lane, 0, sizeof pfx_lane);
    }
    if (!held && !pfx_any && !lane)
        return;
    pfx_any = 0;
    for (k = 0; k < NTRK; k++) {
        track_t *t = &trk[k];
        int drum = t->engine == ENGI_DRUM;
        uint32_t on = (!pfx_tgt || (pfx_tgt == 1u) == !drum ? held : 0u) | (!pfx_ltgt || (pfx_ltgt == 1u) == !drum ? lane : 0u);
        uint32_t old = t->pfx, f, rep, per = 0;
        rep = on & (PF_BIT(PF_S32) | PF_BIT(PF_S16T) | PF_BIT(PF_S16) | PF_BIT(PF_ARP));
        per = (rep & PF_BIT(PF_S32)) ? b / 8u : (rep & PF_BIT(PF_S16T)) ? b / 6u : b / 4u;
        t->pfx_pit = (int16_t)(((on >> PF_OCTU) & 1u) * 192 - ((on >> PF_OCTD) & 1u) * 192);
        f = ((on >> PF_RND) & 1u ? PFX_RND : 0u) | ((on >> PF_HALF) & 1u ? PFX_HALF : 0u) | (rep ? PFX_REP : 0u) |
            (on & (PF_BIT(PF_DSHT) | PF_BIT(PF_DLNG)) ? PFX_DEC : 0u);
        if ((f & PFX_HALF) && !(old & PFX_HALF) && song.playing) {   /* 1/2 TEMPO: where it was */
            t->pfx_idx0 = t->seq_idx;
            t->pfx_pos0 = t->seq_pos;
            t->pfx_el = 0;
            t->pfx_half = 0;
        } else if (!(f & PFX_HALF) && (old & PFX_HALF)) {
            t->pfx_rs = 1;                              /* (seq.c seq_tick: back to the real place) */
        }
        if (rep && !(old & PFX_REP)) {
            t->pfx_si = 0;
            t->pfx_sc = per;                            /* (the first repeat now) */
        } else if (!rep && (old & PFX_REP)) {
            pfx_hush(t);
        }
        t->pfx = (uint8_t)f;
        if (rep) {
            t->pfx_sc += n;
            if (t->pfx_sc >= per) {
                t->pfx_sc = t->pfx_sc - per < per ? t->pfx_sc - per : 0u;
                pfx_fire(t, rep == PF_BIT(PF_ARP));
            } else if (t->pfx_snn && t->pfx_sc >= per / 2u) {
                pfx_hush(t);                            /* gated half */
            }
        }
        if (f & PFX_DEC) {                              /* the decays, short or long */
            int sh = (on >> PF_DSHT) & 1u;
            uint32_t a = drum ? P_E3 : P_DEC, c = drum ? P_E3 : P_REL;
            pfx_keep[k][0] = t->p[a];
            pfx_keep[k][1] = t->p[c];
            if (drum) {
                t->p[a] = (int16_t)clamp(t->p[a] + (sh ? -40 : 40), 0, 127);
            } else {
                t->p[a] = (int16_t)(sh ? t->p[a] / 3 : t->p[a] + (127 - t->p[a]) * 2 / 3);
                t->p[c] = (int16_t)(sh ? t->p[c] / 3 : t->p[c] + (127 - t->p[c]) * 2 / 3);
            }
        }
        if (f || t->pfx_rs || t->pfx_pit)
            pfx_any = 1;
    }
}

/* after the parts: what DEC- / DEC+ changed, back */
static __attribute__((noinline)) void pfx_end(void)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (trk[k].pfx & PFX_DEC) {
            int drum = trk[k].engine == ENGI_DRUM;
            trk[k].p[drum ? P_E3 : P_DEC] = pfx_keep[k][0];
            trk[k].p[drum ? P_E3 : P_REL] = pfx_keep[k][1];
        }
}
