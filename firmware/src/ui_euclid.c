/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT V10.6: SEQ > EUCLID and MODES, a pattern generator in MUTATE's place. Included by ui_graph.c.
 * EUCLID: KNOB 1 HITS, 2 STEPS, 3 ROTATE spread the hits as evenly as the steps allow (the euclidean rhythm); KNOB 4 a
 * DRUM track's LANE (each of its eight sounds its own HITS / STEPS / ROTATE, the pattern their least common length up to
 * 64), a melodic track's VARY. MODES: four amounts that add to what is generated, 0 none:
 *   DRUM    ACCENT (a second euclid of accents), GHOST (soft hits between), RATCHET (x2 / x4 hits), VARY
 *   melodic MOTION (how far the melody moves), HARMONY (chords of the scale on strong beats), OCTAVE, SLIDE
 * VARY loosens the even grid: some hits move a step (towards the offbeat, more in the second half: it answers the
 * first), a pickup before a strong beat, a hit left out; on a melodic track notes held over the gap after them.
 * A melodic track's notes: its SCL (ROOT, SCALE) when one is set; else the key the other tracks' notes fit best (the
 * screen says FROM Tn); else a scale and root of the seed's (NEW). Turning a knob writes the whole pattern again (the
 * steps of the track; its automation stays); a new gesture after two seconds is an UNDO point. OCT+ a new variation
 * (the seed), OCT- the one before. The settings are the session's (RAM), the pattern is the track's as any other */
#define EU_M 4u
typedef struct { uint8_t h, n, r; } eul_t;
typedef struct { uint8_t on, lane, vary, src, root, ctr; uint16_t mask, seed, prev; uint8_t m[EU_M]; eul_t l[NLANE]; } eu_t;
static eu_t eu[NTRK];
static uint32_t eu_s, eu_ms;
enum { EK_HITS, EK_STEPS, EK_ROT, EK_K4, EK_M0, EK_N = EK_M0 + EU_M };

static uint32_t eu_rnd(void) { eu_s ^= eu_s << 13; eu_s ^= eu_s >> 17; eu_s ^= eu_s << 5; return eu_s; }
static uint32_t eu_ch(uint32_t pct) { return eu_rnd() % 100u < pct; }
static uint32_t eu_hit(uint32_t i, const eul_t *l) { uint32_t n = l->n ? l->n : 1u, j = (i + n - l->r % n) % n; return l->h && (j * l->h) % n < l->h; }

/* track k's settings, made the first time (a kit's four usual lanes; a melody of five in sixteen) */
static eu_t *eu_of(uint32_t k)
{
    static const eul_t KIT[NLANE] = {{4, 16, 0}, {2, 16, 4}, {0, 16, 0}, {8, 16, 2}, {2, 16, 6}, {0, 16, 0}, {0, 16, 0}, {0, 16, 0}};
    eu_t *e = &eu[k % NTRK];
    if (!e->on) {
        uint32_t i;
        e->on = 1;
        e->seed = e->prev = (uint16_t)(1u + k * 7u);
        e->vary = 40;
        for (i = 0; i < NLANE; i++) e->l[i] = KIT[i];
        if (drum_track(&trk[k])) { e->m[0] = 40; e->m[3] = 30; }
        else { e->l[0].h = 5; e->m[0] = 45; e->m[1] = 20; e->m[2] = 10; e->m[3] = 15; }
    }
    return e;
}

/* VARY on hits on[] of n steps (v 0..100); len[] (0: none) the steps each note holds */
static void eu_vary(uint8_t *on, uint8_t *len, uint32_t n, uint32_t v)
{
    uint32_t i, g;
    for (i = 1; i < n; i++) {
        if (on[i] != 1u) continue;
        if (eu_ch(v * (i >= n / 2u ? 7u : 4u) / 10u)) {   /* a step on (to the offbeat mostly) or back */
            uint32_t j = (i & 1u) ? (eu_ch(50) ? i + 1u : i - 1u) : (eu_ch(60) ? i + 1u : i - 1u);
            if (j > 0u && j < n && !on[j]) { on[i] = 0; on[j] = 2; }
        } else if ((i & 3u) && eu_ch(v * 18u / 100u)) on[i] = 0;
    }
    for (i = 3; i < n; i += 4u) if (!on[i] && eu_ch(v * 22u / 100u)) on[i] = 1;   /* a pickup */
    for (i = 0; len && i < n; i++) {
        len[i] = 1;
        if (!on[i]) continue;
        for (g = 1; i + g < n && !on[i + g] && g < 4u; g++) {}
        if (g > 1u && eu_ch(v * 7u / 10u)) len[i] = (uint8_t)(2u + eu_rnd() % (g - 1u));
    }
}

/* the key of melodic track k: SCL's, else the one the other tracks' notes fit (src 1 + the track with the most), else
 * the seed's (NEW); e->root and e->mask */
static void eu_key(uint32_t k, eu_t *e)
{
    static const uint8_t TRY[7] = {1, 2, 3, 4, 8, 9, 7}, NEWS[6] = {2, 1, 3, 6, 8, 9};   /* MAJ MIN DOR MIX PHRY LYD HARM */
    const track_t *t = &trk[k];
    uint32_t hist[12] = {0}, best = 0, most = 0, tot = 0, j, i, r, c;
    int32_t sc, top = -0x7FFFFFFF;
    if (t->p[P_SCALE] > 0) { e->root = (uint8_t)(t->p[P_ROOT] % 12); e->mask = (uint16_t)scale_mask(t); e->src = 0; return; }
    for (j = 0; j < trk_live && j < NTRK; j++) {
        uint32_t n = 0, len = (uint32_t)clamp(trk[j].p[P_SLEN], 1, NSTEP);
        if (j == k || drum_track(&trk[j])) continue;
        for (i = 0; i < len; i++) {
            const step_t *s = &trk[j].step[i];
            if (s->time != ST_NOTE) continue;
            for (c = 0; c < s->n && c < 4u; c++, n++) hist[s->note[c] % 12u] += (i & 3u) ? 1u : 2u;
        }
        if (n > most) { most = n; best = j; }
        tot += n;
    }
    if (tot >= 4u) {
        for (r = 0; r < 12u; r++)
            for (i = 0; i < 7u; i++) {
                uint32_t m = SCALE_MASK[TRY[i]];
                for (sc = (int32_t)hist[r], c = 0; c < 12u; c++)
                    sc += (m >> ((c + 12u - r) % 12u)) & 1u ? 2 * (int32_t)hist[c] : -4 * (int32_t)hist[c];
                if (sc > top) { top = sc; e->root = (uint8_t)r; e->mask = (uint16_t)m; }
            }
        e->src = (uint8_t)(1u + best);
        return;
    }
    eu_s = e->seed * 2654435761u + 11u;
    e->root = (uint8_t)(eu_rnd() % 12u);
    e->mask = (uint16_t)SCALE_MASK[NEWS[eu_rnd() % 6u]];
    e->src = 7;
}

/* scale degree d (any integer) of the key from root pitch base: its note */
static int32_t eu_deg(const uint8_t *pc, uint32_t np, int32_t base, int32_t d)
{
    int32_t o = d >= 0 ? d / (int32_t)np : -((-d + (int32_t)np - 1) / (int32_t)np);
    return base + 12 * o + pc[d - o * (int32_t)np];
}

static void eu_drums(track_t *t, eu_t *e)
{
    uint8_t on[64];
    uint32_t l, i, len = 1, n;
    for (l = 0; l < NLANE; l++) {                        /* the pattern: the lanes' least common length, up to 64 */
        uint32_t a = len, b = e->l[l].n;
        if (!e->l[l].h) continue;
        while (b) { uint32_t x = a % b; a = b; b = x; }
        len = len / a * e->l[l].n > 64u ? 64u : len / a * e->l[l].n;
    }
    if (len < 2u) len = 16;
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    t->p[P_SLEN] = (int16_t)len;
    for (l = 0; l < NLANE; l++) {
        const eul_t *L = &e->l[l];
        eul_t acc = {(uint8_t)(L->h * e->m[0] / 100u + (e->m[0] && L->h ? 1u : 0u)), L->n, (uint8_t)(L->r + 1u)};
        if (!L->h || !(n = L->n)) continue;
        eu_s = e->seed * 2654435761u + l * 97u + 1u;
        for (i = 0; i < n; i++) on[i] = (uint8_t)eu_hit(i, L);
        eu_vary(on, 0, n, l ? e->m[3] : e->m[3] / 2u);
        for (i = 0; i < len; i++) {
            if (on[i % n]) {
                grid_hit(t, i, l, 1);
                if (e->m[0] && eu_hit(i % n, &acc)) grid_acc(t, i, l, 1);
                if (eu_ch(e->m[2] / 2u)) grid_hit_set(t, i, l, 1u + (eu_ch(70) ? LM_X2 : LM_X4));
            } else if (l && eu_ch(e->m[1] * 35u / 100u)) {
                grid_hit_set(t, i, l, 1u + LM_GHOST);
            }
        }
    }
}

static void eu_melody(uint32_t k, track_t *t, eu_t *e)
{
    uint8_t on[64], ln[64], pc[12];
    uint32_t n = e->l[0].n ? e->l[0].n : 16u, i, c, np = 0, g = 0;
    int32_t ctr = 0, base, deg = 0, mo = e->m[0], ha = e->m[1];
    for (i = 0; !e->ctr && i < (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP); i++)   /* the register: the notes it had at the
                                                         * first turn (none: C4), kept */
        if (t->step[i].time == ST_NOTE && t->step[i].n) { ctr += t->step[i].note[0]; g++; }
    if (!e->ctr) e->ctr = (uint8_t)clamp(g ? ctr / (int32_t)g : 60, 36, 84);
    ctr = e->ctr;
    eu_key(k, e);
    for (c = 0; c < 12u; c++)
        if ((e->mask >> c) & 1u) pc[np++] = (uint8_t)c;
    if (!np) { pc[0] = 0; np = 1; }
    base = ctr - ((ctr - e->root + 120) % 12);           /* the root at or below the middle */
    for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
    t->p[P_SLEN] = (int16_t)n;
    eu_s = e->seed * 2654435761u + k * 131u + 7u;
    for (i = 0; i < n; i++) on[i] = (uint8_t)eu_hit(i, &e->l[0]);
    eu_vary(on, ln, n, e->vary);
    for (i = 0; i < n; i++) {
        step_t *s = &t->step[i];
        int32_t note, sh = 0;
        if (!on[i]) continue;
        if (i) {
            int32_t span = mo * 5 / 100, d = span ? (int32_t)(eu_rnd() % (uint32_t)(2 * span + 1)) - span : 0;
            if (!(i & 3u) && eu_ch(50)) d = -deg + (eu_ch(50) ? 0 : (np >= 7u ? 4 : 3));   /* home or the fifth */
            deg = clamp(deg + d, -3, 9);
        }
        if (eu_ch(e->m[2] * 6u / 10u)) sh = eu_ch(50) ? 12 : -12;
        note = clamp(eu_deg(pc, np, base, deg) + sh, 24, 108);
        s->time = ST_NOTE;
        s->vel = 100;
        s->note[0] = (uint8_t)note;
        s->n = 1;
        if (!(i & 3u) && eu_ch((uint32_t)ha)) {          /* a chord of the scale on a strong beat */
            s->note[s->n++] = (uint8_t)clamp(eu_deg(pc, np, base, deg + 2) + sh, 24, 108);
            if (ha > 50) s->note[s->n++] = (uint8_t)clamp(eu_deg(pc, np, base, deg + 4) + sh, 24, 108);
        }
        if (eu_ch(e->m[3] * 7u / 10u)) s->flags |= SF_SLIDE;
        for (c = 1; c < ln[i] && i + c < n; c++) {       /* held over the gap */
            t->step[i + c].time = ST_TIE;
            t->step[i + c].n = 0;
        }
    }
}

/* the pattern of track k written again from its settings */
static void eu_gen(uint32_t k)
{
    track_t *t = &trk[k % NTRK];
    eu_t *e = eu_of(k);
    if (fm1_ms - eu_ms > 2000u) undo_mark();             /* (a new gesture: an UNDO point) */
    eu_ms = fm1_ms;
    load_begin(t, LOAD_PAT);
    if (drum_track(t)) eu_drums(t, e); else eu_melody(k, t, e);
    load_end(t);
    pat_sig[k % NTRK] = ~steps_sig(t);                   /* (the user's steps now) */
    ui.force = 1;
}

/* the knob views (params.c page_desc: ids ID_EU0 + EK_*): their values and descriptions for the selected track */
static int16_t eu_v[EK_N];
static param_desc_t eu_d;
static const param_desc_t *eu_desc(uint32_t k, int16_t **valp)
{
    static const char *const LN[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    static const char *const MD[2][EU_M] = {{"MOTION", "HARMONY", "OCTAVE", "SLIDE"}, {"ACCENT", "GHOST", "RATCHET", "VARY"}};
    eu_t *e = eu_of(song.sel);
    uint32_t dr = drum_track(TSEL) != 0;
    eul_t *L = &e->l[dr ? e->lane % NLANE : 0];
    k %= EK_N;
    eu_d.fmt = F_INT; eu_d.min = 0; eu_d.def = 0; eu_d.names = 0; eu_d.unit = 0;
    if (k == EK_HITS) { eu_d.label = "HITS"; eu_d.max = L->n; eu_v[k] = L->h; }
    else if (k == EK_STEPS) { eu_d.label = "STEPS"; eu_d.min = 2; eu_d.max = 64; eu_v[k] = L->n; }
    else if (k == EK_ROT) { eu_d.label = "ROTATE"; eu_d.max = L->n > 1u ? L->n - 1 : 0; eu_v[k] = L->r; }
    else if (k == EK_K4 && dr) { eu_d.label = "LANE"; eu_d.fmt = F_ENUM; eu_d.max = NLANE - 1; eu_d.names = LN; eu_v[k] = e->lane; }
    else if (k == EK_K4) { eu_d.label = "VARY"; eu_d.max = 100; eu_v[k] = e->vary; }
    else { eu_d.label = MD[dr][k - EK_M0]; eu_d.max = 100; eu_v[k] = e->m[k - EK_M0]; }
    *valp = &eu_v[k];
    return &eu_d;
}
/* knob k turned to v: the setting, then (not LANE) the pattern again */
static void eu_put(uint32_t k, int32_t v)
{
    eu_t *e = eu_of(song.sel);
    uint32_t dr = drum_track(TSEL) != 0;
    eul_t *L = &e->l[dr ? e->lane % NLANE : 0];
    if (k == EK_K4 && dr) { e->lane = (uint8_t)clamp(v, 0, NLANE - 1); ui.force = 1; return; }
    if (k == EK_HITS) L->h = (uint8_t)clamp(v, 0, L->n);
    else if (k == EK_STEPS) { L->n = (uint8_t)clamp(v, 2, 64); if (L->h > L->n) L->h = L->n; if (L->r >= L->n) L->r = 0; }
    else if (k == EK_ROT) L->r = (uint8_t)clamp(v, 0, L->n - 1);
    else if (k == EK_K4) e->vary = (uint8_t)clamp(v, 0, 100);
    else e->m[(k - EK_M0) % EU_M] = (uint8_t)clamp(v, 0, 100);
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    eu_gen(song.sel);
}
/* OCT+ a new variation, OCT- the one before */
static void eu_oct(int up)
{
    eu_t *e = eu_of(song.sel);
    if (chain_busy()) { ui_message("STOP TO EDIT"); return; }
    if (up) { e->prev = e->seed; e->seed = (uint16_t)(e->seed * 25173u + 13849u); }
    else { uint16_t s = e->seed; e->seed = e->prev; e->prev = s; }
    eu_gen(song.sel);
    ui_message(up ? "NEW VARIATION" : "VARIATION BEFORE");
}

/* ------------------------------------------------------------- the graph --- */
/* the pattern as it is: a ring of its steps (DRUM: the lane picked) at the left; DRUM: the eight lanes as rows (HITS /
 * STEPS) at the right; melodic: the key in the ring and the notes as a roll at the right, where the key came from */
static void graph_euclid(const track_t *t)
{
    static const char *const LN[NLANE] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB"};
    static const char *const NN[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    const eu_t *e = eu_of(song.sel);
    uint32_t dr = drum_track(t) != 0, len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP), lane = e->lane % NLANE, i, n;
    int32_t h = (int32_t)graph_h(), cy = h / 2, cx = 64, rad = h / 2 - 12 < 54 ? h / 2 - 12 : 54;
    uint32_t now = song.playing ? t->seq_idx % len : ~0u;
    char b[12];
    n = dr ? (e->l[lane].n < len ? e->l[lane].n : len) : len;
    for (i = 0; i < n; i++) {                            /* the ring */
        uint32_t a = i * 65536u / n + 49152u;
        int32_t x = cx + ((rad * og_cos(a)) >> 15), y = cy + ((rad * og_sin(a)) >> 15), r;
        const step_t *s = &t->step[i];
        uint32_t hit = dr ? (step_lanes(s) >> lane) & 1u : s->time == ST_NOTE && s->n;
        uint32_t acc = dr ? (step_accents(s) >> lane) & 1u : 0u, gh = dr && hit && step_lmode(s, lane) == LM_GHOST;
        r = !hit ? 1 : gh ? 2 : acc ? 5 : n > 24u ? 3 : 4;
        cv_rrect(x - r, y - r, 2 * r + 1, 2 * r + 1, r, !hit ? T_DIM : gh ? T_MID : dr ? T_ACCENT : T_THEME, T_SURF);
        if (hit && dr && (step_lmode(s, lane) == LM_X2 || step_lmode(s, lane) == LM_X4))
            cv_frame(x - r - 2, y - r - 2, 2 * r + 5, 2 * r + 5, T_ACCENT);
        if (i == now) cv_frame(x - 7, y - 7, 15, 15, T_TEXT);
    }
    if (dr) {
        cv_text_in(cx - 30, cy - 8, 60, &AF_M, LN[lane], T_ACCENT, T_SURF);
        for (i = 0; i < NLANE; i++) {                    /* the lanes */
            int32_t y = 6 + (int32_t)i * (h - 12) / NLANE, rh = (h - 12) / NLANE - 2;
            uint16_t c = i == lane ? T_TEXT : e->l[i].h ? T_MID : T_DIM;
            if (i == lane) cv_rect(136, y, 98, rh, T_RAISE);
            cv_rect(136, y, 3, rh, i == lane ? T_ACCENT : T_DIM);
            cv_text(144, y + (rh - 8) / 2, &AF_X, LN[i], c);
            b[0] = 0; fmt_int(b, e->l[i].h); str_cpy(b + str_len(b), "/", 2); fmt_int(b + str_len(b), e->l[i].n);
            cv_text_r(230, y + (rh - 8) / 2, &AF_X, b, c, i == lane ? T_RAISE : T_SURF);
        }
        return;
    }
    str_cpy(b, NN[e->root % 12u], sizeof b);             /* the key, in the ring: its root, its scale */
    cv_text_in(cx - 30, cy - 14, 60, &AF_M, b, T_TEXT, T_SURF);
    for (i = 1; i < sizeof SCALE_MASK / sizeof SCALE_MASK[0] && SCALE_MASK[i] != e->mask; i++) {}
    if (i < sizeof SCALE_MASK / sizeof SCALE_MASK[0])
        cv_text_in(cx - 30, cy + 6, 60, &AF_X, N_SCALE[i], T_MID, T_SURF);
    {                                                    /* the roll */
        int32_t x0 = 134, y0 = 8, w = 100, rh = h - 30, lo = 127, hi = 0, cw;
        uint32_t c;
        for (i = 0; i < len; i++)
            for (c = 0; t->step[i].time == ST_NOTE && c < t->step[i].n && c < 4u; c++) {
                lo = t->step[i].note[c] < lo ? t->step[i].note[c] : lo;
                hi = t->step[i].note[c] > hi ? t->step[i].note[c] : hi;
            }
        if (hi < lo) { lo = 60; hi = 72; }
        if (hi - lo < 12) { lo -= (12 - (hi - lo)) / 2; hi = lo + 12; }
        cw = w / (int32_t)(len ? len : 1u);
        cw = cw < 1 ? 1 : cw;
        cv_frame(x0 - 1, y0 - 1, w + 2, rh + 2, T_DIM);
        for (i = 0; i < len && (int32_t)i * cw < w; i++) {
            const step_t *s = &t->step[i];
            uint32_t g = 1;
            if (s->time != ST_NOTE) continue;
            while (i + g < len && t->step[i + g].time == ST_TIE) g++;
            for (c = 0; c < s->n && c < 4u; c++) {
                int32_t y = y0 + rh - 3 - (s->note[c] - lo) * (rh - 4) / (hi - lo);
                cv_rect(x0 + (int32_t)i * cw, y, (int32_t)g * cw - 1 > 1 ? (int32_t)g * cw - 1 : 1, 3, c ? T_ACCENT : T_THEME);
            }
        }
        if (now != ~0u) cv_rect(x0 + (int32_t)now * cw, y0, 1, rh, T_TEXT);
        if (e->src == 0u) str_cpy(b, "SCL", sizeof b);
        else if (e->src == 7u) str_cpy(b, "NEW KEY", sizeof b);
        else { str_cpy(b, "FROM T1", sizeof b); b[6] = (char)('0' + e->src); }
        cv_text_in(x0, y0 + rh + 6, w, &AF_X, b, e->src == 7u ? T_ACCENT : T_MID, T_SURF);
    }
}
