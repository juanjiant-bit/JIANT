/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.21) JIANT BYTE's DRONE CLAY (GPL, its clay.c of 93e9eb1), ported: CL_N as CLAY_NF (fx.c's CL_N is CLOUD's),
 * the state in the bss (zeroed at power-on: no clay_boot), no FORM part to migrate, no white punch-ins' time here */
/* (BYTE) DRONE CLAY, a track's sequencer (the owner: FORM out as an engine; "the voice the track has, a drone,
 * endless timbres"). Each audio block, before the parts (fx.c mix_block, after the macros): for every track with it
 * ON and running (RUN SONG: while the transport plays; FREE: always), FORM's composer (eng_form.c) on the track's
 * settings: the euclids strike gestures on the STEP, the gestures summed into what each modulator adds now (clay_v,
 * mod.c mod_begin moves its DEST with it); a NOTE gesture (ARP, CHORD ..) a real note on the voice, on the scale,
 * two octaves over the drone; the drone itself, a very low note (C0 .. C3 on the track's ROOT) held, or struck again
 * every RATE. Off, stopped, muted: its notes end. Included by seq.c (its notes, the scale) */
static void ui_message(const char *s);                  /* (ui.c) */
static uint32_t clay_free;                              /* (FREE stopped: its own beat, Q16) */
static int16_t clay_sp[CLAY_SPN];                        /* (the audio's FORM slots: clay_fill) */
static uint32_t clay_b0[NTRK], clay_pq[NTRK];         /* (BYTE's ff242c4) a punch-in's restart: the time it began; what was held */
static void clay_stop(uint32_t tr)
{
    track_t *t = &trk[tr];
    if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
    if (clay_nn[tr]) seq_note_off(t, clay_nn[tr]);
    clay_dn[tr] = clay_nn[tr] = 0;
    clay_v[tr][0] = clay_v[tr][1] = clay_v[tr][2] = clay_v[tr][3] = 0;
    form_lit[tr] = 0;
}
static __attribute__((noinline)) void clay_tick(uint32_t n)
{
    static const uint32_t PER[8] = {0, 16u << 16, 8u << 16, 4u << 16, 2u << 16, 1u << 16, 1u << 15, 1u << 14};
    uint32_t s16 = div_samples(2), bps = beat_samples(), b, tr, k;
    if (song.playing && clk_pos != CLK_START)
        b = (((uint32_t)clk_n << 16) + ((uint32_t)clk_pos << 16) / (s16 ? s16 : 1u)) >> 2;
    else
        b = clay_free += (n << 16) / (bps | 1u);
    for (tr = 0; tr < NTRK; tr++) {
        track_t *t = &trk[tr];
        const int16_t *c = clay[tr];
        form_st_t *st = &form_st[tr];
        uint32_t end = b, seed = (uint32_t)clamp(c[CL_SEED], 0, 127) * 977u + 1u, root, dn;
        int32_t D[FV_N] = {0};
        if (!c[CL_ON] || tr >= trk_live || t->p[P_MUTE] || (!song.playing && c[CL_RUN] != 1)) {
            if (clay_dn[tr] | clay_nn[tr] | form_lit[tr]) clay_stop(tr);
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
        clay_fill(c, clay_sp);
        st->clk += (n << 18) / (bps ? bps : 1u);
        for (k = 0; k < 4u; k++) {                     /* each modulator on the STEP: its euclid strikes a gesture */
            int32_t ds = (int32_t)form_vds_x(clay_sp, FORM_MC, k), dec = clamp(clay_sp[FORM_MC[k][3]], 0, 127);
            uint32_t s1 = end >> (16 - form_mexpo(clay_sp, ds));
            if ((int32_t)s1 == st->lastm[k]) continue;
            st->lastm[k] = (int32_t)s1;
            form_lit[tr] = (uint8_t)(form_lit[tr] & ~(1u << k));
            if (form_hit(clay_sp, k, s1, seed)) {
                form_spawn(st, (uint32_t)((ds & 511) >> 3), k, (s1 & 3u) ? 90u : 127u, 0u, 64u + (uint32_t)dec * 32u, 0u);
                form_lit[tr] |= (uint8_t)(1u << k);
            }
        }
        form_eval(st, clay_sp, D, t->at);
        for (k = 0; k < 4u; k++)
            clay_v[tr][k] = (int16_t)clamp(st->mv[k] * 256, -32767, 32767);
        root = (uint32_t)clamp(t->p[P_ROOT], 0, 11);
        dn = 12u * (uint32_t)(clamp(c[CL_OCT], 0, 3) + 1) + root;   /* the drone: C0 .. C3 on the ROOT */
        {
            uint32_t per = PER[clamp(c[CL_RATE], 0, 7)], cnt = per ? end / per : 0u, j, busy = 0;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (BYTE's ff242c4, the owner: the sequencers live with it) one voice:
                                                         * a step's or a key's note takes it, the drone back after */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (busy) {
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                clay_dn[tr] = 0;
            } else if (clay_dn[tr] != dn || (per && cnt != clay_lr[tr])) {   /* new, moved, or its RATE's strike */
                if (clay_dn[tr]) seq_note_off(t, clay_dn[tr]);
                seq_note_on(t, dn, 110u);
                clay_dn[tr] = (uint8_t)dn;
            }
            clay_lr[tr] = cnt;
        }
        {   /* a NOTE gesture alive: its note on the voice, on the scale, two octaves over the drone */
            int32_t sem = D[FV_ESUM] > 0 ? D[FV_SEM] * 32767 / (D[FV_ESUM] > 32767 ? D[FV_ESUM] : 32767) : 0;
            uint32_t nn = 0, j, busy = 0;
            if ((t->p[P_VOICE] & 3) != V_POLY)          /* (one voice: a step's or a key's note before a gesture's) */
                for (j = 0; j < t->nmono && j < 8u; j++)
                    busy |= t->mono_stack[j] != clay_dn[tr] && t->mono_stack[j] != clay_nn[tr];
            if (D[FV_ESUM] > 3000 && !busy)
                nn = (uint32_t)clamp(scale_snap(t, (int32_t)dn + 24 + (sem * 12 + (sem < 0 ? -2048 : 2048)) / 4096), 12, 108);
            if (nn != clay_nn[tr]) {
                if (clay_nn[tr]) seq_note_off(t, clay_nn[tr]);
                if (nn && nn != dn) seq_note_on(t, nn, 100u);
                clay_nn[tr] = (uint8_t)(nn != dn ? nn : 0u);
            }
        }
    }
}

/* the pages' knobs (params.c page_desc: ids ID_CL0 + CL_*): the selected track's settings, written in place (ui_input.c
 * edit_param, then clay_put) */
static const char *const CL_ONN[] = {"OFF", "ON", 0};
static const char *const CL_RATEN[] = {"HOLD", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", 0};
static const char *const CL_RUNN[] = {"SONG", "FREE", 0};
static const char *const CL_NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const param_desc_t *clay_desc(const track_t *t, uint32_t f, int16_t **valp)
{
    static const char *nm[CLAY_ND + 1], *on[5];   /* (DEST's: the sound's knobs by name) */
    static char octn[4][4];
    static param_desc_t d;
    static const char *const BASE[CLAY_ND] = {"OFF", "PITCH", "CUTOFF", "SHAPE", "VOLUME", "PAN", "DIST", "CHORUS", "DELAY",
                                              "REVERB", "LFO RATE", "VIBRATO", 0, 0, 0, 0, 0, 0, 0, 0, "VOICE CUT", "LEVEL"};   /* (FM: no LIFE
                                                         * destination: its own HOME > LIFE) */
    static const char *const DECN[4] = {"DECAY 1", "DECAY 2", "DECAY 3", "DECAY 4"};
    static const char *const EN[8] = {"E1", "E2", "E3", "E4", "E5", "E6", "E7", "E8"};
    uint32_t tr = (uint32_t)(t - trk) % NTRK, i;
    f %= CLAY_NF;
    *valp = &clay[tr][f];
    d.def = 0; d.names = 0; d.fmt = F_ENUM; d.min = 0;
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
    } else if (f == CL_RATE) { d.label = "RATE"; d.max = 7; d.names = CL_RATEN; }
    else if (f == CL_RUN) { d.label = "RUN"; d.max = 1; d.names = CL_RUNN; }
    else if (f < CL_DEC) {
        uint32_t k = (f - CL_M0) & 3u;
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
        } else if (k == 2u) { d.label = "EUCLID"; d.max = FORM_NEU - 1; d.names = FORM_EU; }
        else { d.label = "AMOUNT"; d.fmt = F_PCT; d.max = 127; d.def = 100; }
    } else if (f < CL_STEP) { d.label = DECN[f - CL_DEC]; d.fmt = F_PCT; d.max = 127; }
    else if (f == CL_STEP) { d.label = "STEP"; d.max = 3; d.def = 2; d.names = FORM_RATE; }
    else { d.label = "SEED"; d.fmt = F_INT; d.max = 127; }
    return &d;
}
/* a knob of the pages turned (its value already in): ON from a blank one, its first sound (clay_defaults) */
static void clay_put(uint32_t idx, int32_t was, int32_t v)
{
    uint32_t tr = idx / CLAY_NF % NTRK, f = idx % CLAY_NF, m, any = 0;
    if (f != CL_ON || !v || was)
        return;
    for (m = 0; m < 4u; m++) any |= clay[tr][CL_MF(m, 3u)] != 0;
    if (!any)
        clay_defaults(clay[tr]);
    ui_message("DRONE CLAY ON");
}
