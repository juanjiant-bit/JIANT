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
/* Two CTL blocks share each production DMA deadline. Convert the deterministic
 * per-part TIMER4 ticks above to the host ISR clock only after both renders. */
static void bass_block(void)
{
    static uint32_t elapsed, frames;
    core_ticks[0] = core_ticks[1] = 0;
    mix_block(out, CTL);
    uint32_t longer = core_ticks[0] > core_ticks[1] ? core_ticks[0] : core_ticks[1];
    elapsed += base_pct * CTL * LOAD_FRAME_TICKS / 100 + longer;
    frames += CTL;
    if (frames == HALF_FRAMES) {
        uint32_t pct = elapsed * 100 / (frames * LOAD_FRAME_TICKS);
        if (pct > peak) peak = pct;
        if (pct < low) low = pct;
        shed_check(elapsed * HALF_TICKS / (frames * LOAD_FRAME_TICKS));
        frames = elapsed = 0;
    }
}

static int audible_bass(void)
{
    for (unsigned j = 0; j < CTL; j++)
        if (stem_buf[1][j] > 32 || stem_buf[1][j] < -32) return 1;
    return 0;
}

static int bass_phrase(unsigned tracks, unsigned live_base)
{
    static const uint8_t drums[] = {36, 38, 39, 42};
    unsigned onsets = 0, lost = 0, windows = 0, silent = 0, hits = 0, lost_hits = 0, reduced = 0;
    int previous = -1;
    host_tracks_init();
    trk_live = tracks;
    mc_host = fx_stems = 1;
    sys_load = LOAD_FULL;
    host_preset(&trk[0], ENGI_DRUM, 0);
    dx_kit_load_of(&trk[0], 0);
    host_preset(&trk[1], ENGI_ANALOG, 14);   /* FAT BASS: factory LEGATO, original envelope and FX */
    song.g[G_BPM] = 120;
    for (unsigned step = 0; step < 16; step++) {
        uint8_t pitch = 36 + step % 7;
        unsigned beat = !(step % 4);
        put_step(&trk[0], step, beat ? 4 : 1, beat ? drums : drums + 3, ST_NOTE, 0);
        put_step(&trk[1], step, beat, &pitch, beat ? ST_NOTE : ST_REST, 0);
    }
    voice_ticks = 1600;
    base_pct = 35;
    for (unsigned b = 0; b < 256; b++) bass_block();
    transport_req = 1;
    base_pct = 59;
    peak = 0;
    low = 100;
    for (unsigned b = 0; b < 16000; b++) {
        unsigned age = vage;
        bass_block();
        int step = trk[1].seq_idx;
        if (step != previous) {
            for (unsigned j = step % 4 ? 3 : 0; j < 4; j++) {
                voice_t *v = drum_reuse(&trk[0], drums[j]);
                hits++;
                if (!v || v->note != drums[j] || v->age <= age) lost_hits++;
            }
            if (!(step % 4)) {
                onsets++;
                if (trk[1].v[0].age <= age || !present(1, 36 + step % 7)) lost++;
            }
            previous = step;
        }
        if (!(step % 4) && trk[1].seq_pos > 3 * CTL && trk[1].seq_pos < 12 * CTL) {
            windows++;
            if (!audible_bass() || !trk[1].v[0].gate) silent++;
            reduced += VH_ST(vh_c[NVOICE]) == VH_ON;
        }
    }
    printf("TRACKS=%u kit=%u/%u bass=%u/%u audible gate blocks=%u/%u "
           "half-rate blocks=%u modeled deadline=%u..%u%%\n", tracks,
           hits - lost_hits, hits, onsets - lost, onsets, windows - silent, windows, reduced, low, peak);
    unsigned bad = onsets != 47 || hits != 327 || windows != 399 || lost || silent || lost_hits || !reduced;
    /* A real press through each input path with the drum sequence still running. */
    memset(trk[1].step, 0, sizeof trk[1].step);
    for (unsigned i = 0; i < NSTEP; i++) trk[1].step[i].time = ST_REST;
    song.sel = 1;
    base_pct = live_base;
    unsigned live_ok = 0, over = 0;
    for (unsigned i = 0; i < 8; i++) {
        for (unsigned b = 0; b < 192; b++) bass_block();
        uint8_t key = 7 + i, pitch = 37 + i;
        /* The average alone is a lower bound even after all speculative debt is
         * reconciled. Keep permitted denials visible in the 59% boundary run. */
        unsigned floor = load_all_q8 + load_cost_q8(&trk[1]);
        if (i & 1) fm1_in.notes = 1u << key;
        else midi_in_q[mi_w++ % MQ] = 9u | (0x91u << 8) | ((uint32_t)pitch << 16) | (100u << 24);
        for (unsigned b = 0; b < 8; b++) bass_block();
        if (i & 1) pitch = kb_note[key];
        if (!present(1, pitch) || !trk[1].v[0].gate || !audible_bass()) {
            if (live_base == 59 && floor > (load_limit() << 8)) {
                over++;
                printf("live %s withheld at measured floor %.2f%%\n", i & 1 ? "key" : "MIDI", floor / 256.0);
            } else {
                printf("missing live %s %u\n", i & 1 ? "key" : "MIDI", pitch);
                bad++;
            }
        } else live_ok++;
        if (i & 1) fm1_in.notes = 0;
        else midi_in_q[mi_w++ % MQ] = 8u | (0x81u << 8) | ((uint32_t)pitch << 16);
        for (unsigned b = 0; b < 8; b++) bass_block();
        if (trk[1].v[0].gate) bad++;
    }
    /* The same bass after its envelope ends is still subject to real overload. */
    base_pct = 94;
    for (unsigned b = 0; b < 512; b++) bass_block();
    midi_in_q[mi_w++ % MQ] = 9u | (0x91u << 8) | (48u << 16) | (100u << 24);
    for (unsigned b = 0; b < 8; b++) bass_block();
    int blocked = load_tight && !present(1, 48) && !audible_bass();
    printf("live base=%u%% key/MIDI=%u/8, over limit=%u, errors=%u, overload blocked=%d\n",
           live_base, live_ok, over, bad, blocked);
    return !!bad || !blocked;
}

static int six_parts(unsigned mode, unsigned kit_size, unsigned rotate)
{
    static const uint8_t pitches[] = {48, 52, 55, 59};
    unsigned kit = rotate % NTRK, chord = (kit + 1) % NTRK;
    unsigned hits = 0, misses = 0, gates = 0, gaps = 0, peak_voices = 0, overlap = 0;
    unsigned audio[NTRK] = {0}, heard[NTRK] = {0}, strong = 0, soft_steps = 0;
    unsigned ringing = 0, rate_blocks = 0;
    int previous = -1;
    host_tracks_init();
    trk_live = NTRK;
    mc_host = fx_stems = 1;
    sys_load = mode;
    song.g[G_BPM] = 120;
    for (unsigned p = 0; p < NTRK; p++) {
        track_t *t = &trk[p];
        host_preset(t, p == kit ? ENGI_DRUM : p == chord ? ENGI_FM6 : ENGI_ANALOG, p == kit ? 0 : p == chord ? 4 : 14);
        if (p == kit) dx_kit_load_of(t, 0);
        if (p == chord) t->p[P_VOICE] = V_POLY;
        t->p[P_SLEN] = 4;
        for (unsigned step = 0; step < 4; step++) {
            uint8_t lead = 36 + p + step;
            if (p == kit) {
                put_step(t, step, 0, pitches, ST_NOTE, 0);
                t->step[step].hit = step & 1 ? 0x0b : (1u << kit_size) - 1u;
            } else if (p == chord) {
                put_step(t, step, step ? 0 : 4, pitches, step ? ST_TIE : ST_NOTE, 0);
            } else put_step(t, step, 1, &lead, ST_NOTE, 0);
        }
    }
    /* Start on the very first control block, before any audio deadline reading.
     * Repeated masked hits overlap the earlier one-shot tails on odd steps. */
    base_pct = 35; voice_ticks = 850; transport_req = 1;
    for (unsigned b = 0; b < 3400; b++) {
        if (b == 1700) base_pct = 45;
        unsigned age = vage;
        if (b == 2 && kit_size == 8)
            midi_in_q[mi_w++ % MQ] = 9u | ((0x90u + kit) << 8) | (36u << 16) | (110u << 24);
        bass_block();
        if (b == 2 && kit_size == 8) {
            voice_t *kick = drum_reuse(&trk[kit], 36);
            if (kick && kick->age > age && voices_busy() == 16) {
                for (unsigned l = 1; l < 8; l++) {
                    voice_t *tail = drum_reuse(&trk[kit], DRUM_LANE_NOTE[l]);
                    ringing += tail && tail->active && tail->age <= age;
                }
            }
        }
        for (unsigned j = 0; j < NVOICE; j++)
            if (VH_ST(vh_c[chord * NVOICE + j]) == VH_ON) {rate_blocks++; break;}
        int step = trk[kit].seq_idx;
        unsigned used = voices_busy();
        if (used > peak_voices) peak_voices = used;
        if (step != previous) {
            if (previous >= 0 && strong != (1u << NTRK) - 1u) soft_steps++;
            strong = 0;
            unsigned mask = trk[kit].step[step].hit;
            for (unsigned l = 0; l < kit_size; l++) {
                voice_t *v = drum_reuse(&trk[kit], DRUM_LANE_NOTE[l]);
                if (mask & (1u << l)) {
                    hits++;
                    if (!v || !v->active || v->age <= age) misses++;
                } else if (v && v->active && v->age <= age) overlap++;
            }
            previous = step;
        }
        if (trk[kit].seq_pos > 2 * CTL && trk[kit].seq_pos < 10 * CTL) {
            for (unsigned p = 0; p < NTRK; p++) {
                audio[p]++;
                int maximum = 0;
                for (unsigned f = 0; f < CTL; f++)
                    if (abs(stem_buf[p][f]) > maximum) maximum = abs(stem_buf[p][f]);
                heard[p] += maximum > 0;
                if (maximum > 32) strong |= 1u << p;
                if (p == kit) continue;
                unsigned n = p == chord ? 4 : 1;
                for (unsigned j = 0; j < n; j++) {
                    gates++;
                    if (!present(p, p == chord ? pitches[j] : 36 + p + step)) gaps++;
                }
            }
        }
    }
    unsigned silent = 0;
    for (unsigned p = 0; p < NTRK; p++) silent += audio[p] - heard[p];
    printf("six LOAD=%u kit track=%u cap=%u peak=%u/%u drum=%u/%u held=%u/%u "
           "stem gaps=%u/%u overlapping tails=%u modeled deadline=%u..%u%%\n",
           mode, kit + 1, voice_cap(), peak_voices, kit_size + 8, hits - misses, hits,
           gates - gaps, gates, silent, NTRK * audio[0], overlap, low, peak);
    printf("FM6 reduced blocks=%u, earlier kit voices under MIDI kick at sixteen=%u\n", rate_blocks, ringing);
    int bad = (kit_size == 8 && (!rate_blocks || ringing != 7)) || voice_cap() != 16 || peak_voices != kit_size + 8 || misses || gaps || silent || soft_steps || !overlap;
    if (kit_size == 8) {
        /* A fade still occupies its own paid renderer. Replaying it must neither
         * introduce another slot nor cut another part while all sixteen are busy. */
        base_pct = 35;
        for (unsigned b = 0; b < 400 && (voices_busy() != 16 || load_tight ||
                 load_pred_q8 > (load_limit() << 8)); b++) bass_block();
        /* A prior overload can keep the guard latched even after real deadline
         * room has returned. The active physical slot is still on the clock. */
        for (unsigned b = 0; b < 128; b++) shed_check(HALF_TICKS * 95 / 100);
        for (unsigned b = 0; b < 400; b++) shed_check(HALF_TICKS * 65 / 100);
        if (!load_tight || guard_run) bad++;
        unsigned lead = (chord + 1) % NTRK;
        unsigned parts[] = {kit, chord, lead};
        for (unsigned j = 0; j < 3; j++) {
            track_t *t = &trk[parts[j]];
            voice_t *v = j ? &t->v[0] : drum_reuse(t, 36);
            if (!v || !v->active || voices_busy() != 16) {bad++; break;}
            unsigned note = v->note, age = v->age;
            voice_kill(v);
            unsigned kills = voice_kills;
            trk_note_on(t, note, 100);
            if (v->age == age || v->stage == 4 || voice_kills != kills || voices_busy() != 16)
                bad++;
        }
        for (unsigned b = 0; b < 128; b++) shed_check(HALF_TICKS * 95 / 100);
        voice_t *v = &trk[lead].v[0];
        unsigned age = v->age;
        voice_kill(v);
        trk_note_on(&trk[lead], v->note, 100);
        if (v->age != age || v->stage != 4 || voices_load() != 16) bad++;
        printf("own-slot retriggers after recovery and refusal during overload: errors=%u\n", bad);
    }
    return !!bad;
}

int main(int argc, char **argv)
{
    if (argc == 5 && !strcmp(argv[1], "six")) return six_parts(atoi(argv[2]), atoi(argv[3]), atoi(argv[4]));
    if (argc == 4 && !strcmp(argv[1], "bass")) return bass_phrase(atoi(argv[2]), atoi(argv[3]));
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
