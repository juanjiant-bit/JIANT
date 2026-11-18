/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Stopped editor audition. The main loop publishes one atomic request; only
 * events_block touches voices. No recording, arp, MIDI OUT or transport edits.
 * A newer selection replaces a pending request. A preview never steals a
 * playing key or another part's tail, and owns only the slot ages it started. */
#define PREVIEW_CANCEL 0xFFFFFFFFu
static uint32_t preview_age[NVOICE] __attribute__((section(".pool")));
static uint8_t preview_track __attribute__((section(".pool")));
static uint8_t preview_mask __attribute__((section(".pool")));

static int preview_owns(uint32_t p, uint32_t i)
{
    return p == preview_track && (preview_mask & (1u << i)) &&
           trk[p].v[i].age == preview_age[i];
}
static void preview_end(void)
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (preview_owns(preview_track, i) && trk[preview_track].v[i].active)
            voice_kill(&trk[preview_track].v[i]);
    preview_left = 0;
}
static __attribute__((noinline)) void preview_block(uint32_t n)
{
    uint32_t q = preview_req, p, i, waiting = 0;
    preview_req = 0;
    if (q == PREVIEW_CANCEL || song.playing || song.rec || cin_left || transport_req ||
        panic_all || panic_req || fm1_in.notes || mi_r != mi_w || pf.act ||
        (!q && preview_left && preview_track != song.sel)) {
        preview_end();
        return;
    }
    if (!q) {
        if (preview_left <= n) preview_end();
        else preview_left -= n;
        return;
    }
    preview_end();
    p = (q >> 16) - 1u;
    if (p >= trk_live || p != song.sel || (q & 255u) >= (uint32_t)trk[p].p[P_SLEN] ||
        trk[p].eng_req != trk[p].engine || trk[p].p[P_MUTE]) return;
    /* Wait for our short fades; refuse to compete with any unrelated voice. */
    for (i = 0; i < NTRK * NVOICE; i++)
        if (trk[i / NVOICE].v[i % NVOICE].active) {
            if (!preview_owns(i / NVOICE, i % NVOICE)) return;
            waiting = 1;
        }
    if (waiting) { preview_req = q; return; }
    track_t *t = &trk[p];
    const step_t *s = &t->step[q & 255u];
    uint32_t f = (q >> 8) & 255u, age = vage, vel = s->vel ? s->vel : 96u;
    if (s->time != ST_NOTE) return;
    preview_track = (uint8_t)p;
    preview_mask = 0;
    if (t->engine == ENGI_DRUM) {
        uint32_t lane = f ? f - 1u : 0u;
        if (lane >= NLANE) return;
        if (step_lanes_of(t, s) & (1u << lane))
            trk_note_on(t, DRUM_LANE_NOTE[lane], tvel(t, vel));
    } else {
        for (i = 0; i < s->n && i < 4u; i++)
            if (!f || f == i + 1u) trk_note_on(t, s->note[i], tvel(t, vel));
    }
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && (int32_t)(t->v[i].age - age) > 0) {
            preview_age[i] = t->v[i].age;
            preview_mask |= (uint8_t)(1u << i);
        }
    if (preview_mask) preview_left = FS / 3u;
}
