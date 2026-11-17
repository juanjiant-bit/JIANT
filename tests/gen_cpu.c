/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.39) GEN at full, its CPU (the owner: "glitches and the CPU at 90 %"): every genre, EXPLORE and RANDOM too, many
 * seeds, ENERGY 100 and every amount 100 (COMPLEX 50 and 100), each piece's song (FORM PARTS) played from its start to its
 * end and its audio halves (HALF_FRAMES of mix_block: the whole audio path) timed on the host, against SAVE > TOOLS 6 CPU
 * TEST's song (TRACKS 4) timed the same way: what the FM-1 is measured on, the heaviest it plays cleanly. The time is the
 * thread's own (another process on the host not counted); the render is deterministic, so each piece is played REP times
 * and each half keeps its fastest, and a piece near the budget is measured twice more and keeps its least. The numbers on
 * CPU TEST's scale, its callgrind count (tests/cpu_baseline_vg.txt song/cputest, GEN_CPU_NOW: 2836 in V10.7.43), against
 * the device's budget: the old CPU TEST's 3083 instructions a sample (GEN_CPU_REF; V10.7.43: CPU TEST got cheaper, the
 * FM-1 did not get slower: a piece is measured against what the FM-1 played cleanly). avg the whole song, win4 its heaviest
 * 4 s (the budget's measure), win its heaviest second, p99.5 and worst its halves; then GEN's estimate (gen.c gcpu_est,
 * the same scale), the tempo, the reverb, a field a track (role, engine and preset, voices, MONO, sends), F the pieces'
 * numbers for the fit (engine, voices, notes a step, gcpu_est's voices) and the voices each track sounded.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_cpu tests/gen_cpu.c -lm
 *   build/host/gen_cpu [GENRE_FROM GENRE_TO [SEEDS [SECONDS [REP]]]]   (GENRE 1..NGENRE, NGENRE + 1 EXPLORE, 0 RANDOM)
 *   env GEN_CPU_CHECK=N: run_tests.sh's check, N seeds a genre at TRACKS 4 and 6 (exit 1: a piece over CPU TEST's);
 *   GEN_CPU_ONE="GENRE SEED COMPLEX": that piece; GEN_CPU_LIST=FILE: those ("GENRE SEED COMPLEX" lines); GEN_CPU_DRY=1:
 *   the pieces made, not played (and their hash: H); GEN_CPU_SIX=1: TRACKS 6; GEN_CPU_NORMAL=1: ENERGY 50, every amount
 *   AUTO, COMPLEX 50; GEN_CPU_LOOP=1: section A as a loop; GEN_CPU_TWEAK=vN|rR|qQ: VOICES N, reverb TYPE R, its RATE Q
 *   on every track after GEN (experiments); GEN_CPU_WALL=1: the wall clock */
#define UI_TEST_NO_MAIN 1
#define HOST_LIFE LIFE_DEF                              /* (the sound as it ships: LIFE on, as cpu_vg) */
#include "ui_test.c"
#include <time.h>
#include <valgrind/callgrind.h>

static const char *RLN[6] = {"DRUM", "BASS", "PAD", "LEAD", "ARP", "TEX"};
static clockid_t gc_clock = CLOCK_THREAD_CPUTIME_ID;    /* (the thread's own time: another process's on the host not
                                                         * counted; GEN_CPU_WALL=1 the wall clock) */
static double gc_ns(void)
{
    struct timespec ts;
    clock_gettime(gc_clock, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}
#define MAXH 40000u                                     /* halves timed at most (116 s) */
static double half_ns[MAXH];
static uint32_t vsum[NTRK], vmax[NTRK], vn;             /* the voices each track sounded (the first play): sum, most */
static char feat[4096];                                 /* the piece as GEN made it (before it plays: its sections mute) */
typedef struct { double avg, win, win4, p99, worst; } cost_t;
static void (*remake)(void);                            /* the piece made again (deterministic: the same) */
static uint32_t fg, fs, fc, normal, loop_a, dry;            /* the piece: genre, seed, COMPLEX; normal settings; A alone */
static void cpu_test_now(void);

static void tracks_set(void)                            /* MENU > TRACKS: 4 (the default) or 6 (GEN_CPU_SIX=1) */
{
    if (getenv("GEN_CPU_SIX") && atoi(getenv("GEN_CPU_SIX"))) ui_rec_prefs |= PREF_SIX >> 8;
    else ui_rec_prefs &= (uint8_t)~(PREF_SIX >> 8);
    rp_apply();
}
static void cpu_test_now(void)                          /* (the device's: TRACKS 4, whatever the pieces') */
{
    ui_power_on(); go_home(); frames(4);
    ui_rec_prefs &= (uint8_t)~(PREF_SIX >> 8); rp_apply();
    cpu_test_song();
}
static void gen_full_now(void)
{
    uint32_t k;
    ui_power_on(); go_home(); frames(4);
    tracks_set();
    gp.genre = (uint8_t)fg; gp.seed = (uint16_t)fs; gp.energy = normal ? 50 : 100; gp.form = 1;
    for (k = 0; k < GP_N; k++) gp.amt[k] = normal ? GA_AUTO : 100;
    gp.complex = (uint8_t)fc; gp.swing = GA_AUTO; gp.human = 30;
    gen_go();
    if (getenv("GEN_CPU_TWEAK")) {                      /* (an experiment on section A: GEN_CPU_LOOP=1) vN: every POLY
                                                         * melodic track's VOICES at most N; rR: the reverb's type R */
        const char *w = getenv("GEN_CPU_TWEAK");
        for (; *w; w++) {
            if (*w == 'v') for (k = 0; k < NTRK; k++) { if (trk[k].eng_req != ENGI_DRUM && trk[k].p[P_VOICE] == V_POLY) trk[k].p[P_VCAP] = (int16_t)(atoi(w + 1) + 1); }
            if (*w == 'r') song.g[G_RTYPE] = (int16_t)atoi(w + 1);
            if (*w == 'q') song.g[G_RRATE] = (int16_t)atoi(w + 1);
        }
    }
}
static uint32_t gc_occ(const track_t *t)                /* gen.c gcpu_est's voices of a track (Q4), for the fit */
{
    const step_t *s = t->step;
    uint32_t e = t->eng_req, len = (uint32_t)t->p[P_SLEN], occ = 0, held = 0, i, w;
    uint32_t rel = TIME_MS_X10[t->p[P_REL]] * (uint32_t)song.g[G_BPM] / 6262u + (uint32_t)t->p[P_SGATE] / 8u;
    for (i = 0; i < len; i++, s++) {
        if (s->time == ST_TIE) { occ += held << 4; continue; }
        held = s->time != ST_NOTE ? 0u : s->n ? s->n : 1u;
        occ += held * rel;
    }
    occ /= len;
    w = t->p[P_VOICE] != V_POLY || e == ENGI_DRUM ? 16u : 128u;
    return occ < w ? occ : w;
}
#define snprintf(b, z, ...) ((z) ? snprintf(b, z, __VA_ARGS__) : 0)   /* (feat full: nothing more) */
static uint32_t gc_fnv(uint32_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    while (n--) h = (h ^ *b++) * 16777619u;
    return h;
}
static uint32_t gc_hash(void)                         /* (GEN_CPU_DRY) the whole piece: every track, the song, its sections */
{
    uint32_t k, h = 2166136261u;
    for (k = 0; k < NTRK; k++) {
        h = gc_fnv(h, trk[k].p, sizeof trk[k].p); h = gc_fnv(h, &trk[k].eng_req, 1); h = gc_fnv(h, &trk[k].preset, sizeof trk[k].preset);
        h = gc_fnv(h, trk[k].step, sizeof trk[k].step);
    }
    h = gc_fnv(h, song.g, sizeof song.g); h = gc_fnv(h, motion.event, motion.count * sizeof motion.event[0]);
    h = gc_fnv(h, dx_kit, sizeof dx_kit); h = gc_fnv(h, glane, sizeof glane); h = gc_fnv(h, fg_set, sizeof fg_set);
    h = gc_fnv(h, macro_v, sizeof macro_v); h = gc_fnv(h, &chain_config, sizeof chain_config);
    for (k = 0; k < 4u; k++) h = gc_fnv(h, proj_slot[k].raw, sizeof proj_slot[k].raw);
    return h;
}
static void snapshot(void)                              /* feat: the piece as made */
{
    uint32_t k, n = 0;
    n += (uint32_t)snprintf(feat + n, n < sizeof feat ? sizeof feat - n : 0u, " est %4u bpm %d rtype %d |", gcpu_est(), song.g[G_BPM], song.g[G_RTYPE]);
    for (k = 0; k < NTRK; k++) {
        const track_t *t = &trk[k];
        const engine_t *e = ENGINES[t->eng_req % NENGINES];
        if (k >= trk_live || t->p[P_MUTE] || !gs.act[gs.role[k]]) { n += (uint32_t)snprintf(feat + n, n < sizeof feat ? sizeof feat - n : 0u, " T%u -", k + 1u); continue; }
        n += (uint32_t)snprintf(feat + n, n < sizeof feat ? sizeof feat - n : 0u, " T%u %s:%s/%s v%u%s r%d d%d c%d", k + 1u, RLN[gs.role[k] % 6u], e->name,
                                t->preset < e->npresets ? e->presets[t->preset].name : "-", trk_nvoice(t),
                                t->p[P_VOICE] == V_POLY ? "" : "m", t->p[P_REV], t->p[P_DLY], t->p[P_CHOR]);
    }
    n += (uint32_t)snprintf(feat + n, n < sizeof feat ? sizeof feat - n : 0u, " F rt=%d", song.g[G_RTYPE]);
    for (k = 0; k < NTRK; k++) {                        /* (the numbers: engine, VOICES, mode, most notes a step, the share
                                                         * of steps starting notes, their notes on average, components) */
        const track_t *t = &trk[k];
        uint32_t i, mx = 0, on = 0, len = (uint32_t)t->p[P_SLEN], nn = 0, co = 0;
        if (k >= trk_live || t->p[P_MUTE] || !gs.act[gs.role[k]]) continue;
        if (len < 1u || len > NSTEP) len = 16;
        for (i = 0; i < len; i++) {
            const step_t *st = &t->step[i];
            uint32_t m = st->n, l;
            if (st->time != ST_NOTE) continue;
            if (t->eng_req == ENGI_DRUM) for (m = 0, l = 0; l < 8u; l++) m += (st->hit >> l) & 1u;
            if (!m) continue;
            on++; nn += m; if (m > mx) mx = m;
            co += (st->comp != 0) + (st->comp2 != 0);
        }
        n += (uint32_t)snprintf(feat + n, n < sizeof feat ? sizeof feat - n : 0u, " t=%u:%u:%u:%u:%.3f:%.2f:%u:%u:%.2f:%u:%u:%d:%d", t->eng_req % NENGINES, trk_nvoice(t),
                                (uint32_t)t->p[P_VOICE] & 3u, mx, (double)on / len, on ? (double)nn / on : 0.0, co, gs.role[k] % 6u,
                                gc_occ(t) / 16.0, (t->p[P_M1SRC] != 0) + (t->p[P_M2SRC] != 0) + (t->p[P_M3SRC] != 0) + (t->p[P_M4SRC] != 0),
                                (t->p[P_LD_FLT] != 0) + (t->p[P_LD_PIT] != 0) + (t->p[P_LD_AMP] != 0), t->p[P_DETUNE], t->p[P_DIST]);
    }
}
/* the piece made, played from its start for nh halves, rep times: each half's fastest kept */
#undef snprintf
static uint32_t song_halves(uint32_t cap)                /* a song's length (its rows, 16 steps a bar at its tempo) and a
                                                         * second more, in halves; a loop (or GEN_CPU_LOOP): cap */
{
    uint32_t k, bars = 0, nh;
    if (!live.mode || loop_a) return cap;
    for (k = 0; k < chain_config.count; k++) bars += chain_config.row[k].bars;
    nh = (uint32_t)((uint64_t)bars * 4u * 60u * FS / (uint32_t)(song.g[G_BPM] > 0 ? song.g[G_BPM] : 120) / HALF_FRAMES) + (uint32_t)FS / HALF_FRAMES;
    return nh < cap ? nh : cap;
}
static uint32_t play_timed(uint32_t nh, uint32_t rep)
{
    uint32_t r, h, b;
    if (nh > MAXH) nh = MAXH;
    for (h = 0; h < nh; h++) half_ns[h] = 1e30;
    CALLGRIND_ZERO_STATS;
    for (r = 0; r < rep; r++) {
        uint32_t f = 0, nsamp = 0;
        remake();
        if (!r && remake != cpu_test_now) { nh = song_halves(nh); for (h = 0; h < nh; h++) half_ns[h] = 1e30; }
        if (!r) { snapshot(); memset(vsum, 0, sizeof vsum); memset(vmax, 0, sizeof vmax); vn = 0; }
        song.master_q12 = 4096;
        if (loop_a) live.mode = 0;                      /* (GEN_CPU_LOOP=1: section A, every part, as a loop) */
        if (live.mode) chain_play_ui(); else transport_req = 1;
        for (h = 0; h < nh; h++) {
            double t0, t1;
            uint32_t upto = (uint32_t)((uint64_t)(f + 1u) * FS / 30u);
            if (nsamp >= upto) { f++; ui_input(); song_poll(); host_ticks += 1000000u / 30u; fm1_ms += 1000u / 30u; }
            t0 = gc_ns();
            if (h >= 8u) CALLGRIND_START_INSTRUMENTATION;
            for (b = 0; b < HALF_FRAMES; b += CTL) {
                int32_t o[2 * CTL];
                mix_block(o, CTL);
            }
            if (h >= 8u) CALLGRIND_STOP_INSTRUMENTATION;
            t1 = gc_ns();
            nsamp += HALF_FRAMES;
            if (t1 - t0 < half_ns[h]) half_ns[h] = t1 - t0;
            if (!r) {
                uint32_t k, i, n;
                for (k = 0; k < NTRK; k++) {
                    for (i = n = 0; i < NVOICE; i++) n += trk[k].v[i].active != 0;
                    vsum[k] += n; if (n > vmax[k]) vmax[k] = n;
                }
                vn++;
            }
        }
        transport_req = 2; ui_input(); song_poll();
        { int32_t o[2 * CTL]; mix_block(o, CTL); }
    }
    return nh;
}
static cost_t summ(uint32_t nh)
{
    static double s[MAXH];
    cost_t c = {0, 0, 0, 0, 0};
    uint32_t h, i, j, W = (uint32_t)FS / HALF_FRAMES, W4 = 4u * W;
    double acc = 0, acc4 = 0;
    for (h = 8; h < nh; h++) {                          /* (the first halves: the start) */
        c.avg += half_ns[h];
        if (half_ns[h] > c.worst) c.worst = half_ns[h];
        acc += half_ns[h];
        if (h >= 8u + W) acc -= half_ns[h - W];
        if (h + 1u >= 8u + W && acc / W > c.win) c.win = acc / W;
        acc4 += half_ns[h];                             /* (the heaviest 4 s: a section's, a burst of the host's
                                                         * noise a small part of it) */
        if (h >= 8u + W4) acc4 -= half_ns[h - W4];
        if (h + 1u >= 8u + W4 && acc4 / W4 > c.win4) c.win4 = acc4 / W4;
    }
    if (nh < 8u + W4) c.win4 = c.win;
    c.avg /= (double)(nh - 8u);
    for (h = 8, j = 0; h < nh; h++) s[j++] = half_ns[h];
    for (i = 1; i < j; i++) { double x = s[i]; int32_t k = (int32_t)i - 1; while (k >= 0 && s[k] > x) { s[k + 1] = s[k]; k--; } s[k + 1] = x; }
    c.p99 = s[(uint32_t)((double)j * 0.995)];
    return c;
}
static uint32_t played;                                 /* the halves the last measure played */
static cost_t measure(uint32_t nh, uint32_t rep)
{
    played = play_timed(nh, rep);
    return summ(played);
}
static cost_t least(cost_t a, cost_t b)
{
    a.avg = a.avg < b.avg ? a.avg : b.avg; a.win = a.win < b.win ? a.win : b.win; a.win4 = a.win4 < b.win4 ? a.win4 : b.win4;
    a.p99 = a.p99 < b.p99 ? a.p99 : b.p99; a.worst = a.worst < b.worst ? a.worst : b.worst;
    return a;
}
static double scale;                                    /* ns a half -> CPU TEST's instructions a sample */
static void report(const char *name, uint32_t seed, uint32_t cx, cost_t c)
{
    uint32_t k;
    printf("%-14s seed %4u cx %3u %3.0fs avg %5.0f win4 %5.0f win %5.0f p99.5 %5.0f worst %5.0f%s ", name, seed, cx, (double)played * HALF_FRAMES / FS, c.avg * scale, c.win4 * scale, c.win * scale,
           c.p99 * scale, c.worst * scale, feat);
    for (k = 0; k < NTRK; k++)
        if (vmax[k]) printf(" [T%u %.1f/%u]", k + 1u, vn ? (double)vsum[k] / vn : 0.0, vmax[k]);
    printf("\n");
    fflush(stdout);
}
int main(int argc, char **argv)
{
    uint32_t from = argc > 1 ? (uint32_t)atoi(argv[1]) : 0, to = argc > 2 ? (uint32_t)atoi(argv[2]) : NGENRE + 1u;
    uint32_t seeds = argc > 3 ? (uint32_t)atoi(argv[3]) : 8, rep = argc > 5 ? (uint32_t)atoi(argv[5]) : 3;
    double secs = argc > 4 ? atof(argv[4]) : 12.0, ref = getenv("GEN_CPU_REF") ? atof(getenv("GEN_CPU_REF")) : 3083.0, now = 0, bud;
    uint32_t nh = (uint32_t)(secs * FS / HALF_FRAMES), g, s, c;
    cost_t ct, x;
    if (nh > MAXH) nh = MAXH;
    normal = getenv("GEN_CPU_NORMAL") && atoi(getenv("GEN_CPU_NORMAL"));
    dry = getenv("GEN_CPU_DRY") && atoi(getenv("GEN_CPU_DRY"));
    if (getenv("GEN_CPU_WALL")) gc_clock = CLOCK_MONOTONIC;
    loop_a = getenv("GEN_CPU_LOOP") && atoi(getenv("GEN_CPU_LOOP"));
    remake = cpu_test_now;
    if (dry) goto pieces;
    ct = least(measure(nh, rep), measure(nh, rep));
    for (c = 0; c < 6u && ct.win4 > ct.avg * 1.03; c++)   /* (its heaviest 4 s ~1 % over its average: a busy host the rest) */
        ct = least(ct, measure(nh, rep));
    CALLGRIND_DUMP_STATS_AT("cputest");
    if (getenv("GEN_CPU_NOW"))                          /* (V10.7.43) CPU TEST today, on callgrind's scale */
        now = atof(getenv("GEN_CPU_NOW"));
    else {
        FILE *bf = fopen("tests/cpu_baseline_vg.txt", "r");
        char ln[96];
        while (bf && fgets(ln, sizeof ln, bf))
            if (sscanf(ln, "song/cputest %lf", &now) == 1)
                break;
        if (bf) fclose(bf);
    }
    if (now <= 0) now = ref;
    scale = now / ct.avg;
    bud = ref / now;                                    /* (the budget over CPU TEST's own: 3083 / 2836 = 1.087) */
    feat[0] = 0;
    report("CPU TEST", 0, 0, ct);
pieces:
    remake = gen_full_now;
    if (getenv("GEN_CPU_ONE")) {
        fg = fs = 0; fc = 50;
        sscanf(getenv("GEN_CPU_ONE"), "%u %u %u", &fg, &fs, &fc);
        x = measure(nh, rep);
        CALLGRIND_DUMP_STATS_AT("one");
        report(gs.g->name, fs, fc, x);
        return 0;
    }
    if (getenv("GEN_CPU_CHECK")) {                      /* (run_tests.sh) N seeds a genre, EXPLORE and RANDOM too,
                                                         * COMPLEX 50 and 100, TRACKS 4 and 6, at full: each piece's
                                                         * heaviest 4 s under CPU TEST's own; and GEN's estimate within
                                                         * its limit unless no measure was left (no FM6, no CLOUD) */
        uint32_t n = (uint32_t)atoi(getenv("GEN_CPU_CHECK")), six, bad = 0, cnt = 0, k, fm;
        double worst = 0;
        for (six = 0; six < 2u; six++) {
            setenv("GEN_CPU_SIX", six ? "1" : "0", 1);
            for (g = 0; g <= NGENRE + 1u; g++)
                for (c = 0; c < 2u; c++)
                    for (s = 0; s < n; s++) {
                        fg = g; fs = 1u + s * 1237u % 9973u + g * 7u; fc = c ? 100u : 50u;
                        x = measure(nh, rep);
                        if (x.win4 > ct.win4 * bud * 0.97)   /* (near: twice more, the least) */
                            x = least(x, least(measure(nh, rep), measure(nh, rep)));
                        for (k = fm = 0; k < trk_live; k++) fm += trk[k].eng_req == ENGI_FM6 && gs.act[gs.role[k]] && !trk[k].p[P_MUTE];
                        cnt++;
                        if (x.win4 / (ct.win4 * bud) > worst) worst = x.win4 / (ct.win4 * bud);
                        if (x.win4 > ct.win4 * bud || (gcpu_est() > GCPU_MAX && (fm || song.g[G_RTYPE] == 4))) {
                            printf("gen_cpu: OVER  TRACKS %u ", six ? 6u : 4u);
                            report(gs.g->name, fs, fc, x);
                            bad++;
                        }
                    }
        }
        printf("gen_cpu: %u pieces at full (%u a genre, TRACKS 4 and 6), the heaviest %.0f instructions a sample (the budget %.0f, "
               "the old CPU TEST; CPU TEST now %.0f: %.1f %% of the budget): %s\n", cnt, 4u * n, worst * ct.win4 * bud * scale,
               ct.win4 * bud * scale, ct.win4 * scale, worst * 100.0, bad ? "FAILED" : "all within the budget");
        return bad ? 1 : 0;
    }
    if (getenv("GEN_CPU_LIST")) {                       /* (a file of "GENRE SEED COMPLEX" lines: those pieces) */
        FILE *f = fopen(getenv("GEN_CPU_LIST"), "r");
        while (f && fscanf(f, "%u %u %u", &fg, &fs, &fc) == 3) {
            x = measure(nh, rep);
            if (x.win4 * scale > 0.85 * ref) x = least(x, least(measure(nh, rep), measure(nh, rep)));
            printf("%-2u ", fg);
            report(gs.g->name, fs, fc, x);
        }
        if (f) fclose(f);
        return 0;
    }
    for (g = from; g <= to; g++)
        for (c = 0; c < (normal ? 1u : 2u); c++)
            for (s = 0; s < seeds; s++) {
                fg = g; fs = 1u + s * 1237u % 9973u + g * 7u; fc = c ? 100u : 50u;
                if (dry) {                              /* (GEN_CPU_DRY=1: the pieces as made, not played) */
                    gen_full_now(); snapshot();
                    printf("%-2u %-14s seed %4u cx %3u %s H %08x\n", g, gs.g->name, fs, fc, feat, gc_hash());
                    continue;
                }
                x = measure(nh, rep);
                if (x.win4 * scale > 0.85 * ref)         /* (near the budget: twice more, the least) */
                    x = least(x, least(measure(nh, rep), measure(nh, rep)));
                printf("%-2u ", g);
                report(gs.g->name, fs, fc, x);
            }
    return 0;
}
