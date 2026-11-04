/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 JIANT port: Copyright (C) 2026 Israel Peña (pedeplei1281)
 * After SLOOP's free take (github.com/isod89/sloop-fm1, seq.c ft_*; GPL-3.0). */
/* SMK-37: REC from stop, off the SEQ pages (smk37-port.yaml rec_free_take; the UI half: smk_take_ui.c). Armed, the first
 * note played on the selected track starts it:
 *  - an empty project: a FREE TAKE. Play freely, no tempo, no click; REC on the "1" after your last bar closes the loop:
 *    the time from the first note to that press is 1, 2 or 4 bars at the tempo nearest the one set (within 3 % of it the
 *    tempo is kept); the notes go into the selected track quantised to 1/16 with their lengths (TIEs) and levels, every
 *    track takes that length (1/16 steps), the tempo is set and the loop plays at once. PLAY drops the take; it closes
 *    by itself after 24 s.
 *  - a project with notes (MENU > COUNT-IN OFF): the note starts the transport at the tempo set and is step 1, recorded
 *    (as the count-in's kept notes: cin_record).
 * Included by seq.c (the audio side: input_on, input_off, keyboard_block). */
enum { FT_NONE, FT_ARM_FREE, FT_ARM_NOTE };
#define FT_MAX 192u
#define FT_BLOCKS (24u * FS / CTL)                     /* 24 s: 4 bars at 40 BPM */
#define FT_OPEN 0xFFFFu                                /* the key is still down */
typedef struct { uint16_t t, d; uint8_t note, vel; } ft_ev_t;   /* t, d in blocks from the first note */
static ft_ev_t ft_ev[FT_MAX];
static uint32_t ft_n;
static volatile uint8_t ft_arm;                        /* FT_ARM_*: armed by the UI, waiting for the first note */
static volatile uint8_t ft_on;                         /* a free take runs */
static uint8_t ft_trk;
static volatile uint32_t ft_t;                         /* blocks since its first note */
static volatile uint8_t ft_close_req, ft_drop_req;     /* the UI: REC closes it, PLAY drops it */
static volatile uint16_t ft_res;                       /* the result for the UI: bars << 8 | bpm - 40 + 1; 0xFFFF none
                                                        * fitted, 0xFFFE dropped; 0 nothing new */
static void seq_start(void);

/* a note on track t (input_on): 1 when the take has it (input_on records nothing then) */
static int ft_note_on(track_t *t, uint32_t note, uint32_t vel)
{
    uint32_t i = trk_index(t) % NTRK, k;
    if (ft_arm == FT_ARM_NOTE && i == song.sel % NTRK && !song.playing) {   /* the first note starts the loop */
        ft_arm = FT_NONE;
        cin_n[i] = 0;
        cin_up[i] = 0;
        seq_start();
        cin_flush = 1;                                 /* (step 1 starts in this block; cin_record after it) */
    }
    if (cin_flush && song.playing && ((song.rec >> i) & 1u) && !t->p[P_AMODE]) {   /* .. and its chord's notes */
        for (k = 0; k < cin_n[i] && cin_note[i][k] != note; k++)
            ;
        if (k == cin_n[i] && k < 4u) {
            cin_note[i][k] = (uint8_t)note;
            cin_vel[i][k] = (uint8_t)vel;
            cin_oct[i][k] = 0;
            cin_n[i]++;
        }
        return 1;
    }
    if (ft_arm == FT_ARM_FREE && i == song.sel % NTRK && !song.playing) {   /* a free take starts */
        ft_arm = FT_NONE;
        ft_on = 1;
        ft_trk = (uint8_t)i;
        ft_t = 0;
        ft_n = 0;
    }
    if (!ft_on || i != ft_trk)
        return 0;
    if (ft_n < FT_MAX) {
        ft_ev[ft_n].t = (uint16_t)ft_t;
        ft_ev[ft_n].d = FT_OPEN;
        ft_ev[ft_n].note = (uint8_t)note;
        ft_ev[ft_n].vel = (uint8_t)vel;
        ft_n++;
    }
    return 1;
}

/* a note let go (input_off): its length */
static void ft_note_off(track_t *t, uint32_t note)
{
    uint32_t i = ft_n;
    if (!ft_on || trk_index(t) % NTRK != ft_trk)
        return;
    while (i--)
        if (ft_ev[i].note == note && ft_ev[i].d == FT_OPEN) {
            ft_ev[i].d = (uint16_t)(ft_t - ft_ev[i].t);
            return;
        }
}

/* the loop of T blocks: 1, 2 or 4 bars at the tempo nearest the one set, its BPM in *bpm (0: none in 40..240) */
static uint32_t ft_fit(uint32_t T, uint32_t *bpm)
{
    static const uint8_t BARS[3] = {1, 2, 4};
    uint32_t cur = (uint32_t)song.g[G_BPM], best = 0, bb = 0, i;
    for (i = 0; i < 3u; i++) {
        uint32_t num = BARS[i] * 240u * FS;            /* beats x 60 s x FS = bpm x samples (<= 4 x 240 x FS: 32 bits;
                                                        * no 64-bit division on the chip) */
        uint32_t b = (num + T * CTL / 2u) / (T * CTL), d, db;   /* (T <= FT_BLOCKS: T x CTL < 2^21) */
        if (b < (uint32_t)GP[G_BPM].min || b > (uint32_t)GP[G_BPM].max)
            continue;
        d = b > cur ? b - cur : cur - b;
        db = best ? (bb > cur ? bb - cur : cur - bb) : 0xFFFFu;
        if (d < db) {
            best = BARS[i];
            bb = b;
        }
    }
    if (best && (bb > cur ? bb - cur : cur - bb) * 100u <= cur * 3u)
        bb = cur;                                      /* (within 3 %: the tempo set is kept) */
    *bpm = bb;
    return best;
}

static void ft_close(void)
{
    track_t *t = &trk[ft_trk % NTRK];
    uint32_t T = ft_t, bars, bpm = 0, len, i, k;
    ft_on = 0;
    bars = ft_n && T >= FS / CTL / 2u ? ft_fit(T, &bpm) : 0u;
    if (!bars) {
        ft_res = 0xFFFFu;                              /* nothing played, too short, or no tempo fits */
        song.rec = 0;
        return;
    }
    len = 16u * bars;
    for (i = 0; i < NTRK; i++) {                       /* (an empty project: one loop length, 1/16 steps) */
        trk[i].p[P_SLEN] = (int16_t)len;
        trk[i].p[P_SDIV] = 2;
    }
    for (i = 0; i < NSTEP; i++)
        step_clear(&t->step[i]);
    for (i = 0; i < ft_n; i++) {                       /* the notes, to the nearest step */
        uint32_t idx = (ft_ev[i].t * len * 2u + T) / (2u * T) % len;
        step_t *s = &t->step[idx];
        if (drum_track(t)) {
            rec_drum(t, s, ft_ev[i].note, ft_ev[i].vel, 0);
            continue;
        }
        if (s->time != ST_NOTE || !s->n) {
            step_clear(s);
            s->time = ST_NOTE;
        }
        for (k = 0; k < s->n && s->note[k] != ft_ev[i].note; k++)
            ;
        if (k == s->n && s->n < 4u)
            s->note[s->n++] = ft_ev[i].note;
        if (ft_ev[i].vel > s->vel)
            s->vel = ft_ev[i].vel;
        if (ft_ev[i].vel > 110u)
            s->flags |= SF_ACCENT;
    }
    if (!drum_track(t))
        for (i = 0; i < ft_n; i++) {                   /* their lengths: TIE steps, up to the next note */
            uint32_t d = ft_ev[i].d == FT_OPEN || ft_ev[i].t + ft_ev[i].d > T ? T - ft_ev[i].t : ft_ev[i].d;
            uint32_t idx = (ft_ev[i].t * len * 2u + T) / (2u * T) % len;
            uint32_t steps = (d * len * 2u + T) / (2u * T);
            for (k = 1; k < steps && k < len; k++) {
                step_t *s = &t->step[(idx + k) % len];
                if (s->time == ST_NOTE && s->n)
                    break;
                step_clear(s);
                s->time = ST_TIE;
            }
        }
    t->seq_active = 1;
    song.g[G_BPM] = (int16_t)bpm;
    song.rec = 0;                                      /* (the loop plays; REC again overdubs) */
    ft_res = (uint16_t)(bars << 8 | (bpm - 40u + 1u));
    seq_start();                                       /* this is the downbeat: the loop plays */
}

/* once a block (keyboard_block): the take's time, its close (REC), its drop (PLAY), its 24 s */
static void ft_block(void)
{
    if (ft_arm && (song.playing || seq_counting()))
        ft_arm = FT_NONE;                              /* (started otherwise: PLAY, MIDI) */
    if (ft_drop_req) {
        ft_drop_req = 0;
        if (ft_on) {
            ft_on = 0;
            song.rec = 0;
            ft_res = 0xFFFEu;
        }
    }
    if (!ft_on) {
        ft_close_req = 0;
        return;
    }
    ft_t++;
    if (ft_close_req || ft_t >= FT_BLOCKS) {
        ft_close_req = 0;
        ft_close();
    }
}
