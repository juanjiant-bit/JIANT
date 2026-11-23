/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT V10.25: SCL > SCENE, the multitimbral mode's page (seq.c scene_key plays it). Included by ui_graph.c.
 * KNOB 1 SCENE OFF / ON (on: the keys play every track with a role), KNOB 2 the TRACK, KNOB 3 its ROLE (KEEP CHORD ARP
 * BASS LEAD), KNOB 4 its CHORD (the chord CHORD and ARP play; OFF: a triad of the scale). A role sets its track up:
 * ARP turns the track's arp on (UP) if it was off, CHORD BASS LEAD turn it off, CHORD makes the voice POLY. A kit
 * keeps its sequence (KEEP): the keys do not reach it. The graph: six columns, each track's role and the notes the
 * last key played makes on it */
static uint8_t sc_trk = 5;                               /* the track KNOB 2 picked (T6) */
static int16_t sc_v[4];
static param_desc_t sc_d;
static const char *const SC_ROLE[SC_N] = {"KEEP", "CHORD", "ARP", "BASS", "LEAD", "DRUM"};
/* (V10.6j) KNOB 4 by the role: BASS LEAD their pattern (seq.c SC_PAT), DRUM its beat's style */
static const char *const SC_PN[3][SC_GEN] = {{"ROOT", "PUMP", "OCTAVE", "R-5", "WALK", "ROLL", "BOUNCE", "ACID"},
                                         {"HOOK", "ARC", "RISE", "STEPS", "SPARSE", "BOUNCE", "CONVRG", "RANDOM"},
                                         {"FOUR", "BREAK", "HALF", "SPARSE"}};   /* (V10.7.63, the owner: GEN gone, confusing) */
static const param_desc_t *sc_desc(uint32_t k, int16_t **valp)
{
    static const char *const ONOFF[9] = {"OFF", "KEYS", "T1", "T2", "T3", "T4", "T5", "T6", "PRESS"};   /* (V10.7.64,
                                                         * the owner: PRESS, the keys on every track at once) */   /* (V10.7.63:
                                                         * who leads the roles: the keys, or a track's notes) */
    static const char *const TN[NTRK] = {"T1", "T2", "T3", "T4", "T5", "T6"};
    static const char *const KR[2] = {"KEEP", "DRUM"};
    track_t *t;
    uint32_t r, dr;
    if (trk_live && sc_trk >= trk_live) sc_trk = (uint8_t)(trk_live - 1u);   /* (V10.7.64: never an asleep track,
                                                         * T6 was the default with four live) */
    t = &trk[sc_trk % NTRK]; r = scn_role[sc_trk % NTRK]; dr = drum_track(t);
    k &= 3u;
    if (k == 3u && r != SC_BASS && r != SC_LEAD && !(dr && r == SC_DRUM)) {   /* the picked track's own CHORD */
        *valp = &t->p[P_CHRD];
        return track_desc(t, P_CHRD);
    }
    sc_d.fmt = F_ENUM; sc_d.min = 0; sc_d.def = 0;
    if (k == 0u) { sc_d.label = "LEADER"; sc_d.max = 8; sc_d.names = ONOFF; sc_v[0] = scn_on; }
    else if (k == 1u) { sc_d.label = "TRACK"; sc_d.max = (int16_t)(trk_live > 0u && trk_live < NTRK ? trk_live - 1u : NTRK - 1u);   /* (V10.7.64:
                                                         * the live tracks only, T5 T6 asleep with four) */
                        sc_d.names = TN; sc_v[1] = sc_trk; }
    else if (k == 2u && dr) { sc_d.label = "ROLE"; sc_d.max = 1; sc_d.names = KR; sc_v[2] = r == SC_DRUM; }   /* (V10.6j) a kit: KEEP, DRUM */
    else if (k == 2u) { sc_d.label = "ROLE"; sc_d.max = SC_DRUM - 1; sc_d.names = SC_ROLE; sc_v[2] = r < SC_DRUM ? r : SC_KEEP; }
    else { sc_d.label = r == SC_DRUM ? "STYLE" : "PATTERN"; sc_d.max = r == SC_DRUM ? SC_DGEN - 1 : SC_GEN - 1;
           sc_d.names = SC_PN[r == SC_DRUM ? 2 : r == SC_LEAD]; sc_v[3] = (int16_t)(scn_pat[sc_trk % NTRK] % (r == SC_DRUM ? SC_DGEN : SC_GEN)); }
    *valp = &sc_v[k];
    return &sc_d;
}
static void sc_setup(uint32_t i)                        /* track i set up for its role */
{
    track_t *t = &trk[i % NTRK];
    uint32_t r = scn_role[i % NTRK];
    if (drum_track(t) || r == SC_KEEP) return;
    if (r == SC_ARP) { if (!t->p[P_AMODE]) t->p[P_AMODE] = AM_UP; }
    else if (r == SC_BASS || r == SC_LEAD) {           /* (V10.6j) the pattern: the arp over the chord's notes */
        const uint8_t *q = SC_PAT[r == SC_LEAD][scn_pat[i % NTRK] & 7u];
        t->p[P_AMODE] = q[1]; t->p[P_ARATE] = q[2]; t->p[P_AOCT] = 1; t->p[P_AGATE] = q[3]; t->p[P_APROB] = q[4];
    }
    else t->p[P_AMODE] = AM_OFF;
    if (r == SC_CHORD) t->p[P_VOICE] = V_POLY;
}
/* (V10.26) the roles diced: one track for each of CHORD ARP BASS LEAD at most (never two doing the same, never two
 * playing chords: the polyphony), the melodic tracks awake only, the rest KEEP; at least two roles */
/* (V10.6n) the scene's key: every melodic track in one mode (its ROOT kept); a track with no scale (CHR) made a
 * chord of the major scale: the scene sounded major whatever was played. sc < 0: CHR tracks only, minor */
static void sc_key(int32_t sc)
{
    static const uint8_t MD[6] = {2, 3, 8, 7, 4, 1}, MW[6] = {40, 20, 10, 5, 10, 15};   /* MIN DOR PHRY HARM MIX MAJ */
    uint32_t i, k = 0, x = rng() % 100u;
    while (k < 5u && x >= MW[k]) x -= MW[k++];
    for (i = 0; i < trk_live && i < NTRK; i++)
        if (!drum_track(&trk[i]) && (sc >= 0 || !trk[i].p[P_SCALE]))
            trk[i].p[P_SCALE] = (int16_t)(sc >= 0 ? MD[k] : 2);
}
static void sc_dice(void)
{
    uint32_t i, j, m[NTRK], nm = 0, r[4] = {SC_CHORD, SC_ARP, SC_BASS, SC_LEAD};
    for (i = 0; i < NTRK; i++) {
        scn_role[i] = SC_KEEP;
        if (i < trk_live && !drum_track(&trk[i])) m[nm++] = i;
    }
    for (i = nm; i > 1u; i--) { j = rng() % i; uint32_t x = m[i - 1u]; m[i - 1u] = m[j]; m[j] = x; }   /* (the tracks shuffled) */
    for (i = 4u; i > 1u; i--) { j = rng() % i; uint32_t x = r[i - 1u]; r[i - 1u] = r[j]; r[j] = x; }   /* (the roles too) */
    j = nm < 4u ? nm : 2u + rng() % 3u;                  /* (2 .. 4 roles, as many tracks as there are) */
    for (i = 0; i < j && i < nm; i++) { scn_role[m[i]] = (uint8_t)r[i]; sc_setup(m[i]); }
    sc_key(1);                                           /* (V10.6n) and a mode, mostly dark */
}
static __attribute__((noinline)) void sc_put(uint32_t k, int32_t v)
{
    uint32_t i, need = 0;
    if (k == 0u) {
        scn_on = (uint8_t)clamp(v, 0, 8);
        scn_frel = 1;                                    /* (V10.7.63: a led scene's notes let go) */
        if (scn_on == 8u) {                              /* (V10.7.64) PRESS: the key's note on every live track, its own
                                                         * sound each; no roles, no dice, the scale as it is */
            ui_message("EVERY TRACK PLAYS THE KEYS");
            return;
        }
        if (scn_on >= 2u && !drum_track(&trk[scn_on - 2u]))
            scn_role[scn_on - 2u] = SC_KEEP;             /* (the leading track plays its own sequence) */
        for (i = 0, need = 0; i < NTRK; i++) need |= scn_role[i];
        if (scn_on && !need) sc_dice();                  /* (all KEEP: a scene diced, so ON plays something) */
        if (scn_on) sc_key(-1);                          /* (V10.6n) no scale: minor, not the major of CHR */
        need = 0;
        for (i = 0; scn_on && i < NTRK; i++) {
            sc_setup(i);
            need |= (uint32_t)(i >= trk_live && scn_role[i] && !drum_track(&trk[i]));
        }
        {
            char b[32] = "SCENE: T1'S NOTES LEAD THE ROLES";
            b[8] = (char)('1' + (scn_on >= 2u ? scn_on - 2u : 0u));
            ui_message(!scn_on ? "SCENE OFF: KEYS PLAY ONE TRACK" : need ? "T5 T6 ASLEEP: MENU > 6 TRACKS" :
                       scn_on == 1u ? "SCENE: KEYS PLAY THE ROLES" : b);
        }
    } else if (k == 1u)
        sc_trk = (uint8_t)clamp(v, 0, (int32_t)(trk_live > 0u && trk_live < NTRK ? trk_live - 1u : NTRK - 1u));
    else if (k == 3u) {                                  /* (V10.6j) BASS LEAD: the pattern; DRUM: the style */
        scn_pat[sc_trk % NTRK] = (uint8_t)clamp(v, 0, SC_GEN - 1);
        sc_setup(sc_trk);
    } else if (k == 2u && drum_track(&trk[sc_trk % NTRK]))
        scn_role[sc_trk % NTRK] = v ? SC_DRUM : SC_KEEP;
    else if (k == 2u) {
        scn_role[sc_trk % NTRK] = (uint8_t)clamp(v, 0, SC_DRUM - 1);
        for (i = 0; v != SC_KEEP && i < NTRK; i++)        /* (V10.26) a role is one track's: another with it goes KEEP */
            if (i != sc_trk && scn_role[i] == (uint8_t)v) {
                char b[16] = "T  ROLE TO KEEP";
                b[1] = (char)('1' + i);
                scn_role[i] = SC_KEEP;
                ui_message(b);
            }
        sc_setup(sc_trk);
    }
    ui.force = 1;
}
/* (V10.8.14, the owner's "teclado y grados") SCL's pictures share a key's names, a chord's name and an octave of keys */
static const char NN12[12][3] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
static const char RN7[7][4] = {"I", "II", "III", "IV", "V", "VI", "VII"};
/* the triad on degree d of the seven notes pc (semitones from ROOT rt) as b (4): its root's name, "-" minor, the degree
 * sign diminished, "+" augmented (the faces have no lowercase) */
static void chord_nm(char *b, const uint8_t *pc, uint32_t d, uint32_t rt)
{
    uint32_t r = pc[d % 7u], th = (pc[(d + 2u) % 7u] + 12u - r) % 12u, fi = (pc[(d + 4u) % 7u] + 12u - r) % 12u;
    str_cpy(b, NN12[(r + rt) % 12u], 4);
    str_cpy(b + str_len(b), fi == 6u ? "\xB0" : fi == 8u ? "+" : th == 3u ? "-" : "", 2);
}
/* an octave of keys from C at y, h high, in x 8..232 as the device's: the notes lit (bit n: note n) in their colour (col
 * 0) or col, their names on the white keys (the black ones' when lit), the root a dot */
static void piano(int32_t y, int32_t h, uint32_t lit, uint32_t rt, uint16_t col)
{
    static const uint8_t WH[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BX[5] = {1, 2, 4, 5, 6};
    uint32_t i;
    int32_t bh = h * 58 / 100;
    for (i = 0; i < 12u; i++) {
        uint32_t blk = i >= 7u, n = blk ? BK[i - 7u] : WH[i], on = lit >> n & 1u;
        int32_t x = blk ? 2 + (int32_t)BX[i - 7u] * 32 - 10 : 8 + (int32_t)i * 32, w = blk ? 19 : 30, kh = blk ? bh : h;
        uint16_t c = on ? (col ? col : note_col(n)) : blk ? 0x10A2u : 0xCE59u;
        cv_rect(x, y, w, kh, c);
        if (!blk || on)
            cv_text_in(x, y + kh - (blk ? 12 : 22), w, &AF_X, NN12[n], on || !blk ? T_INK : T_DIM, c);
        if (n == rt)
            cv_rect(x + w / 2 - 2, y + kh - (blk ? 4 : 8), 4, 4, blk && !on ? 0xFFFFu : 0x0000u);
    }
}
/* (V10.7.70; V10.8.14) SCL > PROG: a card a chord of the progression, its name, its degree and its bar, the one playing
 * lit, under them where the progression is; OFF: the cards dim and what turns it on */
static __attribute__((noinline)) void graph_harm(void)
{
    const track_t *t = trk;
    uint32_t i, m, np = 0, off = !song.g[G_HPROG] || !hm_n, n = off ? 4u : hm_n, now = song.playing && !off ? hm_now() : 9u;
    int32_t w = 224 / (int32_t)n;
    uint8_t pc[12];
    char b[8];
    while (t < trk + trk_live - 1 && drum_track(t)) t++;
    m = scale_mask(t);
    if (__builtin_popcount(m & 0xFFFu) != 7) m = 0xAB5u;   /* (no mode of seven: major's, as seq.c's chords) */
    for (i = 0; i < 12u; i++) if ((m >> i) & 1u) pc[np++] = (uint8_t)i;
    for (i = 0; i < n; i++) {
        int32_t x = 8 + (int32_t)i * w;
        uint32_t d = off ? i : (uint32_t)hm_dg[i] % 7u;
        uint16_t c = i == now ? T_ACCENT : T_RAISE, ink = i == now ? T_INK : off ? T_DIM : T_TEXT;
        cv_rect(x, 2, w - 6, 72, c);
        if (off) str_cpy(b, "-", 2); else chord_nm(b, pc, d, (uint32_t)t->p[P_ROOT]);
        cv_text_in(x, 12, w - 6, text_w(&AF_L, b) <= w - 8 && large_face_has(b) ? &AF_L : &AF_M, b, ink, c);
        if (!off) cv_text_in(x, 44, w - 6, &AF_X, RN7[d], ink, c);
        str_cpy(b, "BAR ", 8); fmt_int(b + 4, (int32_t)i + 1);
        cv_text_in(x, 58, w - 6, &AF_X, b, ink, c);
    }
    cv_rect(8, 82, 218, 3, T_RAISE);
    if (now < n) cv_rect(8, 82, 218 * (int32_t)(now + 1u) / (int32_t)n, 3, T_ACCENT);
    if (off) cv_text(8, 94, &AF_X, "PROG OFF \xB7 KNOB 1 PICKS ONE", T_DIM);
}
/* (V10.8.14) SCL > SCENE: a row a track, its colour's pill, its role, the notes it plays (their range: names and a bar
 * over C2 .. C7), the one picked its role lit; one line: what that role does */
static void graph_scene(void)
{
    static const char *const RD[SC_N] = {"PLAYS ITS OWN SEQUENCE", "HOLDS EACH CHORD", "ARPEGGIATES EACH CHORD",
                                         "A BASS LINE UNDER THE NOTES", "A LINE ABOVE THE NOTES", "A NEW BEAT EACH KEY HELD"};
    uint32_t i, j, n;
    uint8_t o[KB_CH + 1];
    char b[48];
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        int32_t y = 2 + (int32_t)i * 19;
        uint32_t r = drum_track(t) && scn_role[i] != SC_DRUM ? SC_KEEP : scn_role[i], lo = 127, hi = 0;
        const char *rn = scn_on == 8u ? "PRESS" : scn_on >= 2u && i == scn_on - 2u ? "LEADS" : drum_track(t) && r == SC_KEEP ? "KIT" : SC_ROLE[r];
        b[0] = 'T'; b[1] = (char)('1' + i); b[2] = 0;
        cv_rect(4, y, 26, 16, i < trk_live ? trk_col(i) : T_RAISE);
        cv_text_in(4, y + CAP_IN(X, 16), 26, &AF_X, b, i < trk_live ? trk_ink(i) : T_DIM, i < trk_live ? trk_col(i) : T_RAISE);
        cv_text(36, y + 1, &AF_M, rn, i == sc_trk ? T_ACCENT : r == SC_KEEP ? T_DIM : T_TEXT);
        cv_rect(150, y + 6, 84, 5, T_RAISE);
        if (i >= trk_live || r == SC_KEEP || r == SC_DRUM || scn_on == 8u) {
            cv_text(98, y + 4, &AF_X, i >= trk_live ? "ASLEEP" : r == SC_DRUM ? "BEAT" : "OWN SEQ", T_DIM);
            continue;
        }
        n = scn_notes(t, r, scn_on >= 2u ? scn_fnote : last_note, o, 1);
        for (j = 0; j < n; j++) { if (o[j] < lo) lo = o[j]; if (o[j] > hi) hi = o[j]; }
        if (!n) continue;
        str_cpy(b, NN12[lo % 12u], 4); fmt_int(b + str_len(b), (int32_t)lo / 12 - 1);
        str_cpy(b + str_len(b), "-", 2);
        str_cpy(b + str_len(b), NN12[hi % 12u], 4); fmt_int(b + str_len(b), (int32_t)hi / 12 - 1);
        cv_text(98, y + 4, &AF_X, b, T_MID);
        lo = (uint32_t)clamp((int32_t)lo, 36, 96); hi = (uint32_t)clamp((int32_t)hi, 36, 96);
        cv_rect(150 + (int32_t)(lo - 36u) * 84 / 60, y + 6, (int32_t)(hi - lo) * 84 / 60 + 2, 5, trk_col(i));
    }
    str_cpy(b, "T1 ", 48);
    b[1] = (char)('1' + sc_trk % NTRK);
    if (scn_on == 8u) str_cpy(b, "EVERY TRACK PLAYS THE KEYS", 48);
    else if (scn_on >= 2u && sc_trk % NTRK == scn_on - 2u) str_cpy(b + 3, "ITS NOTES LEAD THE OTHERS", 45);
    else str_cpy(b + 3, RD[scn_role[sc_trk % NTRK] % SC_N], 45);
    cv_text_in(4, 120, 232, &AF_X, scn_on ? b : "SCENE OFF: K1 KEYS OR A TRACK", scn_on ? T_TEXT : T_DIM, T_SURF);
}

/* (V10.6j) SCENE's DRUM: key k's beat into the kit, 16 steps (main loop: ui_input.c's poll; the key in the audio side
 * only asks, seq.c scn_dreq). STYLE FOUR BREAK HALF SPARSE; the higher the key in its octave, the busier: hats in 16ths,
 * a clap, an open hat, a fill */
static __attribute__((noinline)) void sc_beat(void)
{
    static const uint16_t KICK[4] = {0x1111, 0x0481, 0x0801, 0x0101}, SNR[4] = {0x1010, 0x1010, 0x0100, 0x1000},
                          HAT[4] = {0x4444, 0x5555, 0x5555, 0x4040};
    uint8_t hit[16], acc[16];
    uint32_t k = scn_dreq, i, j, st, d, x;
    if (!k) return;
    scn_dreq = 0;
    for (i = 0; i < NTRK && !(drum_track(&trk[i]) && scn_role[i] == SC_DRUM); i++) ;
    if (i == NTRK) return;
    st = scn_pat[i] % SC_DGEN; d = (k - 1u) % 12u; x = k * 2654435761u + st * 40503u;
    for (j = 0; j < 16u; j++) {
        uint32_t b = 1u << j, h = 0;
        x = x * 1664525u + 1013904223u;
        if (KICK[st] & b || (d > 6u && st == 1u && j == 7u)) h |= 1u;
        if (d > 1u && SNR[st] & b) h |= 2u;
        if (d > 5u && st == 0u && SNR[0] & b) h |= 4u;
        if (HAT[st] & b || (d > 3u && (x >> 28) < d)) h |= 8u;
        if (d > 7u && j == 14u) h = (h & ~8u) | 16u;
        if (d > 9u && j >= 12u && (j & 1u)) h |= 2u;
        hit[j] = (uint8_t)h; acc[j] = (uint8_t)(j % 4u ? 0u : (h & 1u));
    }
    load_begin(&trk[i], LOAD_PAT);
    load_grid16(&trk[i], hit, acc);
    load_end(&trk[i]);
    ui.force = 1;
}
