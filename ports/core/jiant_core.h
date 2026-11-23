/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* JIANT's core outside the FM-1: the whole firmware (engines, sequencer, songs, generator, FX, the UI and its
 * 240 x 240 screen, projects in a simulated flash) as one C library, for every port that is not the FM-1 itself:
 * the MPC plugin (ports/mpc-plugin), the MPC Sample app and the desktop builds. It is the browser emulator's host
 * layer (web/emu/felucca_web.c) behind a stable API: the same sources, so a port plays what the FM-1 and the
 * emulator play, sample for sample.
 *
 * One device per process: the firmware keeps its state in globals (several: jiant_copies.h, the core compiled once per
 * device). Not thread safe: call everything from one
 * thread at a time (a plugin calls it from its audio thread and hands state requests over; ports/mpc-plugin).
 *
 *   boot:    jiant_boot(seed, 1)            (1: the power-on splash runs here, not in the first seconds of audio)
 *   audio:   jiant_render(l, r, n)          any n, at JIANT_RATE (44.1 kHz stereo float)
 *   play:    jiant_midi(...), jiant_buttons(...), jiant_keys(...), jiant_encoder(...)
 *   see:     jiant_screen() (RGB565), jiant_screen_draws()
 *   keep:    jiant_state_save(...), jiant_state_load(...) */
#ifndef JIANT_CORE_H
#define JIANT_CORE_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JIANT_RATE 44100u
#define JIANT_SCREEN_W 240u
#define JIANT_SCREEN_H 240u
#define JIANT_KEYS 27u                     /* F3 .. G5, bit k = key k */

/* the FM-1's buttons by their printed labels (bit b of jiant_buttons; firmware/src/panel.c) */
enum jiant_button {
    JIANT_B_FX, JIANT_B_SCL, JIANT_B_ENV, JIANT_B_LFO, JIANT_B_EDIT, JIANT_B_GLO, JIANT_B_HOME, JIANT_B_SAVE,
    JIANT_B_ARP, JIANT_B_SEQ, JIANT_B_PLAY, JIANT_B_REC, JIANT_B_OCTDN, JIANT_B_OCTUP, JIANT_NBUTTONS
};
/* the encoders by role (firmware/src/panel.c EN_*) */
enum jiant_encoder {
    JIANT_E_SELECT, JIANT_E_ALGO, JIANT_E_PRESET, JIANT_E_K1, JIANT_E_K2, JIANT_E_K3, JIANT_E_K4, JIANT_NENCODERS
};

const char *jiant_version(void);

/* power on: an erased flash (the power-on parts), this session's seed (the macros' dice); skip_splash 1 runs the
 * splash's ~1.7 s here so the first rendered audio already answers the keys. Once per process. 0 = booted. */
int jiant_boot(uint32_t seed, int skip_splash);
/* the device's own flash (1 MiB NOR: settings, songs, projects, user presets), for a port that keeps it in a file:
 * jiant_flash_restore before jiant_boot gives back a saved image (it boots from it, as the FM-1 does from its flash;
 * 0 done, 1 too late or the wrong size); jiant_flash is the image now, jiant_flash_writes counts what the firmware
 * wrote to it (a change: time to save it, once the firmware is done, as the browser emulator does) */
int jiant_flash_restore(const uint8_t *image, size_t size);
const uint8_t *jiant_flash(size_t *size);
uint32_t jiant_flash_writes(void);
/* the autosave (project.c, MENU > SYSTEM > RESTORE LAST) written now when the music changed since the last one, not
 * after its 10 s of quiet: for a port about to close (the transport is stopped first). 1 written, 0 nothing to do. */
int jiant_autosave_now(void);
int jiant_booted(void);

/* n frames at JIANT_RATE into l and r (any n, either pointer may be 0 to drop the channel) */
void jiant_render(float *l, float *r, uint32_t n);

/* a MIDI message as USB MIDI brings it: channel messages (0x80..0xEF) and the realtime clock (0xF8 clock, 0xFA
 * start, 0xFB continue, 0xFC stop: they count only with jiant_clock_external(1)). 0 = the input ring was full. */
int jiant_midi(uint8_t status, uint8_t d1, uint8_t d2);
/* the web editor (web/editor.html, web/EDITOR_PROTOCOL.md) over the host's MIDI, as the FM-1 answers it over USB: the
 * firmware's own handler. jiant_sysex takes one complete request (F0 7D 46 4C .. F7): 0 handled (its reply is queued),
 * 1 not the editor's (another SysEx: the host may route it elsewhere). jiant_sysex_out gives the next queued message
 * (a reply, or a live push while the editor WATCHes) into buf, its length; 0 none; more than cap: the length it needs,
 * nothing taken. jiant_editor_poll (every ~10 ms, or after each jiant_render) lets the device push what changed on its
 * panel. jiant_editor_host says which device answers (INFO's 'D' tag, the editor's label): 3 a computer (the default:
 * a plugin, the standalone app), 4 the MPC Sample app, 5 the MPC plugin. All on the core's thread, as the rest */
int jiant_sysex(const uint8_t *msg, size_t n);
size_t jiant_sysex_out(uint8_t *buf, size_t cap);
void jiant_editor_poll(void);
void jiant_editor_host(uint32_t board);
/* 1: the sequencer follows the MIDI clock given to jiant_midi (GLO > CLOCK USB); 0: its own tempo */
void jiant_clock_external(int on);
void jiant_play(int on);                   /* PLAY / STOP, as the button does */
int jiant_playing(void);
uint32_t jiant_bpm(void);                  /* the tempo, BPM (its own, or the last the clock gave) */

/* the load, measured on this computer's CPU in real time (the firmware's own meter: a half's render against its
 * 2.9 ms): now (averaged over ~93 ms) and the worst half since boot, in %; the voices the overload guard faded and
 * whether the load governor has the sound lighter now (fewer voices shared, or LOAD LITE): 0 on a CPU with room */
uint32_t jiant_cpu_percent(void);
uint32_t jiant_cpu_peak_percent(void);
uint32_t jiant_voices_shed(void);
int jiant_load_governed(void);
uint32_t jiant_voices_sounding(void);
/* the emulator's bench song into the tracks (web/emu felucca_web.c web_test_heavy): FM6 PAD, TRIO and VOICE CHOIR
 * holding 4-note chords on every step, and a drum track; PLAY is the caller's. For the tests and --bench */
void jiant_test_heavy(void);

/* the panel: the buttons held now (bits by enum jiant_button), the keys held now (bit k = key k), detents turned */
void jiant_buttons(uint32_t held);
void jiant_keys(uint32_t held);
void jiant_encoder(enum jiant_encoder role, int32_t steps);
void jiant_master(uint32_t level);         /* the MASTER knob, 0..1023 */

/* the screen: 240 x 240 RGB565, high byte first as the panel takes it; draws counts the changes */
const uint16_t *jiant_screen(void);
uint32_t jiant_screen_draws(void);
/* LEDs: bit b = button b lit (bit 14: PLAY's green), keys bit k */
uint32_t jiant_led_buttons(void);
uint32_t jiant_led_keys(void);

/* the music (the live section, the songs, their index, the user presets and their FM6 patches) as one block, the
 * objects the editor's backup carries: jiant_state_save writes it to buf (cap bytes) and returns its length, or the
 * length it needs when cap is short (buf may be 0). jiant_state_load stops the transport and restores it: 0 done,
 * 1 not a JIANT state, 2 damaged, 3 an object refused (the others restored). */
size_t jiant_state_save(uint8_t *buf, size_t cap);
size_t jiant_state_max(void);              /* the most jiant_state_save can need */
int jiant_state_load(const uint8_t *buf, size_t len);

/* the song generator (SONG > GENE): genre 0 RANDOM, 1.. a genre, seed 0..9999, energy 0..100, form 0 LOOP 1 PARTS
 * 2 SONG; its name after */
void jiant_generate(uint32_t genre, uint32_t seed, uint32_t energy, uint32_t form);
uint32_t jiant_genre_count(void);
const char *jiant_genre_name(uint32_t i);
int jiant_song_select(uint32_t song);      /* 0..: 0 = done (stopped first) */

/* ------------------------------------------------------------------ samples (16 slots) ---
 * The SAMPLE and GRAIN engines and DRUM-X's sample layer play 16 slots of up to 60 s each, mono or stereo, at
 * JIANT_RATE, held by the core (firmware/src/smp_slots.c). Threads: as everything else, the slot calls below are the
 * audio thread's, except jiant_sample_make* / jiant_sample_free / jiant_service, which touch no device state the
 * audio thread writes but through atomics, and are meant for another thread (the host's loader or service thread),
 * so a big load never copies or allocates in the audio callback:
 *
 *   loader thread:  o = jiant_sample_make(l, r, frames, rate, "KICK")     (resampled to 44.1 kHz there)
 *   audio thread:   jiant_slot_put(3, o)                                  (a pointer swap)
 *   service thread: jiant_service()      every ~100 ms: frees what the slots let go of, keeps a spare object ready
 *                                        for the recorder (without it a recording has no room after the first)
 *
 * jiant_slot_info / jiant_slot_pcm read a slot for saving it: changed is 1 after a recording or DESTROY wrote it
 * since the host called jiant_slot_saved with its gen. Read the PCM from the thread that calls jiant_service (an
 * object is freed only there) and copy it before the next jiant_service. */
#define JIANT_SLOTS 16u
#define JIANT_SLOT_MAX_FRAMES (60u * JIANT_RATE)
typedef struct jiant_sample jiant_sample;
/* a sample from the host's PCM: l (and r, 0 = mono) float -1..1 at rate Hz (any: resampled), frames long (the first
 * 60 s at 44.1 kHz kept); name (31 characters). 0: no memory or no frames. Any thread */
jiant_sample *jiant_sample_make(const float *l, const float *r, uint32_t frames, uint32_t rate, const char *name);
/* .. the same from interleaved int16 (ch 1 or 2) */
jiant_sample *jiant_sample_make_i16(const int16_t *pcm, uint32_t ch, uint32_t frames, uint32_t rate, const char *name);
void jiant_sample_free(jiant_sample *o);
/* slot (0..15) now plays o (0: empty), which belongs to the core from here; what the slot held is freed by
 * jiant_service. 0 done, 1 a bad slot */
int jiant_slot_put(uint32_t slot, jiant_sample *o);
/* make + put + service at once (allocates and resamples on this thread: a test's, a host's at start). 0 done */
int jiant_slot_load(uint32_t slot, const float *l, const float *r, uint32_t frames, uint32_t rate, const char *name);
int jiant_slot_clear(uint32_t slot);
typedef struct {
    uint32_t frames;                       /* 0: empty */
    uint32_t channels;
    char name[32];
    uint32_t gen;                          /* changed: gen != the last jiant_slot_saved's */
    int changed;
} jiant_slot_info_t;
int jiant_slot_info(uint32_t slot, jiant_slot_info_t *info);
/* the slot's PCM as it plays (DESTROY's working copy), channel ch (0 L, 1 R; mono: both the same), int16; 0 empty */
const int16_t *jiant_slot_pcm(uint32_t slot, uint32_t ch);
void jiant_slot_saved(uint32_t slot, uint32_t gen);
/* the object slot holds now (0: empty), and an object's PCM: channel ch's plane (as it plays), its frames and channels.
 * For the service thread (a host saving a slot: the object stays whole until its own next jiant_service), or a host
 * playing an object it made (an audition) */
const jiant_sample *jiant_slot_object(uint32_t slot);
const int16_t *jiant_sample_data(const jiant_sample *o, uint32_t ch, uint32_t *frames, uint32_t *channels);
/* thread safe (above): the objects freed */
uint32_t jiant_service(void);

/* the device's audio input, stereo at JIANT_RATE, each block before jiant_render (recording, GRAIN's LIVE) */
void jiant_input(const float *l, const float *r, uint32_t n);

/* recording into a slot: source JIANT_REC_INPUT, JIANT_REC_MASTER (JIANT's own output: resampling) or
 * JIANT_REC_TRACK1 + k (track k's output); bars at the tempo, from the next bar (at once when stopped), 0 = until
 * jiant_rec_stop (or 60 s). 0 armed; 1 a recording under way; 2 no spare object yet (jiant_service); 3 bad arguments.
 * The engines' REC (SAMPLE, GRAIN) arm the same recorder from inside JIANT */
enum { JIANT_REC_INPUT, JIANT_REC_MASTER, JIANT_REC_TRACK1 };
int jiant_rec_start(uint32_t slot, uint32_t source, uint32_t bars);
void jiant_rec_stop(void);
/* 0 idle, 1 armed (waiting for the bar), 2 recording; *slot its slot, *frames recorded so far (either may be 0) */
int jiant_rec_state(uint32_t *slot, uint32_t *frames);

#ifdef __cplusplus
}
#endif
#endif
