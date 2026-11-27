/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Several JIANTs in one process (a DAW with JIANT FM on several tracks): the core (jiant_core.h) is one device, its
 * state in the firmware's globals, so it is compiled JIANT_COPIES times (jiant_copy.c), each copy with every external
 * symbol of its translation unit renamed (copy_symbols.h: panel -> jc3_panel, jiant_render -> jc3_jiant_render), so
 * the copies share nothing: no global, no static, no buffer. Each copy is a whole device, with the same API as a table
 * of functions (jiant_api below, one per copy): a plugin instance claims a free copy, calls the core through it, and
 * gives it back when it goes.
 *
 *   const jiant_api *c = jiant_copy_claim(&k);      a free copy (0: all taken); k its number, 0..JIANT_COPIES-1
 *   c->jiant_boot(seed, 1); c->jiant_render(l, r, n); ...      (as jiant_core.h, one thread at a time per copy)
 *   jiant_copy_release(c);
 *
 * Copies are independent: two instances may run their copies on two audio threads at once. A copy keeps its device
 * when given back (booted once per process, as the single core): the next claimer finds it as it was left (a plugin
 * puts its own music in). Memory: each copy's code (~1 MB) and zeroed data (~6.4 MB, the 1 MiB
 * flash image, the screen, GRAIN's and SAMPLE's working buffers) are in the binary for every copy, the data pages
 * untouched (so not resident) until that copy boots; the sample slots are allocated as they are loaded, plus the
 * recorder's 60 s spare once a copy's jiant_service runs.
 *
 * The renamed symbols are listed in copy_symbols.h, made from the core's object (ports/core/build.sh symbols) and
 * checked by ports/core/build.sh test: a new global in the firmware without it shows as a duplicate symbol when a
 * port links the copies. ports/core's own single library (libjiant_core.a, jiant_core.h) is unchanged by this. */
#ifndef JIANT_COPIES_H
#define JIANT_COPIES_H
#include "jiant_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JIANT_COPIES
#define JIANT_COPIES 8
#endif

/* the API, X(return type, name, parameters) */
#define JIANT_API_LIST(X) \
    X(const char *, jiant_version, (void)) \
    X(int, jiant_boot, (uint32_t seed, int skip_splash)) \
    X(int, jiant_flash_restore, (const uint8_t *image, size_t size)) \
    X(const uint8_t *, jiant_flash, (size_t *size)) \
    X(uint32_t, jiant_flash_writes, (void)) \
    X(int, jiant_autosave_now, (void)) \
    X(int, jiant_booted, (void)) \
    X(void, jiant_render, (float *l, float *r, uint32_t n)) \
    X(int, jiant_midi, (uint8_t status, uint8_t d1, uint8_t d2)) \
    X(int, jiant_sysex, (const uint8_t *msg, size_t n)) \
    X(size_t, jiant_sysex_out, (uint8_t *buf, size_t cap)) \
    X(void, jiant_editor_poll, (void)) \
    X(void, jiant_editor_host, (uint32_t board)) \
    X(void, jiant_clock_external, (int on)) \
    X(void, jiant_play, (int on)) \
    X(int, jiant_playing, (void)) \
    X(uint32_t, jiant_bpm, (void)) \
    X(uint32_t, jiant_cpu_percent, (void)) \
    X(uint32_t, jiant_cpu_peak_percent, (void)) \
    X(uint32_t, jiant_voices_shed, (void)) \
    X(int, jiant_load_governed, (void)) \
    X(uint32_t, jiant_voices_sounding, (void)) \
    X(void, jiant_test_heavy, (void)) \
    X(void, jiant_buttons, (uint32_t held)) \
    X(void, jiant_keys, (uint32_t held)) \
    X(void, jiant_encoder, (enum jiant_encoder role, int32_t steps)) \
    X(void, jiant_master, (uint32_t level)) \
    X(const uint16_t *, jiant_screen, (void)) \
    X(uint32_t, jiant_screen_draws, (void)) \
    X(uint32_t, jiant_led_buttons, (void)) \
    X(uint32_t, jiant_led_keys, (void)) \
    X(size_t, jiant_state_save, (uint8_t *buf, size_t cap)) \
    X(size_t, jiant_state_max, (void)) \
    X(int, jiant_state_load, (const uint8_t *buf, size_t len)) \
    X(void, jiant_generate, (uint32_t genre, uint32_t seed, uint32_t energy, uint32_t form)) \
    X(uint32_t, jiant_genre_count, (void)) \
    X(const char *, jiant_genre_name, (uint32_t i)) \
    X(int, jiant_song_select, (uint32_t song)) \
    X(jiant_sample *, jiant_sample_make, (const float *l, const float *r, uint32_t frames, uint32_t rate, const char *name)) \
    X(jiant_sample *, jiant_sample_make_i16, (const int16_t *pcm, uint32_t ch, uint32_t frames, uint32_t rate, const char *name)) \
    X(void, jiant_sample_free, (jiant_sample *o)) \
    X(int, jiant_slot_put, (uint32_t slot, jiant_sample *o)) \
    X(int, jiant_slot_load, (uint32_t slot, const float *l, const float *r, uint32_t frames, uint32_t rate, const char *name)) \
    X(int, jiant_slot_clear, (uint32_t slot)) \
    X(int, jiant_slot_info, (uint32_t slot, jiant_slot_info_t *info)) \
    X(const int16_t *, jiant_slot_pcm, (uint32_t slot, uint32_t ch)) \
    X(void, jiant_slot_saved, (uint32_t slot, uint32_t gen)) \
    X(const jiant_sample *, jiant_slot_object, (uint32_t slot)) \
    X(const int16_t *, jiant_sample_data, (const jiant_sample *o, uint32_t ch, uint32_t *frames, uint32_t *channels)) \
    X(uint32_t, jiant_service, (void)) \
    X(void, jiant_input, (const float *l, const float *r, uint32_t n)) \
    X(int, jiant_rec_start, (uint32_t slot, uint32_t source, uint32_t bars)) \
    X(void, jiant_rec_stop, (void)) \
    X(int, jiant_rec_state, (uint32_t *slot, uint32_t *frames))

#define JIANT_API_FIELD_(ret, name, args) ret (*name) args;
typedef struct jiant_api {
    uint32_t jiant_api_size;               /* sizeof(jiant_api), as the copy was built */
    uint32_t jiant_api_index;              /* its number, 0..JIANT_COPIES-1 */
    JIANT_API_LIST(JIANT_API_FIELD_)
} jiant_api;
#undef JIANT_API_FIELD_

/* a free copy, claimed (0: all JIANT_COPIES taken); *index its number when given. Any thread */
const jiant_api *jiant_copy_claim(uint32_t *index);
/* the copy given back (0: nothing). Any thread, once its user is done with it */
void jiant_copy_release(const jiant_api *c);
/* copy k, claimed or not (0: no such copy): for tests */
const jiant_api *jiant_copy_get(uint32_t k);
uint32_t jiant_copies_free(void);

#ifdef __cplusplus
}
#endif
#endif
