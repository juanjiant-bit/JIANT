/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Two audible tracks across the real ISR admission and recovery path. */
#define HOST_BAL 0                      /* (V10.7.30) the master's BALANCE off: the USB and DAC levels exact */
#define main hostsim_main
#define HOST_OWN_TICKS 1                    /* (fm1_ticks below) */
#include "hostsim.c"
#undef main

#define FM1_AUDIO_HALF 0x80u
#define FM1_TICKS_PER_US 1u
static uint32_t host_clock, host_us, host_half_reads, host_dma_advanced, host_acks, host_nest;
static uint8_t host_pending;
static volatile uint32_t t5_nested_ticks;              /* audio.c's: TIMER5 nested in the render (main.c) */
static uint32_t fm1_ticks(void) { t5_nested_ticks = host_nest; host_clock += host_us; return host_clock; }
static uint8_t fm1_audio_pending(void) { return host_pending; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return host_dma_advanced && host_half_reads++ > 0u; }
static void fm1_audio_ack_half(void) { host_acks++; }
static void fm1_audio_init(int32_t *b, uint32_t n, void (*isr)(void), uint32_t p)
{ (void)b; (void)n; (void)isr; (void)p; }
void isr_alnk0(void) {}
#define FELUCCA_UAC 1                                  /* audio.c taps USB audio: here into host_tap */
static int32_t host_tap[2 * 64];
static uint32_t host_tap_n;
static void uac_tap(const int32_t *out, uint32_t n) { memcpy(host_tap, out, 8u * n); host_tap_n = n; }
static uint32_t host_acks_at_start;
static void uac_render_start(void) { host_acks_at_start = host_acks; }   /* (just before the blocks) */
static void uac_render_end(void) {}
#include "../firmware/src/audio.c"
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t host_pot = 1023;                        /* the MASTER pot as the ADC reads it (-1: timeout) */
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? host_pot : 620; }
#include "../firmware/src/master.c"


/* Only two audible tracks, even when TRACKS is set to six. Exercise the real
 * shed_check path: the previous kit test forced voice_headroom on and missed
 * its permanent collapse after a transient. Percentages below are simulated
 * deadline measurements, not a CPU benchmark of the physical FM-1. */
static int32_t out[CTL * 2];
static unsigned failed, synth, notes, checked;
static const unsigned chord[] = {48, 52, 55, 59};
static void check(const char *tag, int ok)
{
    checked++;
    if (!ok && failed++ < 20)
        printf("FAIL tracks=%u synth=%u notes=%u: %s\n", trk_live, synth + 1, notes, tag);
}
static void init(unsigned tracks)
{
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song); memset(&chain, 0, sizeof chain);
    memset(drum_kit, 0, sizeof drum_kit);
    host_tracks_init(); trk_live = tracks; sys_load = LOAD_FULL; mc_host = 1; mc_off = 0;
    voice_headroom = poly_calm = load_tight = guard_cap = guard_run = 0; guard_calm = 0;
    vh_held = vh_wait = vh_calm = frz_on = ev_off = 0;
    memset(vh_c, 0, sizeof vh_c); t5_nested_ticks = 0;
    load_avg_q8 = load_all_q8 = load_pred_q8 = load_fx_q8 = 0;
    memset(mc_tk, 0, sizeof mc_tk); memset(mc_nv, 0, sizeof mc_nv);
    for (unsigned t = 0; t < NTRK; t++) {
        host_preset(&trk[t], t ? ENGI_ANALOG : ENGI_DRUM, 0);
        trk[t].p[P_VOICE] = t && notes == 1 ? V_MONO : V_POLY;
        trk[t].p[P_VCAP] = trk[t].p[P_DDST] = 0;
        trk[t].p[P_SUS] = 127; trk[t].p[P_REL] = 50;
    }
}
static unsigned count(unsigned t)
{
    unsigned n = 0;
    for (unsigned i = 0; i < NVOICE; i++) n += trk[t].v[i].active && trk[t].v[i].stage != 4;
    return n;
}
static int live_note(unsigned t, unsigned note)
{
    for (unsigned i = 0; i < NVOICE; i++) {
        voice_t *v = &trk[t].v[i];
        if (v->active && v->stage != 4 && v->note == note) return 1;
    }
    return 0;
}
static void halves(unsigned n, unsigned pct)
{
    for (unsigned b = 0; b < n; b++) {
        mix_block(out, CTL);
        shed_check(HALF_TICKS * pct / 100);
    }
}
static void phrase(unsigned step)
{
    for (unsigned t = 0; t < NTRK; t++)
        for (unsigned i = 0; i < NVOICE; i++)
            if (trk[t].v[i].gate) trk_note_off(&trk[t], trk[t].v[i].note);
    ev_off = step % 3 ? 17 : 0;
    for (unsigned pass = 0; pass < 2; pass++) {
        if ((pass ^ step) & 1) {
            for (unsigned j = 0; j < notes; j++) trk_note_on(&trk[synth], chord[j] + step % 3 * 2, 100);
        } else {
            for (unsigned l = 0; l < DV_NLANE; l++) trk_note_on(&trk[0], DRUM_LANE_NOTE[l], 110);
        }
    }
    ev_off = 0;
    for (unsigned l = 0; l < DV_NLANE; l++) check("kit onset", live_note(0, DRUM_LANE_NOTE[l]));
    for (unsigned j = 0; j < notes; j++) check("synth onset", live_note(synth, chord[j] + step % 3 * 2));
    halves(1, 55);
    for (unsigned l = 0; l < DV_NLANE; l++) check("kit rendered", live_note(0, DRUM_LANE_NOTE[l]));
    halves(63, 55);
    for (unsigned j = 0; j < notes; j++) check("sustained synth", live_note(synth, chord[j] + step % 3 * 2));
}
int main(void)
{
    for (unsigned tracks = 4; tracks <= 6; tracks += 2)
        for (synth = 1; synth < tracks; synth++)
            for (notes = 1; notes <= 4; notes += 3) {
                unsigned before = failed;
                init(tracks); halves(32, 35); phrase(0);
                halves(128, 95);   /* overload with no new notes; recovery must not stay at eight */
                check("real overload engages guard", load_tight);
                check("guard never expands beyond the sounding work", voice_cap() <= 8 + notes);
                halves(6000, 55);
                check("guard recovers at moderate measured load", !load_tight);
                check("shared ceiling returns", voice_cap() == (tracks == 6 ? 16 : 12));
                unsigned kills = voice_kills;
                for (unsigned step = 0; step < 16; step++) phrase(step);
                check("no recovery steals or fades", voice_kills == kills);
                printf("two tracks: TRACKS=%u ANALOG=%u %s missing=%u kit=%u synth=%u cap=%u\n",
                       tracks, synth + 1, notes == 4 ? "POLY" : "MONO", failed - before, count(0), count(synth), voice_cap());
            }
    notes = 4; synth = 1;
    init(6); load_all_q8 = load_avg_q8 = load_pred_q8 = 55u << 8;
    halves(16, 55);
    check("startup at moderate load can earn the larger ceiling", voice_cap() == 16);
    init(6); halves(16, 70);
    check("a cold average cannot hide an already busy deadline", !voice_headroom);
    init(6); halves(32, 35); phrase(0); halves(128, 95);
    unsigned busy = voices_busy();
    trk_note_on(&trk[2], 72, 100);
    check("overload blocks a new track despite earned ceiling", !live_note(2, 72) && voices_busy() == busy);
    halves(32, 35); mc_host = 0; shed_check(0);
    check("core fault clears earned ceiling", !voice_headroom && voice_cap() <= 8);
    printf("two-track admission: %u checks, %u failures\n", checked, failed);
    return !!failed;
}
