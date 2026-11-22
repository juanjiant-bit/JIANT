/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): the song's design inspired by SLOOP 2.4.1's (Andre Rezemini, isod89/sloop-fm1, GPL-3.0:
 * sections, quick chain, SONG REC), written anew on Felucca's song chain state (song_chain.c). */
/* The song, the main loop's part (song_chain.c: what it is). Included by project.c. */
static project_store_t song_keep __attribute__((section(".pool")));   /* the music as it was before PLAY */

/* ---- the songs (NSONG, four sections each: storage.c) */
static uint8_t song_idx_dirty;                    /* the index changed (rows, the current song), not in flash yet */
static uint32_t song_idx_t;                       /* .. since (fm1_ms): written once the rows rest a while */

static void jam_load(void);
static void song_rows_fix(chain_config_t *c)      /* a song's rows as chain_config takes them */
{
    if (!chain_valid(c))
        chain_defaults(c);
}
/* persist_boot, before the slots are filled: the index from flash (none or broken: song 1, its rows taken from its
 * section A at song_index_boot: Felucca's song, kept in the project) */
static void song_index_load(void)
{
    uint32_t k;
#if FELUCCA_FLASH
    int n = st_load(OBJ_SONGIDX, &song_idx, sizeof song_idx);
    if (song_idx.magic == SONG_MAGIC && song_idx.cur < NSONG_IDX &&
        ((song_idx.version == SONG_IDX_V && n == (int)sizeof song_idx) ||
         (song_idx.version == 3u && n == (int)__builtin_offsetof(song_index_t, tvar)) ||
         (song_idx.version == 2u && n == (int)__builtin_offsetof(song_index_t, tsrc)) ||
         (song_idx.version == 1u && n == (int)__builtin_offsetof(song_index_t, scene)))) {
        if (song_idx.version == 1u)                 /* (before the scenes: none) */
            memset(song_idx.scene, 0, sizeof song_idx.scene);
        if (song_idx.version < 3u)                  /* (add15: before ARRANGE 2, every track its row's section) */
            memset(song_idx.tsrc, 0, sizeof song_idx.tsrc);
        if (song_idx.version < 4u)                  /* (add15-beta4: before VAR, no variations) */
            memset(song_idx.tvar, 0, sizeof song_idx.tvar);
        song_idx.version = SONG_IDX_V;
        for (k = 0; k < NSONG_IDX; k++)
            song_rows_fix(&song_idx.rows[k]);
        if (song_idx.cur >= NSONG)                  /* (V10.6p) an older index on song 7 or 8 (retired): song 6 */
            song_idx.cur = NSONG - 1u;
        song_cur = song_idx.cur;
        return;
    }
#endif
    memset(&song_idx, 0, sizeof song_idx);
    song_idx.magic = SONG_MAGIC;
    song_idx.version = SONG_IDX_V;
    for (k = 0; k < NSONG_IDX; k++)
        chain_defaults(&song_idx.rows[k]);
    song_cur = 0;
    song_idx.rsv[0] = 1;                          /* (no index yet: song_index_boot takes section A's rows) */
}
/* persist_boot, after the slots: the current song's rows */
static void song_index_boot(void)
{
    if (song_idx.rsv[0]) {
        song_idx.rsv[0] = 0;
        if (proj_import(&proj_scratch, &proj_slot[0], sizeof(project_store_t)) && chain_valid(&proj_scratch.chain))
            song_idx.rows[0] = proj_scratch.chain;
    }
    chain_config = song_idx.rows[song_cur];
    song_mode_restore();
    jam_load();                                         /* (V10.8) the song's JAM take */
}
/* the current song's name: its section A's, else SONG n (b: PROJ_NAME_LEN + 1 bytes) */
static void song_name(char *b)
{
    if (!project_name(0, b) || !b[0]) {
        str_cpy(b, "SONG ", PROJ_NAME_LEN + 1u);
        b[5] = (char)('1' + song_cur);
        b[6] = 0;
    }
}

/* (add15-beta4) ARRANGE 2's VAR: track k's steps (the first len) as row `row` plays them. The same row, the same
 * variation: VAR 1..4 draw from a seed of the song, the row and the track, never rng() */
static void row_vary(step_t *st, uint32_t len, int kit, uint32_t v, uint32_t seed)
{
    step_t *tmp = mscr.row_tmp;                         /* (the main loop's scratch: core.h mscr) */
    uint32_t i, h, n = len > NSTEP ? NSTEP : (len ? len : 1u), half = n / 2u ? n / 2u : 1u;
    int32_t last = -1;
    if (v == RV_NONE || v == RV_MUTE)
        return;
    memcpy(tmp, st, n * sizeof *st);
    for (i = 0; i < n; i++) {
        step_t s = tmp[i];
        h = (seed ^ (i + 1u) * 2654435761u) * 2246822519u;
        h ^= h >> 15;
        switch (v) {
        case RV_THIN:                                   /* the off-beats out (the 16ths between the 8ths) */
            if (i & 1u) { s.time = ST_REST; s.n = 0; s.hit = s.acc = 0; }
            break;
        case RV_FILL:                                   /* the last beat: every step plays (a roll into the next row) */
            if (i + 4u >= n) {
                if (kit) { s.hit |= (uint8_t)((tmp[n - 1u].hit | 2u) & ~1u); s.time = ST_NOTE; }   /* (lane 1 and the last
                                                         * step's, never the kick) */
                else if (last >= 0 && s.time != ST_NOTE) { memset(&s, 0, sizeof s); s.n = 1; s.time = ST_NOTE; s.vel = 100; s.note[0] = (uint8_t)last; }
            }
            break;
        case RV_HALF: s = tmp[i % half]; break;         /* the first half twice */
        case RV_REV: s = tmp[n - 1u - i]; if (s.time == ST_TIE) s.time = ST_NOTE; break;
        case RV_SHIFT: s = tmp[(i + n - (2u % n)) % n]; break;   /* an 8th late, round in LEN */
        case RV_OCTU: case RV_OCTD:
            if (!kit) for (uint32_t j = 0; j < s.n && j < 4u; j++) s.note[j] = (uint8_t)clamp(s.note[j] + (v == RV_OCTU ? 12 : -12), 0, 127);
            break;
        default: {                                      /* VAR 1..4: hits / notes moved, dropped, added */
            uint32_t amt = 40u + 30u * (v - RV_MUT1);   /* (of 256) */
            if (kit) {
                for (uint32_t l = 1; l < NLANE; l++)
                    if (((h >> (l * 3u)) & 255u) < amt / 2u) s.hit ^= (uint8_t)(1u << l);
                s.acc &= s.hit;
                s.time = s.hit || s.n ? ST_NOTE : ST_REST;
            } else if (s.time == ST_NOTE && s.n) {
                if ((i & 3u) && (h & 255u) < amt / 2u) { s.time = ST_REST; s.n = 0; }
                else if (((h >> 8) & 255u) < amt / 2u)
                    for (uint32_t j = 0; j < s.n && j < 4u; j++) s.note[j] = (uint8_t)clamp(s.note[j] + ((h >> 16) & 1u ? 12 : 7), 0, 127);
            } else if (s.time == ST_REST && (i & 1u) && last >= 0 && (h & 255u) < amt / 3u) {
                memset(&s, 0, sizeof s); s.n = 1; s.time = ST_NOTE; s.vel = 90; s.note[0] = (uint8_t)last;
            }
        } }
        if (s.time == ST_NOTE && s.n) last = s.note[0];
        st[i] = s;
    }
}

/* section s (a project slot) into sec_stage for row `row` (SEC_LIVE: a live jump); 0 = staged. (add15) A song's row may
 * take tracks from other sections (ARRANGE 2: row_src): each of those is decoded in turn, its tracks (sound, steps, FM6
 * patch, automation; DRUM's kit with the DRUM track) taken from it; the row's own section last, with the rest.
 * (V10.6s) clip_mix (CLIPS): the row's own is the music as it is (song_keep, 4), the tracks asked for (clip_go) from
 * their parts, a track asked to stop muted; s the section the stage is said to be (SONG REC's row: its tracks' tsrc) */
static int song_stage(uint32_t s, uint32_t row)
{
    project_t *p = &proj_scratch;
    uint32_t own = clip_mix ? 4u : s & 3u, k, i, u, n = 0, on = 0, dx_from = own, use = 0, srcs[NTRK], src = 0;
    sec_stage.ready = 0;
    for (k = 0; k < NTRK; k++) {
        uint32_t c = (uint32_t)(clip_go[k] ? clip_go[k] : clip_now[k]) - 1u;
        srcs[k] = clip_mix ? ((uint32_t)clip_go[k] - 1u < 4u ? clip_go[k] - 1u : 4u) :
                  row < CHAIN_ROWS && !chain.loop ? row_src(row, k, s) : s & 3u;
        use |= 1u << srcs[k];
        if (clip_mix && c < 4u && c != (s & 3u))
            src |= (c + 1u) << (3u * k);
    }
    for (u = 0; u < 5u; u++) {
        uint32_t sec = u < 4u ? u : own;
        if (u < 4u && (sec == own || !((use >> sec) & 1u)))
            continue;                                 /* (the row's own goes last: its globals) */
        if (!proj_import(p, sec < 4u ? &proj_slot[sec] : &song_keep, sizeof(project_store_t)))
            return 1;
        for (k = 0; k < NTRK; k++) {
            sec_trk_t *d = &sec_stage.t[k];
            const proj_trk_t *t = &p->t[k];
            uint32_t e = eng_live(t->engine);
            if (srcs[k] != sec)
                continue;
            sec_stage_fit[k] = fit_valid(p->fit[k]);
            d->engine = (uint8_t)e;
            d->preset = (uint8_t)(ENGINES[e]->npresets ? (t->preset >= PROJ_DEF_KEEP ? 0u : t->preset) % ENGINES[e]->npresets
                                                       : 0u);
            for (i = 0; i < P_COUNT; i++)
                d->p[i] = (int16_t)param_fit(param_desc_of(e, i), t->p[i]);
            memcpy(d->step, t->step, sizeof d->step);
            proj_steps(d->step);
            if (row < CHAIN_ROWS && !chain.loop) {      /* (add15-beta4) ARRANGE 2's VAR */
                uint32_t v = row_var(row, k);
                row_vary(d->step, (uint32_t)d->p[P_SLEN], e == ENGI_DRUM, v,
                         (song_cur + 1u) * 40503u ^ (row + 1u) * 104729u ^ (k + 1u) * 1299709u ^ v * 15485863u);
                if (v == RV_MUTE) d->p[P_MUTE] = 1;
            }
            if (clip_mix && clip_go[k] == CLIP_STOP)    /* (V10.6s) CLIPS' OCT-: the track stops */
                d->p[P_MUTE] = 1;
            fm6_unpack(p->fm6[k], d->fm6);
            fm6_sanitize(d->fm6);
            on |= p->motion.on & (1u << k);
            if (e == ENGI_DRUM && sec != own)
                dx_from = sec;
        }
        for (i = 0; i < p->motion.count && i < MOTION_MAX && n < MOTION_MAX; i++)
            if (srcs[MOTION_TRK(&p->motion.event[i]) % NTRK] == sec)
                sec_stage.motion.event[n++] = p->motion.event[i];
        if (sec == dx_from) {
            memcpy(sec_stage.dx, p->dx, sizeof sec_stage.dx);
            sec_stage.dx_mute = (uint16_t)(p->dx_mute & DXM_ALL);
        }
    }
    memset(sec_stage.motion.event + n, 0, (MOTION_MAX - n) * sizeof sec_stage.motion.event[0]);
    sec_stage.motion.count = (uint8_t)n;
    sec_stage.motion.on = (uint8_t)(on | (p->motion.on & ~((1u << NTRK) - 1u)));
    memcpy(sec_stage.motion.rsv, p->motion.rsv, sizeof sec_stage.motion.rsv);
    memcpy(sec_stage.pfx_lane, p->pfx_lane, sizeof sec_stage.pfx_lane);
    memcpy(sec_stage.slg, &p->g[G_SLMODE], sizeof sec_stage.slg);   /* (0.6.3) the section's SLICER */
    sec_stage.pfx_ltgt = p->pfx_ltgt > 2u ? 0u : p->pfx_ltgt;
    sec_stage.row = (uint8_t)row;
    sec_stage.section = (uint8_t)(s & 3u);
    sec_stage.src = src;
    RING_PUBLISH();
    sec_stage.ready = 1;
    return 0;
}

/* a quick chain's row: the bars section s needs to play its longest track through once (its LEN steps, or FIT's span,
 * at its DIV), in whole 4/4 bars, 1 .. CHAIN_BARS; 1 when the section does not decode */
static uint32_t quick_bars(uint32_t s)
{
    project_t *p = &proj_scratch;
    uint32_t bar = 16u * div_samples(2), most = 1, k;
    if (!bar || !proj_import(p, &proj_slot[s & 3u], sizeof(project_store_t)))
        return 1;
    for (k = 0; k < NTRK; k++) {
        uint32_t steps = (uint32_t)clamp(p->t[k].p[P_SLEN], 1, NSTEP), need;
        if (p->fit[k] & 128u)                         /* (FIT on: its span, not LEN) */
            steps = fit_value_span(p->fit[k]);
        need = (steps * div_samples((uint32_t)p->t[k].p[P_SDIV]) + bar - 1u) / bar;
        most = need > most ? need : most;
    }
    return most > CHAIN_BARS ? CHAIN_BARS : most;
}
/* (V10.6o) PARTS / TIMELINE: section s as proj_scratch holds it (just decoded: graph_project_used): its tracks that
 * play (part_play) */
static uint32_t part_scan(uint32_t s)
{
    const project_t *p = &proj_scratch;
    uint32_t k, i, n, m = 0;
    for (k = 0; k < NTRK; k++) {
        n = (uint32_t)clamp(p->t[k].p[P_SLEN], 1, NSTEP);
        for (i = 0; i < n && p->t[k].step[i].time != ST_NOTE; i++) {}
        if (i < n && !p->t[k].p[P_MUTE])
            m |= 1u << k;
    }
    return m;
}

/* 3 + the section of the song's first row that does not decode (not saved), 0 when every row's does */
static uint32_t song_unsaved(const chain_config_t *c)
{
    uint32_t r;
    for (r = 0; r < c->count; r++)
        if (!proj_import(&proj_scratch, &proj_slot[c->row[r].slot], sizeof(project_store_t)))
            return 3u + c->row[r].slot;
    return 0;
}
/* Main loop only: PLAY a song (V10.6o: from row r0). 0 = armed, 1 = no rows, 2 = busy, 3 + s = section s not saved */
static uint32_t chain_prepare_at(uint32_t r0)
{
    uint32_t rc;
    if (transport_busy())
        return 2;
    if (!chain_valid(&chain_config) || r0 >= chain_config.count)
        return 1;
    rc = song_unsaved(&chain_config);
    if (rc)
        return rc;
    jam_lock = 0;                                     /* (V10.8.1: the song plays, not the JAM take) */
    project_capture(&proj_scratch);                   /* the music as it is: back when the song ends */
    chain.kept = proj_pack(&song_keep, &proj_scratch) ? (uint8_t)(1u | ((uint32_t)(live.cur + 1) << 1)) : 0u;
    chain.config = chain_config;
    chain.loop = 0;
    chain.pass = 0;
    chain.ended = 0;
    if (song_stage(chain.config.row[r0].slot, r0))
        return 3u + chain.config.row[r0].slot;
    RING_PUBLISH();
    chain.armed = 1;
    transport_req = 1;
    return 0;
}

/* section s as the music now, the song's rows kept (a section is a project: its own rows are not the song's) */
static int section_load(uint32_t s)
{
    chain_config_t keep = chain_config;
    uint8_t row = ui.song_row;
    int rc;
    proj_lfo_keep = 1;                                  /* (0.9.18: the song plays on, MODE FREE's LFOs not faded again) */
    rc = !proj_import(&proj_scratch, &proj_slot[s & 3u], sizeof(project_store_t)) || project_restore_runtime(&proj_scratch);
    proj_lfo_keep = 0;
    if (rc)
        return 1;
    chain_config = keep;
    ui.song_row = row;
    proj_cur = (uint8_t)(s & 3u);
    live.cur = (int8_t)(s & 3u);
    memset(clip_now, (int)(s & 3u) + 1, sizeof clip_now);   /* (V10.6s) CLIPS: every track that part's */
    memset(clip_go, 0, sizeof clip_go);
    sec_stage.src = 0;                                  /* (SONG REC's first row: the whole section) */
    return 0;
}

/* (V10.6o) what plays stops being a song or a quick chain (the music it brought plays on, the song's REC arming back);
 * a live jump asked for forgotten */
static void chain_cut(void)
{
    fm1_irq_off();                                    /* (the ISR may be applying one) */
    if (chain.running && !chain.loop) {
        song.rec = chain.rec;
        scene_pfx = 0;
    }
    chain.running = 0;
    chain.armed_bar = 0;
    chain.hold = 0;
    live.req = -1;
    fm1_irq_on();
}
/* (V10.6o) the song from row r (PARTS' OCT-, TIMELINE's PLAY): playing, on the next bar; stopped, now. hold: that row
 * round and round until PLAY lets it go (tl_play). 0 done */
static int chain_goto(uint32_t r, uint32_t hold)
{
    if (!song.playing) {
        uint32_t rc = transport_busy() ? 2u : chain_prepare_at(r);
        if (rc) {
            ui_message(rc == 1u ? "ADD A SONG ROW" : rc == 2u ? "STOP FIRST" : "PART EMPTY");
            return 1;
        }
    } else {
        if (r >= chain_config.count || !chain_valid(&chain_config)) { ui_message("ADD A SONG ROW"); return 1; }
        chain_cut();
        chain.config = chain_config;
        chain.loop = 0;
        chain.pass = 0;
        chain.ended = 0;
        chain.kept = 0;
        chain.scn = 0;
        if (song_stage(chain.config.row[r].slot, r)) { ui_message("PART EMPTY"); return 1; }
        RING_PUBLISH();
        chain.armed_bar = 1;                          /* (chain_tick: on the next bar) */
    }
    chain.hold = (uint8_t)hold;
    return 0;
}
/* the song layer (ui_layer.c), main loop. Section s: in on the next bar (stopped: now). 0 done */
static int section_jump(uint32_t s)
{
    char b[8] = "A";
    s &= 3u;
    b[0] = (char)('A' + s);
    if (chain.running && !chain.loop) {
        ui_message("STOP THE SONG FIRST");
        return 1;
    }
    if (!project_used(s)) {
        ui_say(b, " IS EMPTY");
        return 1;
    }
    if (!song.playing && !transport_busy()) {
        if (section_load(s))
            return 1;
        ui_say("PART ", b);
        return 0;
    }
    chain_cut();                                      /* (a single section ends a quick chain) */
    if (song_stage(s, SEC_LIVE))
        return 1;
    live.req = (int8_t)s;
    ui_say(b, " NEXT BAR");
    return 0;
}
/* (V10.6s) SAVE > CLIPS: track k's cell of part s in on the next bar (stopped: now, and PLAY): the other tracks play on
 * as they are (the music now, song_keep, the stage's own: their sounds, steps, motion; the kit unless the DRUM track's
 * cell goes in), what plays stops being a song. s 4: the track stops (muted on the bar); k NTRK: every track, the part
 * (a live jump: section_jump). Cells asked for in one bar go in together. 0 done */
static int clip_launch(uint32_t k, uint32_t s)
{
    uint32_t sec = live.cur >= 0 ? (uint32_t)live.cur : s & 3u, i;
    if (s < 4u && !project_used(s)) {
        ui_message("PART EMPTY");
        return 1;
    }
    chain_cut();
    live.mode = 0;
    if (k >= NTRK) {
        if (section_jump(s))
            return 1;
        if (song.playing)
            memset(clip_go, (int)s + 1, sizeof clip_go);
    } else {
        project_capture(&proj_scratch);
        if (!proj_pack(&song_keep, &proj_scratch)) {
            ui_message("PART FULL");
            return 1;
        }
        clip_go[k] = (uint8_t)(s + 1u);
        clip_mix = 1;
        i = (uint32_t)song_stage(sec, SEC_LIVE);
        clip_mix = 0;
        if (i) {
            clip_go[k] = 0;
            return 1;
        }
        if (song.playing) {
            live.req = (int8_t)sec;
            ui_message("NEXT BAR");
        } else {
            fm1_irq_off();                              /* (stopped: in now, as the ISR would) */
            sec_apply();
            fm1_irq_on();
        }
    }
    if (!song.playing && !transport_busy())
        transport_req = 1;
    return 0;
}
/* (song_poll) what went in: each track's part (CLIPS' cells asked for, else the section) */
static void clip_seen(void)
{
    uint32_t k, any = 0;
    for (k = 0; k < NTRK; k++)
        any |= clip_go[k];
    for (k = 0; k < NTRK; k++) {
        if (clip_go[k] || !any)
            clip_now[k] = clip_go[k] ? clip_go[k] : (uint8_t)(((uint32_t)live.cur & 3u) + 1u);
        clip_go[k] = 0;
    }
}

/* (V10.8) JAM: track k's sequence as it plays now into part s (its edits kept), when it differs; an empty part is
 * made from the music now, the tracks not on it empty */
static void step_clear(step_t *st);
static void jam_put(uint32_t k, uint32_t s)
{
    project_t *p = &proj_scratch;
    uint32_t j, i;
    s &= 3u;
    if (!proj_import(p, &proj_slot[s], sizeof(project_store_t))) {
        project_capture(p);
        for (j = 0; j < NTRK; j++)
            if (j != k && clip_now[j] != s + 1u)
                for (i = 0; i < NSTEP; i++) step_clear(&p->t[j].step[i]);
    } else if (!memcmp(p->t[k].step, trk[k].step, sizeof trk[k].step) && p->t[k].p[P_SLEN] == trk[k].p[P_SLEN])
        return;
    memcpy(p->t[k].step, trk[k].step, sizeof p->t[k].step);
    p->t[k].p[P_SLEN] = trk[k].p[P_SLEN];
    if (!proj_pack(&proj_wire, p)) { proj_err(); return; }
    proj_wire_gen++;
    memcpy(&proj_slot[s], &proj_wire, sizeof proj_slot[s]);
    live.dirty |= (uint8_t)(1u << s);
}
static uint32_t jam_cur(uint32_t k)                     /* the sequence track k plays (0..3) */
{
    return clip_now[k] - 1u < 4u ? clip_now[k] - 1u : live.cur >= 0 ? (uint32_t)live.cur & 3u : 0u;
}
/* track k to sequence s: staged, in on CAMBIO's step (stopped: at once); 0 done */
static int jam_switch(uint32_t k, uint32_t s)
{
    project_t *p = &proj_scratch;
    uint32_t cur = jam_cur(k), i;
    s &= 3u;
    chain_cut();
    live.mode = 0;
    fm1_irq_off(); jam_rdy &= (uint8_t)~(1u << k); jam_at[k] = 0; fm1_irq_on();
    clip_go[k] = 0;
    if (s == cur)
        return 0;                                       /* (back to the one playing: nothing asked) */
    jam_put(k, cur);
    clip_now[k] = (uint8_t)(cur + 1u);
    if (proj_import(p, &proj_slot[s], sizeof(project_store_t))) {
        memcpy(sec_stage.t[k].step, p->t[k].step, sizeof sec_stage.t[k].step);
        proj_steps(sec_stage.t[k].step);
        sec_stage.t[k].p[P_SLEN] = (int16_t)clamp(p->t[k].p[P_SLEN], 1, NSTEP);
    } else {
        for (i = 0; i < NSTEP; i++) step_clear(&sec_stage.t[k].step[i]);
        sec_stage.t[k].p[P_SLEN] = trk[k].p[P_SLEN];
    }
    clip_go[k] = (uint8_t)(s + 1u);
    jam_vos |= (uint8_t)(jam_lock << k);                /* (V10.8.2: over the take, the player's; jam_poll: the take's) */
    RING_PUBLISH();
    fm1_irq_off();
    if (song.playing) jam_rdy |= (uint8_t)(1u << k);
    else jam_in(&trk[k], k);
    fm1_irq_on();
    return 0;
}

/* the music now into section s: RAM at once (playing too), flash once stopped (song_poll) */
static void section_store(uint32_t s)
{
    char b[8] = "A";
    s &= 3u;
    dice_secs &= (uint8_t)~(1u << s);                 /* (0.6.81) a STORE: the section yours (DICE ALL marks its own after) */
    b[0] = (char)('A' + s);
    project_capture(&proj_scratch);
    if (!proj_pack(&proj_wire, &proj_scratch)) {        /* (0.6: packed aside first, the section kept if it does not fit) */
        proj_err();
        return;
    }
    proj_wire_gen++;
    memcpy(&proj_slot[s], &proj_wire, sizeof proj_slot[s]);
    live.cur = (int8_t)s;
    live.dirty |= (uint8_t)(1u << s);
    ui_say("STORED ", b);
}
/* RECALL: the section playing back as it was stored */
static void section_recall(void)
{
    if (live.cur < 0) {
        ui_message("NO PART YET");
        return;
    }
    if (!section_jump((uint32_t)live.cur))
        str_cpy(ui.msg2, "RECALL", sizeof ui.msg2);
}
/* a quick chain of n sections (n >= 2), each its longest pattern's bars, round and round */
static void quick_chain(const uint8_t *s, uint32_t n)
{
    uint32_t i;
    if (chain.running && !chain.loop) {
        ui_message("STOP THE SONG FIRST");
        return;
    }
    for (i = 0; i < n; i++)
        if (!project_used(s[i])) {
            char b[8] = "A";
            b[0] = (char)('A' + (s[i] & 3u));
            ui_say(b, " IS EMPTY");
            return;
        }
    chain_cut();
    jam_lock = 0;                                       /* (V10.8.1: a quick chain plays, not the JAM take) */
    chain_defaults(&chain.config);
    chain.config.count = (uint8_t)n;
    for (i = 0; i < n; i++) {
        chain.config.row[i].slot = (uint8_t)(s[i] & 3u);
        chain.config.row[i].bars = (uint8_t)quick_bars(s[i]);
    }
    chain.loop = 1;
    chain.kept = 0;
    chain.ended = 0;
    if (song_stage(chain.config.row[0].slot, 0))
        return;
    RING_PUBLISH();
    if (song.playing)
        chain.armed_bar = 1;                          /* on the next bar */
    else {
        chain.armed = 1;                              /* from the top, now */
        transport_req = 1;
    }
    ui_message("QUICK CHAIN");
}
/* (V10.8) the songs' JAM takes in flash (OBJ_JAM: NSONG takes of JAM_N events), the current one in live.jam */
#ifndef OBJ_JAM
#define OBJ_JAM 65u                            /* (storage.c's) */
#endif
static void jam_load(void)
{
    uint8_t *b = (uint8_t *)&proj_scratch;
    memset(live.jam, 0, sizeof live.jam);
#if FELUCCA_FLASH
    if (st_load(OBJ_JAM, b, NSONG * sizeof live.jam) == (int)(NSONG * sizeof live.jam))
        memcpy(live.jam, b + song_cur % NSONG * sizeof live.jam, sizeof live.jam);
#else
    (void)b;
#endif
    jam_lock = 0;
    jam_undo = 0;
}
static void jam_store(void)
{
#if FELUCCA_FLASH
    uint8_t *b = (uint8_t *)&proj_scratch;
    if (st_load(OBJ_JAM, b, NSONG * sizeof live.jam) != (int)(NSONG * sizeof live.jam))
        memset(b, 0, NSONG * sizeof live.jam);
    memcpy(b + song_cur % NSONG * sizeof live.jam, live.jam, sizeof live.jam);
    (void)st_save(OBJ_JAM, b, NSONG * sizeof live.jam);
#endif
    jam_undo = 0;
}
static uint32_t jam_end(void)                           /* (V10.8.2) the take's END index (none: its events' count) */
{
    uint32_t i;
    for (i = 0; i < JAM_N && live.jam[i] && JE_K(live.jam[i]) != 7u; i++) {}
    return i;
}
static void jam_sort(uint32_t n)                        /* the take's first n events in time order (stable) */
{
    uint32_t i, j;
    uint16_t e;
    for (i = 1; i < n; i++) {
        e = live.jam[i];
        for (j = i; j && JE_POS(live.jam[j - 1u]) > JE_POS(e); j--) live.jam[j] = live.jam[j - 1u];
        live.jam[j] = e;
    }
}
/* (V10.8.2) a track's round ended: its old changes out, the round's in (where it crossed the take's start: its
 * sequence there at 0), in time order, END last; OCT- undoes it (the take as stored) until it is stored */
static void jam_round_end(void)
{
    uint32_t K, i, n, w = 0, s0 = 8u, p;
    uint16_t end = live.jam[jam_e];
    fm1_irq_off(); K = (jam_tr & 7u) - 1u; jam_tr = 0; n = live.nrec; live.nrec = 0; fm1_irq_on();
    for (i = jam_e + 1u; i < n; i++) {
        p = JE_POS(live.jam[i]);
        if (!p) s0 = 9u;
        else if (p >= jam_r0 && s0 != 9u) s0 = JE_S(live.jam[i]);
    }
    for (i = 0; i < jam_e; i++)
        if (JE_K(live.jam[i]) != K) live.jam[w++] = live.jam[i];
    if (jam_r0 && s0 < 4u) live.jam[w++] = JAM_EV(0u, K, s0);
    for (i = jam_e + 1u; i < n; i++) live.jam[w++] = live.jam[i];
    jam_sort(w);
    live.jam[w++] = end;
    memset(&live.jam[w], 0, (JAM_N - w) * sizeof live.jam[0]);
    jam_vos &= (uint8_t)~(1u << K);                     /* (the player's is the take's now) */
    jam_undo = 1;
    live.rec_done = 1;
    ui_message("TRACK DONE: OCT- UNDO");
}
static uint32_t jam_len(void)                           /* the take's length in 16ths (its END), 0 none */
{
    uint32_t i;
    for (i = 0; i < JAM_N && live.jam[i]; i++)
        if (JE_K(live.jam[i]) == 7u) return JE_POS(live.jam[i]);
    return 0;
}
/* LOCK: each track's next change of the take staged ahead (in on its 16th: seq.c jam_step); stopped, the take's start */
static void jam_poll(void)
{
    uint32_t len = jam_len(), k, i, g, rel, best, bs, p, s;
    if (!jam_lock && jam_tr) {                          /* (V10.8.2) LOCK off: a track's round cut */
        fm1_irq_off(); srec_stop(); fm1_irq_on();
    }
    if (!jam_lock || !len || live.srec)
        return;
    if (!song.playing) {
        jam_base = 0;
        for (i = 0; i < JAM_N && live.jam[i] && !JE_POS(live.jam[i]); i++) {   /* (the tracks at the start) */
            k = JE_K(live.jam[i]);
            if (k < NTRK && !clip_go[k] && jam_cur(k) != JE_S(live.jam[i])) jam_switch(k, JE_S(live.jam[i]));
        }
        jam_vos = 0;                                    /* (the take's, not the player's) */
        return;
    }
    g = gl_step_at(0);
    if (jam_tr && !(jam_tr & 8u) && g >= jam_t0) {     /* (V10.8.2) LOCK's REC: one track, from its bar, a round */
        for (k = jam_base; g >= k + jam_tl; k += jam_tl) {}
        jam_r0 = (uint16_t)(g - k);
        i = (jam_tr & 7u) - 1u;
        fm1_irq_off();
        jam_t0 = k;
        jam_add(JAM_EV(jam_r0, i, jam_cur(i)));
        jam_tr |= 8u;
        fm1_irq_on();
    }
    if ((jam_tr & 8u) && g - jam_t0 >= jam_r0 + (uint32_t)jam_tl)
        jam_round_end();                                /* (V10.8.2) a track's round done */
    while (g >= jam_base + len) jam_base += len;        /* (round and round) */
    rel = g - jam_base;
    for (k = 0; k < NTRK; k++) {
        if (clip_go[k] || ((jam_rdy >> k) & 1u) || ((jam_tr & 8u) && (jam_tr & 7u) == k + 1u))
            continue;                                   /* (one staged at a time: the player's or the take's; a track
                                                         * recorded again: the player's) */
        best = 0xFFFFFFFFu; bs = 0;
        for (i = 0; i < JAM_N && live.jam[i] && JE_K(live.jam[i]) != 7u; i++) {
            if (JE_K(live.jam[i]) != k) continue;
            p = JE_POS(live.jam[i]);
            p = p > rel ? p : p + len;                  /* (past this round: the next) */
            if (p < best) { best = p; bs = JE_S(live.jam[i]); }
        }
        if (best == 0xFFFFFFFFu || bs == jam_cur(k) || best - rel > 32u)
            continue;                                   /* (staged two bars ahead at most) */
        s = bs;
        jam_switch(k, s);
        jam_vos &= (uint8_t)~(1u << k);
        jam_at[k] = jam_base + best + 1u;
    }
}
/* (V10.8.2, the owner's mock-up D3) TAKE: the JAM take's changes edited, one at a time (take_i: KNOB 1's); kept in
 * flash when the page is left, stopped (song_poll) */
static int take_busy(void)
{
    if (jam_tr || live.srec || !jam_len()) { ui_message("NO TAKE"); return 1; }
    return 0;
}
static void take_pick(uint16_t v)                       /* the change v picked again, after the sort */
{
    uint32_t i;
    for (i = 0; i < JAM_N - 1u && live.jam[i] != v; i++) {}
    take_i = (uint8_t)i;
    live.rec_done = 1;
}
static void take_knob(uint32_t slot, int32_t steps)
{
    uint32_t e = jam_end(), len = jam_len(), p;
    uint16_t v;
    if (take_busy()) return;
    if (take_i >= e) take_i = (uint8_t)(e - 1u);
    v = live.jam[take_i];
    p = JE_POS(v);
    if (slot == 0u) {
        take_i = (uint8_t)clamp((int32_t)take_i + steps, 0, (int32_t)e - 1);
    } else if (slot == 1u) {                            /* when: a 16th a detent (each track's start stays) */
        if (!p) return;                                 /* (each track's start stays) */
        live.jam[take_i] = JAM_EV((uint32_t)clamp((int32_t)p + steps, 1, (int32_t)len - 1), JE_K(v), JE_S(v));
        v = live.jam[take_i];
        jam_sort(e);
        take_pick(v);
    } else if (slot == 2u) {                            /* its sequence */
        live.jam[take_i] = JAM_EV(p, JE_K(v), (uint32_t)clamp((int32_t)JE_S(v) + steps, 0, 3));
        live.rec_done = 1;
    } else {                                            /* the take's bars: shorter, what falls past its end goes */
        uint32_t i;
        p = (uint32_t)clamp((int32_t)(len >> 4) + steps, 1, 127) << 4;
        for (i = 0; i < e && JE_POS(live.jam[i]) < p; i++) {}
        live.jam[i] = JAM_EV(p, 7u, 0u);
        memset(&live.jam[i + 1u], 0, (JAM_N - 1u - i) * sizeof live.jam[0]);
        if (take_i >= i) take_i = (uint8_t)(i - 1u);
        live.rec_done = 1;
    }
}
static void take_new(void)                              /* OCT+: a change a beat after the one picked, the next sequence */
{
    uint32_t e = jam_end(), len = jam_len(), p;
    uint16_t v;
    if (take_busy()) return;
    if (e >= JAM_N - 2u) { ui_message("TAKE FULL"); return; }
    if (take_i >= e) take_i = (uint8_t)(e - 1u);
    v = live.jam[take_i];
    p = JE_POS(v) + 4u;
    live.jam[e + 1u] = live.jam[e];
    live.jam[e] = v = JAM_EV(p < len ? p : len - 1u, JE_K(v), (JE_S(v) + 1u) & 3u);
    jam_sort(e + 1u);
    take_pick(v);
}
static void jam_undo_round(void)                        /* (V10.8.2) JAM's OCT- after a track's round: the take stored */
{
    uint8_t l = jam_lock;
    jam_load();
    jam_lock = l;
    live.rec_done = 0;
    ui_message("ROUND UNDONE");
}
static void take_del(void)                              /* OCT-: the change picked out */
{
    uint32_t e = jam_end(), i;
    if (take_busy() || take_i >= e) return;
    if (!JE_POS(live.jam[take_i])) return;
    for (i = take_i; i < JAM_N - 1u; i++) live.jam[i] = live.jam[i + 1u];
    live.jam[JAM_N - 1u] = 0;
    if (take_i >= e - 1u) take_i--;
    live.rec_done = 1;
}
/* SONG REC on / off */
static void song_rec_toggle(void)
{
    if (chain.running && !chain.loop) {
        ui_message("STOP THE SONG FIRST");
        return;
    }
    if (jam_lock && jam_len() && !live.srec) {          /* (V10.8.2) LOCK: REC the track picked again, a round */
        uint32_t e = jam_end();
        fm1_irq_off();
        if (jam_tr) srec_stop();
        else if (e < JAM_N - 3u) {
            jam_e = (uint8_t)e; jam_tl = (uint16_t)jam_len(); live.nrec = e + 1u;
            jam_t0 = song.playing ? (gl_step_at(0) | 15u) + 1u : 0u;   /* (from the next bar) */
            jam_tr = (uint8_t)(song.sel % NTRK + 1u);
        }
        fm1_irq_on();
        ui_message(jam_tr ? "REC TRACK" : "REC OFF");
        return;
    }
    fm1_irq_off();
    if (live.srec)
        srec_stop();
    else
        live.srec = 1;
    fm1_irq_on();
    if (live.srec) {
        jam_lock = 0;                                   /* (V10.8: a new take, the old one's LOCK off) */
        ui_message(song.playing ? "JAM REC NEXT BAR" : "JAM REC: PLAY");
    }
}

/* the current song's stored sections into flash: 0 done (or no flash) */
static int song_flush(void)
{
    uint32_t k;
    if (song_draft) return 0;
#if FELUCCA_FLASH
    if (!flash_ok)
        return 0;
    for (k = 0; k < 4u; k++)
        if (((live.dirty >> k) & 1u) && st_save(proj_obj(k), &proj_slot[k], sizeof proj_slot[k]))
            return 1;
#else
    (void)k;
#endif
    live.dirty = 0;
    return 0;
}
/* (JIANT) the index changed outside the rows (a scene): into flash as the rows go (song_poll) */
static void song_idx_touch(void)
{
    song_idx_dirty = 1;
    song_idx_t = fm1_ms;
}
/* the index into flash (the songs' rows, the current song): 0 done (or no flash) */
static int song_index_save(void)
{
    if (song_draft) return 0;
    song_idx.rows[song_cur] = chain_config;
    song_idx.cur = song_cur;
#if FELUCCA_FLASH
    if (flash_ok && st_save(OBJ_SONGIDX, &song_idx, sizeof song_idx))
        return 1;
#endif
    song_idx_dirty = 0;
    return 0;
}

/* Commit the old project before borrowing its four RAM buffers. All later
 * generations are drafts until an explicit SAVE; no new audio storage. */
static int song_draft_begin(void)
{
    if (!bt_hw_seg()) { seg_missing(); return 1; }
    if (transport_busy()) { ui_message("STOP TO GENERATE"); return 1; }
#if FELUCCA_FLASH
    if (!flash_ok) { ui_message("NO FLASH"); return 1; }
#endif
    if (!song_draft) {
        if (song_flush() || song_index_save()) { ui_message("SAVE ERROR"); return 1; }
        song_save_target = song_cur;
    }
    song_draft = 1;
    dice_secs = 0;
    snd_held = snd_seen = 0;                          /* (V10.6z8: a throw, the sections' sounds) */
    live.dirty = 0; live.cur = live.req = -1; sec_stage.ready = 0;
    proj_cur = PROJ_NO_SLOT; proj_name[0] = 0;
    memset(proj_slot, 0, sizeof proj_slot);
    proj_wire_gen++;
    return 0;
}

#include "song_save.c"

/* song n (0..NSONG - 1) in, stopped only: the current one kept first, then n's sections and rows; load_a: its
 * section A becomes the music (an empty song: the music stays, a new song to store sections into); else (JIANT 0.6.12,
 * SAVE > PROJECT) the music stays as it is, to be stored into one of n's sections or replaced by one. 0 done */
static int song_switch(uint32_t n, int load_a)
{
    char nm[PROJ_NAME_LEN + 1u];
    uint32_t k;
    if (n >= NSONG || (n == song_cur && !song_draft))
        return 1;
    if (transport_busy()) {
        ui_message(load_a ? "STOP TO CHANGE SONG" : "STOP TO CHANGE PROJECT");
        return 1;
    }
    if (song_draft) {
        song_draft = 0; live.dirty = 0;
        song_index_load();
    } else {
        if (song_flush()) { ui_message("SAVE ERROR"); return 1; }
        song_idx.rows[song_cur] = chain_config;
    }
    /* The original index is restored before opening another saved project. */
    song_cur = (uint8_t)n;
    chain_config = song_idx.rows[n];
    jam_load();                                         /* (V10.8) its JAM take */
    if (song_index_save()) {                          /* (failed: written again by song_poll) */
        song_idx_dirty = 1;
        song_idx_t = fm1_ms;
    }
    for (k = 0; k < 4u; k++) {
        memset(proj_slot[k].raw, 0, 4);
#if FELUCCA_FLASH
        if (flash_ok)
            proj_fetch(k);
#endif
    }
    ui.song_row = 0;
    live.cur = -1;
    live.req = -1;
    if (load_a && project_used(0))
        (void)section_load(0);
    else
        proj_cur = PROJ_NO_SLOT;
    ui.force = 1;
    if (load_a) {
        song_mode_restore();
        song_name(nm);
        ui_say(nm, "");
    } else {                                          /* PROJECT n, then its section A's name */
        char b[12] = "PROJECT  ";
        b[8] = (char)('1' + n);
        if (!project_name(0, nm) || !nm[0]) b[9] = 0;
        ui_say(b, b[9] ? nm : "");
    }
    return 0;
}
static int song_select(uint32_t n) { return song_switch(n, 1); }   /* (the song layer's KNOB 1) */
static int song_open(uint32_t n) { return song_switch(n, 0); }     /* (SAVE > PROJECT's KNOB 1) */

/* Main loop, each pass: the next section staged; the music back after the song; SONG REC's rows; stored sections
 * into flash once stopped */
static void song_poll(void)
{
    uint32_t k;
    if (chain.applied) {
        chain.applied = 0;
        clip_seen();                                  /* (V10.6s) CLIPS: what sounds now */
        for (k = 0; k < NTRK; k++)
            fm6_adopt(k);                             /* (an FM6 track's SLOT: F n or OWN) */
        sync_reload = 1;
        ui.force = 1;
    }
    if (chain.running && !sec_stage.ready) {
        uint32_t next = chain.row + !chain.hold;      /* (V10.6o: held, the row again) */
        if (next >= chain.config.count && (chain.loop || SONG_END(&chain.config) == SONG_FOREVER ||
                                           (SONG_END(&chain.config) >= 2u && chain.pass + 1u < SONG_END(&chain.config))))
            next = 0;                                 /* (0.6.22: the song's END: round again) */
        if (next < chain.config.count)
            (void)song_stage(chain.config.row[next].slot, next);
    }
    if (live.rec_done && !song.playing && (ui.home || !SONG_TAKE(cur_page()))) {   /* (V10.8) the JAM take into flash,
                                                         * stopped (V10.8.2: TAKE's edits when it is left) */
        live.rec_done = 0;
        jam_store();
        ui_message("JAM TAKE KEPT");
    }
    jam_poll();
    if (chain.ended && !song.playing) {
        uint8_t cur = proj_cur;
        char name[PROJ_NAME_LEN + 1u];
        uint32_t held = 0;
        chain.ended = 0;
        str_cpy(name, proj_name, sizeof name);
        for (k = 0; k <= NTRK; k++)                   /* (V10.6z8) the sounds the player changed (the last section's */
            held |= snd_own(k) << k;                  /* changes too: motion_end put the sounds' own values back) */
        if (held) {                                   /* (V10.7.6) the music as the song left it, aside */
            project_capture(&proj_scratch);
            proj_wire_gen++;
        }
        if (chain.kept && (!held || proj_pack(&proj_wire, &proj_scratch)) &&
            proj_import(&proj_scratch, &song_keep, sizeof song_keep) && !project_restore_runtime(&proj_scratch)) {
            proj_cur = cur;                           /* (the music before the song: its slot and name) */
            live.cur = (int8_t)((int32_t)(chain.kept >> 1) - 1);
            str_cpy(proj_name, name, PROJ_NAME_LEN + 1u);
            ui_message("SONG END");
            if (held && proj_import(&proj_scratch, &proj_wire, sizeof proj_wire)) {   /* (V10.7.6) then the held */
                if (!(held >> NTRK & 1u))             /* sounds over it (engine, preset, sound, FM6 patch: sound_put; */
                    memcpy(proj_scratch.dx, dx_kit, sizeof proj_scratch.dx);   /* the kit when held): nothing else lost */
                for (k = 0; k < NTRK; k++)
                    if (held >> k & 1u)
                        sound_put(&proj_scratch, k);
                memcpy(dx_kit, proj_scratch.dx, sizeof dx_kit);
                snd_held = (uint8_t)held;             /* (PLAY again: still the player's) */
            }
        }
    }
    if (song_draft) return;
    chain_config.rsv[2] = SONG_MODE_TAG | (live.mode ? 1u : 0u);
    if (memcmp(&chain_config, &song_idx.rows[song_cur], sizeof chain_config)) {   /* the rows or PLAY mode changed */
        song_idx.rows[song_cur] = chain_config;
        song_idx_dirty = 1;
        song_idx_t = fm1_ms;
    }
#if FELUCCA_FLASH
    if (song_idx_dirty && flash_ok && !transport_busy() && (uint32_t)(fm1_ms - song_idx_t) >= 2000u &&
        song_index_save())
        song_idx_t = fm1_ms;                          /* (failed: tried again later) */
    if (live.dirty && flash_ok && !transport_busy())
        for (k = 0; k < 4u; k++)
            if (((live.dirty >> k) & 1u) && !st_save(proj_obj(k), &proj_slot[k], sizeof proj_slot[k]))
                live.dirty &= (uint8_t)~(1u << k);    /* (a failed write stays dirty: tried again) */
#else
    live.dirty = 0;
    song_idx_dirty = 0;
#endif
}

