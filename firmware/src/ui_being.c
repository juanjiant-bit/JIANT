/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT 0.5: the beings. A sound page without a graph of its own (the engine's EDIT pages) shows a creature seen by a
 * thermal camera: a cell of 3 x 3 px pixels on a 4 px grid (a sensor), each as hot as the being is there (heat_col).
 * One body per engine (a genome: how many spikes, how long, how many lobes, how stretched, how they twist), shaped
 * by the page's four knobs and breathing with the sound:
 *   KNOB 1  the spikes: more of them          KNOB 3  the membrane: it wobbles into lobes
 *   KNOB 2  the spikes: longer                KNOB 4  the nuclei: 1 .. 4, orbiting (a cell dividing)
 * The output's level warms it (and swells it) and drives its time: it moves while it sounds, in silence it rests. Integer maths only: the radius of the
 * membrane at the cell's angle (dsp.c's sine table), the spikes a power of a cosine round it, a bright wall, a cold
 * band under it, the nuclei hot. 55 x 28 pixels. */
#define BG_CELL 4                                       /* px per sensor pixel (3 lit, 1 gap) */
#define BG_CW 55                                        /* 220 px */
#define BG_CH 28                                        /* 112 px */
#define BG_X0 10
#define BG_Y0 5
typedef struct {
    uint8_t k0, kk;                                     /* spikes: k0 + KNOB 1 x kk */
    uint8_t ss;                                         /* their length at KNOB 2 full (16 = the radius x 1.6) */
    uint8_t lob;                                        /* lobes of the membrane */
    uint8_t ax;                                         /* stretched: x / ax (16 = round) */
    uint8_t tw;                                         /* the spikes twist with the distance (a spiral) */
    uint8_t sh;                                         /* their sharpness (a power of the cosine) */
    uint8_t vac;                                        /* vacuoles: cold holes */
} genome_t;
static const genome_t GENOME[8] = {
    {6, 18, 16, 3, 22, 0, 6, 0},                        /* a radiolarian: a sun of thin rays */
    {3, 5, 14, 2, 26, 3, 2, 1},                         /* an amoeba: its arms swirl */
    {4, 6, 10, 4, 16, 0, 1, 1},                         /* a diatom: blunt, square */
    {10, 30, 8, 5, 19, 1, 10, 0},                       /* a ciliate: a fringe of hairs */
    {5, 9, 12, 3, 18, 2, 4, 1},                         /* a spore */
    {2, 4, 16, 2, 30, 1, 3, 0},                         /* a worm: long, two horns */
    {8, 12, 12, 6, 16, 0, 8, 1},                        /* a pollen grain */
    {3, 7, 15, 3, 20, 4, 5, 0},                         /* a hydra: curled tentacles */
};
static uint32_t bg_isqrt(uint32_t x)
{
    uint32_t r = 0, b = 1u << 30;
    while (b > x) b >>= 2;
    while (b) {
        if (x >= r + b) { x -= r + b; r = (r >> 1) + b; }
        else r >>= 1;
        b >>= 2;
    }
    return r;
}
/* atan2 as a 16-bit turn (0x10000 = 360 degrees), within ~0.2 degree */
static uint32_t bg_atan2(int32_t y, int32_t x)
{
    int32_t ax = x < 0 ? -x : x, ay = y < 0 ? -y : y, t, a;
    if (!ax && !ay) return 0;
    t = ax > ay ? (ay << 15) / ax : (ax << 15) / ay;   /* 0..1, Q15 */
    a = (t * (8192 + ((2847 * (32768 - t)) >> 15))) >> 15;
    if (ay > ax) a = 16384 - a;
    if (x < 0) a = 32768 - a;
    if (y < 0) a = 65536 - a;
    return (uint32_t)a & 0xFFFFu;
}
static int32_t bg_sin(uint32_t ang) { return sine_i(ang << 16); }   /* Q15 */
/* the output's level now, 0..256, slewed (fast up, slow down) */
static int32_t bg_energy(void)
{
    static int32_t e;
    uint32_t w = scope_w, i;
    int32_t pk = 0, v;
    for (i = 1; i <= 256u; i += 2u) {
        v = scope_buf[(w - i) & (SCOPE_N - 1u)];
        if (v < 0) v = -v;
        if (v > pk) pk = v;
    }
    v = pk * 256 / 12000;
    if (v > 256) v = 256;
    e += v > e ? (v - e) / 2 : (v - e) / 8;
    return e;
}
/* the being: genome g, the knobs k[0..3] (0..256), energy en (0..256), time t (a 16-bit turn) */
static void being_draw(const genome_t *g, const int32_t *k, int32_t en, uint32_t t)
{
    int32_t i, j, n, nn = 1 + k[3] * 3 / 257, spikes = g->k0 + k[0] * g->kk / 256;
    int32_t s = (48 + k[1] * 208 / 256) * g->ss / 16, w = 40 + k[2] * 216 / 256;   /* spike length (Q8 of x1.6), wobble */
    int32_t r0 = BG_CH * 77 + BG_CH * 77 * en * 38 / 65536;   /* the radius (Q8 px of the grid): 0.3 of the height */
    int32_t nx[4], ny[4], nr = r0 * 97 / 256;           /* the nuclei and their reach */
    for (n = 0; n < nn; n++) {
        uint32_t a = t + (uint32_t)n * 25033u;          /* (2.4 rad apart) */
        nx[n] = n ? (bg_sin(a + 0x4000u) * (r0 * 115 >> 8)) >> 15 : 0;
        ny[n] = n ? (bg_sin(a + t / 3u) * (r0 * 90 >> 8)) >> 15 : 0;
    }
    for (j = 0; j < BG_CH; j++)
        for (i = 0; i < BG_CW; i++) {
            int32_t dx = (2 * i + 1 - BG_CW) * 128 * 16 / g->ax, dy = (2 * j + 1 - BG_CH) * 128, v, c, p, r, rb;
            int32_t d = (int32_t)bg_isqrt((uint32_t)(dx * dx + dy * dy));
            uint32_t th = bg_atan2(dy, dx);
            r = r0 + ((((r0 * w) >> 8) * ((9175 * bg_sin(g->lob * th + t) + 3932 * bg_sin((g->lob + 2u) * th - 2u * t)) >> 15)) >> 15);
            c = bg_sin(((uint32_t)spikes * th >> 1) + (uint32_t)(g->tw * d * 6) + 0x4000u);
            p = c > 0 ? c : 0;
            for (n = 1; n < g->sh; n++)
                p = (p * c) >> 15;
            if (p < 0) p = 0;
            rb = r + ((((r0 * s) >> 8) * p >> 15) * 26 >> 4);
            if (d >= rb + 300) continue;                /* outside, beyond the halo */
            if (d >= rb) v = 12;                        /* the halo: a faint ring */
            else if (d > r) v = 20 + (rb - d) * 70 / (rb - r + 1);   /* a spike: warmer toward the body */
            else if (d > r - 333) v = 150 + 60 * en / 256;   /* the wall */
            else if (d > r - 666) v = 25;               /* the cold band under it */
            else v = 30 + (256 - d * 256 / (rb + 1)) * (154 + 102 * en / 256) * 120 / 65536;
            if (d < rb && !(d > r - 666 && d <= r)) {   /* the nuclei (not over the wall) */
                for (n = 0; n < nn; n++) {
                    int32_t ex = dx - nx[n] * 16 / g->ax, ey = dy - ny[n];
                    int32_t nd = (int32_t)bg_isqrt((uint32_t)(ex * ex + ey * ey));
                    if (nd < nr)
                        v += (256 - nd * 256 / nr) * (128 + 128 * en / 256) * 130 / 65536;
                }
                if (g->vac && ((uint32_t)(i * 7919 + j * 104729) + t / 2048u) % 61u == 0u)
                    v -= 60;                            /* a vacuole */
            }
            v += ((i * 7919 + j * 104729) % 13 - 6) * 3 / 2;   /* the sensor's grain */
            cv_rect(BG_X0 + i * BG_CELL, BG_Y0 + j * BG_CELL, BG_CELL - 1, BG_CELL - 1,
                    v < 16 ? ux_mix(T_SURF, heat_col(v), 33) : heat_col(v));
        }
}
/* the page's being: its engine's genome, its four knobs */
static void graph_being(const track_t *t)
{
    const page_t *pg = cur_page();
    int32_t k[4];
    uint32_t c, e = t->eng_req % NENGINES;
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(pg, c, &vp);
        int32_t r = d && d->label && d->label[0] != '-' ? RATIO(d, enum_rank(d, *vp)) : -1;
        k[c] = r < 0 ? 128 : r * 256 / 1000;
    }
    {
        static uint32_t tm, f0;                         /* its time runs with the sound: in silence it rests */
        int32_t en = bg_energy();
        if (en > 2) tm += (ui.frame - f0) * (uint32_t)(120 + en);
        f0 = ui.frame;
        being_draw(&GENOME[(e + ui.page) % 8u], k, en, tm);
    }
}
