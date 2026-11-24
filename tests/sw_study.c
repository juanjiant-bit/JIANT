/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* SWARM study: scenarios x measurements (host only) */
#define main hostsim_main
#define HOST_OWN_TICKS 1
static unsigned host_clock;
#define fm1_ticks() (host_clock)
#include "hostsim.c"
#undef main
#include <math.h>
#include <time.h>
static void pkt(uint32_t dev, uint32_t lv, uint32_t ch)
{
    uint8_t p[30]; uint32_t i;
    p[0] = 0x00; p[1] = 28;
    for (i = 2; i < 30u; i++) p[i] = (uint8_t)(dev * 41u + i * (dev + 3u) * 17u);
    bt_air_tap(p, 30, lv | lv << 8, ch);
}
static void sw_fresh(uint32_t preset)
{
    host_tracks_init();
    host_preset(&trk[1], ENGI_INPUT, preset);
    memset(sw, 0, sizeof sw); memset(&sm, 0, sizeof sm);
    sw_lmin = 127; sw_lmax = 0; sw_prev = 0; sw_tick = 0; sw_rnd = 0x9E3779B9u; sw_key = 0;
    sw_pxon = 0; sw_pxv = 0; sw_pxf = sw_pxn = 0;
    memset(sw_ko, 0, sizeof sw_ko); memset(sw_kt, 0, sizeof sw_kt);
    ain_tr = 1; ain_sw = 1; host_clock = 0;
#ifdef SWT_N
    memset(sw_tab, 0, sizeof sw_tab); sw_tj = 0; sw_ti = 0; sw_tg = sw_tq = 0; sw_tt = 0; sw_dv = 0; trk[1].p[P_MORPH] = (int16_t)(getenv("WAVE") ? atoi(getenv("WAVE")) : 0);
#endif
    song.playing = 0; song.g[G_BPM] = 120;
}
#define NW 2048
static double win[NW]; static int wn;
typedef struct { double cen[1024], rms[1024]; int n; double pk, dc, jmp, last; } meas_t;
static void spec(meas_t *m)
{
    double re[NW/2], e = 0, num = 0, den = 0; int k, i;
    { double mu = 0; for (i = 0; i < NW; i++) mu += win[i]; mu /= NW; m->dc += fabs(mu); for (i = 0; i < NW; i++) { win[i] -= mu; e += win[i] * win[i]; } }
    /* a coarse DFT: 96 log-spaced bins 40 Hz .. 12 kHz */
    for (k = 0; k < 40; k++) {
        double f = 40.0 * pow(300.0, k / 39.0), w = 2 * M_PI * f / 44100.0, a = 0, b = 0;
        for (i = 0; i < NW; i++) { double h = 0.5 - 0.5 * cos(2 * M_PI * i / NW); a += win[i] * h * cos(w * i); b += win[i] * h * sin(w * i); }
        re[k] = a * a + b * b; num += re[k] * log2(f); den += re[k];
    }
    m->rms[m->n] = sqrt(e / NW); m->cen[m->n] = den > 1e-3 ? pow(2, num / den) : 0; m->n++;
}
/* scen 0 none, 1 one device far->near->far, 2 three devices coming and going, 3 PROX sweep */
static void run(int scen, double secs, meas_t *m)
{
    uint32_t blocks = (uint32_t)(secs * 44100 / CTL), b, i;
    m->n = 0; m->pk = 0; m->dc = 0; wn = 0; m->jmp = 0; m->last = 0;
    for (b = 0; b < blocks; b++) {
        double t = (double)b / blocks;
        host_clock += CTL * 24000000u / 44100u;
        if (scen == 1 && b % 40u == 0) pkt(1, (uint32_t)(6 + 54 * (t < 0.5 ? 2 * t : 2 - 2 * t)), 37);
        if (scen == 2) {
            if (b % 37u == 0 && t < 0.7) pkt(1, 20 + (uint32_t)(20 * t), 37);
            if (b % 53u == 5 && t > 0.2) pkt(2, 45 - (uint32_t)(25 * t), 38);
            if (b % 71u == 9 && t > 0.45 && t < 0.9) pkt(3, 12 + (uint32_t)(40 * (t - 0.45)), 39);
        }
        if (scen == 3) { sw_pxon = 1; sw_pxv = (int16_t)(32767 * (0.5 - 0.5 * cos(2 * M_PI * t * 2))); }
        ain_swarm(CTL);
#ifdef SWT_N
        if (sw_wv) { int q; double r = 0, id = 0; int32_t aux = 0; extern double tab_worst, tab_sum; extern long tab_n; static double hist[24]; static uint32_t hk, htt = ~0u;
            /* (V10.8.38) the tables' largest step against the formula's own: the cycles the tables are made of (the last
             * 24 turns' moments, each as written: its time and drive held; the pole leaves 4 % of an older one) */
            for (q = 0; q < 64; q++) { double d = fabs((double)sw_tab[0][q + 1] - sw_tab[0][q]); if (d > r) r = d;
                d = fabs((double)softclip(lofi_float(SW_WF[sw_wv - 1], sw_tt, ((q + 1) & 63) << 7, &aux) * sw_dv >> 5) - softclip(lofi_float(SW_WF[sw_wv - 1], sw_tt, q << 7, &aux) * sw_dv >> 5)); if (d > id) id = d; }
            if (sw_tt != htt) { htt = sw_tt; hist[hk++ % 24u] = id; }
            for (q = 0, id = 0; q < 24; q++) if (hist[q] > id) id = hist[q];
            r = r / (id + 1); if (b > 400u) { if (r > tab_worst) tab_worst = r; tab_sum += r; tab_n++; } }
#endif
        for (i = 0; i < CTL; i++) {
            double x = sw_on ? (sw_l[i] + sw_r[i]) / 2.0 : 0;
            if (fabs(x) > m->pk) m->pk = fabs(x);
            if (fabs(x - m->last) > m->jmp) m->jmp = fabs(x - m->last);
            m->last = x;
            win[wn++] = x;
            if (wn == NW) { spec(m); wn = 0; }
        }
    }
}
double tab_worst, tab_sum; long tab_n;
static void report(const char *name, meas_t *m)
{
    double lo = 1e9, hi = 0, s = 0, s2 = 0, r = 0, rlo = 1e9, rhi = 0, mv = 0; int i, n = 0, bins[40] = {0}, nb = 0, sil = 0;
    for (i = 0; i < m->n; i++) {
        r += m->rms[i];
        if (m->rms[i] < 30) { sil++; continue; }
        double c = log2(m->cen[i]);
        if (m->cen[i] < lo) lo = m->cen[i]; if (m->cen[i] > hi) hi = m->cen[i];
        if (m->rms[i] < rlo) rlo = m->rms[i]; if (m->rms[i] > rhi) rhi = m->rms[i];
        s += c; s2 += c * c; n++;
        bins[(int)clamp((int)((c - log2(40.0)) * 3), 0, 39)] = 1;
        if (i && m->rms[i - 1] >= 30) mv += fabs(c - log2(m->cen[i - 1]));
    }
    for (i = 0; i < 40; i++) nb += bins[i];
    printf("%-34s rms %6.0f (%.1f dBFS) pk %6.0f dyn %4.1f dB | centroid %5.0f..%5.0f Hz, sd %.2f oct, 1/3-oct bins %2d, move %.3f oct/win, silent %d%% dc %.0f jump %.2f of pk\n",
           name, r / m->n, 20 * log10(r / m->n / 32768.0 + 1e-9), m->pk, n ? 20 * log10(rhi / rlo) : 0, n ? lo : 0, hi,
           n ? sqrt(fabs(s2 / n - (s / n) * (s / n))) : 0, nb, n > 1 ? mv / n : 0, 100 * sil / (m->n ? m->n : 1), m->dc / (m->n ? m->n : 1), m->jmp / (m->pk + 1));
}
static double dist(meas_t *a, meas_t *b, double *dl)
{
    double d = 0, ra = 0, rb = 0; int i, n = a->n < b->n ? a->n : b->n, k = 0;
    for (i = 0; i < n; i++) { ra += a->rms[i]; rb += b->rms[i]; if (a->rms[i] >= 30 && b->rms[i] >= 30 && a->cen[i] > 1 && b->cen[i] > 1) { d += fabs(log2(a->cen[i] / b->cen[i])); k++; } }
    *dl = 20 * log10((rb + 1) / (ra + 1));
    return k ? d / k : 0;
}
int main(void)
{
    static meas_t m, m0, m1;
    static const char *SN[4] = {"alone (no source)", "1 device far>near>far", "3 devices come and go", "PROX sweep x2"};
    static const char *PN[3] = {"SWARM", "SWARM PLUCK", "SWARM CHOIR"};
    static const char *KN[8] = {"SOURCE", "PLUCK", "FM", "MODE", "IMAGE", "DRIFT", "OCTAVE", "CHAOS"};
    int s, p, k; char nm[64];
    if (getenv("TAB")) { for (s = 0; s < 4; s++) { tab_worst = tab_sum = 0; tab_n = 0; sw_fresh(9); run(s, 20, &m); printf("table steps vs the formula's own, %s: worst x%.2f mean x%.2f\n", SN[s], tab_worst, tab_sum / (tab_n + 1)); } return 0; }
    for (p = 0; p < 3; p++)
        for (s = 0; s < 4; s++) { sw_fresh(9u + p); run(s, 20, &m); snprintf(nm, sizeof nm, "%s / %s", PN[p], SN[s]); report(nm, &m); }
    if (getenv("KNOBS")) {
    { double dl, d; sw_fresh(9); run(1, 10, &m0); sw_fresh(9); run(1, 10, &m1); d = dist(&m0, &m1, &dl); printf("baseline (same run twice): cen %.3f oct lvl %+.2f dB\n", d, dl); }
    printf("knobs (preset SWARM, 0 vs 127, 10 s)\n");
    for (s = 0; s < 4; s++)
        for (k = 1; k < 8; k++) {
            double dl, d;
            if (k == 3) continue;
            sw_fresh(9); trk[1].p[P_E0 + k] = 0; run(s, 10, &m0);
            sw_fresh(9); trk[1].p[P_E0 + k] = 127; run(s, 10, &m1);
            d = dist(&m0, &m1, &dl);
            printf("  %-22s %-7s cen %.2f oct  lvl %+.1f dB\n", SN[s], KN[k], d, dl);
        }
    }
    { struct timespec a, b2; uint32_t b; int sc;
      for (sc = 0; sc < 3; sc += 2) { sw_fresh(9); for (b = 0; b < 2000; b++) { host_clock += CTL * 24000000u / 44100u; if (sc == 2) { if (b % 7u == 0) pkt(1 + b % 6u, 20 + b % 30u, 37 + b % 3u); } ain_swarm(CTL); }
      clock_gettime(CLOCK_MONOTONIC, &a); for (b = 0; b < 40000; b++) { host_clock += CTL * 24000000u / 44100u; if (sc == 2 && b % 7u == 0) pkt(1 + b % 6u, 20 + b % 30u, 37 + b % 3u); ain_swarm(CTL); } clock_gettime(CLOCK_MONOTONIC, &b2);
      printf("cpu %s: %.0f ns/block\n", sc ? "6 devices" : "alone", ((b2.tv_sec - a.tv_sec) * 1e9 + (b2.tv_nsec - a.tv_nsec)) / 40000); } }
    /* the morph alone: where does it live */
    { uint32_t b, hist[9] = {0}; sw_fresh(9); for (b = 0; b < 30 * 44100 / CTL; b++) { host_clock += CTL * 24000000u / 44100u; ain_swarm(CTL); hist[sm.mq >> 4]++; }
      printf("\nalone: morph histogram (0 FAR .. 8 NEAR), 30 s:"); for (b = 0; b < 9; b++) printf(" %u%%", hist[b] * 100 / (30 * 44100 / CTL)); printf("\n"); }
    return 0;
}
