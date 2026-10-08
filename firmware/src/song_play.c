/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT (FELUCCA TONIC): after SLOOP 2.4.1's arranger (isod89/sloop-fm1, GPL-3.0). */
/* The song, the audio ISR's part (song_chain.c: what it is). Included at the end of seq.c. */
static uint32_t chain_bar(void) { return 16u * div_samples(2); }        /* a 4/4 bar: 16 steps of 1/16 (DIV 2) */

/* (ISR) the staged section in: every track from its step 0 (seq_tick: the carried samples into it) */
static void chain_apply(void)
{
    uint32_t i, j;
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        const sec_trk_t *s = &sec_stage.t[i];
        seq_release(t);
        motion_restore(t);
        if (t->eng_req != s->engine)
            panic_req |= (uint8_t)(1u << i);              /* (another engine: what the keys hold ends too) */
        t->eng_req = s->engine;
        t->preset = s->preset;
        t->user = 0;
        for (j = 0; j < P_COUNT; j++)
            t->p[j] = s->p[j];
        memcpy(t->step, s->step, sizeof t->step);
        memcpy(fm6_patch[i], s->fm6, FP_SIZE + 1u);
        fm6_pgen[i]++;
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFFu;
        t->rh_n = t->rskip_n = 0;
        t->rat_left = 0;
    }
    motion = sec_stage.motion;
    memset(motion_active, 0, sizeof motion_active);
    memset(motion_locked, 0, sizeof motion_locked);
    motion_base_valid = 0;
    chain.row = sec_stage.row;
    chain.remaining = chain.config.row[chain.row].bars;
    sec_stage.ready = 0;
    chain.applied = 1;
}
static void chain_start(void)
{
    if (!chain.armed)
        return;
    chain.armed = 0;
    if (!sec_stage.ready || sec_stage.row != 0u)
        return;                                       /* (cannot happen: chain_prepare staged row 0) */
    chain.rec = song.rec;
    song.rec = 0;                                     /* a song plays, it does not record */
    chain.carry = 0;
    chain.bar_pos = 0;
    chain.running = 1;
    chain_apply();
}
static void chain_stop(void)
{
    chain.armed = 0;
    if (!chain.running)
        return;
    song.rec = chain.rec;
    chain.running = 0;
    chain.ended = 1;
}
static void chain_tick(uint32_t n)
{
    uint32_t bar;
    if (!chain.running)
        return;
    bar = chain_bar();
    chain.bar_pos += n;
    if (!bar || chain.bar_pos < bar)
        return;
    chain.bar_pos -= bar;                             /* (a bar is longer than a block) */
    if (chain.remaining > 1u) {
        chain.remaining--;
        return;
    }
    if (chain.row + 1u >= chain.config.count) {       /* the song's end */
        seq_stop();
        return;
    }
    if (!sec_stage.ready || sec_stage.row != chain.row + 1u)
        return;                                       /* not staged yet: this row one bar more */
    chain.carry = chain.bar_pos;
    chain_apply();
}

