/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.53, the owner: "todos los gráficos del sistema vengan del mismo motor así se achica el espacio que ocupan")
 * the pictures' shared engine: a few primitives every lens and page picture draws with (included by ui_graph.c)
 *   gx_col    a column of 5 px cells from yb up (v 0..1000 of h lit, the rest SURF; c 0: the heat ramp by its row)
 *   gx_sine   a sine's line (k cycles over w, phase ph, amplitude a, clamped to lo .. hi)
 *   gx_chip   a box with its word centred (X), the pictures' chips and tags
 *   gx_keys   oct octaves of keys from x (white keys sp apart, wh tall; the black ones 2/3 of them), col[r] the colour
 *             of semitone r (0: the key's own), lab[r] its word on it (0: none) */
static uint16_t heat_col(int32_t v);
static int32_t bg_sin(uint32_t a);
static void gx_col(int32_t x, int32_t yb, int32_t cw, int32_t h, int32_t v, uint16_t c)
{
    int32_t r, nr = h / 6, on = clamp(v, 0, 1000) * nr / 1000;
    for (r = 0; r < nr; r++)
        if (cv_vm)                                      /* (V10.8.67) HOME > VISUAL: the cell a line, through its map */
            cv_line(x, yb - (r + 1) * 6 + 2, x + cw - 1, yb - (r + 1) * 6 + 2, r < on ? (c ? c : heat_col(60 + r * 190 / (nr ? nr : 1))) : T_SURF);
        else
            cv_rect(x, yb - (r + 1) * 6, cw, 5, r < on ? (c ? c : heat_col(60 + r * 190 / (nr ? nr : 1))) : T_SURF);
}
static void gx_sine(int32_t x, int32_t mid, int32_t w, int32_t a, uint32_t k, uint32_t ph, uint16_t c, int32_t lo, int32_t hi)
{
    int32_t i, py = 0;
    for (i = 0; i <= w; i++) {
        int32_t yy = clamp(mid + (bg_sin((uint32_t)(i * 65536 / (w ? w : 1)) * k + ph) * a >> 15), lo, hi);
        if (i) cv_line_t(x + i - 1, py, x + i, yy, c, 2);
        py = yy;
    }
}
static void gx_chip(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t bg, const char *s, uint16_t ink)
{
    cv_rect(x, y, w, h, bg);
    if (s && s[0])
        cv_text_in(x, y + CAP_IN(X, h), w, &AF_X, s, ink, bg);
}
static void gx_keys(int32_t x, int32_t y, uint32_t oct, int32_t sp, int32_t wh, const uint16_t *col, const char *const *lab)
{
    static const uint8_t WK[7] = {0, 2, 4, 5, 7, 9, 11}, BK[5] = {1, 3, 6, 8, 10}, BX[5] = {1, 2, 4, 5, 6};
    uint32_t o, k;
    int32_t bw = sp * 2 / 3, bh = wh * 6 / 10;
    for (o = 0; o < oct; o++)
        for (k = 0; k < 12u; k++) {
            int blk = k >= 7u;
            uint32_t r = o * 12u + (blk ? BK[k - 7u] : WK[k]);
            int32_t kx = x + (int32_t)(o * 7u) * sp + (blk ? (int32_t)BX[k - 7u] * sp - bw / 2 : (int32_t)k * sp);
            int32_t kw = blk ? bw : sp - 1, kh = blk ? bh : wh;
            uint16_t c = col[r] ? col[r] : blk ? 0x10A2u : T_RAISE;
            gx_chip(kx, y + kh - 14, kw, 14, c, lab[r], T_INK);
            cv_rect(kx, y, kw, kh - 14, c);
        }
}

/* (V10.8.64, the owner: "LIFE en grande ... cuánto se puede agrandar el nombre de lo que hace la página") a page's name
 * large at the panel's top left (AF_L), its state or hint small under it: one call a page, the same place on each */
static __attribute__((noinline)) void page_big(const char *a, const char *b)
{
    cv_text(6, 2, large_face_has(a) && text_w(&AF_L, a) <= 228 ? &AF_L : &AF_M, a, T_TEXT);   /* (V10.8.65: a long name,
                                                         * KEY SPLIT 2, in M) */
    if (b)
        cv_text(8, 34, &AF_X, b, T_DIM);
}
