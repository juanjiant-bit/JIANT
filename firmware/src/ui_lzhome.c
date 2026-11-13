/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.8.5) HOME's lens (ui_lens.c): its panels in the lens's panel (240 x 140 at y 84), as the approved mock-up
 * (lente_sistema.html, HOME) drew ECO. Included by ui_graph.c after ui_being.c. The beings are JIANT's own (ui_being.c:
 * the same genomes, isotherms, nuclei, the same swelling, exciting and swimming with each track's sound, graph_ecosys's
 * maths); the mock-up gave the chrome around them.
 *   MACROS (V10.7.24: was ECO's picture, ECO and BEING gone) the six tracks' beings swimming free over the whole panel,
 *          each by itself (wandering, bouncing off the walls, a hit kicking it on, swelling with its sound), its number
 *          under it; the selected one larger, its number in the theme's colour (no ring, no grid, no names). The
 *          cytoplasm's granules drift behind them. (0.9.7, the approved variant A "Hoy con carita") each being today's
 *          (the spiky outline of its species, the same spikes as it sounds) in three isotherms, coloured by its own level
 *          (quiet cyan and blue .. loud red, orange, yellow), and where its nuclei were a small face: two eyes that shut on
 *          a hit, a mouth that opens with the level; muted, asleep (a dim outline, its eyes shut). MUTATE: when a mutation
 *          lands the being looks up, its number lights white and one ring in the accent opens. At the top left the
 *          macros' routes counted, a macro turned: where it goes ("M1 T1 CUT+40 .."); LATCH / HOLD LFO at the right
 *   SCOPE  (V10.7.24) the mix's oscilloscope over the whole panel (ui_graph.c graph_scope)
 * Nothing in the audio path: the parts' peaks and the scope the UI already reads. */
#define ECO_N NTRK
static const int16_t ECO_PX[ECO_N] = {44, 120, 196, 64, 146, 206}, ECO_PY[ECO_N] = {34, 28, 40, 84, 78, 86};   /* (where they
                                                         * start, as 0.8.4's HOME) */
static struct {
    int32_t en[ECO_N], pen[ECO_N];                      /* each track's level now (0..256, slewed); (codex/anim) where it
                                                         * was a step before (Q8 px, y << 16 | x: eco_at) */
    int32_t sx[ECO_N], sy[ECO_N], vx[ECO_N], vy[ECO_N];   /* the swim: where it is in the panel and its speed, Q8 px */
    int32_t r[ECO_N];                                   /* its radius now (Q4 px: swollen, pulsing) */
    uint32_t tm[ECO_N];                                 /* each being's time (it runs with its sound) */
    uint32_t f0;                                        /* (codex/anim) the swim's last step (ui.c anim_fr ticks) */
    uint8_t init, top;                                  /* (top: unused since V10.7.23, AIR gone: the RAM's layout kept) */
} eco;
/* (0.9.7, variant A) each being's face and colours, worked out once a drawn frame by eco_step for eco_being: its three
 * isotherms' colours by its level (c0 outer .. c2), its last hit's time (fm1_ms, low 16 bits: the eyes shut 0.1 s), its face's EF_*
 * (fl); init: the hits' times set. (Sized as the 0.9.6 critters were, in their place: the RAM's layout and so the sound
 * code's addressing as they were; EH_N the beings' room, ECO_N used) */
#define EH_N 36u
static struct {
    uint16_t c0[EH_N], c1[EH_N], c2[EH_N], ms[EH_N];
    uint8_t fl[EH_N];
    uint8_t init;
} eco_fc;
enum { EF_SHUT = 1, EF_SLEEP = 2, EF_UP = 4 };
#define eco_ft (eco_fc.ms[ECO_N])                       /* (codex/anim, in the room left: the levels' last look, ticks) */          /* (0.9.7) the face's: eyes shut (a hit), asleep, looking up */
static int eco_off(uint32_t c) { return trk[c].p[P_MUTE] || c >= trk_live; }   /* (muted, or asleep: TRACKS 4) */
static const genome_t *eco_genome(uint32_t c)
{
    uint32_t e = trk_eng(&trk[c]);
    return e == ENGI_DRUM ? &LANE_G_HOME : &GENOME[EGEN[e]];
}
/* track c's features: its genome, its HOME knobs, excited by its level k-fold (ui_being.c) */
static void eco_feat(uint32_t c, bfeat_t *f, int32_t k)
{
    uint8_t ids[4];
    uint32_t v, e = trk_eng(&trk[c]);
    for (v = 0; v < 4u; v++) ids[v] = (uint8_t)ENGINES[e]->knob[v];
    bg_feat(&trk[c], eco_genome(c), ids, f);
    bg_excite(f, eco.en[c], k);
}
/* (JIANT 0.5, as 0.8.4's HOME: graph_ecosys's life) the beings once a drawn frame (dt frames since the last): each
 * track's level (the part's peak, slewed: a hit at once, a quick fall), its time, its size (it pulses, and swells up to
 * ~2.2 x; the selected one larger), its swim over the whole panel (wandering, bouncing off the walls, a hit kicking it
 * on up to ~7 x). Still with ANIM OFF */
/* (0.8.9) the beings' water: the panel; its floor, the numbers' line under it */
static int32_t eco_bot(void) { return (int32_t)graph_h() - 18; }
/* (codex/anim) the swim in fixed steps of ECO_STEP ticks (~11 a second, each its ticks one by one, the wander from the
 * clock's tick: the same path whatever the frame rate), drawn a step behind, between the last two (eco_at): a game's
 * fixed physics and its drawn frames in between. The levels, the size and the faces each drawn frame, by the ticks gone */
#define ECO_STEP 6u
static void eco_at(uint32_t c, int32_t *x, int32_t *y)   /* where being c is drawn now (Q8 px) */
{
    int32_t a = (int32_t)(anim_at - eco.f0 * ANIM_TICK) * 256 / (int32_t)(ECO_STEP * ANIM_TICK);
    a = clamp(a, 0, 256);
    int32_t qx = eco.pen[c] & 0xFFFF, qy = (int32_t)((uint32_t)eco.pen[c] >> 16);
    *x = qx + ((eco.sx[c] - qx) * a >> 8);
    *y = qy + ((eco.sy[c] - qy) * a >> 8);
}
/* .. the swim's steps due, each its ticks (its own: the code before the sound's in the flash the size it was) */
static __attribute__((noinline)) void eco_swim(uint32_t now, int32_t bot)
{
    uint32_t c, k, n;
    if (now - eco.f0 > 4u * ECO_STEP)                   /* (a pause: on from here, no jump) */
        eco.f0 = now - ECO_STEP;
    for (n = eco.f0; now - n >= ECO_STEP; n += ECO_STEP)   /* the swim's steps due, each its ticks */
        for (c = 0; c < ECO_N; c++) {
            int32_t rad = (c == song.sel ? 17 : 12) * (272 + eco.en[c] * 5 / 4) >> 8, sp = 256 + eco.en[c] * 6;   /* (its
                                                         * radius at its breath's fullest: the walls the same whatever
                                                         * frame the step is worked out in) */
            eco.pen[c] = (int32_t)((uint32_t)eco.sy[c] << 16 | (uint32_t)eco.sx[c]);
            for (k = n; k < n + ECO_STEP; k++) {        /* swimming in the panel, bouncing off its walls, wandering */
                eco.vx[c] += bg_sin(k * 211u + c * 16000u) >> 12;
                eco.vy[c] += bg_sin(k * 157u + c * 23000u) >> 12;
                eco.vx[c] = clamp(eco.vx[c], -140, 140);
                eco.vy[c] = clamp(eco.vy[c], -110, 110);
                eco.sx[c] += eco.vx[c] * sp >> 8;
                eco.sy[c] += eco.vy[c] * sp >> 8;
                if (eco.sx[c] < (rad + 8) << 8) { eco.sx[c] = (rad + 8) << 8; eco.vx[c] = -eco.vx[c]; }
                if (eco.sx[c] > (232 - rad) << 8) { eco.sx[c] = (232 - rad) << 8; eco.vx[c] = -eco.vx[c]; }
                if (eco.sy[c] < (rad + 4) << 8) { eco.sy[c] = (rad + 4) << 8; eco.vy[c] = -eco.vy[c]; }
                if (eco.sy[c] > (bot - rad) << 8) { eco.sy[c] = (bot - rad) << 8; eco.vy[c] = -eco.vy[c]; }
            }
        }
    eco.f0 = n;
}
static uint32_t eco_step(void)
{
    uint32_t c, k, now = anim_fr, dt = (uint16_t)((uint16_t)now - eco_ft);
    int32_t bot = eco_bot();                            /* (the panel's floor, the number's line under it: 0.8.4's 104 of
                                                         * 122, the lens's 140 its 122) */
    if (!eco.init) {
        eco.init = 1;
        for (c = 0; c < ECO_N; c++) {
            eco.sx[c] = ECO_PX[c] << 8; eco.sy[c] = ECO_PY[c] << 8;
            eco.pen[c] = (int32_t)((uint32_t)eco.sy[c] << 16 | (uint32_t)eco.sx[c]);
            eco.vx[c] = (c & 1u) ? 90 : -110; eco.vy[c] = (c & 2u) ? 70 : -60;
        }
        eco.f0 = now;
    }
    if (dt > 16u) dt = 16u;                             /* (held under load, ui.c ui_hold, or ANIM OFF: the clock still) */
    for (c = 0; c < ECO_N; c++) {
        track_t *t = &trk[c];
        int32_t v = t->peak * 256 / 7000, r = (c == song.sel ? 17 : 12) * 16, pen = eco.en[c];
        if (v > 256) v = 256;
        if (dt) {                                       /* (once a frame: a redraw in the same frame draws the same) */
            t->peak = 0;
            for (k = 0; k < (((now >> 1) - (eco_ft >> 1)) & 0x7FFFu) && k < 8u; k++)   /* (the fall: a step each two ticks, as
                                                         * the frames drawn at full pace) */
                eco.en[c] = v > eco.en[c] ? v : eco.en[c] + (v - eco.en[c]) / 4;
            if (eco.en[c] - pen > 48)                   /* (0.9.7) a hit: the face's eyes shut for 0.1 s */
                eco_fc.ms[c] = (uint16_t)fm1_ms;
        }
        if (!eco_fc.init || (uint16_t)((uint16_t)fm1_ms - eco_fc.ms[c]) > 1000u)   /* (none yet, or long ago: kept so, the
                                                         * 16 bits never wrapping round to a hit) */
            eco_fc.ms[c] = (uint16_t)(fm1_ms - 1000u);
        {   /* (0.9.7) its isotherms' colours: cyan / blue quiet .. red, orange, yellow loud; the inner ones warmer */
            int32_t h = 23 + eco.en[c] * 118 / 256, st = 38 + eco.en[c] * 13 / 256;
            eco_fc.c0[c] = vk_heat(h);
            eco_fc.c1[c] = vk_heat(h + st);
            eco_fc.c2[c] = vk_heat(h + 2 * st);
        }
        eco_fc.fl[c] = (uint8_t)((((uint16_t)((uint16_t)fm1_ms - eco_fc.ms[c]) < 100u && eco.en[c] > 77 ? EF_SHUT : 0u)));
        if (eco.en[c] > 2) eco.tm[c] += dt * (uint32_t)(200 + eco.en[c] * 5);
        else eco.tm[c] += dt * 60u;                     /* (silent: it still breathes, slowly) */
        r = r * (256 + eco.en[c] * 5 / 4 + (bg_sin(eco.tm[c] / 3u) >> 11)) / 256;
        eco.r[c] = r;
    }
    eco_ft = (uint16_t)now;
    eco_swim(now, bot);
    eco_fc.init = 1;
    return dt;
}
/* (0.9.7, variant A) a small ellipse (Q4: centre, radii at least 0.6 px) in ten lines */
static void eco_ell(int32_t cx, int32_t cy, int32_t rx, int32_t ry, uint16_t c)
{
    uint32_t i;
    rx = rx < 10 ? 10 : rx;
    ry = ry < 10 ? 10 : ry;
    for (i = 0; i <= 10u; i++) {
        uint32_t a = i * 65536u / 10u;
        pl_pt(i, 10u, cx + ((rx * og_cos(a)) >> 15), cy + ((ry * og_sin(a)) >> 15), c, 256);
    }
}
/* .. the face (the mock-up's, the bichos'): at (cx, cy) Q4 of a being of radius r Q4, scaled k (Q8); its mouth open e (0..256),
 * its eyes shut (a hit) or asleep (muted), looking up (MUTATE) */
static void eco_face(int32_t cx, int32_t cy, int32_t r, int32_t k, int32_t e, uint32_t fl)
{
    int32_t rk = r * k >> 8, s = rk >= 19 * 16 ? 3 : 2, ex = rk * 77 >> 8, d;
    int32_t ey = cy - (rk * 31 >> 8), my, mo;
    uint16_t ec = (fl & EF_SLEEP) ? T_DIM : vk_heat(248);
    if (ex < 40) ex = 40;                               /* (2.5 px apart at least) */
    if (fl & EF_UP)                                     /* (MUTATE: it looks up) */
        ey -= rk * 31 >> 8 > 24 ? rk * 31 >> 8 : 24;
    for (d = -1; d <= 1; d += 2) {
        int32_t X = (cx + d * ex + 8) >> 4, Y = (ey + 8) >> 4;
        if (fl & (EF_SHUT | EF_SLEEP))
            cv_rect(X - (s + 1) / 2, Y, s + 1, 1, ec);
        else
            cv_rect(X - s / 2, Y - s / 2, s, s, ec);
    }
    my = ey + (rk * 92 >> 8 > 56 ? rk * 92 >> 8 : 56);
    mo = (e * 333 >> 8 > 256 ? 256 : e * 333 >> 8) * (rk * 97 >> 8) >> 8;   /* (its level x 1.3, at most the whole) */
    if (fl & EF_SLEEP)
        cv_rect(((cx + 8) >> 4) - 1, (my + 8) >> 4, 3, 1, ec);
    else if (fl & EF_UP)
        eco_ell(cx, my + 16, 22, 26, vk_heat(200));
    else if (mo > 21)
        eco_ell(cx, my + mo / 2 - 8, (rk * 26 >> 8) + (mo * 72 >> 8) > 24 ? (rk * 26 >> 8) + (mo * 72 >> 8) : 24, mo / 2, vk_heat(200));
    else {
        int32_t w = ((rk * 31 >> 8) + 8) >> 4;
        if (w < 1) w = 1;
        cv_rect(((cx + 8) >> 4) - w, (my + 8) >> 4, 2 * w + 1, 1, vk_heat(200));
    }
}
/* track c's being at (cx, cy) Q4, radius rq Q4, as HOME draws it (the most excited): (0.9.7, variant A) three isotherms by
 * its own level (outer: its species' spikes), its face; muted: asleep, a dim outline without spikes, its eyes shut. A
 * mutation just landed (mut_flash): it looks up, a ring in the accent opens round it */
static void eco_being(uint32_t c, int32_t cx, int32_t cy, int32_t rq)
{
    static const int16_t SC[3] = {256, 179, 113};
    const genome_t *g = eco_genome(c);
    bfeat_t f;
    int32_t en = eco.en[c], rr = rq + rq * en * 30 / 65536, i;
    uint32_t t = eco.tm[c] + c * 20000u;
    if (eco_off(c)) {
        bfeat_t a = {0, 0, 40, 3, 1, 0, 0, 0};
        uint16_t dc = T_DIM;
        bg_nv_in = 40;
        bg_outline(g, &a, cx, cy, rq, 0, eco.tm[c], dc);
        bg_nv_in = 0;
        eco_face(cx, cy, rq, 230, 0, EF_SLEEP);
        return;
    }
    uint16_t col[3] = {eco_fc.c0[c], eco_fc.c1[c], eco_fc.c2[c]};
    eco_feat(c, &f, 4);
    bg_nv_in = 40;                                      /* (the inner two: 40 vertices) */
    for (i = 0; i < 3; i++)                             /* (their colours by its level: eco_step) */
        bg_outline(g, &f, cx, cy, rr * SC[i] >> 8, !i, t, col[i]);
    bg_nv_in = 0;
    eco_face(cx, cy, rr, 159, en, eco_fc.fl[c]);
}

/* (0.8.5) a gauge, as the mock-up's: an arc of 270 degrees (from 7:30, clockwise) round (cx, cy) px, radius r, w px
 * thick, the value f (0..1000) in vc over the track tc; antialiased lines (og_line), 24 a turn */
static void lz_arc(int32_t cx, int32_t cy, int32_t r, int32_t w, int32_t f, uint16_t tc, uint16_t vc)
{
    int32_t k, i;
    for (k = 0; k < w; k++) {
        int32_t rr = (r - k) * 16, px = 0, py = 0;
        for (i = 0; i <= 18; i++) {                     /* (18 segments over 270 degrees) */
            uint32_t a = 0x6000u + (uint32_t)i * 0xC000u / 18u;   /* (from 135 degrees, y down) */
            int32_t x = cx * 16 + (og_cos(a) * rr >> 15), y = cy * 16 + (og_sin(a) * rr >> 15);
            if (i)
                og_line(px, py, x, y, i * 1000 <= f * 18 ? vc : tc);
            px = x;
            py = y;
        }
    }
}

/* ------------------------------------------------------------------------------------------------ the panels --- */
static uint32_t routes_count(void)                      /* the MOD slots whose SRC is a macro, every track */
{
    uint32_t i, k, n = 0;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < 4u; k++) {
            const int16_t *p = &trk[i].p[P_M1SRC + 3u * k];
            n += p[0] >= MS_M1 && p[0] < MS_M1 + 4 && p[1] && p[2];
        }
    return n;
}
/* ECO: 0.8.4's HOME (graph_ecosys) in the lens's panel: the cytoplasm's granules drifting, the six beings swimming free
 * over the whole panel (0.9.7: variant A, eco_being), each with its number under it (the selected one larger, its number in
 * the theme's colour; a mutation landed, white) */
static void graph_eco(void)
{
    uint32_t c, k, still = ANIM_OFF() != 0;
    int32_t bx[ECO_N], by[ECO_N], bot = eco_bot();
    (void)eco_step();                                   /* (the swim, once a frame) */
    (void)bg_energy();                                  /* (the output's level kept as it was read: once a frame) */
    char b[48];
    for (k = 0; k < 22u; k++) {                         /* the cytoplasm: granules drifting */
        uint32_t h = k * 2654435761u;
        int32_t gx = 10 + (int32_t)((h >> 8) % 220u + anim_fr * (1u + (h >> 28)) / 6u) % 220;   /* (still: where they were) */
        int32_t gy = 6 + (int32_t)(((h >> 16) % (uint32_t)(bot + 6)) + (still ? 0 : (uint32_t)(bg_sin((uint32_t)(anim_fr * 300u + h)) >> 13))) %
                         (uint32_t)(bot + 6);
        cv_rect(gx, gy, 1 + (int32_t)(h >> 31), 1 + (int32_t)(h >> 31), ux_mix(T_SURF, T_THEME, 22 + (int32_t)(h >> 29) * 4));
    }
    for (c = 0; c < ECO_N; c++) {
        eco_at(c, &bx[c], &by[c]);                      /* (codex/anim: between its last two steps) */
        bx[c] >>= 4;
        by[c] >>= 4;
    }
    for (c = 0; c < ECO_N; c++) {
        int32_t rad = eco.r[c] >> 4, px = bx[c] >> 4, py = by[c] >> 4, ny;
        char num[2] = {(char)('1' + c), 0};
        eco_being(c, bx[c], by[c], eco.r[c]);
        ny = clamp(py + rad / 2 + 4, 4, bot);
        cv_text_in(px - 30, ny, 60, &AF_S, num, c == song.sel ? T_THEME : eco_off(c) ? T_DIM : T_MID, T_SURF);
    }
    if (ui.hot_t && ui.hot_col < 4u) {                  /* MACROS: a macro turned, where it goes ("M2 T1 CUT+40 T3 ..",
                                                         * the MACRO layer's list); else the routes counted; LATCH */
        b[0] = 'M';
        b[1] = (char)('1' + ui.hot_col);
        b[2] = ' ';
        if (!macro_routes(ui.hot_col, b + 3, sizeof b - 3u))
            str_cpy(b + 3, "NO ROUTE", sizeof b - 3u);
        cv_clip_text(6, 2, &AF_X, b, T_MID, T_SURF, 176);
    } else {
        fmt_int(b, (int32_t)routes_count());
        str_cpy(b + str_len(b), " ROUTES", 8);
        cv_text(6, 2, &AF_X, b, T_DIM);
    }
    cv_text_r(234, 2, &AF_X, mac_latch ? "LATCH" : "HOLD LFO", mac_latch ? T_ACCENT : T_DIM, T_SURF);
}
/* (V10.7.24) HOME's panel: MACROS the beings (graph_eco), SCOPE the mix's oscilloscope over the whole panel (ui_graph.c
 * graph_scope, as hot as it is loud) */
static void graph_home_lz(void)
{
    if (home_level() == 1u)
        graph_scope(ACC);
    else
        graph_eco();
}
