/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Nonzero per-part timing through the production mixer and admission path. */
#define HOST_BAL 0                      /* (V10.7.30) the master's BALANCE off: the USB and DAC levels exact */
#include <stdint.h>
static uint32_t cost_clock(void *);
#define MC_PART_TICKS(t) cost_clock(t)
#define main hostsim_main
#define HOST_OWN_TICKS 1                    /* (fm1_ticks below) */
#include <hostsim.c>
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
#include <audio.c>
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t host_pot = 1023;                        /* the MASTER pot as the ADC reads it (-1: timeout) */
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? host_pot : 620; }
#include <master.c>



/* Deterministic, nonzero per-part wall times. Audio, allocation and timing
 * smoothing are production code; these are simulated device ticks, not host
 * benchmark percentages. Each track costs 80 ticks + voice_ticks per sounding voice; eligible half-rate
 * work saves 25% in this conservative synthetic model. */
static uint32_t tick, pending, cost, core_ticks[2], voice_ticks = 1000, base_pct = 30;
static uint32_t cost_clock(void *ptr)
{
    track_t *t = ptr;
    if (!pending) {
        cost = 80;
        unsigned ti = (unsigned)(t - trk);
        for (unsigned j = 0; j < NVOICE; j++) if (t->v[j].active) {
            unsigned reduced = ENGINES[t->engine]->half && (((vh_held >> ti) & 1) || VH_ST(vh_c[ti * NVOICE + j]));
            cost += reduced ? voice_ticks * 3 / 4 : voice_ticks;
        }
        pending = 1;
    } else {
        tick += cost;
        core_ticks[MC_CPU() & 1] += cost;
        pending = 0;
    }
    return tick;
}
static int32_t out[2 * CTL];
static unsigned missing[6], requested[6], peak, low = 100, active_tracks = 3;
static unsigned present(unsigned t, unsigned note)
{
    for (unsigned i = 0; i < NVOICE; i++)
        if (trk[t].v[i].active && trk[t].v[i].stage != 4 && trk[t].v[i].note == note) return 1;
    return 0;
}
static void note(unsigned t, unsigned n)
{
    trk_note_on(&trk[t], n, 110);
    requested[t]++;
    if (!present(t, n)) {
        missing[t]++;
        if (missing[t] < 5) printf("MISS t%u note%u cpu%u predict%u cost%u ticks%u nv%u cap%u used%u\n",t,n,load_all_q8>>8,load_pred_q8>>8,load_cost_q8(&trk[t])>>8,mc_tk[t],mc_nv[t],voice_cap(),voices_busy());
    }
}
static void block(void)
{
    core_ticks[0] = core_ticks[1] = 0;
    mix_block(out, CTL);
    unsigned pct = base_pct + (core_ticks[0] > core_ticks[1] ? core_ticks[0] : core_ticks[1]) * 100 / (CTL * LOAD_FRAME_TICKS);
    if (pct > peak) peak = pct;
    if (pct < low) low = pct;
    shed_check(HALF_TICKS * pct / 100);
}
static int cost_units(void)
{
    host_tracks_init(); trk_live=6; sys_load=LOAD_FULL; mc_host=voice_headroom=load_cores2=1;
    host_preset(&trk[0],ENGI_ANALOG,0);
    trk[0].p[P_VOICE]=V_POLY;trk[0].p[P_VCAP]=9;trk[0].p[P_SUS]=127;
    for(unsigned j=0;j<8;j++)trk_note_on(&trk[0],48+j,100);
    for(unsigned b=0;b<128;b++)block();
    unsigned before=load_cost_q8(&trk[0]);
    for(unsigned j=1;j<8;j++)trk[0].v[j].active=trk[0].v[j].gate=0;
    block();
    unsigned after=load_cost_q8(&trk[0]);
    printf("matched measurement 8 -> 1: cost Q8 %u -> %u, ticks=%u nv=%u\n",before,after,mc_tk[0],mc_nv[0]);
    return after > before * 5 / 4;
}
int main(int argc, char **argv)
{
    if(argc>1&&!strcmp(argv[1],"units"))return cost_units();
    if (argc > 1) active_tracks = atoi(argv[1]);
    if (argc > 2) base_pct = atoi(argv[2]);
    if (argc > 3) voice_ticks = atoi(argv[3]);
    host_tracks_init(); trk_live = 6; sys_load = LOAD_FULL; mc_host = 1;
    for (unsigned t = 0; t < 6; t++) {
        host_preset(&trk[t], t ? ENGI_ANALOG : ENGI_DRUM, 0);
        trk[t].p[P_VOICE] = t > 1 ? V_MONO : V_POLY;
        trk[t].p[P_VCAP] = trk[t].p[P_DDST] = 0;
        trk[t].p[P_SUS] = 127; trk[t].p[P_REL] = 5;
    }
    for (unsigned b = 0; b < 256; b++) block();
    for (unsigned step = 0; step < 128; step++) {
        if (step == 0) for (unsigned l = 0; l < 8; l++) note(0, DRUM_LANE_NOTE[l]);
        else {
            note(0, 36);
            if (step % 2 == 0) {note(0, 38); note(0, 39);}
            note(0, 42);
        }
        if (active_tracks > 1 && step % 4 == 0) {
            for (unsigned j = 0; j < NVOICE; j++) if (trk[1].v[j].gate) trk_note_off(&trk[1], trk[1].v[j].note);
            for (unsigned j = 0; j < 4; j++) note(1, 48 + j * 4 + step % 3);
        }
        for (unsigned t = 2; t < active_tracks; t++) note(t, 60 + t + step % 3);
        for (unsigned b = 0; b < 86; b++) {
            if (b == 24 && active_tracks > 1) for (unsigned j = 0; j < NVOICE; j++) if (trk[1].v[j].gate) trk_note_off(&trk[1], trk[1].v[j].note);
            block();
            if(b<24 && step%4==0 && active_tracks>1)for(unsigned j=0;j<4;j++)
                if(!present(1,48+j*4+step%3))missing[1]++;
        }
    }
    unsigned fail = 0;
    for (unsigned t = 0; t < active_tracks; t++) {printf("t%u missing %u/%u\n", t, missing[t], requested[t]);fail += missing[t];}
    printf("load %u..%u guard%u halfmask%u\n",low,peak,load_tight,vh_held);
    return !!fail;
}
