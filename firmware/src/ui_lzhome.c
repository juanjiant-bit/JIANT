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
    }
    if (home_set())                                     /* (V10.8.64, the owner: "LIFE en grande") LIFE / MACROS: the
                                                         * page's name large, its state under it */
        page_big(home_set() == 1u ? "LIFE" : "MACROS", home_set() == 1u ? (life_on ? "ON" : "OFF") : "HOME+SELECT DICE");
}
/* (V10.8.60 prototype, the owner: "no inventar tres dibujos y repetirlos: usar la estética que ya tiene el sistema";
 * "tomá comportamientos, cómo se mueven los gusanitos ... pueden aparecer enormes en toda la pantalla, ANIMAL y VOICE
 * integrados y deformados, morph entre cosas") HOME > VISUAL and the saver: the song's own creatures (each track's
 * engine draws its being, vx_draw: VOICE's tract, ANIMAL's morph, DRUM-X's kit, FM6's colony, ANALOG, PHASE, MATH,
 * STRING, WEST; never WHEEL's drawbars) drawn huge, one to three, through one field (og_wf: every line and node they draw
 * passes through vis_wp): WHEEL's worms' wave travelling down them, a twist, the lobes breathing, a melt, a fold (a
 * mirror, four ways), the scope's thread joining them; a kick thumps and jumps it, a snare pulls the rows apart, the
 * hats shiver it, a note sends a wave down it; the main being morphs into another track's (both drawn through the same
 * field, crossfading, the field at its wildest half way) on a section's change, every twelfth kick, or ten seconds
 * still. The seed (vis_song: the song's) picks the beings, their places and sizes, the field's terms and the motion.
 * KNOB 1 DENSITY (how many beings), 2 WOBBLE (the field), 3 PALETTE (MIXED: the beings' own colours), 4 REACT */
#define VQ_KI vq.e[0]
#define VQ_SN vq.e[1]
#define VQ_HA vq.e[2]
#define VQ_ME vq.e[3]
static struct {
    uint32_t fr, r, t, sd, ms;
    int32_t lv, jx, jy, e[4], ow, wx, wy, tw, wv, br, ml, m;
    uint8_t mo, fo, pa, pb, nk, sec, np, no;
    uint16_t sg[NTRK];
} vq __attribute__((section(".bss.tape")));
static VX_SEG uint32_t vr(void) { return vx_rnd(&vq.r); }
/* a track whose engine draws a being worth fusing (WHEEL's drawbars, WEST's labelled circuit, the inputs' meters left out) */
static VX_SEG int vis_ok(uint32_t k)
{
    const track_t *t = &trk[k % NTRK];
    const engine_t *e = trk_engp(t);
    return trk_eng(t) == ENGI_DRUM || trk_eng(t) == ENGI_FM6 || e == &ENG_ANALOG ||
           (e == &ENG_PHASE || e == &ENG_LOFI || e == &ENG_STRING || e == &ENG_FORMANT || e == &ENG_ANIMAL);
}
static VX_SEG uint32_t vis_pick(uint32_t from)          /* the next track from `from` with a being (NTRK: none) */
{
    uint32_t i;
    for (i = 0; i < NTRK; i++)
        if (vis_ok(from + i))
            return (from + i) % NTRK;
    return NTRK;
}
/* the field: a point (Q4, the strip's) bent; c the line's colour (0: a point only) by PALETTE */
static const uint16_t VIS_PL[4][2] = {{0x041F, 0xFC60}, {0x07E8, 0xF800}, {0x8200, 0xFD20}, {0x4208, 0xFFFF}};
static VX_SEG void vis_wp(int32_t *p, uint16_t *c)
{
    int32_t x = p[0] - vq.wx, y = p[1] + vq.ow - vq.wy, r2, a, s, k = 256 + (VQ_KI >> (vq.mo == 2u ? 0 : 1));
    uint32_t t = vq.t, v = (uint32_t)vis_v[2];
    if (c && v) {                                       /* PALETTE: the line's brightness through the palette (MIXED: its own) */
        uint32_t l = ((*c >> 11) + ((*c >> 6) & 31u) + (*c & 31u)) * 256u / 93u;
        *c = v == 1u ? vk_heat((int32_t)l) : ux_mix(0, VIS_PL[(v - 2u) & 3u][l > 140u], (int32_t)l);
    }
    if (vq.fo) {                                        /* the fold: a mirror, or four */
        if (x < 0) x = -x;
        if (vq.fo == 2u && y < 0) y = -y;
    }
    y += vq.wv * og_sin((uint32_t)x * 36u - t * 500u) >> 15;   /* WHEEL's worms: a wave travelling down the body */
    x += vq.wv * og_sin((uint32_t)y * 28u - t * 380u) >> 15;
    x += (VQ_ME >> 1) * og_sin((uint32_t)y * 90u - t * 1800u) >> 14;   /* a note: a ripple running through it */
    r2 = (x >> 4) * (x >> 4) + (y >> 4) * (y >> 4);     /* (px squared) */
    a = vq.tw * r2 >> 6;                                /* the twist, more out from the middle */
    if (vq.mo == 1u) a += (int32_t)(t * 9u);            /* (turning) */
    s = 4096 + (vq.br * og_sin((uint32_t)(x * 14 + y * 11) + t * 300u) >> 15);   /* the lobes breathing */
    s = s * k >> 8;
    {
        int32_t cs = og_cos((uint32_t)a), sn = og_sin((uint32_t)a), nx = (x * cs - y * sn) >> 15, ny = (x * sn + y * cs) >> 15;
        x = nx * s >> 12; y = ny * s >> 12;
    }
    if (y > 0) y += (vq.ml * ((32768 + og_sin((uint32_t)x * 50u + t * 200u)) >> 8) >> 8) * (y >> 4) >> 6;   /* the melt, dripping */
    if (((y + vq.wy) >> 8) & 1) x += VQ_SN * 2;         /* a snare: the rows pulled apart */
    y += VQ_HA * og_sin((uint32_t)x * 160u + t * 4000u) >> 16;   /* the hats: a shiver */
    p[0] = x + vq.wx + vq.jx;
    p[1] = y + vq.wy + vq.jy - vq.ow;
}
/* (once a frame) the level, the hits, the time, the morph */
static VX_SEG void vis_tick(void)
{
    int32_t lv;
    uint32_t k;
    if (vq.fr == anim_fr)
        return;
    lv = bg_energy() * vis_v[3] >> 7;
    vq.fr = anim_fr;
    vq.lv = lv;
    vq.t += 2u + (uint32_t)lv / 24u;
    for (k = 0; k < NTRK; k++) {                        /* the hits: a track's new note (the sequencer, the keys, MIDI) */
        const track_t *tr = &trk[k];
        uint32_t n = tr->seq_n ? tr->seq_notes[0] : tr->nheld ? tr->held[0] : 0u, r;
        uint16_t g = (uint16_t)(tr->seq_idx * 16u + tr->nheld + tr->rat_left);
        if (n && g != vq.sg[k]) {
            r = tr->engine != ENGI_DRUM ? 3u : n < 37u ? 0u : n < 41u ? 1u : 2u;   /* (GM's keys) a kick, a snare, hats; a note */
            vq.e[r] = 255 * vis_v[3] / 60;
            vq.no = (uint8_t)n;
            if (!r) {                                   /* a kick: the scene jumps (motion 3 far); every twelfth a morph */
                r = vr();
                vq.jx = ((int32_t)(r & 511u) - 256) << (vq.mo == 3u);
                vq.jy = ((int32_t)(r >> 9 & 255u) - 128) << (vq.mo == 3u);
                if (++vq.nk >= 12u) { vq.nk = 0; vq.ms = 0; }
            }
        }
        vq.sg[k] = g;
    }
    for (k = 0; k < 4u; k++)
        vq.e[k] -= vq.e[k] >> 2;
    vq.jx -= vq.jx >> 3; vq.jy -= vq.jy >> 3;
    if (vq.m) {                                         /* a morph running: the next being comes in */
        vq.m += 6;
        if (vq.m >= 256) { vq.m = 0; vq.pa = vq.pb; }
    } else if (fm1_ms - vq.ms > 10000u || !vq.ms) {     /* (a section, twelve kicks or ten seconds: the next) */
        uint32_t b = vis_pick(vq.pa + 1u + vr() % 3u);
        vq.ms = fm1_ms | 1u;
        if (b < NTRK && b != vq.pa) { vq.pb = (uint8_t)b; vq.m = 1; }
    }
}
/* a being: track k's, its middle (px, the screen's) and size, at strength al */
static VX_SEG void vis_being(uint32_t k, int32_t x, int32_t y, int32_t w, int32_t al)
{
    if (k >= NTRK || al <= 0)
        return;
    og_wa = al;
    vx_draw(&trk[k], x - w / 2, y - w * 3 / 8 - (vq.ow >> 4), w, w * 3 / 4, -1);
}
/* (V10.8.61, the owner: "ANIMAL y VOICE integrados y deformados") a ghost: VOICE's tract (g 1) or ANIMAL's morph (g 2) drawn
 * from track k's parameters whatever its engine, fused into the scene through the same field (vx_draw's frames) */
static VX_SEG void vis_ghost(uint32_t k, uint32_t g, int32_t x, int32_t y, int32_t w, int32_t al)
{
    int32_t cx = x * 16, cy = y * 16 - vq.ow, h = w * 3 / 4, s;
    if (k >= NTRK || al <= 0)
        return;
    og_wa = al;
    if (g == 1u) {
        s = vx_fit(w, h, 80, 62);
        vx_voice(&trk[k], cx + 5 * s / 4, cy - 37 * s / 16, s, -1);
    } else
        vx_animal(&trk[k], cx, cy, vx_fit(w, h, 103, 78), -1);
}
/* the scope's thread: from (x0 y0) to (x1 y1) (px), the output's last samples across it */
static VX_SEG void vis_thread(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t amp)
{
    uint32_t i;
    int32_t px = 0, py = 0, nx = -(y1 - y0), ny = x1 - x0, d = (nx < 0 ? -nx : nx) + (ny < 0 ? -ny : ny) + 1;
    og_wa = 220;
    for (i = 0; i <= 48u; i++) {
        int32_t v = scope_buf[(scope_w - 192u + i * 4u) & (SCOPE_N - 1u)] * amp / 32768;
        int32_t x = (x0 + (x1 - x0) * (int32_t)i / 48 + v * nx / d) * 16, y = (y0 + (y1 - y0) * (int32_t)i / 48 + v * ny / d) * 16 - vq.ow;
        if (i) og_line_a(px, py, x, y, vk_heat(96 + vq.lv / 2 + (v < 0 ? -v : v)), 256);
        px = x; py = y;
    }
}
static uint32_t vis_song(void);   /* (gen.c) */
static void vis_fig(int32_t cx, int32_t cy, int32_t R, uint32_t sd)
{
    uint32_t b, i, n;
    int32_t X[3], Y[3], W[3], m;
    if (!vis_seed) {                                    /* (the first time: the knobs' defaults, a seed) */
        vis_v[0] = 50; vis_v[1] = 50; vis_v[3] = 70;
        vis_seed = 1791;
    }
    (void)R;
    if (sd != vq.sd && !((sd ^ vq.sd) & 0xFFFFFFu) && vq.sd) {   /* a section (A..D): the same scene, the next being */
        vq.sd = sd;
        vq.ms = 0;
    } else if (sd != vq.sd) {                           /* a new scene (a song, OCT+, the saver): its beings, its field */
        vq.sd = sd;
        vq.r = sd * 2654435761u | 1u;
        vq.mo = (uint8_t)(sd & 3u);
        vq.pa = (uint8_t)vis_pick(vr());
        vq.m = 0; vq.ms = fm1_ms | 1u;
    }
    vis_tick();
    vq.ow = (120 - (cy >> 4)) * 16;
    vq.r = vq.sd * 2654435761u | 1u;                    /* (the scene's numbers, the same each frame and strip) */
    b = vr();
    vq.fo = (uint8_t)(b % 5u < 2u ? b % 5u + 1u : 0u);
    vq.tw = ((int32_t)(b >> 4 & 15u) - 7) * (1 + vis_v[1] / 40);
    vq.wv = ((int32_t)(b >> 8 & 7u) + 2) * (8 + vis_v[1] / 5) + vq.lv / 4;
    vq.br = ((int32_t)(b >> 11 & 7u)) * (20 + vis_v[1]) + VQ_KI * 2;
    vq.ml = (b >> 18 & 1u) ? (int32_t)(b >> 14 & 15u) * (2 + vis_v[1] / 10) : 0;
    m = vq.m ? vq.m : 0;
    vq.wv += m * (256 - m) >> 8;                        /* (a morph: the field at its wildest half way) */
    vq.tw += m * (256 - m) >> 12;
    n = 1u + (uint32_t)vis_v[0] / 40u;                  /* DENSITY: one to three beings */
    for (i = 0; i < 3u; i++) {
        uint32_t q = vr();
        X[i] = 120 + ((int32_t)(q & 127u) - 64) * (i ? 2 : 1);
        Y[i] = 120 + ((int32_t)(q >> 7 & 127u) - 64) * (i ? 2 : 1);
        W[i] = (i ? 120 : 220) + (int32_t)(q >> 14 & 127u) * (i ? 1 : 2) + vq.lv / 2;   /* (huge: the main one past the screen) */
    }
    vq.wx = X[0] * 16; vq.wy = Y[0] * 16;
    og_wf = vis_wp;
    if (b >> 20 & 1u)                                /* the scope's thread, one being to the next or across */
        vis_thread(n > 1u ? X[1] : -20, n > 1u ? Y[1] : Y[0] + 60, n > 1u ? X[0] : 260, n > 1u ? Y[0] : Y[0] - 60, 40 + vq.lv / 3);
    for (i = 1; i < n; i++)                             /* the others: the next tracks' beings */
        vis_being(vis_pick(vq.pa + i + (b >> 22)), X[i], Y[i], W[i], 160);
    if (b % 3u == 0u)                                   /* (a scene in three) VOICE or ANIMAL fused in, breathing in and out */
        vis_ghost(vq.pa, 1u + (b >> 24 & 1u), X[1], Y[1], W[1] + 60, 120 + (og_sin(vq.t * 90u) >> 8));
    vis_being(vq.pa, X[0], Y[0], W[0], m ? 256 - m : 256);   /* the main one, and the one it becomes */
    if (m)
        vis_being(vq.pb, X[0], Y[0], W[0] * (192 + m / 4) / 256, m);
    og_wf = 0;
    og_wa = 256;
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
