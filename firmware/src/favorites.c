/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
#define FELUCCA_FAVORITES 1
/* Stable engine/preset references; fixed capacity independent of optional engines. */
typedef struct {
    uint8_t factory[16][32]; /* engine 0..15, preset 0..255 */
    uint32_t user, filter;
} favorites_t;
static favorites_t favorites;
/* (0.9.9-snd5) the stars of GRAIN's and SLICE's numbers (retired) cleared once: they are STRING's and ADD's now (ui.c
 * prefs_update: the settings' revision 3) */
static void settings_save(void);
/* (V10.7.43) the FX keys' layouts (ui_layer.c pfm_say): favorites.factory[7|2|13][5..31] (WHEEL's PHASE's ADD's rows past
 * their stars: none has 40 sounds), a key's action + 1 (0: the layout's own); the layout factory[6][18] (TRIO's retired
 * row) */
static uint8_t *pfm_row(uint32_t l) { static const uint8_t R[3] = {7, 2, 13}; return &favorites.factory[R[l % 3u]][5]; }
static __attribute__((noinline)) void fav_rows_new(void)
{
    memset(favorites.factory[8], 0, sizeof favorites.factory[8]);           /* (core.h ENGI_STRING) */
    memset(favorites.factory[13], 0, sizeof favorites.factory[13]);         /* (core.h ENGI_ADD) */
    favorites.factory[15][25] = 3u;                    /* (prefs_rev) */
    settings_save();
}
static int favorite_has(uint32_t engine, uint32_t preset)
{
    if (engine == NENGINES)
        return preset < UP_SLOTS && ((favorites.user >> preset) & 1u);
    return engine < NENGINES && engine < 16u && preset < 256u &&
        ((favorites.factory[engine][preset / 8u] >> (preset % 8u)) & 1u);
}
static int favorite_set(uint32_t engine, uint32_t preset, int on)
{
    if (engine > NENGINES || (engine == NENGINES ? preset >= UP_SLOTS : engine >= 16u || preset >= 256u))
        return 0;
    if (favorite_has(engine, preset) == !!on) return 0;
    if (engine == NENGINES) {
        if (on) favorites.user |= 1u << preset;
        else favorites.user &= ~(1u << preset);
    } else {
        uint8_t *b = &favorites.factory[engine][preset / 8u];
        if (on) *b |= (uint8_t)(1u << (preset % 8u));
        else *b &= (uint8_t)~(1u << (preset % 8u));
    }
    return 1;
}
