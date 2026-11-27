/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* JIANT's core outside the FM-1 (jiant_core.h): the browser emulator's host layer, built natively, behind the
 * stable API. Nothing of the firmware or of the emulator changes for this file.
 *   cc -O2 -ffp-contract=off -std=gnu99 -w -Ibuild/gen -Ifirmware/src -c ports/core/jiant_core.c
 * (after the generated headers: python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()") */
/* a computer's CPU (core.h FELUCCA_HOST_POWER), its real time in the load (felucca_web.c JIANT_REAL_LOAD) */
#ifndef FELUCCA_HOST_POWER
#define FELUCCA_HOST_POWER 1                  /* (0: ports/core/build.sh test's FM-1 personality only) */
#endif
#ifndef JIANT_NO_REAL_LOAD                   /* (web/emu/felucca_hp.c: the browser's clock is its audio's, as the emulator) */
#define JIANT_REAL_LOAD 1
#endif
#include <stdint.h>
#include <time.h>
#if defined(_WIN32)
/* Windows: no clock_gettime (the host tests use it too) and no <windows.h> here (its macros meet the firmware's names):
 * C11's timespec_get */
#define CLOCK_MONOTONIC 1
static int clock_gettime(int id, struct timespec *ts)
{
    (void)id;
    return timespec_get(ts, TIME_UTC) ? 0 : -1;
}
uint32_t jiant_host_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u);
}
#else
uint32_t jiant_host_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint32_t)((uint64_t)ts.tv_sec * 1000000u + (uint64_t)ts.tv_nsec / 1000u);
}
#endif
/* the host tests' main (tests/hostsim.c, never called here) forks its renders: where a process may not fork (iOS) or
 * has no POSIX (Windows), those calls are stubbed (ports/core/compat for Windows' headers) */
#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <unistd.h>
#include <sys/wait.h>
#define fork() (-1)
#endif
#endif
#include "../../web/emu/felucca_web.c"
#include "jiant_core.h"

#if defined(__GNUC__)
#define JC_API __attribute__((visibility("default")))
#else
#define JC_API
#endif

JC_API const char *jiant_version(void) { return FELUCCA_VERSION; }

static int jc_restored;
JC_API int jiant_flash_restore(const uint8_t *image, size_t size)
{
    if (web_booted || !image || size != sizeof nor)
        return 1;
    memcpy(nor, image, sizeof nor);
    jc_restored = 1;
    return 0;
}
JC_API const uint8_t *jiant_flash(size_t *size)
{
    if (size)
        *size = sizeof nor;
    return nor;
}
JC_API uint32_t jiant_flash_writes(void) { return web_flash_writes; }

JC_API int jiant_autosave_now(void)
{
    uint32_t sig;
    if (!web_booted || !flash_ok || !(ui_prefs & PREF_RESTORE_ON))
        return 0;
    if (song.playing)
        seq_stop();
    sig = autosave_sig();
    if (sig == as.saved)
        return 0;
    project_capture(&proj_scratch);                       /* (project.c autosave_poll's write) */
    proj_wire_gen++;
    as.last = fm1_ms;
    as.wrote = 1;
    as.err = !proj_pack(&proj_wire, &proj_scratch) || st_save(OBJ_AUTOSAVE, &proj_wire, sizeof proj_wire) != 0;
    if (as.err)
        return 0;
    as.saved = as.seen = sig;
    as.writes++;
    return 1;
}

#if FELUCCA_HOST_POWER
JC_API uint32_t jiant_service(void);
#endif
JC_API int jiant_boot(uint32_t seed, int skip_splash)
{
    if (web_booted)
        return 1;
    if (!jc_restored)
        web_nor_erase();
    web_seed(seed ? seed : 1u);
    web_boot();
#if FELUCCA_HOST_POWER
    jiant_service();                      /* (the recorder's first room: a host that never calls jiant_service records once) */
#endif
    if (skip_splash) {                    /* the splash and the boot's 30 ms, rendered and dropped */
        while (web_boot_ms)
            web_render(128);
        ring_r = ring_w;
    }
    return 0;
}
JC_API int jiant_booted(void) { return (int)web_booted; }

#ifndef JIANT_NO_EDITOR
/* ------------------------------------------------------------------ the web editor's SysEx (jiant_core.h) ---
 * The firmware's own handler (firmware/src/editor.c, web/EDITOR_PROTOCOL.md), as the FM-1 runs it over USB: a request
 * goes in whole (jiant_sysex), the replies and the live pushes come out whole (jiant_sysex_out). In place of usb.c's
 * SysEx plumbing (FELUCCA_OTA: the firmware updates, never here): one frame in, a ring of messages out */
static uint8_t jc_ed_in[640];
static uint32_t jc_ed_in_n, jc_ed_ready, jc_ed_on;
static uint8_t jc_ed_out[16384];                           /* F0..F7 messages, back to back */
static uint32_t jc_ed_w, jc_ed_r;                          /* (free-running) */
#if !FELUCCA_OTA
#define SXQ 64u                                            /* (usb.c's outgoing ring, which editor.c ed_room asks: never full here) */
static uint32_t so_w, so_r;
#endif
static uint32_t ota_now_ms(void) { return fm1_ms; }
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    if (n > sizeof jc_ed_out - (jc_ed_w - jc_ed_r))
        return -1;                                          /* (nobody reads: dropped whole) */
    for (i = 0; i < n; i++)
        jc_ed_out[(jc_ed_w + i) % sizeof jc_ed_out] = p[i];
    jc_ed_w += n;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!jc_ed_ready)
        return 0;
    *p = jc_ed_in;
    *n = jc_ed_in_n;
    return 1;
}
static void ota_frame_done(void) { jc_ed_ready = 0; }
#include "../../firmware/src/editor.c"

JC_API int jiant_sysex(const uint8_t *msg, size_t n)
{
    if (!web_booted || !msg || n < 6u || msg[0] != 0xF0u || msg[n - 1u] != 0xF7u || msg[1] != ED_HDR0 || msg[2] != ED_HDR1 ||
        msg[3] != ED_HDR2 || n - 2u > sizeof jc_ed_in)
        return 1;
    if (!jc_ed_on) {                                        /* the editor's link is up (editor.c ed_sync: WATCH) */
        jc_ed_on = 1;
        usb.config = 1;
    }
    memcpy(jc_ed_in, msg + 1, n - 2u);
    jc_ed_in_n = (uint32_t)(n - 2u);
    jc_ed_ready = 1;
    ed_service();
    jc_ed_ready = 0;
    return 0;
}
JC_API void jiant_editor_poll(void)
{
    if (jc_ed_on && web_booted)
        ed_service();                                       /* the live pushes (WATCH) */
}
JC_API size_t jiant_sysex_out(uint8_t *buf, size_t cap)
{
    uint32_t n = 0;
    if (jc_ed_r == jc_ed_w)
        return 0;
    while (jc_ed_r + n != jc_ed_w && jc_ed_out[(jc_ed_r + n) % sizeof jc_ed_out] != 0xF7u)
        n++;
    n++;                                                    /* (its F7) */
    if (!buf || n > cap)
        return n;                                           /* the length it needs */
    for (uint32_t i = 0; i < n; i++)
        buf[i] = jc_ed_out[(jc_ed_r + i) % sizeof jc_ed_out];
    jc_ed_r += n;
    return n;
}
JC_API void jiant_editor_host(uint32_t board)
{
#if FELUCCA_HOST_POWER
    ed_board = (uint8_t)(board ? board : 3u);
#else
    (void)board;
#endif
}
#endif

JC_API void jiant_render(float *l, float *r, uint32_t n)
{
    while (n) {
        uint32_t m = n < 1024u ? n : 1024u;
        web_render(m);
        if (l) {
            memcpy(l, out_l, m * sizeof *l);
            l += m;
        }
        if (r) {
            memcpy(r, out_r, m * sizeof *r);
            r += m;
        }
        n -= m;
    }
}

JC_API int jiant_midi(uint8_t status, uint8_t d1, uint8_t d2)
{
    if (status == 0xF8u || status == 0xFAu || status == 0xFBu || status == 0xFCu)   /* usb.c midi_in_event */
        return midi_enqueue(0xFu | (uint32_t)status << 8, 1u);
    return (int)web_midi(status, d1, d2);
}
JC_API void jiant_clock_external(int on) { song.g[G_CLOCK] = (uint8_t)(on ? 1u : 0u); }   /* 1: USB's clock */
JC_API void jiant_play(int on) { web_play((uint32_t)on); }
JC_API int jiant_playing(void) { return (int)song.playing; }
JC_API uint32_t jiant_bpm(void) { return (uint32_t)song.g[G_BPM]; }

JC_API uint32_t jiant_cpu_percent(void) { return song.cpu_q8 * 100u / 256u; }
JC_API uint32_t jiant_cpu_peak_percent(void) { return load_diag.peak; }
JC_API uint32_t jiant_voices_shed(void) { return load_diag.shed; }
JC_API int jiant_load_governed(void) { return load_tight || vh_held != 0u; }   /* (the guard tripped, voices at half the rate) */
JC_API uint32_t jiant_voices_sounding(void) { return voices_busy(); }

JC_API void jiant_test_heavy(void) { web_test_heavy(); }

JC_API void jiant_buttons(uint32_t held) { web_buttons(held); }
JC_API void jiant_keys(uint32_t held) { web_keys(held); }
JC_API void jiant_encoder(enum jiant_encoder role, int32_t steps) { web_enc((uint32_t)role, steps); }
JC_API void jiant_master(uint32_t level) { web_master(level); }

JC_API const uint16_t *jiant_screen(void) { return web_fb; }
JC_API uint32_t jiant_screen_draws(void) { return web_draws; }
JC_API uint32_t jiant_led_buttons(void) { return web_lit_buttons(); }
JC_API uint32_t jiant_led_keys(void) { return web_lit_keys(); }

/* ------------------------------------------------------------------ the state ---
 * "JNTS", version, count, then per object: id, length, its bytes; a CRC-32 of all before it last. Little endian.
 * The objects are the backup's (web/fm1backup.js; web_bk_len / web_bk_put): written in the order stems.js restores
 * them, the song index first, the songs' sections, the user presets and their FM6 patches, the live section last. */
#define JS_MAGIC 0x53544E4Au                /* "JNTS" */
#define JS_VERSION 1u
static uint32_t js_ids(uint32_t *ids)
{
    uint32_t n = 0, i;
    ids[n++] = 10u;
    for (i = 0; i < NSONG * 4u; i++)
        ids[n++] = 40u + i;
    ids[n++] = 6u;
    ids[n++] = 7u;
    ids[n++] = 9u;
    ids[n++] = 0u;
    return n;
}
static uint32_t js_crc(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu, k;
    while (n--) {
        c ^= *p++;
        for (k = 0; k < 8u; k++)
            c = c >> 1 ^ (c & 1u ? 0xEDB88320u : 0u);
    }
    return ~c;
}
static void js_put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t js_get32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
/* (web_bk_len: id 6, 7 and 9 have no reader there; the user presets and their FM6 patches are read here as
 * editor_backup.c reads them) */
static uint32_t js_len(uint32_t id)
{
    uint8_t *raw = proj_wire_u.raw;
    if (id == 6u || id == 7u) {                          /* (editor_backup.c ed_bk_src: the RAM bank) */
        if (up_bank[id - 6u].magic != UP_BANK_MAGIC)
            return 0;
        memcpy(raw, &up_bank[id - 6u], sizeof up_bank[0]);
        return (uint32_t)sizeof up_bank[0];
    }
    if (id == 9u) {
        if (!upf_valid(&upf))
            return 0;
        memcpy(raw, &upf, sizeof upf);
        return (uint32_t)sizeof upf;
    }
    return web_bk_len(id);
}

/* an object into the device: web_bk_put's, and the user presets into RAM too (editor_backup.c ed_bk_commit) */
static uint32_t js_put(uint32_t id, uint32_t len)
{
    if (id == 6u || id == 7u) {
        const up_bank_t *p = (const up_bank_t *)proj_wire_u.raw;
        uint32_t b = id - 6u;
        if (len && (len != sizeof *p || p->magic != UP_BANK_MAGIC || p->rsize != sizeof(up_rec_t) ||
                    p->nslot != UP_PER_BANK))
            return 2;
        if (len && st_save(OBJ_UPRESET0 + b, proj_wire_u.raw, len))
            return 4;
        memset(&up_bank[b], 0, sizeof up_bank[b]);
        if (len)
            memcpy(&up_bank[b], proj_wire_u.raw, len);
        up_bank_check(b, (int)len);
        up_gen++;
        return 0;
    }
    return web_bk_put(id, len);
}

JC_API size_t jiant_state_save(uint8_t *buf, size_t cap)
{
    uint32_t ids[64], n = js_ids(ids), i, count = 0;
    size_t at = 12u;
    for (i = 0; i < n; i++) {
        uint32_t len = js_len(ids[i]);
        if (!len)
            continue;
        if (buf && at + 8u + len <= cap) {
            js_put32(buf + at, ids[i]);
            js_put32(buf + at + 4u, len);
            memcpy(buf + at + 8u, proj_wire_u.raw, len);
        }
        at += 8u + len;
        count++;
    }
    if (!buf || at + 4u > cap)
        return at + 4u;
    js_put32(buf, JS_MAGIC);
    js_put32(buf + 4u, JS_VERSION);
    js_put32(buf + 8u, count);
    js_put32(buf + at, js_crc(buf, at));
    return at + 4u;
}

JC_API size_t jiant_state_max(void)
{
    uint32_t ids[64];
    return 16u + (size_t)js_ids(ids) * (8u + sizeof proj_wire_u.raw);
}

JC_API int jiant_state_load(const uint8_t *buf, size_t len)
{
    uint32_t count, i;
    size_t at = 12u;
    int rc = 0;
    if (!buf || len < 16u || js_get32(buf) != JS_MAGIC || js_get32(buf + 4u) != JS_VERSION)
        return 1;
    if (js_get32(buf + len - 4u) != js_crc(buf, len - 4u))
        return 2;
    count = js_get32(buf + 8u);
    for (i = 0; i < count; i++) {                         /* every object whole before anything is written */
        uint32_t olen;
        if (at + 8u > len - 4u)
            return 2;
        olen = js_get32(buf + at + 4u);
        if (olen > sizeof proj_wire_u.raw || olen > len - 4u - at - 8u)
            return 2;
        at += 8u + olen;
    }
    if (song.playing)
        seq_stop();
    for (i = 0, at = 12u; i < count; i++) {
        uint32_t id = js_get32(buf + at), olen = js_get32(buf + at + 4u);
        memcpy(proj_wire_u.raw, buf + at + 8u, olen);
        if (js_put(id, olen))
            rc = 3;
        at += 8u + olen;
    }
    ui.force = 1;
    return rc;
}

JC_API void jiant_generate(uint32_t genre, uint32_t seed, uint32_t energy, uint32_t form)
{
    web_gen(genre, seed, energy, form);
}
JC_API uint32_t jiant_genre_count(void) { return web_gen_count(); }
JC_API const char *jiant_genre_name(uint32_t i) { return web_gen_name(i); }
JC_API int jiant_song_select(uint32_t s) { return (int)web_song_select(s); }

/* ------------------------------------------------------------------ samples (firmware/src/smp_slots.c) --- */
#if FELUCCA_HOST_POWER
#include <stdlib.h>
/* a sample object: the header and its planes (orig, work; one each a channel) in one allocation */
static smp_obj_t *jc_obj_new(uint32_t frames, uint32_t ch, const char *name)
{
    size_t plane = (size_t)frames * sizeof(int16_t);
    smp_obj_t *o;
    if (!frames || frames > SMP_MAXF || ch < 1u || ch > 2u)
        return 0;
    o = (smp_obj_t *)calloc(1, sizeof *o + 2u * ch * plane + 16u);
    if (!o)
        return 0;
    o->cap = o->frames = frames;
    o->ch = ch;
    o->orig[0] = (int16_t *)(void *)(o + 1);
    o->work[0] = o->orig[0] + (size_t)frames * ch;
    o->orig[1] = ch > 1u ? o->orig[0] + frames : o->orig[0];
    o->work[1] = ch > 1u ? o->work[0] + frames : o->work[0];
    if (name)
        str_cpy(o->name, name, sizeof o->name);
    return o;
}
/* a frame of x (n frames) at time t (its frames), band-limited to fc (of its Nyquist): a windowed sinc, 16 taps a
 * side (Blackman), normalised */
#define JC_PI 3.14159265358979323846   /* (M_PI is not in MSVC's math.h without _USE_MATH_DEFINES) */
static float jc_src_at(const float *x, uint32_t n, double t, double fc)
{
    int32_t i0 = (int32_t)floor(t), k;
    double s = 0, w = 0;
    for (k = -15; k <= 16; k++) {
        int32_t i = i0 + k;
        double d = t - (double)i, a = d * fc, h, win;
        if (i < 0 || i >= (int32_t)n || fabs(d) >= 16.0)
            continue;
        h = fabs(a) < 1e-9 ? 1.0 : sin(JC_PI * a) / (JC_PI * a);
        win = 0.42 + 0.5 * cos(JC_PI * d / 16.0) + 0.08 * cos(2.0 * JC_PI * d / 16.0);
        s += x[i] * h * win;
        w += h * win;
    }
    return (float)(w > 1e-3 ? s / w : s);
}
/* one plane of n frames at rate -> frames frames at JIANT_RATE (the host's thread) */
static void jc_plane(int16_t *d, const float *x, uint32_t n, uint32_t rate, uint32_t frames)
{
    uint32_t i;
    double step = (double)rate / (double)JIANT_RATE, fc = step > 1.0 ? 1.0 / step : 1.0;
    for (i = 0; i < frames; i++) {
        float v = rate == JIANT_RATE ? (i < n ? x[i] : 0.0f) : jc_src_at(x, n, (double)i * step, fc);
        d[i] = smp_i16(smp_finite(v) * 32768.0f);     /* (an int16 WAV through float and back: the same samples) */
    }
}
JC_API jiant_sample *jiant_sample_make(const float *l, const float *r, uint32_t frames, uint32_t rate, const char *name)
{
    uint32_t out, ch = r ? 2u : 1u;
    smp_obj_t *o;
    if (!l || !frames || rate < 1000u || rate > 768000u)
        return 0;
    out = (uint32_t)((double)frames * JIANT_RATE / rate);
    if (out > SMP_MAXF)
        out = SMP_MAXF;
    o = jc_obj_new(out ? out : 1u, ch, name);
    if (!o)
        return 0;
    jc_plane(o->orig[0], l, frames, rate, o->frames);
    if (ch > 1u)
        jc_plane(o->orig[1], r, frames, rate, o->frames);
    memcpy(o->work[0], o->orig[0], (size_t)o->frames * ch * sizeof(int16_t));
    return o;
}
JC_API jiant_sample *jiant_sample_make_i16(const int16_t *pcm, uint32_t ch, uint32_t frames, uint32_t rate, const char *name)
{
    float *l, *r = 0;
    uint32_t i;
    jiant_sample *o;
    if (!pcm || ch < 1u || ch > 2u || !frames)
        return 0;
    l = (float *)malloc((size_t)frames * sizeof *l);
    if (ch > 1u)
        r = (float *)malloc((size_t)frames * sizeof *r);
    if (!l || (ch > 1u && !r)) {
        free(l);
        free(r);
        return 0;
    }
    for (i = 0; i < frames; i++) {
        l[i] = pcm[(size_t)i * ch] / 32768.0f;
        if (r)
            r[i] = pcm[(size_t)i * ch + 1u] / 32768.0f;
    }
    o = jiant_sample_make(l, r, frames, rate, name);
    free(l);
    free(r);
    return o;
}
JC_API void jiant_sample_free(jiant_sample *o) { free(o); }
JC_API int jiant_slot_put(uint32_t slot, jiant_sample *o)
{
    if (slot >= SMP_NSLOT)
        return 1;
    smp_put(slot, o);
    return 0;
}
JC_API uint32_t jiant_service(void)
{
    uint32_t n = 0, r = __atomic_load_n(&smp_ret_r, __ATOMIC_RELAXED);
    while (r != __atomic_load_n(&smp_ret_w, __ATOMIC_ACQUIRE)) {
        free(smp_ret[r % SMP_NRET]);
        __atomic_store_n(&smp_ret_r, ++r, __ATOMIC_RELEASE);
        n++;
    }
    if (!__atomic_load_n(&smp_spare, __ATOMIC_ACQUIRE)) {   /* the recorder's next room: 60 s stereo */
        smp_obj_t *o = jc_obj_new(SMP_MAXF, 2u, "REC");
        if (o) {
            o->frames = 0;
            __atomic_store_n(&smp_spare, o, __ATOMIC_RELEASE);
        }
    }
    return n;
}
JC_API int jiant_slot_load(uint32_t slot, const float *l, const float *r, uint32_t frames, uint32_t rate, const char *name)
{
    jiant_sample *o;
    if (slot >= SMP_NSLOT)
        return 1;
    o = jiant_sample_make(l, r, frames, rate, name);
    if (!o)
        return 2;
    smp_put(slot, o);
    jiant_service();
    return 0;
}
JC_API int jiant_slot_clear(uint32_t slot) { return jiant_slot_put(slot, 0); }
JC_API int jiant_slot_info(uint32_t slot, jiant_slot_info_t *info)
{
    const smp_obj_t *o;
    if (slot >= SMP_NSLOT || !info)
        return 1;
    o = smp_get(slot);
    memset(info, 0, sizeof *info);
    if (o) {
        info->frames = o->frames;
        info->channels = o->ch;
        str_cpy(info->name, o->name, sizeof info->name);
    }
    info->gen = smp_slot[slot].gen;
    info->changed = o && smp_slot[slot].gen != smp_slot[slot].saved;
    return 0;
}
JC_API const int16_t *jiant_slot_pcm(uint32_t slot, uint32_t ch)
{
    const smp_obj_t *o = slot < SMP_NSLOT ? smp_get(slot) : 0;
    return o ? o->work[ch & 1u] : 0;
}
JC_API void jiant_slot_saved(uint32_t slot, uint32_t gen)
{
    if (slot < SMP_NSLOT)
        smp_slot[slot].saved = gen;
}
JC_API const jiant_sample *jiant_slot_object(uint32_t slot)
{
    return slot < SMP_NSLOT ? smp_get(slot) : 0;
}
JC_API const int16_t *jiant_sample_data(const jiant_sample *o, uint32_t ch, uint32_t *frames, uint32_t *channels)
{
    if (frames)
        *frames = o ? o->frames : 0u;
    if (channels)
        *channels = o ? o->ch : 0u;
    return o ? o->work[ch & 1u] : 0;
}
JC_API void jiant_input(const float *l, const float *r, uint32_t n) { smp_input(l, r, n); }
JC_API int jiant_rec_start(uint32_t slot, uint32_t source, uint32_t bars)
{
    return smp_rec_arm(slot, source, bars, SR_HOST);
}
JC_API void jiant_rec_stop(void) { smp_rec_stop(); }
JC_API int jiant_rec_state(uint32_t *slot, uint32_t *frames)
{
    if (slot)
        *slot = smp_rec.slot;
    if (frames)
        *frames = smp_rec.st == SR_REC ? smp_rec.pos : 0u;
    return smp_rec.st;
}
#endif
