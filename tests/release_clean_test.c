/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.43, the owner: "a little bitcrush in the releases, the hits of DRUM-X and the plucks are not clean") the
 * tails as the DAC gets them, through hostsim.c:
 *   build/host/release_clean_test [WAVDIR]
 * Each sound (the DRUM-X kick, snare, hat, tom, clap; STRING NYLON, WEST PLUCK, ANALOG PLUCK) struck once and
 * rendered as the DAC word (audio.c audio_block: Q15 << 7 and, since V10.7.43, the master's bits below Q15, fx.c
 * out_fr), two ways: dry (MASTER full, BALANCE and LIFE off) and in a project (BALANCE and LIFE at their defaults,
 * MASTER at half the pot: -12 dB); no sends either way (a reverb's tail is a noise of its own).
 * The tail: the first 4096 samples 48 .. 60 dB under the hit's loudest 1024. Its staircase: the mean step of the DAC
 * word where it moves (Q15 units; a smooth tail moving under a step a sample shows the quantiser's step). Its floor:
 * a third difference (the tail's partials, under ~2 kHz there, 40 dB and more under it; a white floor of rms s reads
 * s). V10.7.42: steps of ~3.7 dry (the mix's 2 low bits dropped before the master and before each part's LEVEL, the
 * knob's product cut to whole Q15 units, the DAC fed Q15 << 7) and ~1.1 .. 1.4 in the project (BALANCE's bands cut
 * to steps of 16 before their gains, its ~200 Hz band stuck under +-40); now ~1.1 .. 1.5 and ~0.35 .. 0.4. The
 * tonal tails (the kick, the tom, the plucks) are held to it; the noises' (snare, hat, clap) numbers shown.
 * WAVDIR: each render written there as a 24-bit WAV (rel_<SOUND>_<dry|proj>.wav), the DAC's word (x2: full scale) */
#define HOST_BAL 0
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "jiant_test_util.h"

static int fails;
#define check(what, ok, ...) (fails += jt_verdict("release: ", 66, (what), (ok), __VA_ARGS__))

#define NS (FS * 3u)
static int32_t dac[NS];                                 /* L, the DAC's word (Q22: Q15 << 7) */
static int32_t dacr[NS];

typedef struct { const char *name; uint32_t eng, pr, note, drum, tonal; } snd_t;
static const snd_t SND[] = {
    {"KICK", ENGI_DRUM, 0, 36, 1, 1}, {"SNARE", ENGI_DRUM, 0, 38, 1, 0}, {"HAT", ENGI_DRUM, 0, 42, 1, 0},
    {"TOM", ENGI_DRUM, 0, 45, 1, 1}, {"CLAP", ENGI_DRUM, 0, 39, 1, 0},
    {"STRING", ENGI_STRING, 0, 57, 0, 1}, {"WEST", ENGI_WEST, 8, 57, 0, 1}, {"ANALOG", ENGI_ANALOG, 8, 57, 0, 1},
};
#define NSND (sizeof SND / sizeof SND[0])

static void render(const snd_t *s, int proj)
{
    track_t *t = &trk[0];
    uint32_t k, i;
    for (k = 0; k < NTRK * NVOICE; k++)
        trk[k / NVOICE].v[k % NVOICE].active = 0;
    host_tracks_init();
    lev_env = 0; lev_g = lev_cur = 32768; lev_dg = 0; lim_env = LIM_T;
    dc_l = dc_r = dce_l = dce_r = 0;
    host_preset(t, s->eng, s->pr);
    if (s->drum)
        dx_kit_load(0);
    t->p[P_DIST] = 0;
    if (proj) {
        bal_v = BAL_DEF;
        life_v[LIFE_AMT] = LIFE_DEF;
        life_reset();
        song.master_q12 = master_of_pot(512);           /* (half the pot: -12 dB) */
    }
    t->p[P_CHOR] = t->p[P_DLY] = t->p[P_REV] = 0;       /* (no sends: a reverb's tail is a noise of its own) */
    trk_note_on(t, s->note, 100);
    if (s->drum)
        trk_note_off(t, s->note);
    for (k = 0; k < NS; k += CTL) {
        int32_t o[2 * CTL];
        if (!s->drum && k == FS / 5u)
            trk_note_off(t, s->note);
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
#ifdef MASTER_FRB
            dac[k + i] = o[2 * i] * 128 + (out_fr[2 * i] << (7 - MASTER_FRB));
            dacr[k + i] = o[2 * i + 1] * 128 + (out_fr[2 * i + 1] << (7 - MASTER_FRB));
#else
            dac[k + i] = o[2 * i] * 128;                /* (V10.7.42 and before: Q15 << 7) */
            dacr[k + i] = o[2 * i + 1] * 128;
#endif
        }
    }
}

/* the tail's window: the first 4096 samples (on a 1024 grid) whose rms is 48 .. 60 dB under the loudest 1024's;
 * its level, its floor above ~5 kHz (Q15 units), its flat runs */
typedef struct { double at, lvl, floor, snr, flat, step; } tail_t;
static int tail(tail_t *r)
{
    double pk = 0, x;
    uint32_t s, i;
    for (s = 0; s + 1024u <= NS; s += 1024u) {
        double e = 0;
        for (i = 0; i < 1024u; i++) { x = dac[s + i]; e += x * x; }
        e = sqrt(e / 1024.0);
        if (e > pk) pk = e;
    }
    for (s = 0; s + 4096u <= NS; s += 1024u) {
        double e = 0, d3 = 0, db, st = 0;
        uint32_t flat = 0;
        for (i = 0; i < 4096u; i++) { x = dac[s + i]; e += x * x; }
        e = sqrt(e / 4096.0);
        db = 20.0 * log10(e / pk + 1e-12);
        if (db > -48.0 || db < -60.0)
            continue;
        for (i = 3; i < 4096u; i++) {
            double d = (double)dac[s + i] - 3.0 * dac[s + i - 1u] + 3.0 * dac[s + i - 2u] - dac[s + i - 3u];
            d3 += d * d;
            flat += dac[s + i] == dac[s + i - 1u];
            st += fabs((double)dac[s + i] - dac[s + i - 1u]);
        }
        r->at = s / (double)FS;
        r->lvl = e / 128.0;
        r->floor = sqrt(d3 / 4093.0 / 20.0) / 128.0;
        r->snr = 20.0 * log10(r->floor / r->lvl + 1e-12);
        r->flat = flat / 4093.0;
        r->step = flat < 4093u ? st / (4093.0 - flat) / 128.0 : 0.0;   /* the mean step where it moves, Q15 */
        return 1;
    }
    return 0;
}

static void wav24(const char *dir, const char *name)
{
    char path[512];
    FILE *f;
    uint32_t i, v;
    uint16_t a = 1, ch = 2, ba = 6, bits = 24;
    snprintf(path, sizeof path, "%s/%s", dir, name);
    if (!(f = fopen(path, "wb")))
        return;
    fwrite("RIFF", 1, 4, f); v = 36 + NS * 6u; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); v = FS; fwrite(&v, 4, 1, f); v = FS * 6u; fwrite(&v, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = NS * 6u; fwrite(&v, 4, 1, f);
    for (i = 0; i < NS; i++) {
        int32_t s2[2] = {dac[i], dacr[i]};
        uint32_t c;
        for (c = 0; c < 2u; c++) {
            int32_t q = s2[c] > 4194303 ? 4194303 : s2[c] < -4194304 ? -4194304 : s2[c];
            uint8_t b[3];
            q *= 2;                                     /* (the DAC's word is Q22: x2 to the WAV's full scale) */
            b[0] = (uint8_t)q; b[1] = (uint8_t)(q >> 8); b[2] = (uint8_t)(q >> 16);
            fwrite(b, 1, 3, f);
        }
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    uint32_t k, p;
    char what[160], nm[96];
    for (p = 0; p < 2u; p++)
        for (k = 0; k < NSND; k++) {
            const snd_t *s = &SND[k];
            tail_t r = {0, 0, 0, 0, 0, 0};
            int ok;
            render(s, (int)p);
            if (argc > 1) {
                snprintf(nm, sizeof nm, "rel_%s_%s.wav", s->name, p ? "proj" : "dry");
                wav24(argv[1], nm);
            }
            ok = tail(&r);
            if (s->tonal) {                             /* (a tonal tail: its own slope well under a step) */
                snprintf(what, sizeof what, "%s %s: the tail's steps under %.1f, its floor under %.2f (Q15)", s->name,
                         p ? "in a project" : "dry", p ? 0.6 : 1.8, p ? 0.3 : 0.6);
                check(what, ok && r.step < (p ? 0.6 : 1.8) && r.floor < (p ? 0.3 : 0.6),
                      "at %.2f s: tail %.1f, step %.3f, floor %.3f, %.1f dB, flat %.3f", r.at, r.lvl, r.step, r.floor,
                      r.snr, r.flat);
            } else {                                    /* (a noise's tail: its own HF; its numbers shown) */
                snprintf(what, sizeof what, "%s %s: its numbers (a noise: not held)", s->name, p ? "in a project" : "dry");
                check(what, 1, "at %.2f s: tail %.1f, step %.3f, floor %.3f, %.1f dB, flat %.3f", r.at, r.lvl, r.step,
                      r.floor, r.snr, r.flat);
            }
        }
    printf("release: %s\n", fails ? "FAIL" : "all ok");
    return fails != 0;
}
