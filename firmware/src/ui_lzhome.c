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
        if (home_set() && bot > 48)                     /* (V10.8.65) LIFE / MACROS: the beings under the page's large name */
            by[c] = 40 * 16 + by[c] * (bot - 40) / bot;
    }
    if (home_set() == 1u && life_on) {                        /* (V10.7.62) LIFE: who moves whom, as bright as the mover sounds */
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
        if (ott_v[c][OT_DEPTH])                         /* (V10.8.71, BYTE's) its OTT on: a tag under its number, in the
                                                         * thermal colour of the most it moves a band (ui_lzfx.c ott_col) */
            cv_text_in(px - 30, ny + 12, 60, &AF_X, "OTT", eco_off(c) ? T_DIM :
                       ott_col(ott_s[c].g[0], ott_s[c].g[1]), T_SURF);
    }
    if (home_set())                                     /* (V10.8.64, the owner: "LIFE en grande") LIFE / MACROS: the
                                                         * page's name large, its state under it */
        page_big(home_set() == 1u ? "LIFE" : "MACROS", home_set() == 1u ? (life_on ? "ON" : "OFF") : "HOME+SELECT DICE");
}
/* (V10.8.67, the owner: "No quiero que copies las visuales de BYTE, tratemos de darle onda a las nuestras con poco. Usaría
 * las gráficas de FX, ENVELOPE, LFO, las de HOME, ANIMAL haciendo el morph, pero deformadas, repetidas, rotas") HOME >
 * VISUAL and the saver: the system's own pictures, drawn by their own functions (FX's DELAY REVERB CHORUS DIST MASTER,
 * ENV's curve, the LFO, HOME's ECO, a track's being, ANIMAL's creature morphing), every line, box and node of them
 * through one map (gfx.c cv_vm, ui_organic.c og_wf; their words and lone pixels left out):
 *   DEFORMED  a warp, a twist, polar (the picture wound into a ring), a fold
 *   REPEATED  a kaleidoscope, a stutter, a trail, tiles: copies turned, moved, smaller, fading
 *   BROKEN    strips slipping, gaps, scanline tears, blocks out of place
 * Each segment drawn swims on its own oscillator: a kick scatters them and they gather, a snare turns the copies, the
 * hats shiver them, a note runs down the segments as a ripple; still at rest, the sound brings it. The song (vis_song:
 * GEN's seed, genre, energy; the section) picks two pictures and the three operators; a section, every twelfth kick or
 * ten seconds the second picture changes, fading in. KNOB 1 DENSITY (the copies), 2 WOBBLE (the deformation), 3 PALETTE
 * (MIXED: the pictures' own colours, lighter), 4 REACT */
#define VQ_KI vq.e[0]
#define VQ_SN vq.e[1]
#define VQ_HA vq.e[2]
#define VQ_ME vq.e[3]
enum { VI_ENV, VI_LFO, VI_DLY, VI_REV, VI_CHO, VI_DST, VI_MST, VI_ECO, VI_BEING, VI_ANIMAL, VI_N };
static const uint8_t VIS_CY[VI_N] = {62, 50, 60, 60, 60, 60, 60, 64, 90, 90};   /* (each picture's middle, its own rows) */
static struct {
    uint32_t fr, r, t, sd, ms, n, bs, tt, np, bud;
    int32_t lv, e[4], nh[16], sy, k, cs, sn, ox, oy, dx, dy, am, ba, wb, fd, fi, m, cj;
    uint8_t ing[2], op[3], nk, gap, pa;
    uint16_t sg[NTRK];
} vq VX_POOL;
static VX_SEG uint32_t vr(void) { return vx_rnd(&vq.r); }
/* a track whose engine draws a being worth drawing (WHEEL's drawbars, WEST's labelled circuit, the inputs' meters left out) */
static VX_SEG int vis_ok(uint32_t k)
{
    const track_t *t = &trk[k % NTRK];
    const engine_t *e = trk_engp(t);
    return trk_eng(t) == ENGI_DRUM || trk_eng(t) == ENGI_FM6 || e == &ENG_ANALOG ||
           (e == &ENG_PHASE || e == &ENG_LOFI || e == &ENG_STRING || e == &ENG_FORMANT || e == &ENG_ANIMAL);
}
static VX_SEG uint32_t vis_pick(uint32_t from)          /* the next track from `from` with a being (0: none) */
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        if (vis_ok(from + i))
            return (from + i) % NTRK;
    return 0;
}
static const uint16_t VIS_PL[4][2] = {{0x041F, 0xFC60}, {0x07E8, 0xF800}, {0x8200, 0xFD20}, {0x4208, 0xFFFF}};
/* the map: a point (Q4, the picture's own) to the strip's; c the segment's colour (0: a point after its first) */
static VX_SEG void vis_mp(int32_t *p, uint16_t *c)
{
    int32_t x = (p[0] - 120 * 16) * vq.k >> 8, y = (p[1] - vq.sy) * vq.k >> 8, a, r;
    uint32_t tm = vq.t - vq.tt, h, v = (uint32_t)vis_v[2];
    if (++vq.np > vq.bud) {                             /* (past the frame's budget of points: left out) */
        p[0] = p[1] = -4000;
        return;
    }
    if (c) {                                            /* a segment: its own swim (a note's ripple down them), its colour */
        h = ++vq.n * 2654435761u;                       /* (its place swims with its neighbours'; a kick scatters each its
                                                         * own way, then they gather; a note's ripple runs down the rows) */
        a = vq.am + vq.nh[(vq.fr - ((uint32_t)p[1] >> 8)) & 15u];
        vq.dx = (og_sin((uint32_t)p[1] * 48u + tm * 90u) * a >> 15) + (((int32_t)(h >> 24) - 128) * VQ_KI >> 8);
        vq.dy = (og_cos((uint32_t)p[0] * 40u + tm * 70u) * a >> 15) + (((int32_t)(h >> 16 & 255u) - 128) * VQ_KI >> 8);
        vq.gap = vq.op[2] == 1u && ((h ^ vq.bs) >> 24) < (uint32_t)vq.ba / 2u;   /* (at most half) */
        r = (int32_t)(((*c >> 11) + ((*c >> 6) & 31u) + (*c & 31u)) * 275u >> 8);   /* (0..100) */
        r += (100 - r) * r >> 7;                        /* (lighter: the middles lifted, the dark kept dark) */
        a = clamp(r + vq.cj, 0, 100);                   /* (each copy a step along the palette) */
        *c = ux_mix(T_BG, !v ? ux_mix(*c, 0xFFFFu, 25) : v == 1u ? vk_heat(a * 653 >> 8) :
                              ux_mix(VIS_PL[(v - 2u) & 3u][0], VIS_PL[(v - 2u) & 3u][1], a), r < 12 ? 0 : vq.fd);
    }
    if (vq.gap) {                                       /* BROKEN, gaps: the segment gone */
        p[0] = p[1] = -4000;
        return;
    }
    x += vq.dx; y += vq.dy;
    switch (vq.op[1]) {                                 /* DEFORMED */
    case 0:                                             /* a warp */
        y += vq.wb * og_sin((uint32_t)x * 30u - tm * 400u) >> 14;
        x += vq.wb * og_sin((uint32_t)y * 24u + tm * 300u) >> 14;
        break;
    case 1:                                             /* a twist, more out from the middle */
        a = vq.wb * ((x >> 4) * (x >> 4) + (y >> 4) * (y >> 4)) >> 8;
        r = og_cos((uint32_t)a) ; a = og_sin((uint32_t)a);
        h = (uint32_t)((x * r - y * a) >> 15); y = (x * a + y * r) >> 15; x = (int32_t)h;
        break;
    case 2:                                             /* polar: across the turn, its rows the radius */
        a = x * 17 + (int32_t)(tm * 20u);
        r = 80 * 16 + y + (vq.wb * og_sin((uint32_t)x * 60u) >> 13);
        x = og_cos((uint32_t)a) * r >> 15; y = og_sin((uint32_t)a) * r >> 15;
        break;
    default:                                            /* a fold: what passes 96 px out folded back, over and over */
        a = (int32_t)((uint32_t)(x + 960 * 16) % 3072u) - 1536; x = (a < 0 ? -a : a) * 2 - 1536;
        a = (int32_t)((uint32_t)(y + 960 * 16) % 3072u) - 1536; y = (a < 0 ? -a : a) * 2 - 1536;
        x += vq.wb * og_sin((uint32_t)y * 40u + tm * 250u) >> 15;
    }
    a = (x * vq.cs - y * vq.sn) >> 15; y = (x * vq.sn + y * vq.cs) >> 15;   /* REPEATED: the copy turned and moved */
    x = a + vq.ox; y += vq.oy;
    y += VQ_HA * og_sin((uint32_t)x * 160u + tm * 4000u) >> 16;   /* the hats: a shiver */
    h = vq.bs;
    switch (vq.op[2]) {                                 /* BROKEN */
    case 0:                                             /* strips slipping (16 px) */
        x += ((int32_t)((((uint32_t)(y + 64000) >> 8) * 0x9E3779B1u ^ h) >> 26) - 32) * vq.ba >> 5;
        break;
    case 2:                                             /* scanline tears (4 px) */
        h ^= ((uint32_t)(y + 64000) >> 6) * 0x9E3779B1u;
        if ((h >> 28) < 2u) x += ((int32_t)(h >> 12 & 255u) - 128) * vq.ba >> 5;
        break;
    case 3:                                             /* blocks out of place (32 px) */
        h ^= ((uint32_t)(x + 64000) >> 9) * 73856093u ^ ((uint32_t)(y + 64000) >> 9) * 19349663u;
        if (!(h & 7u)) { x += ((int32_t)(h >> 8 & 63u) - 32) * vq.ba >> 4; y += ((int32_t)(h >> 16 & 63u) - 32) * vq.ba >> 5; }
    }
    p[0] = x + 120 * 16;
    p[1] = y + vq.m;
}
/* (once a frame) the level, the hits, the time, the next picture */
static void vis_tick(void)
{
    uint32_t k, cold = anim_fr - vq.fr > 8u;             /* (V10.8.68) back on the page (VISUAL or the saver): nothing of it ran
                                                         * while away; its hits taken as they are, the pictures kept, whole */
    if (vq.fr == anim_fr)
        return;
    vq.lv = bg_energy() * vis_v[3] >> 7;
    vq.fr = anim_fr;
    if (cold) { vq.ms = fm1_ms | 1u; vq.fi = 100; }
    vq.t += 2u + (uint32_t)vq.lv / 24u;
    for (k = 0; k < NTRK; k++) {                        /* the hits: a track's new note (the sequencer, the keys, MIDI) */
        const track_t *tr = &trk[k];
        uint32_t n = tr->seq_n ? tr->seq_notes[0] : tr->nheld ? tr->held[0] : 0u, r;
        uint16_t g = (uint16_t)(tr->seq_idx * 16u + tr->nheld + tr->rat_left);
        if (n && g != vq.sg[k] && !cold) {
            r = tr->engine != ENGI_DRUM ? 3u : n < 37u ? 0u : n < 41u ? 1u : 2u;   /* (GM's keys) a kick, a snare, hats; a note */
            vq.e[r] = 255 * vis_v[3] / 60;
            if (r < 2u) vq.bs = vr();                   /* (a kick, a snare: the breaks elsewhere) */
            if (!r && ++vq.nk >= 12u) { vq.nk = 0; vq.ms = 0; }   /* (every twelfth kick: the next picture) */
        }
        vq.sg[k] = g;
    }
    for (k = 0; k < 4u; k++)
        vq.e[k] -= vq.e[k] >> 2;
    vq.nh[vq.fr & 15u] = VQ_ME / 2;
    vq.am = vq.lv + VQ_KI / 2;
    vq.ba = clamp((vq.lv / 3 + VQ_KI / 2 + VQ_SN) * (2 + (int32_t)(vq.sd & 3u)) / 3, 0, 255);
    if (vq.fi < 100)                                    /* (the second picture fading in) */
        vq.fi += 5;
    if (fm1_ms - vq.ms > 10000u || !vq.ms) {            /* (a section, twelve kicks or ten seconds: the next) */
        vq.ms = fm1_ms | 1u;
        vq.ing[1] = (uint8_t)((vq.ing[1] + 1u + vr() % (VI_N - 2u)) % VI_N);
        if (vq.ing[1] == vq.ing[0]) vq.ing[1] = (uint8_t)((vq.ing[1] + 1u) % VI_N);
        vq.fi = 0;
    }
}
/* picture i, track t's (its own coordinates) */
static void vis_ing(uint32_t i, const track_t *t)
{
    switch (i) {
    case VI_ENV: {
        const page_t *o = page_over;
        uint32_t k;
        for (k = 0; k + 1u < NPAGES && PAGES[k].graph != GR_ADSR; k++) {}
        page_over = &PAGES[k];
        graph_adsr(t, 0);
        page_over = o;
        break;
    }
    case VI_LFO: graph_lfo(t, 0); break;
    case VI_DLY: fxg_delay(0, 0, 240, 120); break;
    case VI_REV: fxg_reverb(0, 0, 240, 120); break;
    case VI_CHO: fxg_chorus(0, 0, 240, 120); break;
    case VI_DST: fxg_dist(t, 60, 0, 120, 120); break;
    case VI_MST: fxg_master(0, 0, 240, 120); break;
    case VI_ECO: graph_eco(); break;
    case VI_BEING: vx_draw(t, 0, 0, 240, 180, -1); break;
    default: {                                          /* ANIMAL's creature: insect, bird, mammal and back (a note: faster) */
        int16_t w0 = vxs.w[0], w1 = vxs.w[1], w2 = vxs.w[2];
        vxs.w[0] = (int16_t)(128 + (og_sin(vq.t * 40u + (uint32_t)VQ_ME * 64u) >> 8));
        vxs.w[1] = 1; vxs.w[2] = (int16_t)anim_fr;
        vx_animal(t, 120 * 16, 90 * 16, vx_fit(240, 180, 103, 78), -1);
        vxs.w[0] = w0; vxs.w[1] = w1; vxs.w[2] = w2;
    }
    }
}
static uint32_t vis_song(void);   /* (gen.c) */
static void vis_fig(int32_t cx, int32_t cy, int32_t R, uint32_t sd)
{
    uint32_t b, i, j, n, hi;
    int32_t o = cv_oy;
    const track_t *t;
    (void)cx; (void)R;
    if (!vis_seed) {                                    /* (the first time: the knobs' defaults, a seed) */
        vis_v[0] = 50; vis_v[1] = 50; vis_v[3] = 70;
        vis_seed = 1791;
    }
    if (sd != vq.sd) {                                  /* a song (OCT+, the saver): its pictures, its operators; a section
                                                         * (A..D): the same, the second picture and the deformation the next */
        int sec = vq.sd && !((sd ^ vq.sd) & 0xFFFFFFu);
        vq.r = sd * 2654435761u | 1u;
        b = vr() ^ vr() ^ vr();                         /* (the seeds' neighbours apart) */
        if (!sec) {
            vq.ing[0] = (uint8_t)(b % VI_N);
            vq.ing[1] = (uint8_t)((vq.ing[0] + 1u + (b >> 4) % (VI_N - 1u)) % VI_N);
            for (i = 0; i < 3u; i++) vq.op[i] = (uint8_t)(vr() >> 13 & 3u);
            vq.pa = (uint8_t)vis_pick(vr());
        } else
            vq.op[1] = (uint8_t)((vq.op[1] + 1u) & 3u);
        vq.sd = sd;
        vq.ms = 0;
    }
    vis_tick();
    t = &trk[vq.pa];
    hi = song.cpu_q8 > 166u;                            /* (the sound busy: the picture yields; ui_draw.c drew it less often) */
    vq.wb = (16 + vis_v[1] * 2) * (vq.op[1] == 1u ? 1 : 3) / 4 + vq.lv / 4;
    vq.m = cy + 30000 * 16;                             /* (the strip's rows, past cv_oy: their own words and pixels gone) */
    vq.np = 0;
    cv_oy = -30000;
    og_wf = cv_vm = vis_mp;
    for (i = 0; i < 2u; i++) {
        uint32_t g = vq.ing[i], sp = vq.t * 6u + (uint32_t)VQ_SN * 40u, d = (uint32_t)vis_v[0];   /* (turning; a snare: a jolt) */
        n = i ? 1u : vq.op[0] == 3u ? 4u : vq.op[0] == 1u ? 2u + d / 25u : 3u + d / 14u;
        vq.bud = hi ? 700u - i * 400u : 1800u - i * 900u;   /* (the points a strip may draw: the first picture 2/3) */
        vq.sy = VIS_CY[g] * 16;
        for (j = 0; j < n; j++) {                       /* REPEATED: the copies */
            int32_t k = 256 + VQ_KI / 3, a = (int32_t)(i || vq.op[0] == 3u ? sp : sp / 8u), f = 100 - (int32_t)j * 9, h = (int32_t)j * 2 - (int32_t)n + 1;
                                                        /* (the first picture turning slower; h: the copy from the middle) */
            vq.ox = vq.oy = 0; vq.tt = 0;
            vq.n = i * 977u;                            /* (each copy's segments the same swim) */
            vq.cj = (int32_t)j * 70 / (int32_t)n - 20;
            switch (i ? 9u : vq.op[0]) {
            case 9: break;                              /* (the second picture: as it is) */
            case 0: a += (int32_t)(j * 65536u / n); f = 100; break;   /* a kaleidoscope */
            case 1: vq.ox = h * (8 * 16 + VQ_KI / 2); vq.oy = h * 4 * 16; break;   /* a stutter */
            case 2: vq.tt = j * 10u; vq.oy = h * 3 * 16; k = k * (256 - (int32_t)j * 12) >> 8; break;   /* a trail */
            default: k /= 2; vq.ox = (j & 1u ? 60 : -60) * 16; vq.oy = (j & 2u ? 60 : -60) * 16; a = (int32_t)(j * 16384u);   /* tiles */
            }
            vq.k = k * (i ? 200 : 256) >> 8;
            vq.cs = og_cos((uint32_t)a); vq.sn = og_sin((uint32_t)a);
            vq.fd = i ? vq.fi : f;
            og_wa = vq.fd * 256 / 100;
            vis_ing(g, t);
            if (!j && vq.np && n > 1u && vq.np * n > vq.bud)   /* (as many copies as the budget holds, all whole) */
                n = clamp(vq.bud / vq.np, 1, n);
            if (hi && vq.op[2] == 2u) vq.op[2] = 0;     /* (busy: no tears) */
        }
        vq.np = 0;
    }
    og_wf = cv_vm = 0;
    og_wa = 256;
    cv_oy = o;
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
