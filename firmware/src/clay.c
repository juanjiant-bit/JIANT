/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.21) JIANT BYTE's DRONE CLAY (GPL, its clay.c of 93e9eb1), ported: CL_N as CLAY_NF (fx.c's CL_N is CLOUD's),
 * the state in the bss (zeroed at power-on: no clay_boot), no FORM part to migrate, no white punch-ins' time here;
 * (V10.8.27) BYTE's 1adbf0a6 + d7f32b11 without its keys (FM's DRONE CLAY has none): each modulator its own RATE, PITCH,
 * the HITs a drum on any engine (a DRUM track's own pads), one MOD on CLAY MOD / CLAY RHYTHM, 'C' 'L' ('C' 'Y' read) */
/* (BYTE) DRONE CLAY, a track's sequencer (the owner: FORM out as an engine; "the voice the track has, a drone,
 * endless timbres"). Each audio block, before the parts (fx.c mix_block, after the macros): for every track with it
 * ON and running (RUN SONG: while the transport plays; FREE: always), FORM's composer (eng_form.c) on the track's
 * settings: each modulator's euclid strikes a gesture on its own RATE's steps, the gestures summed into what each
 * modulator adds now (clay_vk,
 * mod.c mod_begin moves its DEST with it); a NOTE gesture (ARP, CHORD ..) a real note on the voice, on the scale,
 * two octaves over the drone; the drone itself, a very low note (C0 .. C3 on the track's ROOT) held, or struck again
 * every RATE. Off, stopped, muted: its notes end. Included by seq.c (its notes, the scale) */
static void ui_message(const char *s);                  /* (ui.c) */
static uint32_t clay_free;                              /* (FREE stopped: its own beat, Q16) */
static uint32_t clay_b0[NTRK], clay_pq[NTRK];         /* (BYTE's ff242c4) a punch-in's restart: the time it began; what was held */
static int16_t clay_sp[18] __attribute__((used));     /* (V10.8.21's, kept in place, unused: the RAM after it where it was) */
uint32_t clay_rng;                                      /* (clay_tick's noise; not static: after the RAM's others) */
static void clay_stop(uint32_t tr)
{
    track_t *t = &trk[tr];
    form_st_t *st = &form_st[tr];
    if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
    if (clay_nn[tr]) seq_note_off(t, clay_nn[tr]);
    if (st->pn) seq_note_off(t, st->pn);
    clay_dn[tr] = clay_nn[tr] = st->pn = st->rt = 0;
    st->mv[0] = st->mv[1] = st->mv[2] = st->mv[3] = 0;
    st->hb.on = 0;
    form_lit[tr] = 0;
}
/* (BYTE's 1adbf0a6) modulator m strikes its SHAPE: a gesture held for its DECAY, then let go; a HIT on a DRUM track its
 * own pad (its GM note, let go a few blocks on), on a voice that does not sustain (SUS under 3/4: a pluck, any
 * engine's) the drone struck again with it */
static void clay_strike(uint32_t tr, track_t *t, form_st_t *st, uint32_t m, uint32_t sh, uint32_t vel)
{
    form_spawn(st, sh, m, vel, 0u, 64u + (uint32_t)clamp(clay[tr][CL_MF(m, 4u)], 0, 127) * 32u, 0u);
    form_lit[tr] |= (uint8_t)(1u << m);
    if (sh / 12u != FPG_HIT)
        return;
    if (drum_track(t)) {
        if (st->pn) seq_note_off(t, st->pn);
        st->pn = CLAY_HIT[sh % 12u].gm;
        st->pt = 8u;
        seq_note_on(t, st->pn, clamp(vel, 1, 127));
    } else if (t->p[P_SUS] < 96 && vel > st->rt)
        st->rt = (uint8_t)vel;
}
static __attribute__((noinline)) void clay_tick(uint32_t n)
{
    uint32_t s16 = div_samples(2), bps = beat_samples(), b, tr, k, playing = song.playing && clk_pos != CLK_START;
    if (playing)                                        /* (Q16 beats from PLAY: the bar's start; wraps at 16384 bars) */
        b = ((clk_n & 0x3FFFFu) << 14) + (clk_pos << 14) / (s16 ? s16 : 1u);
    else
        b = clay_free += (n << 16) / (bps | 1u);
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        const clay_t *c = clay[tr];
        form_st_t *st = &form_st[tr];
        uint32_t end = b, seed = (uint32_t)clamp(c[CL_SEED], 0, 127) * 977u + 1u, root, dn, drum = drum_track(t), duck = 0;
        clay_out_t o;
        if (st->pt && !--st->pt && st->pn) {            /* (a DRUM track's pad let go) */
            seq_note_off(t, st->pn);
            st->pn = 0;
        }
        if (!c[CL_ON] || tr >= trk_live || t->p[P_MUTE] || (!song.playing && c[CL_RUN] != 1)) {
            if (clay_dn[tr] | clay_nn[tr] | st->pn | form_lit[tr] | st->hb.on) clay_stop(tr);
            continue;
        }
        {   /* (BYTE's ff242c4, the owner) a punch-in on the time or the pitch: the composition from its first hit, the moment
             * it goes in; let go, back on the transport's grid (never out of phase) */
            uint32_t pq = (uint32_t)(t->pfx & (PFX_HALF | PFX_LOOP | PFX_REP)) | (t->pfx_pit ? 1u << 8 : 0u);
            if (pq != clay_pq[tr]) {
                clay_b0[tr] = pq ? b : 0u;
                for (k = 0; k < 4u; k++) st->lastm[k] = -1;
                clay_pq[tr] = pq;
            }
            end = b - clay_b0[tr];
        }
        st->clk += (n << 18) / (bps ? bps : 1u);
        for (k = 0; k < 4u; k++) {                     /* each modulator on its own RATE's steps: its euclid strikes */
            uint32_t eu = (uint32_t)clamp(c[CL_MF(k, 2u)], 0, CLAY_NEU - 1), s1 = clay_step(end, (uint32_t)clamp(c[CL_MF(k, 5u)], 0, CLAY_NRES - 1));
            if (clay_is_hit(c, k) && eu && c[CL_MF(k, 3u)] > (int32_t)duck)
                duck = (uint32_t)c[CL_MF(k, 3u)];      /* (a HIT on the beat: the drone ducked by its AMOUNT between) */
            if ((int16_t)s1 == st->lastm[k]) continue;
            st->lastm[k] = (int16_t)s1;
            form_lit[tr] = (uint8_t)(form_lit[tr] & ~(1u << k));
            if (c[CL_MF(k, 3u)] > 0 && form_hit(eu, k, s1, seed))   /* (V10.8.30, the owner: AMOUNT 0 strikes nothing,
                                                         * the drone left clean) */
                clay_strike(tr, t, st, k, clay_sh(c[CL_MF(k, 1u)]), (s1 & 3u) ? 90u : 127u);
        }
        clay_eval(st, c, &o, t->at);
        root = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
        dn = (uint32_t)clamp(12 * (clamp(c[CL_OCT], 0, 3) + 1) + (int32_t)root + clamp(c[CL_PIT], -24, 36), 1, 127);
                                                         /* the drone: C0 .. C3 on the ROOT, PITCH */
        if (drum)                                       /* (a DRUM track: its pads, no drone) */
            st->hb.on = 0;
        else {                                          /* the HITs' bundle (mod.c clay_mod) */
            int32_t dq = (int32_t)duck * 258, hg = clamp(o.hg, 0, 32767), pit = 0;
            if (o.ha) {                                 /* (the loudest's pitch, back to the drone's as it fades) */
                int32_t w = o.ha * 3 > 32767 ? 32767 : o.ha * 3;
                pit = (o.hp - (int32_t)dn * 16) * w >> 15;
            }
            if (o.hn > 0) {                             /* (the noise: the pitch thrown each block) */
                int32_t nz = clamp(o.hn >> 11, 0, 30 * 16);
                clay_rng = clay_rng * 1664525u + 1013904223u;
                pit += ((int32_t)(clay_rng >> 24) - 128) * nz >> 7;
            }
            st->hb.pit = (int16_t)clamp(pit, -2047, 2047);
            st->hb.gain = (int16_t)clamp(32767 - dq + (dq * hg >> 15), 0, 32767);
            st->hb.cut = (int16_t)((clamp(o.hc, -1966080, 1966080) >> 6) * 5 >> 3);   /* (% x Q15 -> Q15 of the range) */
            st->hb.on = (int16_t)(duck || o.ha);
        }
        {
            uint32_t r = (uint32_t)clamp(c[CL_RATE], 0, CLAY_NRES), cnt = r ? clay_step(end, r - 1u) : 0u, j, busy = 0, rt = st->rt;
            st->rt = 0;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (BYTE's ff242c4, the owner: the sequencers live with it) one voice:
                                                         * a step's or a key's note takes it, the drone back after */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (drum) {                                 /* (a DRUM track: the drone its kick pad, struck each RATE step,
                                                         * HOLD each bar; DRONE and PITCH its tuning) */
                uint32_t c2 = clay_step(end, r ? r - 1u : 6u);
                if (!clay_dn[tr] || (uint16_t)c2 != clay_lr[tr]) {
                    int8_t dp = t->dpit;
                    if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                    t->dpit = (int8_t)clamp((int32_t)dn - 24, -48, 48);
                    seq_note_on(t, 36u, 110u);
                    t->dpit = dp;
                    clay_dn[tr] = 36u;
                }
                cnt = c2;
            } else if (busy) {
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                clay_dn[tr] = 0;
            } else if (clay_dn[tr] != dn || (r && (uint16_t)cnt != clay_lr[tr]) || rt) {   /* new, moved, its RATE's strike, a HIT's */
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                seq_note_on(t, dn, rt ? rt : 110u);
                clay_dn[tr] = (uint8_t)dn;
            }
            clay_lr[tr] = (uint16_t)cnt;
        }
        {   /* a NOTE gesture alive: its note on the voice, on the scale, two octaves over the drone */
            int32_t sem = o.esum > 0 ? o.sem * 32767 / (o.esum > 32767 ? o.esum : 32767) : 0;
            uint32_t nn = 0, j, busy = drum;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (one voice: a step's or a key's note before a gesture's) */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (o.esum > 3000 && !busy)
                nn = (uint32_t)clamp(scale_snap(t, (int32_t)dn + 24 + (sem * 12 + (sem < 0 ? -2048 : 2048)) / 4096), 12, 108);
            if (nn != clay_nn[tr]) {
                if (clay_nn[tr]) seq_note_off(t, clay_nn[tr]);
                if (nn && nn != dn) seq_note_on(t, nn, 100u);
                clay_nn[tr] = (uint8_t)(nn != dn ? nn : 0u);
            }
        }
    }
}

/* the pages' knobs (params.c page_desc: ids ID_CL0 + CL_*): the selected track's settings, through their int16 view
 * (clay_ui: ui_input.c edit_param writes it, clay_put back); MOD (CL_SEL) the modulator the pages' others edit */
static const char *const CL_ONN[] = {"OFF", "ON", 0};
static const char *const CL_RUNN[] = {"SONG", "FREE", 0};
static const char *const CL_MODN[] = {"1", "2", "3", "4", 0};
static const char *const CL_NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const param_desc_t *clay_desc(const track_t *t, uint32_t f, int16_t **valp)
{
    static const char *nm[CLAY_ND + 1], *on[5];   /* (DEST's: the sound's knobs by name) */
    static char octn[4][4];
    static param_desc_t d;
    static const char *const BASE[CLAY_ND] = {"OFF", "PITCH", "CUTOFF", "SHAPE", "VOLUME", "PAN", "DIST", "CHORUS", "DELAY",
                                              "REVERB", "LFO RATE", "VIBRATO", 0, 0, 0, 0, 0, 0, 0, 0, "VOICE CUT", "LEVEL"};   /* (FM: no LIFE
                                                         * destination: its own HOME > LIFE) */
    static const char *const EN[8] = {"E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8"};
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i;
    d.def = 0; d.names = 0; d.fmt = F_ENUM; d.min = 0;
    if (f == CL_SEL) {                                   /* (BYTE, the owner: one MOD, scrolled, as the sequences are) */
        *valp = &clay_km[tr];
        d.label = "MOD"; d.max = 3; d.names = CL_MODN;
        return &d;
    }
    f %= CLAY_NF;
    clay_ui[f] = clay[tr][f];
    *valp = &clay_ui[f];
    if (f == CL_ON) { d.label = "CLAY"; d.max = 1; d.names = CL_ONN; }
    else if (f == CL_OCT) {                              /* (the drone's note: its octave on the track's ROOT) */
        uint32_t r = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
        for (i = 0; i < 4u; i++) {
            str_cpy(octn[i], CL_NN[r], 4);
            octn[i][str_len(octn[i])] = (char)('0' + i);
            octn[i][str_len(CL_NN[r]) + 1u] = 0;
            on[i] = octn[i];
        }
        on[4] = 0;
        d.label = "DRONE"; d.max = 3; d.def = 1; d.names = on;
    } else if (f == CL_RATE) { d.label = "RATE"; d.max = CLAY_NRES; d.names = CLAY_RESN; }
    else if (f == CL_RUN) { d.label = "RUN"; d.max = 1; d.names = CL_RUNN; }
    else if (f == CL_PIT) { d.label = "PITCH"; d.fmt = F_SEMI; d.min = -24; d.max = 36; }
    else if (f == CL_SEED) { d.label = "SEED"; d.fmt = F_INT; d.max = 127; }
    else {
        uint32_t k = (f - CL_M0) % 6u;
        if (k == 0u) {                                   /* DEST: the sound's knobs by their own names */
            for (i = 0; i < (uint32_t)CLAY_ND; i++) nm[i] = BASE[i];
            for (i = 0; i < 8u; i++) {
                const param_desc_t *pd = track_desc(t, P_E0 + i);
                nm[12u + i] = pd && pd->label && pd->label[0] && pd->label[0] != '-' ? pd->label : EN[i];
            }
            nm[CLAY_ND] = 0;
            d.label = "DEST"; d.max = CLAY_ND - 1; d.names = nm;
        } else if (k == 1u) {                            /* SHAPE: the HITS first (the owner) */
            d.label = "SHAPE"; d.max = FORM_NSH - 1; d.names = CLAY_SHN;
        } else if (k == 2u) { d.label = "EUCLID"; d.max = CLAY_NEU - 1; d.names = CLAY_EU; }
        else if (k == 3u) { d.label = "AMOUNT"; d.fmt = F_PCT; d.max = 127; d.def = 100; }
        else if (k == 4u) { d.label = "DECAY"; d.fmt = F_PCT; d.max = 127; }
        else { d.label = "RATE"; d.max = CLAY_NRES - 1; d.def = 2; d.names = CLAY_RESN + 1; }   /* (its own, as FORM's MOD RATE) */
    }
    return &d;
}
/* (params.c page_desc) the pages' knobs: MOD 1's fields stand for the chosen modulator's */
static const param_desc_t *clay_page_desc(const track_t *t, uint32_t f, int16_t **valp)
{
    if (f >= CL_M0 && f < CL_M0 + 6u)
        f = CL_MF((uint32_t)clay_km[(uint32_t)(t - trk) % NTRK] & 3u, f - CL_M0);
    return clay_desc(t, f, valp);
}
/* a knob of the pages turned (its view's value already in): into the track's, ON from a blank one its first sound
 * (clay_defaults) */
static void clay_put(uint32_t tr, uint32_t f, int32_t was, int32_t v)
{
    uint32_t m, any = 0;
    tr %= NTRK; f %= CLAY_NF;
    clay[tr][f] = (clay_t)clamp(v, -128, 127);
    if (f != CL_ON || !v || was)
        return;
    for (m = 0; m < 4u; m++) any |= clay[tr][CL_MF(m, 3u)] != 0;
    if (!any)
        clay_defaults(clay[tr]);
    ui_message("DRONE CLAY ON");
}
/* (BYTE) the stored settings (the project's 'C' 'L'): PITCH biased; a first DRONE CLAY's ('C' 'Y', 26: ON OCT RATE RUN,
 * MOD 1..4 DEST SHAPE EUCLID AMOUNT, their DECAYs, STEP, SEED) as today's: its RATE and EUCLID in the new orders, its
 * STEP every modulator's RATE */
#define CLAY_V1 26u                                    /* (the first layout's count) */
static const clay_t clay_v2_def[CLAY_NF] = {[CL_MF(0, 5u)] = 2, [CL_MF(1, 5u)] = 2, [CL_MF(2, 5u)] = 2, [CL_MF(3, 5u)] = 2};   /* (a shorter block's rest) */
static void clay_pack(const clay_t *c, uint8_t *b)
{
    uint32_t i;
    for (i = 0; i < CLAY_NF; i++) b[i] = (uint8_t)clamp(c[i] + (i == CL_PIT && (c[CL_ON] | c[CL_PIT]) ? CL_PITB : 0), 0, 127);   /* (a
                                                         * track never set: all 0, as clay_unpack reads it back) */
}
static void clay_unpack(const uint8_t *b, clay_t *c)
{
    uint32_t i;
    for (i = 0; i < CLAY_NF; i++) c[i] = (clay_t)((int32_t)(b[i] & 127u) - (i == CL_PIT && (b[CL_ON] | b[CL_PIT]) ? CL_PITB : 0));   /* (an
                                                         * empty block, a track never ON: PITCH 0) */
}
static void clay_from_v1(const uint8_t *o, uint8_t *b)
{
    static const uint8_t RATE1[8] = {0, 9, 8, 7, 6, 5, 4, 3}, STEP1[4] = {4, 3, 2, 1};
    uint32_t m;
    memset(b, 0, CLAY_NF);
    b[CL_ON] = o[0] & 1u; b[CL_OCT] = (uint8_t)(o[1] & 3u); b[CL_RATE] = RATE1[o[2] & 7u]; b[CL_RUN] = o[3] & 1u;
    b[CL_PIT] = CL_PITB; b[CL_SEED] = o[25] & 127u;
    for (m = 0; m < 4u; m++) {
        b[CL_MF(m, 0u)] = o[4u + 4u * m] & 127u; b[CL_MF(m, 1u)] = o[5u + 4u * m] & 127u;
        b[CL_MF(m, 2u)] = CLAY_EU_OLD[o[6u + 4u * m] % 25u]; b[CL_MF(m, 3u)] = o[7u + 4u * m] & 127u;
        b[CL_MF(m, 4u)] = o[20u + m] & 127u; b[CL_MF(m, 5u)] = STEP1[o[24] & 3u];
    }
}
