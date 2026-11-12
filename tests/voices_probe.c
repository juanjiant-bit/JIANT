/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The voices under load, measured on the host (codex/voices): what a voice of each engine costs, what the soft measures
 * (voice.c VSOFT: releases hurried, a tail at half the rate) save, and a stress scene that reproduces the stealing.
 *   voices_probe cost            ns a sample and voice of every engine (held, all presets), relative to ANALOG; the
 *                                same voices at half the rate, as a share of the full one
 *   voices_probe stress [DIR]    the scene with the measures off / on: voices cut, by kind; DIR: its WAVs
 *   voices_probe wav DIR         A/B pairs: the same notes at the full rate and at half of it
 *   voices_probe test            the checks (tests/run_tests.sh): nothing changes with room, releases hurried only
 *                                under pressure and smoothly, half the rate within a tolerance of the full one, no step
 *                                where the rate changes, the budget's count, no flipping of the rate
 * Host timings are RELATIVE (an x86 is not the FM-1's pi32v2: no cache misses, another multiplier); the scene's load is
 * a MODEL (a share of a half per voice by engine, from the firmware's own estimate, voice.c LOAD_W_PCT, scaled by the
 * host's ratios), fed to the guard as audio.c does. Real CPU numbers only exist on the device. */
static unsigned vsoft_host = 0;
#define VSOFT vsoft_host
#define main hostsim_main
#include "hostsim.c"
#undef main

static uint64_t cpu_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}
static volatile uint32_t sink;

/* ------------------------------------------------------------------ cost --- */
/* nv voices of engine e, preset pr, held (retriggered so none ends), rendered for blocks: ns a sample, the least of
 * reps runs. mode 0 held, 1 half the rate forced on every voice (held) */
static void force_half(uint32_t p)                      /* part p's voices at half the rate from their next block */
{
    uint32_t i;
    for (i = 0; i < NVOICE; i++)
        if (trk[p].v[i].active && !VH_ST(vh_c[p * NVOICE + i]))
            vh_c[p * NVOICE + i] = VH_IN;
}
static double cost_run(uint32_t e, uint32_t pr, uint32_t nv, uint32_t mode)
{
    uint32_t rep, b, k, blocks = FS * 2u / CTL;
    int32_t out[CTL];
    double best = 1e30;
    for (rep = 0; rep < 5u; rep++) {
        uint64_t t0;
        host_tracks_init();
        memset(trk[0].v, 0, sizeof trk[0].v);
        memset(vh_c, 0, sizeof vh_c);                   /* (no voice_soft here: the last run's rates) */
        host_preset(&trk[0], e, pr);
        trk[0].p[P_VOICE] = V_POLY;
        if (!ENGINES[e]->ownenv) trk[0].p[P_SUS] = 127;
        for (b = 0; b < blocks + 64u; b++) {
            if (b == 64u) t0 = cpu_ns();
            if (!(b & 127u))
                for (k = 0; k < nv; k++)
                    trk_note_on(&trk[0], e == ENGI_DRUM ? DRUM_LANE_NOTE[k % NELEM(DRUM_LANE_NOTE)] : 48u + k * 5u, 100);
            if (mode) force_half(0);
            track_render(&trk[0], out, CTL);
            sink ^= (uint32_t)out[b % CTL];
        }
        t0 = cpu_ns() - t0;
        if ((double)t0 / (blocks * CTL) < best) best = (double)t0 / (blocks * CTL);
        trk_all_off(&trk[0]);
    }
    return best;
}
static const uint8_t COST_ENG[] = {ENGI_ANALOG, ENGI_PHASE, ENGI_LOFI, ENGI_VOICE, ENGI_WHEEL, ENGI_STRING, ENGI_ADD,
                                   ENGI_NOISE, ENGI_ANIMAL, ENGI_DRUM, ENGI_FM6};
static double eng_rel[NENGINES];                        /* a voice's cost by engine, ANALOG's mean = 1 */
static double eng_half[NENGINES];                       /* .. at half the rate, of its own full one (0: not built) */
static void cost_table(int print)
{
    uint32_t i, pr;
    double an = 0;
    for (i = 0; i < NELEM(COST_ENG); i++) {
        uint32_t e = COST_ENG[i], np = ENGINES[e]->npresets, nv = ENGINES[e]->poly ? ENGINES[e]->poly : 4u;
        double sum = 0, mx = 0, hs = 0;
        if (nv > 4u) nv = 4u;
        for (pr = 0; pr < np; pr++) {
            double c1 = cost_run(e, pr, 1, 0), c4 = cost_run(e, pr, nv, 0), v = nv > 1u ? (c4 - c1) / (nv - 1u) : c1;
            double h1 = cost_run(e, pr, 1, 1), h4 = cost_run(e, pr, nv, 1), hv = nv > 1u ? (h4 - h1) / (nv - 1u) : h1;
            if (v < 0) v = 0;
            sum += v; hs += hv;
            if (v > mx) mx = v;
        }
        eng_rel[e] = sum / np;
        eng_half[e] = sum > 0 ? hs / sum : 0;
        if (e == ENGI_ANALOG) an = eng_rel[e];
        if (print)
            printf("%-8s presets %2u  ns/sample/voice mean %7.2f  max %7.2f  x ANALOG %5.2f (max %5.2f)  half rate %4.0f %% of full\n",
                   ENGINES[e]->name, np, eng_rel[e], mx, eng_rel[e] / an, mx / an, 100.0 * eng_half[e]);
    }
    for (i = 0; i < NELEM(COST_ENG); i++)
        eng_rel[COST_ENG[i]] /= an;
}

/* ----------------------------------------------------------------- level --- */
static int32_t lvl_q15(const track_t *t, const voice_t *v) { return voice_lvl(t, v); }
static double q15_db(int32_t q) { return q > 0 ? 20.0 * log10(q / 32768.0) : -120.0; }

/* ---------------------------------------------------------------- stress --- */
/* The load, modelled: a share of a half for what is not a voice (the buses, the mix, the UI's interrupts) plus each
 * sounding voice's (its engine's cost on the host against ANALOG's, cost_table; a voice at half the rate: its measured
 * share of that), fed to the guard every half as audio.c shed_check does, with its admission of the extra voices (12).
 * an_pct: ANALOG's voice as % of a half (tests/voice_fairness_test.c takes 6 "as measured"; voice.c LOAD_W_PCT's
 * estimate is 3) */
static const double ENG_REL[NENGINES] = {[ENGI_ANALOG] = 1.00, [ENGI_PHASE] = 1.13, [ENGI_LOFI] = 1.36, [ENGI_VOICE] = 1.71,
    [ENGI_WHEEL] = 0.86, [ENGI_STRING] = 0.85, [ENGI_ADD] = 1.16, [ENGI_NOISE] = 1.04, [ENGI_ANIMAL] = 0.70,
    [ENGI_DRUM] = 0.69, [ENGI_FM6] = 1.40};
#define HALF_COST 0.58
static double st_base = 28.0, st_an = 6.0;
static struct {
    uint32_t held, tail_loud, tail_quiet, trims, late, half_vb, vb, hurry_b, busy_max, notes, trips, admit, shed, shed_held;
    double pct_max, pct_sum; uint32_t halves;
    uint32_t ui_old, ui_slow, ui_frz, h_old, h_new;     /* halves the UI was frozen (V10.7.20: 60 / 50 %), slow, frozen (ui.c) */
} stm;
static uint8_t st_calm;
static void st_shed(void);
static void stress_load(void)                           /* a block rendered: the parts' cost; every other one a half ends */
{
    static uint32_t blk;
    uint32_t p, i;
    double pct = st_base;
    for (p = 0; p < NPART; p++) {
        double c = 0;
        for (i = 0; i < NVOICE; i++)
            if (trk[p].v[i].active) {
                double w = ENG_REL[trk[p].engine] * st_an * (VH_ST(vh_c[p * NVOICE + i]) == VH_ON ? HALF_COST : 1.0);
                c += w;
                stm.vb++;
                stm.half_vb += VH_ST(vh_c[p * NVOICE + i]) == VH_ON;
            }
        mc_tk[p] = (uint32_t)(c / 100.0 * CTL * LOAD_FRAME_TICKS);   /* (the part's block in TIMER4 ticks, as fx.c times it) */
        pct += c;
    }
    for (p = i = 0; p < NPART; p++) i |= rel_hurry[p];
    stm.hurry_b += i != 0;
    if (voices_busy() > stm.busy_max) stm.busy_max = voices_busy();
    if (++blk & 1u) return;
    load_half((uint32_t)pct, (uint32_t)pct, 0);
    stm.halves++; stm.pct_sum += pct;
    {   /* ui.c ui_hold_poll on the guard's average, as it was (one step) and as it is (two) */
        uint32_t a = load_avg_q8 >> 8;
        stm.h_old = a < 50u ? 0u : a >= 60u ? 1u : stm.h_old;
        stm.h_new = a < 50u ? 0u : a >= 75u ? 2u : (stm.h_new == 2u ? a < 65u : a >= 60u) ? 1u : stm.h_new;
        stm.ui_old += stm.h_old; stm.ui_slow += stm.h_new == 1u; stm.ui_frz += stm.h_new == 2u;
    }
    if (pct > stm.pct_max) stm.pct_max = pct;
    if (pct > 100.0) { stm.late++; st_shed(); }
    if (sys_load != LOAD_FULL || load_tight || load_all_q8 > (65u << 8)) voice_headroom = st_calm = 0;   /* (audio.c shed_check) */
    else if (!voice_headroom) { if (load_all_q8 < (50u << 8)) { if (++st_calm >= 16u) voice_headroom = 1; } else st_calm = 0; }
}
/* a note-on, watched: the voices it cut (given up, or restarted in place for another note), by what they were */
static void st_on(track_t *t, uint32_t note, uint32_t vel)
{
    struct { uint8_t on, held, st, note; uint32_t age; int32_t lvl; } pre[NPART][NVOICE];
    uint32_t p, i;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[p].v[i];
            pre[p][i].on = v->active; pre[p][i].held = (uint8_t)voice_held(&trk[p], v); pre[p][i].st = v->stage;
            pre[p][i].note = v->note; pre[p][i].age = v->age; pre[p][i].lvl = lvl_q15(&trk[p], v);
        }
    trk_note_on(t, note, vel);
    stm.notes++;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            const voice_t *v = &trk[p].v[i];
            int cut = pre[p][i].on && pre[p][i].st != 4u &&
                      (v->stage == 4u || (v->age != pre[p][i].age && v->note != pre[p][i].note));
            if (!cut || trk_vmode(&trk[p]) != V_POLY) continue;   /* (a MONO / LEGATO line's next note is its own) */
            if (getenv("ST_DBG")) printf("cut: by part %u note %u: part %u voice %u note %u held %u lvl %.0f dB %s busy %u cap %u\n", (unsigned)(t - trk), note, p, i,
                                         pre[p][i].note, pre[p][i].held, q15_db(pre[p][i].lvl), v->stage == 4u ? "killed" : "restarted", voices_busy(), voice_cap());
            if (ENGINES[trk[p].engine]->oneshot) { if (v->stage == 4u) stm.held++; continue; }
            if (pre[p][i].held) stm.held++;
            else if (pre[p][i].lvl > 328) stm.tail_loud++;       /* (a tail over -40 dB) */
            else stm.tail_quiet++;
        }
}
static void st_off(track_t *t, uint32_t note) { trk_note_off(t, note); }
#define ST_STEP 86u                                     /* blocks a sixteenth (~120 bpm) */
static const uint8_t ST_CH[4][4] = {{48, 55, 60, 64}, {45, 52, 57, 60}, {41, 48, 53, 57}, {43, 50, 55, 59}};
/* the overload's last resort, as audio.c shed_voice (not in the host's sources): a late half gives up the quietest
 * released voice, else the oldest held one but each part's bass / lead */
static void st_shed(void)
{
    uint32_t p, i;
    voice_t *best = 0;
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++) {
            voice_t *v = &trk[p].v[i];
            if (v->active && !voice_held(&trk[p], v) && v->stage != 4u && !v->sub && (!best || v->env < best->env)) best = v;
        }
    if (!best) {
        for (p = 0; p < NPART; p++) {
            const track_t *t = &trk[p];
            uint32_t mode = trk_vmode(t), low = mode == V_POLY ? lowest_held(t) : 0u;
            for (i = 0; i < NVOICE; i++) {
                voice_t *v = &trk[p].v[i];
                if (v->active && voice_held(t, v) && v->stage != 4u && !voice_core_drum(t, v) &&
                    ((mode == V_POLY && i != low) || (mode != V_POLY && i > 0u)) &&
                    (!best || v->age < best->age)) best = v;   /* (audio.c shed_voice's copy: KICK SNARE CLAP kept) */
            }
        }
        if (best) stm.shed_held++;
    }
    if (best) { voice_kill(best); stm.shed++; }
}
static void stress_events(uint32_t step, uint32_t dense)
{
    uint32_t bar = step / 16u, s = step % 16u, k;
    const uint8_t *ch = ST_CH[bar % 4u], *pc = ST_CH[(bar + 3u) % 4u];
    if (!dense) {                                       /* six parts, at most ~8 notes held at once, long tails */
        if (!(s & 7u)) st_on(&trk[0], DRUM_LANE_NOTE[0], 110);               /* kick */
        if (s % 8u == 4u) st_on(&trk[0], DRUM_LANE_NOTE[1], 100);            /* snare */
        if (!(s & 1u)) st_on(&trk[0], DRUM_LANE_NOTE[3], 70);                /* hats */
        if (!(s & 1u)) { st_off(&trk[1], 36); st_on(&trk[1], ch[0] - 12u, 100); }   /* bass, eighths (a legato line) */
        if (s == 14u) for (k = 0; k < 3u; k++) st_off(&trk[2], ch[k] + 12u);
        if (s == 0u) for (k = 0; k < 3u; k++) st_on(&trk[2], ch[k] + 12u, 90);   /* the pad's chord */
        if (s == 2u || s == 10u) for (k = 1; k < 3u; k++) st_on(&trk[3], ch[k] + 12u + (s == 10u ? 5u : 0u), 95);   /* the EP's stabs */
        if (s == 4u || s == 12u) for (k = 1; k < 3u; k++) st_off(&trk[3], ch[k] + 12u + (s == 12u ? 5u : 0u));
        if (s == 6u || s == 14u) st_on(&trk[4], ch[(bar + s) & 3u] + 24u, 90);   /* a bell */
        if (s == 7u || s == 15u) st_off(&trk[4], ch[(bar + s - 1u) & 3u] + 24u);
        if (s == 4u || s == 7u || s == 12u || s == 15u) st_on(&trk[5], ch[s & 3u] + 12u + (s % 3u == 0u ? 12u : 0u), 85);   /* the keys */
        if (s == 5u || s == 8u || s == 13u || s == 0u) { uint32_t q = (s + 15u) & 15u; st_off(&trk[5], (s ? ch : pc)[q & 3u] + 12u + (q % 3u == 0u ? 12u : 0u)); }
        return;
    }
    if (!(s & 3u)) st_on(&trk[0], DRUM_LANE_NOTE[0], 110);                   /* kick */
    if (s % 8u == 4u) st_on(&trk[0], DRUM_LANE_NOTE[1], 100);                /* snare */
    st_on(&trk[0], DRUM_LANE_NOTE[3], 70);                                   /* hats */
    if (!(s & 1u)) { st_off(&trk[1], 36); st_on(&trk[1], ch[0] - 12u, 100); }   /* bass, eighths */
    if (s == 0u) { for (k = 0; k < 4u; k++) st_off(&trk[2], pc[k] + 12u); for (k = 0; k < 4u; k++) st_on(&trk[2], ch[k] + 12u, 90); }   /* the pad's chord */
    if (s == 0u || s == 6u || s == 10u) for (k = 0; k < 4u; k++) st_on(&trk[3], ch[k] + 12u, 95);   /* the EP comps */
    if (s == 3u || s == 9u || s == 13u) for (k = 0; k < 4u; k++) st_off(&trk[3], ch[k] + 12u);
    if (!(s & 1u)) { st_off(&trk[4], (s ? ch : pc)[((s >> 1) + 3u) & 3u] + 24u); st_on(&trk[4], ch[(s >> 1) & 3u] + 24u, 90); }   /* bells */
    { uint32_t q = (s + 15u) & 15u; st_off(&trk[5], (s ? ch : pc)[q & 3u] + 12u + (q % 3u == 0u ? 12u : 0u)); }   /* the keys' line, sixteenths */
    st_on(&trk[5], ch[s & 3u] + 12u + (s % 3u == 0u ? 12u : 0u), 85);
}
static void stress_init(void)
{
    uint32_t p;
    host_tracks_init();
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    memset(vh_c, 0, sizeof vh_c);
    memset(&stm, 0, sizeof stm);
    voice_kills = voice_trims = load_steals = guard_trips = 0;
    load_avg_q8 = load_all_q8 = load_pred_q8 = 0; load_tight = guard_cap = guard_run = 0; guard_calm = 0;
    voice_headroom = st_calm = 0; vh_wait = vh_held = 0; memset(rel_hurry, 0, sizeof rel_hurry);
    sys_load = LOAD_FULL; mc_host = 0;
    host_drums(&trk[0]);
    host_preset(&trk[1], ENGI_ANALOG, 2);               /* SQR BASS (a legato line) */
    host_preset(&trk[2], ENGI_ANALOG, 1);               /* SOFT PAD: REL 105 */
    host_preset(&trk[3], ENGI_FM6, 0);                  /* TINE EP .. */
    trk[3].p[P_REL] = 60;                               /* .. with a long release, as a random patch has */
    host_preset(&trk[4], ENGI_FM6, 1);                  /* BELL */
    trk[4].p[P_REL] = 40;
    host_preset(&trk[5], ENGI_ANALOG, 23);              /* GLASS KEYS: REL 65 */
    for (p = 0; p < NPART; p++) { trk[p].p[P_LEVEL] = 100; }
}
/* the scene: bars of it; soft: VSOFT's bits. wav: its mix written there */
static void stress_run(uint32_t soft, uint32_t bars, uint32_t dense, const char *wav)
{
    uint32_t b, i, nb = bars * 16u * ST_STEP, tail = FS * 3u / CTL;
    int32_t o[2 * CTL];
    FILE *f = wav ? fopen(wav, "wb") : 0;
    stress_init();
    vsoft_host = soft;
    if (f) wav_hdr(f, (nb + tail) * CTL);
    for (b = 0; b < 400u; b++) { mix_block(o, CTL); stress_load(); }   /* (the load's average settles) */
    memset(&stm, 0, sizeof stm);
    voice_kills = voice_trims = load_steals = guard_trips = 0;
    for (b = 0; b < nb + tail; b++) {
        if (b < nb && b % ST_STEP == 0u) stress_events(b / ST_STEP, dense);
        if (b == nb) for (i = 0; i < NPART; i++) trk_all_off(&trk[i]);
        mix_block(o, CTL);
        stress_load();
        if (f) for (i = 0; i < CTL; i++) wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    if (f) fclose(f);
    stm.trims = voice_trims; stm.trips = guard_trips; stm.admit = load_steals;
    vsoft_host = 0;
}
static void stress_row(const char *name)
{
    printf("| %-22s | %4u | %9u | %10u | %5u | %5u | %4u | %4u (%u held) | %5.1f | %5.1f | %4u | %4.1f %% | %4.1f %% | %3.0f %% | %3.0f %% / %3.0f %% |\n", name,
           stm.held, stm.tail_loud, stm.tail_quiet, stm.trims, stm.admit, stm.trips, stm.late, stm.shed_held,
           stm.pct_sum / (stm.halves ? stm.halves : 1), stm.pct_max, stm.busy_max, 100.0 * stm.half_vb / (stm.vb ? stm.vb : 1),
           100.0 * stm.hurry_b / (stm.halves ? 2.0 * stm.halves : 1), 100.0 * stm.ui_old / (stm.halves ? stm.halves : 1),
           100.0 * stm.ui_slow / (stm.halves ? stm.halves : 1), 100.0 * stm.ui_frz / (stm.halves ? stm.halves : 1));
}
static void stress_table(const char *dir)
{
    static const struct { double base, an; uint32_t dense; const char *what; } SC[] = {
        {28, 3, 0, "A  six parts, light load: the count of voices rules"},
        {28, 6, 0, "B  six parts, heavy load: the load rules"},
        {28, 6, 1, "C  dense (more notes held than the budget), heavy load"},
    };
    static const struct { uint32_t soft; const char *name; } SOFT[] = {
        {0, "off (V10.7.20)"}, {1, "1 releases hurried"}, {6, "2+4 half rate"}, {3, "1+2, as built"}, {7, "1+2+4 (half count)"}, {11, "1+2+8 (a part's cap)"}};
    uint32_t k, m;
    char path[512];
    for (k = 0; k < NELEM(SC); k++) {
        printf("\n%s; base %.0f %%, an ANALOG voice %.0f %% of a half (a model), 16 bars, ", SC[k].what, SC[k].base, SC[k].an);
        for (m = 0; m < NELEM(SOFT); m++) {
            st_base = SC[k].base; st_an = SC[k].an;
            path[0] = 0;
            if (dir && k == 1u && (!SOFT[m].soft || SOFT[m].soft == 3u))
                snprintf(path, sizeof path, "%s/stress_%s.wav", dir, SOFT[m].soft ? "after" : "before");
            stress_run(SOFT[m].soft, 16, SC[k].dense, path[0] ? path : 0);
            if (!m) {
                printf("%u notes\n", stm.notes);
                printf("| measures (VSOFT)       | held cut | tail cut > -40 dB | tail cut < -40 dB | trims | admit | trips | late halves | load avg %% | max %% | busy max | voice-blocks at half | blocks hurried | UI frozen, V10.7.20's rule | UI slow / frozen, the new rule |\n");
                printf("|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
            }
            stress_row(SOFT[m].name);
        }
    }
    printf("\nheld cut: a held note given up or restarted for another; tail cut: a released one, by its level then; trims: tails\n"
           "under -60 dB let go; admit: tails given up by the admission's prediction; trips: the guard's; late: halves over 100 %%\n"
           "(a click each on the device; each gives up a voice, of them held ones in brackets)\n");
}

/* ------------------------------------------------------------------- wav --- */
/* a phrase on part 0 (engine e, preset pr): its notes at the full rate, at half of it (every voice, held and released),
 * and switching between them every 0.37 s; the part alone, dry (track_render). Prints the difference against the full
 * rate (dB under the signal; under 4 kHz too: a 2-pole low-pass on both) */
typedef struct { const char *name; uint8_t e, pr, nn, mono; uint8_t note[8]; uint16_t gate, gap; int16_t rel; } phrase_t;
static const phrase_t PHR[] = {
    {"fm6_epiano", ENGI_FM6, 0, 6, 0, {48, 60, 64, 67, 72, 76}, 300, 380, 40},
    {"fm6_bell", ENGI_FM6, 1, 5, 0, {72, 76, 79, 84, 88}, 200, 420, -1},
    {"fm6_bass", ENGI_FM6, 2, 6, 1, {36, 36, 48, 39, 41, 43}, 180, 240, -1},
    {"fm6_brass", ENGI_FM6, 3, 4, 0, {55, 60, 64, 67}, 400, 480, -1},
    {"analog_pad", ENGI_ANALOG, 1, 4, 0, {48, 55, 60, 64}, 900, 300, -1},
    {"analog_lead", ENGI_ANALOG, 0, 6, 1, {60, 67, 72, 70, 67, 75}, 200, 240, -1},
    {"analog_strings", ENGI_ANALOG, 11, 4, 0, {52, 59, 64, 67}, 900, 300, -1},
    {"analog_initsaw", ENGI_ANALOG, 9, 5, 0, {36, 48, 60, 72, 84}, 250, 300, -1},
    {"fm6_sine", ENGI_FM6, 11, 5, 0, {36, 48, 60, 72, 84}, 250, 300, -1},
    {"analog_sine", ENGI_ANALOG, 5, 5, 0, {36, 48, 60, 72, 84}, 250, 300, -1},
};
static uint32_t phrase_render(const phrase_t *ph, uint32_t mode, int32_t *buf, uint32_t cap)
{
    uint32_t b, k, n = 0, per = (ph->gate + ph->gap) * FS / 1000u / CTL, on = ph->gate * FS / 1000u / CTL;
    uint32_t total = per * ph->nn + FS * 2u / CTL;
    host_tracks_init();
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    memset(vh_c, 0, sizeof vh_c);
    host_preset(&trk[0], ph->e, ph->pr);
    trk[0].p[P_VOICE] = ph->mono ? V_LEGATO : V_POLY;
    if (ph->rel >= 0) trk[0].p[P_REL] = ph->rel;
    for (b = 0; b < total && n + CTL <= cap; b++) {
        k = b / per;
        if (k < ph->nn && b % per == 0u) {
            if (ph->mono) trk_note_on(&trk[0], ph->note[k], 100);
            else trk_note_on(&trk[0], ph->note[k], 100), k && ph->gap < 300u ? trk_note_off(&trk[0], ph->note[k - 1u]) : (void)0;
        }
        if (k < ph->nn && b % per == on) trk_note_off(&trk[0], ph->note[k]);
        if (mode == 1u || (mode == 2u && (b / 256u) & 1u))
            force_half(0);
        else if (mode == 2u)
            for (k = 0; k < NVOICE; k++)                 /* back to the full rate */
                if (VH_ST(vh_c[k]) == VH_ON) vh_c[k] += VH_OUT - VH_ON;
                else if (VH_ST(vh_c[k]) != VH_OUT) vh_c[k] = VH_OFF;
        track_render(&trk[0], buf + n, CTL);
        n += CTL;
    }
    return n;
}
/* .. in a process of its own: every render from the same state (the random draws, FM6's trim, the LFOs) */
static uint32_t phrase_fork(const phrase_t *ph, uint32_t mode, int32_t *buf, uint32_t cap)
{
    int fd[2];
    uint32_t n = 0, got = 0;
    pid_t pid;
    if (pipe(fd)) return 0;
    pid = fork();
    if (!pid) {
        close(fd[0]);
        n = phrase_render(ph, mode, buf, cap);
        if (write(fd[1], &n, sizeof n) != sizeof n) _exit(1);
        while (got < n * sizeof *buf) {
            ssize_t w = write(fd[1], (char *)buf + got, n * sizeof *buf - got);
            if (w <= 0) _exit(1);
            got += (uint32_t)w;
        }
        _exit(0);
    }
    close(fd[1]);
    if (read(fd[0], &n, sizeof n) != sizeof n) n = 0;
    while (got < n * sizeof *buf) {
        ssize_t r = read(fd[0], (char *)buf + got, n * sizeof *buf - got);
        if (r <= 0) break;
        got += (uint32_t)r;
    }
    close(fd[0]);
    waitpid(pid, 0, 0);
    return got / sizeof *buf;
}
/* x as the interpolation alone would make it: its odd samples kept, the even ones their means (voice.c voice_half's,
 * from a render at the full rate): what half the rate loses by itself, without what the engine does differently */
static void resample_ref(const int32_t *x, int32_t *y, uint32_t n)
{
    uint32_t i;
    for (i = 0; i + 1u < n; i += 2u) {
        y[i] = i ? (x[i - 1u] + x[i + 1u]) >> 1 : x[0];
        y[i + 1u] = x[i + 1u];
    }
}
static double rms_db(const int32_t *a, const int32_t *b, uint32_t n, int lp)   /* a - b against a, dB; lp: under ~4 kHz */
{
    double sa = 0, sd = 0, ya = 0, yb = 0, za = 0, zb = 0, k = 1.0 - exp(-2.0 * M_PI * 4000.0 / FS);
    uint32_t i;
    for (i = 0; i < n; i++) {
        double x = a[i], y = b ? b[i] : 0;
        if (lp) { ya += k * (x - ya); za += k * (ya - za); x = za; yb += k * (y - yb); zb += k * (yb - zb); y = zb; }
        sa += x * x; sd += (x - y) * (x - y);
    }
    return sa > 0 && sd > 0 ? 10.0 * log10(sd / sa) : -200.0;
}
/* the long-term spectrum of x in bands (power, Hann windows of 4096, half overlapped) */
#define NBAND 8
static const double BAND_HZ[NBAND + 1] = {0, 1000, 2000, 4000, 6000, 8000, 11000, 16000, 22059};
static void band_power(const int32_t *x, uint32_t n, double *bp)
{
    enum { N = 4096 };
    static double re[N], im[N], w[N];
    uint32_t i, j, k, m, o;
    for (i = 0; i < N; i++) w[i] = 0.5 - 0.5 * cos(2.0 * M_PI * i / N);
    for (k = 0; k < NBAND; k++) bp[k] = 0;
    for (o = 0; o + N <= n; o += N / 2) {
        for (i = 0; i < N; i++) { re[i] = x[o + i] * w[i]; im[i] = 0; }
        for (i = 1, j = 0; i < N; i++) {                /* bit reversal */
            uint32_t bit = N >> 1;
            for (; j & bit; bit >>= 1) j ^= bit;
            j ^= bit;
            if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; }
        }
        for (m = 2; m <= N; m <<= 1) {
            double ang = -2.0 * M_PI / m;
            for (i = 0; i < N; i += m)
                for (j = 0; j < m / 2; j++) {
                    double c = cos(ang * j), sn = sin(ang * j), a = re[i + j + m / 2], b = im[i + j + m / 2];
                    double tr = a * c - b * sn, ti = a * sn + b * c;
                    re[i + j + m / 2] = re[i + j] - tr; im[i + j + m / 2] = im[i + j] - ti;
                    re[i + j] += tr; im[i + j] += ti;
                }
        }
        for (i = 1; i < N / 2; i++) {
            double f = (double)i * FS / N;
            for (k = 0; k < NBAND; k++)
                if (f >= BAND_HZ[k] && f < BAND_HZ[k + 1]) bp[k] += re[i] * re[i] + im[i] * im[i];
        }
    }
}
static void band_rows(const char *name, const int32_t *full, const int32_t *half, uint32_t n)
{
    double a[NBAND], b[NBAND], tot = 0;
    uint32_t k;
    band_power(full, n, a);
    band_power(half, n, b);
    for (k = 0; k < NBAND; k++) tot += a[k];
    printf("| %-14s full |", name);
    for (k = 0; k < NBAND; k++) printf(" %6.1f |", a[k] > 0 ? 10.0 * log10(a[k] / tot) : -200.0);
    printf("\n| %-14s half - full |", "");
    for (k = 0; k < NBAND; k++) printf(" %+6.1f |", a[k] > 0 && b[k] > 0 ? 10.0 * log10(b[k] / a[k]) : 0.0);
    printf("\n");
}
static void wav_mono(const char *path, const int32_t *x, uint32_t n)
{
    FILE *f = fopen(path, "wb");
    uint32_t i;
    if (!f) return;
    wav_hdr(f, n);
    for (i = 0; i < n; i++) wav_put(f, x[i] >> 1, x[i] >> 1);
    fclose(f);
}
#define WAV_MAX (FS * 12u)
static int wav_pairs(const char *dir)
{
    static int32_t full[WAV_MAX], half[WAV_MAX], sw[WAV_MAX], ref[WAV_MAX];
    uint32_t k, n;
    char path[512];
    printf("the difference from the full rate, dB under the signal (the same notes; -inf = the same samples)\n"
           "| phrase | half, whole band | half, under 4 kHz | the interpolation alone | half vs that (the engine's part) | switching every 0.37 s |\n|---|---|---|---|---|---|\n");
    for (k = 0; k < NELEM(PHR); k++) {
        n = phrase_fork(&PHR[k], 0, full, WAV_MAX);
        phrase_fork(&PHR[k], 1, half, WAV_MAX);
        phrase_fork(&PHR[k], 2, sw, WAV_MAX);
        resample_ref(full, ref, n);
        printf("| %-14s (%s %s) | %6.1f | %6.1f | %6.1f | %6.1f | %6.1f |\n", PHR[k].name, ENGINES[PHR[k].e]->name,
               ENGINES[PHR[k].e]->presets[PHR[k].pr].name, rms_db(full, half, n, 0), rms_db(full, half, n, 1),
               rms_db(full, ref, n, 0), rms_db(ref, half, n, 0), rms_db(full, sw, n, 0));
        if (dir) {
            snprintf(path, sizeof path, "%s/%s_full.wav", dir, PHR[k].name); wav_mono(path, full, n);
            snprintf(path, sizeof path, "%s/%s_half.wav", dir, PHR[k].name); wav_mono(path, half, n);
            snprintf(path, sizeof path, "%s/%s_switching.wav", dir, PHR[k].name); wav_mono(path, sw, n);
        }
    }
    printf("\nthe spectrum by band: the full rate's level (dB of the whole) and half the rate's against it (dB)\n| phrase | 0-1 kHz | 1-2 | 2-4 | 4-6 | 6-8 | 8-11 | 11-16 | 16-22 |\n|---|---|---|---|---|---|---|---|---|\n");
    for (k = 0; k < NELEM(PHR); k++) {
        n = phrase_fork(&PHR[k], 0, full, WAV_MAX);
        phrase_fork(&PHR[k], 1, half, WAV_MAX);
        band_rows(PHR[k].name, full, half, n);
    }
    return 0;
}

/* ------------------------------------------------------------------ test --- */
static int t_fail;
static void t_check(const char *what, int ok)
{
    printf("%s  %s\n", ok ? "ok  " : "FAIL", what);
    t_fail += !ok;
}
static void t_init(uint32_t soft)
{
    host_tracks_init();
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    memset(vh_c, 0, sizeof vh_c);
    memset(rel_hurry, 0, sizeof rel_hurry);
    voice_kills = voice_trims = load_steals = guard_trips = 0;
    load_avg_q8 = load_all_q8 = load_pred_q8 = 0; load_tight = guard_cap = guard_run = 0; guard_calm = 0;
    voice_headroom = 0; vh_wait = vh_held = 0;
    sys_load = LOAD_FULL; mc_host = 0;
    vsoft_host = soft;
}
static uint32_t t_any_soft(void)                        /* a voice at half the rate or on its way, or a part hurried */
{
    uint32_t i, n = 0;
    for (i = 0; i < NTRK * NVOICE; i++) n += VH_ST(vh_c[i]) != VH_OFF;
    for (i = 0; i < NTRK; i++) n += rel_hurry[i];
    return n;
}
/* a pad and an EP with room in the budget (5 of 8 voices): the mix, its checksum; *soft: the measures ever at work */
static uint32_t t_room(uint32_t soft, uint32_t *act, uint32_t *kills)
{
    uint32_t b, i, h = 2166136261u;
    int32_t o[2 * CTL];
    t_init(soft);
    host_preset(&trk[0], ENGI_ANALOG, 1);
    host_preset(&trk[1], ENGI_FM6, 0);
    trk[1].p[P_REL] = 60;
    *act = 0;
    for (b = 0; b < FS * 5u / CTL; b++) {
        if (b == 10u) { trk_note_on(&trk[0], 48, 100); trk_note_on(&trk[0], 55, 100); trk_note_on(&trk[0], 60, 100); }
        if (b == 200u) { trk_note_on(&trk[1], 64, 100); trk_note_on(&trk[1], 67, 100); }
        if (b == 700u) { trk_note_off(&trk[0], 48); trk_note_off(&trk[0], 55); trk_note_off(&trk[0], 60); }
        if (b == 900u) { trk_note_off(&trk[1], 64); trk_note_off(&trk[1], 67); }
        if (b == 1100u) trk_note_on(&trk[0], 52, 100);
        mix_block(o, CTL);
        *act += t_any_soft();
        for (i = 0; i < 2u * CTL; i++) h = (h ^ (uint32_t)o[i]) * 16777619u;
    }
    *kills = voice_kills;
    return h;
}
/* part 0 (engine e, preset pr, REL rel) plays two notes and lets them go at block 300 while parts 1 and 2 hold fill
 * notes: blocks until part 0 is silent (0: never, in 12 s). env: its first voice's amplitude a block (Q15; FM6: its
 * peak), out: the part alone (track_render is the mix's: the part's block is part_buf) */
#define T_REL_BLOCKS (FS * 12u / CTL)
static uint32_t t_release(uint32_t soft, uint32_t e, uint32_t pr, int rel, uint32_t fill, int l4, int32_t *env, uint32_t *kills,
                          uint32_t *jump)
{
    uint32_t b, i, done = 0;
    int32_t o[2 * CTL], last = 0;
    t_init(soft);
    host_preset(&trk[0], e, pr);
    trk[0].p[P_REL] = (int16_t)rel;
    trk[0].p[P_DLY] = trk[0].p[P_REV] = trk[0].p[P_CHOR] = 0;
    if (l4 >= 0) {                                      /* an FM6 patch whose release ends above silence */
        uint8_t v[FP_SIZE + 1u];
        memcpy(v, fm6_patch[0], sizeof v);
        for (i = 0; i < 6u; i++) v[i * FP_OP + FP_L1 + 3u] = (uint8_t)l4;
        fm6_set_patch(0, v);
        fm6_slot[0] = FM6_OWN; trk[0].p[P_E7] = FM6_OWN;
    }
    for (i = 1; i < 3u; i++) { host_preset(&trk[i], ENGI_ANALOG, 5); trk[i].p[P_SUS] = 127; trk[i].p[P_LEVEL] = 0; }
    *jump = 0;
    for (b = 0; b < T_REL_BLOCKS; b++) {
        if (b == 4u) {
            trk_note_on(&trk[0], 60, 100); trk_note_on(&trk[0], 67, 100);
            for (i = 0; i < fill; i++) trk_note_on(&trk[1 + (i & 1u)], 40u + i, 100);
        }
        if (b == 300u) { trk_note_off(&trk[0], 60); trk_note_off(&trk[0], 67); }
        mix_block(o, CTL);
        if (env) env[b] = voice_lvl(&trk[0], &trk[0].v[0]) * (trk[0].v[0].active != 0);
        if (b >= 300u)
            for (i = 0; i < CTL; i++) {
                uint32_t d = (uint32_t)(o[2 * i] > last ? o[2 * i] - last : last - o[2 * i]);
                if (d > *jump) *jump = d;
                last = o[2 * i];
            }
        else last = o[2 * CTL - 2];
        if (b > 300u && !done && !trk[0].v[0].active && !trk[0].v[1].active) done = b - 300u;
    }
    *kills = voice_kills - voice_trims;
    return done;
}
static uint32_t t_room_fork(uint32_t soft, uint32_t *act, uint32_t *kills)   /* (a process each: the same random draws) */
{
    int fd[2];
    uint32_t r[3] = {0, 0, 0};
    pid_t pid;
    if (pipe(fd)) return 0;
    pid = fork();
    if (!pid) {
        r[0] = t_room(soft, &r[1], &r[2]);
        _exit(write(fd[1], r, sizeof r) != sizeof r);
    }
    close(fd[1]);
    if (read(fd[0], r, sizeof r) != sizeof r) r[0] = 0;
    close(fd[0]);
    waitpid(pid, 0, 0);
    *act = r[1]; *kills = r[2];
    return r[0];
}
/* the budget's count: 4 notes held, 4 let go (long tails), then a ninth note */
static void t_count(uint32_t soft, uint32_t *kills, uint32_t *raw, uint32_t *load)
{
    uint32_t b, i;
    int32_t o[2 * CTL];
    t_init(soft);
    for (i = 0; i < 3u; i++) { host_preset(&trk[i], ENGI_ANALOG, 1); trk[i].p[P_SUS] = 127; }
    for (b = 0; b < 400u; b++) {
        if (b == 4u) for (i = 0; i < 4u; i++) { trk_note_on(&trk[0], 48u + i * 3u, 100); trk_note_on(&trk[1], 60u + i * 3u, 100); }
        if (b == 200u) for (i = 0; i < 4u; i++) trk_note_off(&trk[1], 60u + i * 3u);
        if (b == 300u) { *kills = voice_kills; trk_note_on(&trk[2], 72, 100); *kills = voice_kills - *kills; *raw = voices_busy(); *load = voices_load(); }
        mix_block(o, CTL);
    }
}
static double band_db(const int32_t *a, const int32_t *b, uint32_t n, uint32_t k0, uint32_t k1)   /* b against a, bands k0..k1 */
{
    double pa[NBAND], pb[NBAND], sa = 0, sb = 0;
    uint32_t k;
    band_power(a, n, pa); band_power(b, n, pb);
    for (k = k0; k <= k1; k++) { sa += pa[k]; sb += pb[k]; }
    return sa > 0 && sb > 0 ? 10.0 * log10(sb / sa) : 0.0;
}
static int tests(void)
{
    static int32_t full[WAV_MAX], half[WAV_MAX], sw[WAV_MAX], e0[T_REL_BLOCKS], e1[T_REL_BLOCKS];
    static const phrase_t TP[] = {                      /* (sounds without a random draw: the same notes twice) */
        {"FM6 INIT SINE", ENGI_FM6, 11, 5, 0, {36, 48, 60, 72, 84}, 250, 300, -1},
        {"FM6 FM BASS", ENGI_FM6, 2, 6, 1, {36, 36, 48, 39, 41, 43}, 180, 240, -1},
        {"FM6 TINE EP", ENGI_FM6, 0, 6, 0, {48, 60, 64, 67, 72, 76}, 300, 380, 40},
        {"ANALOG SUB BASS", ENGI_ANALOG, 7, 5, 0, {36, 43, 48, 55, 60}, 250, 300, -1},
        {"ANALOG SQR BASS", ENGI_ANALOG, 2, 5, 0, {36, 43, 48, 55, 60}, 250, 300, -1},
        {"ANALOG INIT SAW", ENGI_ANALOG, 9, 5, 0, {36, 48, 60, 72, 84}, 250, 300, -1},
    };
    static const double TOL[NELEM(TP)] = {-45, -55, -30, -35, -25, -14};   /* half against full, dB under the signal */
    uint32_t a0, a7, k0, k7, h0, h7, k, n, i, d0, d1, kl, j0, j1, raw, ld;
    char msg[256];
    /* 1. room: nothing changes */
    h0 = t_room_fork(0, &a0, &k0);
    h7 = t_room_fork(3, &a7, &k7);
    snprintf(msg, sizeof msg, "room in the budget (5 of 8 voices): the mix bit for bit as without the measures (%08x / %08x), none at work (%u), nothing cut (%u)", h0, h7, a7, k7);
    t_check(msg, h0 == h7 && !a7 && !k7 && !k0);
    /* 2. releases hurried only under pressure */
    d0 = t_release(0, ENGI_ANALOG, 1, 110, 6, -1, e0, &kl, &j0);
    d1 = t_release(3, ENGI_ANALOG, 1, 110, 2, -1, e1, &kl, &j1);
    for (i = 0, n = 0; i < T_REL_BLOCKS; i++) n += e0[i] != e1[i];
    snprintf(msg, sizeof msg, "ANALOG pad, REL 110: with room (4 voices) its release as without the measures (%u blocks differ; %.2f s)", n, d1 * (double)CTL / FS);
    t_check(msg, !n && d0 && d1);
    d1 = t_release(1, ENGI_ANALOG, 1, 110, 6, -1, e1, &kl, &j1);
    for (i = 300, n = 0, k = 1; i < T_REL_BLOCKS; i++) {
        if (e0[i] >= HURRY_LVL && e0[i] != e1[i]) n++;                /* (over -20 dB: as set) */
        if (i > 300u && e1[i - 1] > 400 && e1[i] * 100 < e1[i - 1] * 90) k = 0;   /* (no step: under 1 dB a block) */
    }
    snprintf(msg, sizeof msg, "ANALOG pad, the budget full (8 voices): its release %.2f s -> %.2f s, the same down to -20 dB (%u blocks differ), smooth, "
             "none cut (%u), no step in the sound (the largest %u against %u)", d0 * (double)CTL / FS, d1 * (double)CTL / FS, n, kl, j1, j0);
    t_check(msg, d0 && d1 && d1 * 10u < d0 * 7u && !n && k && !kl && j1 <= j0 + j0 / 8u + 8u);
    d0 = t_release(0, ENGI_FM6, 0, 70, 6, -1, e0, &kl, &j0);
    d1 = t_release(1, ENGI_FM6, 0, 70, 6, -1, e1, &kl, &j1);
    for (i = 300, k = 1; i < T_REL_BLOCKS; i++)
        if (i > 300u && e1[i - 1] > 400 && e1[i] * 100 < e1[i - 1] * 80) k = 0;
    snprintf(msg, sizeof msg, "FM6 TINE EP, REL 70, the budget full: its release %.2f s%s -> %.2f s, smooth, none cut (%u), no step (%u against %u)",
             (d0 ? d0 : T_REL_BLOCKS - 300u) * (double)CTL / FS, d0 ? "" : " and on", d1 * (double)CTL / FS, kl, j1, j0);
    t_check(msg, d1 && d1 * 10u < (d0 ? d0 : T_REL_BLOCKS) * 7u && k && !kl && j1 <= j0 + j0 / 8u + 8u);
    d0 = t_release(0, ENGI_FM6, 0, 0, 6, 80, 0, &kl, &j0);
    d1 = t_release(1, ENGI_FM6, 0, 0, 6, 80, 0, &kl, &j1);
    snprintf(msg, sizeof msg, "FM6 patch whose release ends at L4 80: holds its voices for good (%s); the budget full: they end in %.2f s",
             d0 ? "ended" : "12 s and on", d1 * (double)CTL / FS);
    t_check(msg, !d0 && d1 && d1 < FS * 4u / CTL);
    d1 = t_release(1, ENGI_FM6, 0, 0, 2, 80, 0, &kl, &j1);
    t_check("  .. and with room they stay as the patch has them", !d1);
    /* 3. half the rate against the full one */
    for (k = 0; k < NELEM(TP); k++) {
        double d, sd, lo, mx = 0, mh = 0, ms = 0;
        n = phrase_fork(&TP[k], 0, full, WAV_MAX);
        phrase_fork(&TP[k], 1, half, WAV_MAX);
        phrase_fork(&TP[k], 2, sw, WAV_MAX);
        d = rms_db(full, half, n, 0); sd = rms_db(full, sw, n, 0); lo = band_db(full, half, n, 0, 2);
        for (i = 0; i < n; i++) {
            double x = fabs((double)full[i]), dh = fabs((double)full[i] - half[i]), ds = fabs((double)full[i] - sw[i]);
            if (x > mx) mx = x;
            if (dh > mh) mh = dh;
            if (ds > ms) ms = ds;
        }
        snprintf(msg, sizeof msg, "%s at half the rate: %.1f dB under the full rate's (limit %.0f), 0-4 kHz %+.2f dB; switching every 0.37 s: %.1f dB, "
                 "its largest error %.1f %% of the peak (all at half: %.1f %%)", TP[k].name, d, TOL[k], lo, sd, 100.0 * ms / mx, 100.0 * mh / mx);
        t_check(msg, n && d < TOL[k] && fabs(lo) < 0.5 && sd < TOL[k] + 3.0 && ms <= mh * 1.25 + mx * 0.002);
    }
    /* 4. the budget's count */
    t_count(0, &kl, &raw, &ld);
    snprintf(msg, sizeof msg, "without the measures: 4 held + 4 tails, a ninth note cuts a tail (%u cut, %u sounding)", kl, raw);
    t_check(msg, kl == 1u && raw == 8u);
    t_count(3, &kl, &raw, &ld);
    snprintf(msg, sizeof msg, "as built (VSOFT 3): the same, the tail it cuts %u (%u sounding)", kl, raw);
    t_check(msg, kl == 1u && raw == 8u);
    t_count(7, &kl, &raw, &ld);
    snprintf(msg, sizeof msg, "VSOFT 4 (off): the tails at half the rate are half a voice each: the ninth note cuts none (%u cut, %u sounding, the budget's count %u of %u)",
             kl, raw, ld, voice_cap());
    t_check(msg, !kl && raw == 9u && ld <= voice_cap());
    /* 5. the rate does not flip: a tail stays at half until it ends; the held voices go after the guard has stayed
     * tripped, and come back only once it has let go and the load is under HALF_BACK */
    {
        uint32_t b, flips = 0, in_at = 0, out_at = 0, back = 0, calm_at = 0;
        uint8_t was[NVOICE] = {0};
        int32_t o[2 * CTL];
        t_init(3);
        host_preset(&trk[0], ENGI_FM6, 4); trk[0].p[P_SUS] = 127;       /* FM6 PAD: the dearest part */
        host_preset(&trk[1], ENGI_ANALOG, 1);
        for (b = 0; b < 6000u; b++) {
            if (b == 4u) for (i = 0; i < 3u; i++) { trk_note_on(&trk[0], 48u + i * 4u, 100); trk_note_on(&trk[1], 60u + i * 3u, 100); }
            if (b == 100u) for (i = 0; i < 3u; i++) trk_note_off(&trk[1], 60u + i * 3u);
            if (!(b & 1u)) {                            /* a half ends: 95 % for a while, then 60, then 40 */
                uint32_t pct = b < 200u ? 40u : b < 1200u ? 95u : b < 3200u ? 60u : 40u;
                load_half(pct, pct, 0);
            }
            if (b >= 1200u && !load_tight && !calm_at) calm_at = b;
            mix_block(o, CTL);
            for (i = 0; i < 3u; i++) {
                uint32_t st = VH_ST(vh_c[i]) == VH_ON;                /* part 0's held voices */
                if (st != was[i]) { flips++; if (st && !in_at) in_at = b; if (!st && !out_at) out_at = b; }
                was[i] = (uint8_t)st;
                if (trk[1].v[i].active && b > 260u && VH_ST(vh_c[NVOICE + i]) != VH_ON) back++;   /* a tail back at the full rate */
            }
        }
        snprintf(msg, sizeof msg, "the guard tripped at block ~200: the tails at half at once and never back (%u), the dearest part's held voices at half from block %u "
                 "(after %u blocks tripped), back at the full rate at block %u (the guard let go at %u, the load then under %u %%): %u changes of rate for 3 voices",
                 back, in_at, 4u * HALF_WAIT, out_at, calm_at, HALF_BACK, flips);
        t_check(msg, !back && in_at > 200u + 4u * HALF_WAIT && in_at < 500u && out_at > calm_at && calm_at && flips == 6u);
    }
    {   /* (V10.7.21, the owner) KICK SNARE CLAP are never cut: a kit hits them, then three synth parts hold chords past
         * the budget, new notes keep coming and the overload guard sheds: the three hits play on (not given up, not fading) */
        uint32_t l, b, cut = 0, hats_cut = 0, held = 0;
        int32_t o[2 * CTL];
        t_init(3);
        host_preset(&trk[0], ENGI_DRUM, 0);
        host_preset(&trk[1], ENGI_ANALOG, 1); host_preset(&trk[2], ENGI_FM6, 0); host_preset(&trk[3], ENGI_ANALOG, 1);
        for (b = 0; b < 400u; b++) {
            if (!(b % 40u)) for (l = 0; l < 4u; l++) trk_note_on(&trk[0], DRUM_LANE_NOTE[l], 110);   /* (KICK SNARE CLAP HATCL) */
            if (b == 5u) for (l = 0; l < 4u; l++) { trk_note_on(&trk[1], 48 + 4 * l, 100); trk_note_on(&trk[2], 60 + 3 * l, 100); }
            if (b > 10u && !(b % 7u)) { trk_note_on(&trk[3], 50 + b % 24u, 100); trk_note_on(&trk[1], 70 + b % 12u, 100); }
            if (b > 20u && !(b % 5u)) st_shed();         /* (late halves: the guard's last resort, audio.c shed_voice's copy) */
            mix_block(o, CTL);
            if (b % 40u == 39u) continue;
            for (l = 0; l < NVOICE; l++) {
                const voice_t *v = &trk[0].v[l];
                if (!v->active || b % 40u > 6u) continue;  /* (the first blocks after each hit: it still rings) */
                if ((uint32_t)v->s[0] < 3u) { held++; cut += v->stage == 4u; }
                else hats_cut += v->stage == 4u;
            }
        }
        snprintf(msg, sizeof msg, "KICK SNARE CLAP never cut with the budget full and the guard shedding (%u blocks of hits seen, %u fading; "
                 "the hats may go: %u)", held, cut, hats_cut);
        t_check(msg, held > 100u && !cut);
    }
    vsoft_host = 0;
    printf("%s\n", t_fail ? "voices: FAIL" : "voices: all passed");
    return t_fail ? 1 : 0;
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "cost";
    if (!strcmp(mode, "cost")) {
        printf("host timing, relative only (not the FM-1's CPU)\n");
        cost_table(1);
        return 0;
    }
    if (!strcmp(mode, "stress")) { stress_table(argc > 2 ? argv[2] : 0); return 0; }
    if (!strcmp(mode, "wav")) return wav_pairs(argc > 2 ? argv[2] : 0);
    if (!strcmp(mode, "test")) return tests();
    return 2;
}
