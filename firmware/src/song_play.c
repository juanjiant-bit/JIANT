/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Andre Rezemini (SLOOP)
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): after SLOOP 2.4.1's arranger, live sections and SONG REC (isod89/sloop-fm1, GPL-3.0); the
 * SONG REC take (srec_finish, srec_add, srec_bar, srec_stop) follows SLOOP 2.4.1's seq.c; the start, stop and row
 * changes (chain_start, chain_stop, chain_tick, the tracks' restart in sec_apply) Felucca's song_chain.c. */
/* The song, the audio ISR's part (song_chain.c: what it is). Included at the end of seq.c. */
static uint32_t chain_bar(void) { return 16u * div_samples(2); }        /* a 4/4 bar: 16 steps of 1/16 (DIV 2) */

/* (V10.6z8) track k's sound as snd_held sees it (k NTRK: the DRUM-X kit); FM6's SLOT left out (song_poll's
 * fm6_adopt sets it after a section) */
static uint32_t snd_sig(uint32_t k)
{
    const track_t *t = &trk[k];
    uint32_t h = 0, j;
    if (k >= NTRK)
        return as_mix(h, dx_kit, sizeof dx_kit);
    for (j = 0; j < P_COUNT; j++)
        if (!param_kept(j) && (j != P_E7 || t->eng_req != ENGI_FM6))
            h = h * 31u + (uint16_t)t->p[j];
    return as_mix(h + t->eng_req * 977u + t->preset, fm6_patch[k], FP_SIZE);
}
/* .. the player's: changed since the section before put it in */
static uint32_t snd_own(uint32_t k)
{
    if ((snd_seen >> k & 1u) && snd_sig(k) != snd_h[k])
        snd_held |= (uint8_t)(1u << k);
    return snd_held >> k & 1u;
}
/* the staged section in: every track from its step 0 (seq_tick: chain.carry samples into it) */
static void sec_apply(void)
{
    uint32_t i, j, own;
    if (!chain.running)
        snd_held = snd_seen = 0;                      /* (a live jump, CLIPS: the parts as stored) */
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        const sec_trk_t *s = &sec_stage.t[i];
        seq_release(t);
        motion_restore(t);                            /* (the sound's own values, not the motion's: snd_own) */
        own = snd_own(i);
        if (!own) {
            if (t->eng_req != s->engine)
                panic_req |= (uint8_t)(1u << i);          /* (another engine: what the keys hold ends too) */
            t->eng_req = s->engine;
            t->preset = s->preset;
            t->user = 0;
            memcpy(fm6_patch[i], s->fm6, FP_SIZE + 1u);
            fm6_pgen[i]++;
        }
        t->seq_fit = sec_stage_fit[i];
        for (j = 0; j < P_COUNT; j++)
            if (!own || param_kept(j))
                t->p[j] = s->p[j];
        memcpy(t->step, s->step, sizeof t->step);
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFFu;
        t->ext = 0;
        t->rh_n = t->rskip_n = 0;
        t->rat_left = 0;
        snd_h[i] = snd_sig(i);
    }
    motion = sec_stage.motion;
    if (!snd_own(NTRK))
        memcpy(dx_kit, sec_stage.dx, sizeof dx_kit);
    snd_h[NTRK] = snd_sig(NTRK);
    snd_seen = chain.running ? 0xFFu : 0u;
    dx_mute_set(sec_stage.dx_mute);
    memcpy(pfx_lane, sec_stage.pfx_lane, sizeof pfx_lane);
    memcpy(&song.g[G_SLMODE], sec_stage.slg, sizeof sec_stage.slg);   /* (0.6.3) the SLICER's bus as the section has it */
    pfx_ltgt = sec_stage.pfx_ltgt;
    pfx_lph = 0;                                      /* (the lane from the section's start) */
    memset(motion_active, 0, sizeof motion_active);
    memset(motion_locked, 0, sizeof motion_locked);
    motion_base_valid = 0;
    live.cur = (int8_t)sec_stage.section;
    sec_stage.ready = 0;
    chain.applied = 1;
}
/* (JIANT) the scene in effect onto what the section brought: its mutes; a row's own scene also the macros and the
 * punch-in effects (which nothing else sets) */
static void scene_apply(uint32_t row)
{
    const scene_t *s;
    uint32_t i;
    if (chain.loop || row >= CHAIN_ROWS)
        return;                                       /* (a quick chain: no scenes) */
    if (song_idx.scene[song_cur][row].on) {
        s = &song_idx.scene[song_cur][row];
        chain.scn = (uint8_t)(row + 1u);
        memcpy(macro_v, s->mac, sizeof macro_v);
        for (i = 0; i < 4u; i++)
            macro_v[i] &= 127u;
        scene_pfx = ((uint32_t)s->pfx << PF_OCTD) & PF_MIDI;
    } else if (chain.scn) {
        s = &song_idx.scene[song_cur][chain.scn - 1u];
    } else {
        return;
    }
    for (i = 0; i < NTRK; i++)
        trk[i].p[P_MUTE] = (int16_t)(i < 4u ? (s->mute >> i) & 1u : (s->on >> (i - 3u)) & 1u);
    dx_mute_set((uint16_t)((dx_mute & ~DXG_ALL) | ((s->mute >> 4) & DXG_ALL)));
}
static void chain_apply(void)                         /* a song row's section in */
{
    chain.row = sec_stage.row;
    chain.remaining = chain.config.row[chain.row].bars;
    sec_apply();
    scene_apply(chain.row);
    if (!chain.loop)                                  /* (add15-beta4) ARRANGE 2's MUTE: over the scene, for the row */
        for (uint32_t k = 0; k < NTRK; k++)
            if (row_var(chain.row, k) == RV_MUTE)
                trk[k].p[P_MUTE] = 1;
}

/* (V10.8) REC: the JAM take (song_chain.c), from the bar line after REC: the tracks' sequences at its start, each
 * change after as it sounds (seq.c jam_step); REC again or STOP: its END on the next bar line */
static void srec_bar(void)                            /* a bar ended (playing) */
{
    uint32_t k;
    if (live.srec != 1u)
        return;
    live.srec = 2;
    live.nrec = 0;
    jam_t0 = gl_step_at(0);
    for (k = 0; k < NTRK; k++)
        jam_add(JAM_EV(0u, k, (clip_go[k] - 1u < 4u ? clip_go[k] : clip_now[k] - 1u < 4u ? clip_now[k] : 1u) - 1u));
}
static void srec_stop(void)                           /* REC again, or STOP: the take ends on its bar line */
{
    if (live.srec == 2u) {
        uint32_t p = gl_step_at(0) - jam_t0;
        live.jam[live.nrec < JAM_N ? live.nrec : JAM_N - 1u] = JAM_EV((p + 15u) & ~15u ? (p + 15u) & ~15u : 16u, 7u, 0u);
        live.nrec = 0;
        live.rec_done = 1;
    }
    live.srec = 0;
}

static void chain_start(void)                         /* (seq_start) */
{
    chain.carry = 0;
    chain.bar_pos = 0;
    chain.armed_bar = 0;
    if (!chain.armed)
        return;
    chain.armed = 0;
    if (!sec_stage.ready || sec_stage.row >= CHAIN_ROWS)
        return;                                       /* (cannot happen: chain_prepare staged its first row) */
    chain.rec = song.rec;
    song.rec = 0;                                     /* a song plays, it does not record */
    chain.running = 1;
    chain.scn = 0;
    chain_apply();
}
static void chain_stop(void)                          /* (seq_stop) */
{
    chain.armed = 0;
    chain.armed_bar = 0;
    chain.hold = 0;
    live.req = -1;
    srec_stop();
    if (!chain.running)
        return;
    song.rec = chain.rec;
    chain.running = 0;
    scene_pfx = 0;                                    /* (a scene's punch-in effects end with the song) */
    chain.ended = chain.kept;                         /* (a quick chain keeps nothing: what plays stays) */
}
static void chain_tick(uint32_t n)
{
    uint32_t bar;
    if (!song.playing)
        return;
    bar = chain_bar();
    chain.bar_pos += n;
    if (!bar || chain.bar_pos < bar)
        return;
    chain.bar_pos -= bar;                             /* (a bar is longer than a block) */
    chain.carry = chain.bar_pos;
    srec_bar();
    if (chain.running) {
        uint32_t next = chain.row + !chain.hold;      /* (V10.6o: held, the same row again) */
        if (chain.remaining > 1u) {
            chain.remaining--;
            return;
        }
        if (next >= chain.config.count) {
            uint32_t e = SONG_END(&chain.config);
            if (!chain.loop && !(e == SONG_FOREVER || (e >= 2u && chain.pass + 1u < e))) {   /* the song's end */
                seq_stop();
                return;
            }
            if (sec_stage.ready && sec_stage.row == 0u && !chain.loop && chain.pass < 254u)
                chain.pass++;                         /* (0.6.22: its END: once more round) */
            next = 0;                                 /* (a quick chain: round again) */
        }
        if (sec_stage.ready && sec_stage.row == next) {
            chain_apply();                            /* (not staged yet: this row one bar more) */

        }
        return;
    }
    if (chain.armed_bar && sec_stage.ready && sec_stage.row < CHAIN_ROWS) {   /* a quick chain (V10.6o: a song's row) from
                                                         * this bar */
        chain.armed_bar = 0;
        chain.running = 1;
        chain.rec = song.rec;
        chain_apply();

        return;
    }
    if (live.req >= 0 && sec_stage.ready && sec_stage.row == SEC_LIVE) {   /* a live jump */
        live.req = -1;
        song.rec = 0;                                 /* (a take does not run on into another section) */
        sec_apply();

    }
}

