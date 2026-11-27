/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.8.5) HOME's lens (ui_lens.c): the panels of ECO and BEING, drawn as the approved mock-up (lente_sistema.html,
 * HOME) in the lens's panel (240 x 140 at y 84). Included by ui_graph.c after ui_being.c. The beings are JIANT's own
 * (ui_being.c: the same genomes, isotherms, nuclei, the same swelling, exciting and swimming with each track's sound,
 * graph_ecosys's maths); the mock-up gave the chrome around them.
 *   ECO    (0.8.8: as 0.8.4's HOME again, graph_ecosys) the six tracks' beings swimming free over the whole panel, each
 *          by itself (wandering, bouncing off the walls, a hit kicking it on, swelling with its sound), its number under
 *          it; the selected one larger, its number in the theme's colour (no ring, no grid, no names). The cytoplasm's
 *          granules drift behind them. (0.9.7, the approved variant A "Hoy con carita") each being today's (the spiky
 *          outline of its species, the same spikes as it sounds) in three isotherms instead of four, coloured by its own
 *          level (quiet cyan and blue .. loud red, orange, yellow), and where its nuclei were a small face: two eyes that
 *          shut on a hit, a mouth that opens with the level; muted, asleep (a dim outline, its eyes shut). MUTATE: when a
 *          mutation lands the being looks up, its number lights white and one ring in the accent opens (the critters and
 *          the ring of twelve dots, gone). LIFE (V10.7.62): the same, a line from each track to the one it moves
 *          (the ring's), as bright as it sounds (a kit's in the accent)
 *   BEING  the track's being large in the middle, its four HOME knobs as organs at the corners (a gauge each, its name
 *          and value, a vein to the body), the sound's name under it; (V10.8.56) VISUAL: a figure
 *          from a seed (vis_fig), HOME's visuals and the saver's
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
/* ECO: 0.8.4's HOME (graph_ecosys) in the lens's panel: the cytoplasm's granules drifting, the six beings swimming free
 * over the whole panel (0.9.7: variant A, eco_being), each with its number under it (the selected one larger, its number in
 * the theme's colour; a mutation landed, white) */
static void graph_eco(void)
{
    uint32_t c, k, still = ANIM_OFF() != 0;
    int32_t bx[ECO_N], by[ECO_N], bot = eco_bot();
    (void)eco_step();                                   /* (the swim, once a frame) */
    (void)bg_energy();                                  /* (the output's level kept as it was read: once a frame) */
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
    if (home_set() && life_on) {                        /* (V10.7.62) LIFE: who moves whom, as bright as the mover sounds */
        uint32_t n = trk_live < ECO_N ? trk_live : ECO_N, j;
        for (c = 0; c < n; c++) {
            j = (c + n - 1u) % n;
            if (j != c)
                og_line_a(bx[j], by[j], bx[c], by[c], trk[j].engine == ENGI_DRUM ? T_ACCENT : T_THEME,
                          clamp(48 + track_env[j] / 96, 48, 256));
        }
    }
    for (c = 0; c < ECO_N; c++) {
        int32_t rad = eco.r[c] >> 4, px = bx[c] >> 4, py = by[c] >> 4, ny;
        char num[2] = {(char)('1' + c), 0};
        eco_being(c, bx[c], by[c], eco.r[c]);
        ny = clamp(py + rad / 2 + 4, 4, bot);
        cv_text_in(px - 30, ny, 60, &AF_S, num, eco_off(c) ? T_DIM : trk_col(c), T_SURF);   /* (V10.8.10) */
    }
    if (home_set())                                     /* (V10.7.62) LIFE (was MACROS: the FX layer's knobs since V10.7.59) */
        cv_text(6, 2, &AF_X, life_on ? "LIFE ON" : "LIFE OFF", life_on ? T_MID : T_DIM);
}
/* (V10.8.57, the owner: "podría ser mucho mejor" (fifteen references: "anatomía cibernética y data"; "una mezcla de
 * todo eso") HOME > VISUAL and the saver: a composition from a seed, of layers drawn by code (no pictures): a ground
 * (an isometric mesh with digits, topographic lines with a red overlay and the beat's spike, stars flying out of a
 * vanishing point, a rain of glyphs), something connecting (tentacles with square tips, WHEEL's worms), a core on a third (a radial
 * skull diagram: spokes from a hub, the skull's ring, a dashed circle; a scanned head: its outline, a thermal fill of
 * scan lines, the eyes; or one of the creatures, drawn as they are) and the
 * data over it all (tracking boxes joined by lines, their live numbers). Each moves with the sound its own way: the
 * mesh and the contours the output's history, the tentacles the tracks' levels, the core's heat the level, the numbers
 * live, the beat a spike. KNOB 1 DENSITY (layers and elements), 2 WOBBLE, 3 PALETTE (MIXED: a blue ground,
 * orange links, a thermal core, white data), 4 REACT */
static const uint16_t VIS_PC[6][3] = {{0x2B7F, 0xFB62, 0xF185}, {0, 0, 0}, {0x2B7F, 0xFB62, 0xFB62},
                                      {0x3F2B, 0xF185, 0xF185}, {0xFD40, 0xFAC0, 0xFAC0}, {0x9CF3, 0xFFFF, 0xFFFF}};
#define VIS_H ((int8_t *)&vxs.wv[128])                  /* the output's history, 16 x 16 (LOFI's half of vxs.wv: it fills it
                                                         * again each frame it draws) */
static struct {
    uint32_t fr, r, t, sd;
    int32_t lv, sp, pe, bx, by, k, px, py;
    uint8_t hr, na;
    int16_t ax[8], ay[8];
} vq __attribute__((section(".bss.tape")));
/* a role's colour: 0 the core, 1 the ground, 2 the links, 3 the data, 4 the alert; h 0..256 */
static VX_SEG uint16_t vc(uint32_t role, int32_t h)
{
    uint32_t p = (uint32_t)vis_v[2] % 6u;
    uint16_t c;
    h = clamp(h, 0, 256);
    if (role == 3u)
        return 0xFFFF;
    if (p == 1u || (!p && !role))
        return vk_heat(h);
    c = VIS_PC[p][role ? role - 1u : 1u];
    return h < 96 ? ux_mix(0, c, 60) : c;
}
static VX_SEG void vp(uint32_t i, int32_t x, int32_t y, uint16_t c, int32_t a)   /* a polyline's point (i 0: the first) */
{
    x = vq.bx + (x * vq.k >> 8); y = vq.by + (y * vq.k >> 8);
    if (i && a) og_line_a(vq.px, vq.py, x, y, c, a);
    vq.px = x; vq.py = y;
}
static VX_SEG void vm(void)                             /* the point just drawn, marked for the data */
{
    if (vq.na < 8u) { vq.ax[vq.na] = (int16_t)(vq.px >> 4); vq.ay[vq.na++] = (int16_t)(vq.py >> 4); }
}
static VX_SEG uint32_t vr(void) { return vx_rnd(&vq.r); }
static VX_SEG void vring(int32_t x, int32_t y, int32_t r, uint16_t c, uint32_t dash)   /* wobbling, maybe dashed */
{
    uint32_t i;
    for (i = 0; i <= 40u; i++) {
        uint32_t a = i * 1639u;
        int32_t rr = r + (r * og_sin(a * 3u + vq.t * 40u) >> 15) * (vis_v[1] + vq.lv / 4) / 700;
        vp(i, x + (rr * og_cos(a) >> 15), y + (rr * og_sin(a) >> 15), c, dash && (i & 1u) ? 0 : 256);
    }
}
static VX_SEG void vtxt(char *b, uint16_t c) { cv_text((vq.px >> 4) + 3, (vq.py >> 4) - 9, &AF_X, b, c); }
static VX_SEG int32_t vh(uint32_t row, uint32_t col) { return VIS_H[((vq.hr - row) & 15u) * 16u + (col & 15u)]; }
/* grounds: 0 the isometric mesh, the history its relief, digits on it; 1 topographic lines, a red overlay, the beat a
 * spike in the middle */
static VX_SEG void vis_ground(uint32_t kind)
{
    uint32_t n = 8u + (uint32_t)vis_v[0] / 14u, i, j, d;
    int32_t S = 360 / (int32_t)n, h;
    if (kind >= 2u) {                                   /* 2 stars flying out of a vanishing point, streaks on the beat; 3 a
                                                         * rain of glyphs, the heads bright */
        for (i = 0; i < (kind == 2u ? 4u : 2u) * n; i++) {
            uint32_t a = vr(), z = (a >> 16) + vq.t * (3u + (a & 7u)) * (kind == 2u ? 2u : 1u);
            if (kind == 2u) {
                z &= 255u;
                h = (int32_t)(z * z >> 7);
                vp(0, h * og_cos(a) >> 15, -150 + (h * og_sin(a) >> 15), 0, 0);
                h += 6 + vq.sp / 3;
                vp(1, h * og_cos(a) >> 15, -150 + (h * og_sin(a) >> 15), vc(1u, (int32_t)z), 256);
            } else
                for (j = 0; j < 6u; j++) {
                    char c2[2] = {(char)((j + a + (z >> 6)) % 10u + (j & 1u ? 'A' : '0')), 0};
                    vp(0, (int32_t)(a % 560u) - 280, (int32_t)((z >> 2) % 520u) - 380 - (int32_t)j * 24, 0, 0);
                    vtxt(c2, j ? vc(1u, 240 - (int32_t)j * 36) : 0xFFFF);
                }
        }
        return;
    }
    for (d = 0; d < 2u; d++)
        for (i = 0; i < n; i++)
            for (j = 0; j < n; j++) {
                uint32_t a = d ? j : i, b = d ? i : j;
                h = vh(b * 16u / n, a * 16u / n) * (2 + vis_v[1] / 25);
                vp(j, ((int32_t)a - (int32_t)b) * S, ((int32_t)a + (int32_t)b) * S / 2 - S * (int32_t)n / 2 - h, vc(1u, h * 2), 220);
                if (!d && (a * 7u + b * 13u + vq.sd) % 9u == 0u) {
                    char c2[2] = {(char)('0' + (uint32_t)h / 12u % 10u), 0};
                    vtxt(c2, 0xFFFF);
                    if (a == b) vm();
                }
            }
}
/* cores: 0 a radial skull diagram (the skull's ring, spokes from a hub to the callouts, one dashed circle), 1 a scanned
 * head (its outline, a thermal fill of scan lines by the tracks' levels, the eyes); never one shape at several sizes */
static VX_SEG void vis_core(uint32_t k)
{
    uint32_t i, n = 8u + (uint32_t)vis_v[0] / 8u;
    int32_t y, w;
    if (!k) {
        vring(0, 0, 230, 0xFFFF, 1);
        vring(0, 0, 150 + vq.sp / 4, vc(4u, 230), 0);
        for (i = 0; i < n; i++) {                       /* spokes from the hub, a node where they cross the skull */
            uint32_t a = i * 65536u / n + vq.sd;
            vp(0, 14 * og_cos(a) >> 15, 14 * og_sin(a) >> 15, 0, 0);
            vp(1, 150 * og_cos(a) >> 15, 150 * og_sin(a) >> 15, vc(1u, 220), 256);
            cv_rect((vq.px >> 4) - 1, (vq.py >> 4) - 1, 3, 3, 0xFFFF);
            vp(1, 262 * og_cos(a) >> 15, 262 * og_sin(a) >> 15, vc(1u, 160), 180);
            if (i % 3u == 0u) vm();
        }
        return;
    }
    for (i = 0; i <= 48u; i++) {                        /* the head's outline; its scan lines, hot where the tracks sound */
        uint32_t an = i * 1366u;
        w = 150 - (30 * og_sin(an) >> 15);
        vp(i, w * og_cos(an) * 4 / 5 >> 15, w * og_sin(an) >> 15, 0xFFFF, 256);
    }
    for (y = -160; y < 116; y += 9) {
        w = (150 + y / 5) * 4 / 5 - 8;
        w = w * (256 - y * y / 160) / 256;
        vp(0, -w, y, 0, 0);
        vp(1, w, y, vc(0u, trk[(uint32_t)(y + 160) / 46u % NTRK].peak * 256 / 7000 + vq.lv / 3 + (og_sin((uint32_t)y * 900u + vq.t * 20u) >> 10)), 150);
    }
    for (i = 0; i < 2u; i++) {                          /* the eyes */
        vring(i ? 48 : -48, -10, 12 + vq.lv / 24, 0xFFFF, 0);
        vm();
    }
}
/* (once a tick) the level, the beat, the time, the output's history */
static VX_SEG void vis_tick(void)
{
    int32_t lv, v;
    uint32_t k;
    if (vq.fr == anim_fr)
        return;
    {                             /* (once a tick) the level, the beat, the time, the history */
        lv = bg_energy() * vis_v[3] / 100;
        vq.fr = anim_fr;
        vq.sp = vq.sp * 7 / 8 > (lv - vq.pe) * 3 ? vq.sp * 7 / 8 : (lv - vq.pe) * 3;
        vq.pe = vq.lv = lv;
        vq.t += 6u + (uint32_t)lv / 16u;
        if (!(anim_fr & 3u)) {
            vq.hr = (uint8_t)((vq.hr + 1u) & 15u);
            for (k = 0; k < 16u; k++) {
                v = scope_buf[(scope_w - 256u + k * 16u) & (SCOPE_N - 1u)];
                VIS_H[vq.hr * 16u + k] = (int8_t)(4 + (v < 0 ? -v : v) * vis_v[3] / 25600);
            }
        }
    }
}
static void vis_fig(int32_t cx, int32_t cy, int32_t R, uint32_t sd)
{
    uint32_t i, b;
    char t[12];
    if (!vis_seed) {                                    /* (the first time: the knobs' defaults, a seed) */
        vis_v[0] = 50; vis_v[1] = 40; vis_v[3] = 60;
        vis_seed = 1791;
    }
    vis_tick();
    vq.sd = sd; vq.na = 0;
    vq.r = sd * 2654435761u | 1u;
    b = vr();                                           /* the ground low, the links rising from it, the core on a third */
    vq.bx = cx; vq.by = cy + 140 * R / 256; vq.k = 210 * R / 256;
    vis_ground(b & 3u);                                /* (1: the mesh too) */
    vq.by = cy + 30 * R / 256; vq.k = R * 3 / 4;
    {
        int32_t A[4] = {(cx >> 4) - 100, (cy >> 4) + 10, 200, 90};
        vx_gx(&trk[1], A, vq.lv, &VX_G[1]);            /* the links: WHEEL's worms, their crowns */
        vq.bx = cx + ((b & 4u) ? 96 : -96) * R / 256; vq.by = cy - 50 * R / 256; vq.k = (150 + (int32_t)(b >> 8) % 50) * R / 256;
        A[0] = (vq.bx - R / 2) >> 4; A[1] = (vq.by - R / 2) >> 4; A[2] = A[3] = R >> 4;
        if ((b >> 4 & 3u) == 3u)                        /* the core: a skull's diagram, a scanned head, or a creature */
            vx_gx(&trk[0], A, vq.lv, &VX_G[(b >> 12) % 5u == 1u ? 0u : (b >> 12) % 5u]);
        else
            vis_core(b >> 4 & 1u);
    }
    for (i = 0; i < vq.na; i++) {                       /* the data: boxes on the marked points, joined, their live numbers */
        int32_t x = vq.ax[i], y = vq.ay[i], s = 5 + (int32_t)i % 3 * 2 + vq.lv / 64;
        cv_frame(x - s, y - s, 2 * s + 1, 2 * s + 1, 0xFFFF);
        if (i) og_line_a(vq.ax[i - 1u] * 16, vq.ay[i - 1u] * 16, x * 16, y * 16, 0xFFFF, 150);
        fmt_fix(t, (int32_t)((sd + i) * 2654435761u >> 15) % 99991 - 49995 + vq.lv * 37 + (int32_t)(vq.t & 63u), 3);
        cv_text(x + s + 2, y - s - 2, &AF_X, t, 0xFFFF);
    }
}
/* BEING: the track's being large, its HOME knobs as organs at the corners */
static const int16_t ORG_X[4] = {30, 210, 30, 210}, ORG_Y[4] = {28, 28, 116, 116};
static void organ(uint32_t k, int32_t cx, int32_t cy)
{
    int16_t *vp;
    const param_desc_t *d = home_param(k, &vp);
    int32_t gx = ORG_X[k], gy = ORG_Y[k], hot = ui.hot_t && ui.hot_col == k;
    uint16_t c = hot ? T_ACCENT : T_THEME;
    og_quad_a(gx * 16, gy * 16, (gx + cx) * 8, gy * 16, (cx + (gx - cx) * 35 / 100) * 16, (cy + (gy - cy) * 35 / 100) * 16,
              ux_mix(T_SURF, c, 45), 12u, 256);
    og_node(gx * 16, gy * 16, 17, T_RAISE);
    lz_arc(gx, gy, 14, 3, clamp(RATIO(d, enum_rank(d, *vp)), 0, 1000), T_LINE, c);
    /* (its value stood inside the ring, cut to five letters: the card and the ring's arc say it) */
    GFX_HOOK_ALIGN(gx - 30, 0, gx + 30, 0, AL_H, "BEING organ name centred");
    cv_text_in(gx - 30, gy - 28, 60, &AF_X, d->label, hot ? T_ACCENT : T_MID, T_SURF);
}
static __attribute__((noinline)) void graph_home_being(void)
{
    uint32_t c = song.sel % NTRK, k;
    int32_t cx = 120, cy = 68, bx = cx * 16, by = cy * 16;
    (void)eco_step();
    (void)bg_energy();
    char b[16];
    for (k = 0; k < 4u; k++)
        organ(k, cx, cy);
    {   /* the being: graph_being's size, its HOME knobs, the track's own level */
        const genome_t *g = eco_genome(c);
        bfeat_t f;
        eco_feat(c, &f, 2);
        if (vx_draw(&trk[c], cx - 73, cy - 56, 146, 112, eco_off(c) ? 0 : eco.en[c]))
            ;                                       /* (0.9.4 / 0.9.6: the engines' own creatures, ui_engvis.c) */
        else if (eco_off(c))
            bg_outline(g, &f, bx, by, (30 * 16), 1, eco.tm[c], T_DIM);
        else
            being_lines(g, &f, bx, by, (30 * 16) * (256 + eco.en[c] / 3) / 256, eco.en[c], eco.tm[c] + c * 20000u);
    }
    sound_name(&trk[c], b);
    GFX_HOOK_ALIGN(60, 0, 180, 0, AL_H, "BEING sound name centred");
    cv_text_in(60, 129, 120, &AF_X, b[0] ? b : trk_engp(&trk[c])->name, T_MID, T_SURF);
}
static void graph_home_lz(void)
{
    if (home_level() == 1u)
        graph_home_being();
    else
        graph_eco();
}
