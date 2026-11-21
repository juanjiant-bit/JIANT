/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The big-CPU firmware in the browser (devices.html's MPC Sample panel): ports/core as it is built for the MPC
 * (FELUCCA_HOST_POWER 1: every part its own 8 voices, the SAMPLE and GRAIN engines, DRUM-X's sample layer, 16 sample
 * slots, the recorder), compiled to WebAssembly beside felucca.wasm (the FM-1's, unchanged). Everything felucca.wasm
 * exports (worklet.js runs both alike), plus the slots' API for the page:
 *
 *   load:     p = web_alloc(bytes); the page's float PCM (44.1 kHz) there; o = web_sample_make(l, r, frames, rate);
 *             web_slot_put(slot, o) (a pointer swap, as the MPC's audio thread does); web_service() frees what the
 *             slots let go of and keeps the recorder's next object ready (the worklet calls it between render quanta,
 *             never inside one: no allocation in the audio callback)
 *   read:     web_slot_info(slot) -> {frames, channels, name[32], gen, changed}; web_slot_pcm(slot, ch) int16
 *   record:   web_input_l/r() + web_input(n) each quantum before web_render; web_rec_start / stop / state
 *
 * The device clock is the audio's, as in felucca.wasm (no JIANT_REAL_LOAD: the browser's worklet has no fine clock).
 *   web/emu/build.sh  ->  build/emu/felucca_hp.wasm */
#define FELUCCA_HOST_POWER 1
#define JIANT_NO_REAL_LOAD 1
#include "../../ports/core/jiant_core.c"

EXPORT uint32_t web_host_power(void) { return 1u; }
EXPORT void *web_alloc(uint32_t bytes) { return malloc(bytes ? bytes : 1u); }
EXPORT void web_free(void *p) { free(p); }

static char web_name[64];
EXPORT char *web_name_buf(void) { return web_name; }     /* the next sample's name (NUL-terminated, 31 kept) */
EXPORT jiant_sample *web_sample_make(const float *l, const float *r, uint32_t frames, uint32_t rate)
{
    web_name[sizeof web_name - 1u] = 0;
    return jiant_sample_make(l, r, frames, rate, web_name);
}
EXPORT void web_sample_free(jiant_sample *o) { jiant_sample_free(o); }
EXPORT uint32_t web_slot_put(uint32_t slot, jiant_sample *o) { return (uint32_t)jiant_slot_put(slot, o); }
EXPORT uint32_t web_slot_clear(uint32_t slot) { return (uint32_t)jiant_slot_clear(slot); }
EXPORT uint32_t web_service(void) { return jiant_service(); }
EXPORT uint32_t web_slot_count(void) { return JIANT_SLOTS; }

static jiant_slot_info_t web_info;                        /* frames 0, channels 4, name 8..39, gen 40, changed 44 */
EXPORT jiant_slot_info_t *web_slot_info(uint32_t slot)
{
    if (jiant_slot_info(slot, &web_info))
        memset(&web_info, 0, sizeof web_info);
    return &web_info;
}
EXPORT const int16_t *web_slot_pcm(uint32_t slot, uint32_t ch) { return jiant_slot_pcm(slot, ch); }
EXPORT void web_slot_saved(uint32_t slot, uint32_t gen) { jiant_slot_saved(slot, gen); }

static float web_in_l[1024], web_in_r[1024];
EXPORT float *web_input_l(void) { return web_in_l; }
EXPORT float *web_input_r(void) { return web_in_r; }
EXPORT void web_input(uint32_t n) { jiant_input(web_in_l, web_in_r, n > 1024u ? 1024u : n); }

/* source 0 the input (MIC / LINE), 1 the master (RESAMPLE), 2 + k track k; bars 0: until web_rec_stop (60 s) */
EXPORT uint32_t web_rec_start(uint32_t slot, uint32_t source, uint32_t bars) { return (uint32_t)jiant_rec_start(slot, source, bars); }
EXPORT void web_rec_stop(void) { jiant_rec_stop(); }
static uint32_t web_rec_s, web_rec_f;
EXPORT uint32_t web_rec_state(void) { return (uint32_t)jiant_rec_state(&web_rec_s, &web_rec_f); }   /* 0 idle 1 armed 2 rec */
EXPORT uint32_t web_rec_slot(void) { return web_rec_s; }
EXPORT uint32_t web_rec_frames(void) { return web_rec_f; }

/* for the tests (web/emu/browser_mpc_test.mjs): track t on engine e (the audio side switches: render a little), then
 * its preset p, as SELECT + PRESETS would */
EXPORT void web_test_engine(uint32_t t, uint32_t e)
{
    if (t < NTRK && e < NENGINES)
        set_engine_of(&trk[t], e);
    ui.force = 1;
}
EXPORT void web_test_preset(uint32_t t, uint32_t p)
{
    if (t < NTRK)
        apply_preset_to(&trk[t], p);
    ui.force = 1;
}
EXPORT uint32_t web_test_track_engine(uint32_t t) { return t < NTRK ? trk_eng(&trk[t]) : 255u; }
EXPORT uint32_t web_cpu_percent(void) { return jiant_cpu_percent(); }
