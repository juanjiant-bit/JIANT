/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT (FELUCCA TONIC): after SLOOP 2.4.1's song (isod89/sloop-fm1, GPL-3.0). */
/* The song, the main loop's part (song_chain.c: what it is). Included by project.c. */
static project_store_t song_keep __attribute__((section(".pool")));   /* the music as it was before PLAY */

/* section s (a project slot) into sec_stage for row `row`; 0 = staged */
static int song_stage(uint32_t s, uint32_t row)
{
    project_t *p = &proj_scratch;
    uint32_t k, i;
    if (!proj_import(p, &proj_slot[s & 3u], sizeof(project_store_t)))
        return 1;
    for (k = 0; k < NTRK; k++) {
        sec_trk_t *d = &sec_stage.t[k];
        const proj_trk_t *t = &p->t[k];
        uint32_t e = eng_live(t->engine);
        d->engine = (uint8_t)e;
        d->preset = (uint8_t)(ENGINES[e]->npresets ? (t->preset >= PROJ_DEF_KEEP ? 0u : t->preset) % ENGINES[e]->npresets
                                                   : 0u);
        for (i = 0; i < P_COUNT; i++)
            d->p[i] = (int16_t)param_fit(param_desc_of(e, i), t->p[i]);
        memcpy(d->step, t->step, sizeof d->step);
        proj_steps(d->step);
        fm6_unpack(p->fm6[k], d->fm6);
        fm6_sanitize(d->fm6);
    }
    sec_stage.motion = p->motion;
    sec_stage.row = (uint8_t)row;
    RING_PUBLISH();
    sec_stage.ready = 1;
    return 0;
}

/* Main loop only: PLAY a song. 0 = armed, 1 = no rows, 2 = busy, 3 + s = section s not saved */
static uint32_t chain_prepare(void)
{
    uint32_t i;
    if (transport_busy())
        return 2;
    if (!chain_valid(&chain_config) || !chain_config.count)
        return 1;
    for (i = 0; i < chain_config.count; i++)
        if (!proj_import(&proj_scratch, &proj_slot[chain_config.row[i].slot], sizeof(project_store_t)))
            return 3u + chain_config.row[i].slot;
    project_capture(&proj_scratch);                   /* the music as it is: back when the song ends */
    chain.kept = (uint8_t)proj_pack(&song_keep, &proj_scratch);
    chain.config = chain_config;
    chain.ended = 0;
    if (song_stage(chain.config.row[0].slot, 0))
        return 3u + chain.config.row[0].slot;
    RING_PUBLISH();
    chain.armed = 1;
    transport_req = 1;
    return 0;
}

/* Main loop, each pass: the next section staged; the music back after the song */
static void song_poll(void)
{
    uint32_t k;
    if (chain.applied) {
        chain.applied = 0;
        for (k = 0; k < NTRK; k++)
            fm6_adopt(k);                             /* (an FM6 track's SLOT: F n or OWN) */
        sync_reload = 1;
        ui.force = 1;
    }
    if (chain.running && !sec_stage.ready && chain.row + 1u < chain.config.count)
        (void)song_stage(chain.config.row[chain.row + 1u].slot, chain.row + 1u);
    if (chain.ended && !song.playing) {
        uint8_t cur = proj_cur;
        char name[PROJ_NAME_LEN + 1u];
        chain.ended = 0;
        str_cpy(name, proj_name, sizeof name);
        if (chain.kept && proj_import(&proj_scratch, &song_keep, sizeof song_keep) &&
            !project_restore_runtime(&proj_scratch)) {
            proj_cur = cur;                           /* (the music before the song: its slot and name) */
            str_cpy(proj_name, name, PROJ_NAME_LEN + 1u);
            ui_message("SONG END");
        }
    }
}
