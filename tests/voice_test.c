/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* VOICE words test (src/sing.c, src/eng_formant.c, JIANT 0.8.5) on the host, through hostsim.c as regress.c.
 *   build/host/voice_test [DEMODIR]          (run_tests.sh: build/voice_demo)
 *   build/host/voice_test --g2p LANG WORDS..  (prints each word's phonemes, | a syllable)
 * 1. reading: Spanish by rules (a word list: phonemes and syllables), Japanese romaji -> morae, English through the
 *    dictionary (and its letter rules for a word it lacks); every bank word's syllables as gen_lyrics.py counted them.
 * 2. the generator: the same phrase for the same seed, another for another seed, as many syllables as the pattern's
 *    notes, every word a real one of the bank.
 * 3. the sound: the vowels' formants (an LPC-free peak pick on the spectrum of a held vowel) inside their ranges;
 *    s / sh / f frication bands; a plosive's closure and burst; no click at syllable changes; no DC; no clipping at
 *    the corners; LANG OFF bit for bit the vowels of before (the goldens hold that too); (V10.6y) an open A under full
 *    scale, as loud as the other engines; CONS 50 .. 400 % the consonants' length.
 * 4. the lyric: notes sing the next syllable, a TIE holds it, "-" holds the vowel, POS jumps, LOOP restarts; a
 *    project round trip with lyrics, a project without them loads none.
 * 5. cost: host ns per sample and voice, vowels against words (the counts: tests/cpu_vg.c eng/VOICE.*).
 * 6. demos into DEMODIR: Spanish, English, Japanese phrases on a melody, the dice's lyrics, every word preset. */
#define main hostsim_main
#define LY_EN_REF 1              /* felucca_lyrics.h: the English reference words, their code texts and phonemes */
#include <ctype.h>
#include "hostsim.c"
#undef main

#include "jiant_test_util.h"

static int bad;
static void check(const char *what, int ok) { bad += jt_verdict("voice: ", 100, what, ok, NULL); }
static uint32_t eng_voice(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == &ENG_FORMANT)
            return e;
    return 0;
}
static int32_t preset_of(const char *name)
{
    uint32_t k;
    for (k = 0; k < ENG_FORMANT.npresets; k++)
        if (str_eq(ENG_FORMANT.presets[k].name, name))
            return (int32_t)k;
    printf("voice: no preset %s\n", name);
    bad++;
    return 0;
}

/* a word's phonemes as text: "|k o |r a |s o n" */
static void g2p_str(uint32_t lang, const char *w, char *out, uint32_t max)
{
    uint8_t ph[SW_MAX];
    char nw[24];
    uint32_t ns, n = sy_word(lang, nw, sy_norm_lang(lang, w, (uint32_t)strlen(w), nw, sizeof nw), ph, &ns), k, m = 0;
    out[0] = 0;
    for (k = 0; k < n; k++) {
        m += (uint32_t)snprintf(out + m, max - m, "%s%s%s", k ? " " : "", ph[k] & SYB ? "|" : "", PH_NAME[ph[k] & 0x3Fu]);
        if (m >= max)
            break;
    }
}

/* ------------------------------------------------------------------------------------------- rendering --- */
typedef struct { uint8_t note, len; } nt_t;   /* a note (0: a rest), its length in 1/8 beats at 100 BPM */
/* track 0: preset pi, the lyric text; then the notes, each a syllable; the mix into w (if w), the voice sum into y
 * (if y: n samples at most); returns the frames */
static uint32_t sing_phrase(int32_t pi, const char *text, const nt_t *nt, uint32_t nn, FILE *w, double *y, uint32_t ymax)
{
    static int32_t o[2 * CTL];
    uint32_t k, i, frames = 0, eighth = FS * 60u / 100u / 2u;
    memset(trk, 0, sizeof trk);
    memset(sing, 0, sizeof sing);
    memset(sing_io, 0, sizeof sing_io);
    host_tracks_init();
    host_preset(&trk[0], eng_voice(), (uint32_t)pi);
    lyr_set(0, text);
    if (text && text[0])
        trk[0].p[P_VLYR] = 0;                           /* TEXT */
    if (getenv("DRY"))
        trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    for (k = 0; k <= nn; k++) {
        uint32_t len = k < nn ? nt[k].len * eighth : FS, note = k < nn ? nt[k].note : 0;
        if (note)
            trk_note_on(&trk[0], note, 100);
        for (i = 0; i < len; i += CTL) {
            uint32_t j;
            if (note && i + CTL >= len - len / 8u && i < len - len / 8u)
                trk_note_off(&trk[0], note);
            mix_block(o, CTL);
            if (getenv("VDEBUG")) {                   /* each phoneme as it begins */
                static uint32_t last;
                const sing_t *s = &sing[0];
                uint32_t key = (uint32_t)s->nsy << 16 | (uint32_t)s->i << 8 | s->st;
                if (key != last)
                    printf("%6.3f  #%-3u %-12s %-3s st%u  F %4d %4d %4d  ff %4d  v %5d a %5d f %5d\n",
                           (double)frames / FS, s->nsy, s->w, PH_NAME[sing_phon(s)], s->st, s->f[0], s->f[1], s->f[2],
                           s->ff, s->gv, s->ga, s->gf);
                last = key;
            }
            for (j = 0; j < CTL; j++) {
                if (w)
                    wav_put(w, o[2 * j], o[2 * j + 1]);
                if (y && frames + j < ymax)
                    y[frames + j] = (o[2 * j] + o[2 * j + 1]) / 65536.0;
            }
            frames += CTL;
        }
    }
    return frames;
}
static void demo_wav(const char *dir, const char *name, int32_t pi, const char *text, const nt_t *nt, uint32_t nn)
{
    char path[512];
    uint32_t frames;
    FILE *w;
    snprintf(path, sizeof path, "%s/%s.wav", dir, name);
    if (!(w = fopen(path, "wb")))
        return;
    wav_hdr(w, 0);
    frames = sing_phrase(pi, text, nt, nn, w, 0, 0);
    fseek(w, 0, SEEK_SET);
    wav_hdr(w, frames);
    fclose(w);
}

/* (V10.7.33) sing.c sy_es_tok as it was before its table */
static uint32_t es_tok_ref(const char *w, uint32_t n, uint8_t *tc, uint8_t *tk)
{
    uint32_t i, m = 0;
    for (i = 0; i < n && m + 2u <= SW_MAX; i++) {
        int c = w[i], nx = i + 1u < n ? w[i + 1u] : 0, nx2 = i + 2u < n ? w[i + 2u] : 0;
        uint32_t code = 0xFFu, kind = TK_C;
        switch (c) {
        case 'a': code = PH_A; kind = TK_S; break;
        case 'e': code = PH_E; kind = TK_S; break;
        case 'o': code = PH_O; kind = TK_S; break;
        case 'i': code = PH_I; kind = TK_W; break;
        case 'u': code = PH_U; kind = TK_W; break;
        case 'y':
            if (sy_isv(nx)) code = PH_Y;
            else { code = PH_I; kind = TK_W; }
            break;
        case 'c':
            if (nx == 'h') { code = PH_CH; i++; }
            else code = nx == 'e' || nx == 'i' ? PH_S : PH_K;
            break;
        case 'q': code = PH_K; if (nx == 'u') i++; break;
        case 'g':
            if (nx == 'e' || nx == 'i') code = PH_X;
            else { code = PH_G; if (nx == 'u' && (nx2 == 'e' || nx2 == 'i')) i++; }
            break;
        case 'j': code = PH_X; break;
        case 'x': tc[m] = PH_K; tk[m++] = TK_C; code = PH_S; break;
        case 'z': case 's': code = PH_S; break;
        case 'v': case 'b': code = PH_B; break;
        case 'd': code = PH_D; break;
        case 'f': code = PH_F; break;
        case 'k': code = PH_K; break;
        case 'p': code = PH_P; break;
        case 't': code = PH_T; break;
        case 'm': code = PH_M; break;
        case 'w': code = PH_W; break;
        case 'l':
            if (nx == 'l') { code = PH_Y; i++; }
            else code = PH_L;
            break;
        case 'n':
            if (nx == 'y' && sy_isv(nx2)) { code = PH_NY; i++; }
            else code = PH_N;
            break;
        case 'r':
            if (nx == 'r') { code = PH_RR; i++; }
            else code = !m || tc[m - 1u] == PH_N || tc[m - 1u] == PH_L || tc[m - 1u] == PH_S ? PH_RR : PH_RT;
            break;
        default: break;                                  /* h, the rest: silent */
        }
        if (code != 0xFFu) {
            tc[m] = (uint8_t)code;
            tk[m++] = (uint8_t)kind;
        }
    }
    return m;
}
/* (V10.7.33) sing.c sy_en_rules as it was before its table: the reference its table reads every word as */
static uint32_t en_rules_ref(const char *w, uint32_t n, uint8_t *tc, uint8_t *tk)
{
    uint32_t i, m = 0, magic = 0xFFu;
#define EN_T(c, k) do { if (m < SW_MAX) { tc[m] = (uint8_t)(c); tk[m++] = (uint8_t)(k); } } while (0)
    if (n >= 3u && w[n - 1u] == 'e' && !sy_isv(w[n - 2u]) && sy_isv(w[n - 3u]))
        magic = n - 3u;                                  /* a silent e: the vowel before says its name */
    for (i = 0; i < n; i++) {
        int c = w[i], nx = i + 1u < n ? w[i + 1u] : 0, nx2 = i + 2u < n ? w[i + 2u] : 0;
        if (i == n - 1u && c == 'e' && n > 2u) break;    /* (a final e: silent) */
        if (sy_isv(c) || c == 'y') {
            if (c == 'y' && (i == 0u || sy_isv(nx))) { EN_T(PH_Y, TK_C); continue; }
            if (c == 'y') { EN_T(i == n - 1u ? PH_I : PH_IH, TK_S); continue; }
            if (i == magic) {
                if (c == 'a') { EN_T(PH_E, TK_S); EN_T(PH_IH, TK_W); }
                else if (c == 'i') { EN_T(PH_AA, TK_S); EN_T(PH_IH, TK_W); }
                else if (c == 'o') { EN_T(PH_O, TK_S); EN_T(PH_UH, TK_W); }
                else if (c == 'u') EN_T(PH_U, TK_S);
                else EN_T(PH_I, TK_S);
                continue;
            }
            if ((c == 'e' && (nx == 'e' || nx == 'a')) ) { EN_T(PH_I, TK_S); i++; continue; }
            if (c == 'o' && nx == 'o') { EN_T(PH_U, TK_S); i++; continue; }
            if (c == 'o' && (nx == 'u' || nx == 'w')) { EN_T(PH_AA, TK_S); EN_T(PH_UH, TK_W); i++; continue; }
            if (c == 'a' && (nx == 'i' || nx == 'y')) { EN_T(PH_E, TK_S); EN_T(PH_IH, TK_W); i++; continue; }
            if (c == 'o' && nx == 'a') { EN_T(PH_O, TK_S); EN_T(PH_UH, TK_W); i++; continue; }
            if (c == 'o' && (nx == 'i' || nx == 'y')) { EN_T(PH_AO, TK_S); EN_T(PH_IH, TK_W); i++; continue; }
            if (c == 'a' && (nx == 'u' || nx == 'w')) { EN_T(PH_AO, TK_S); i++; continue; }
            if (c == 'i' && nx == 'g' && nx2 == 'h') { EN_T(PH_AA, TK_S); EN_T(PH_IH, TK_W); i += 2u; continue; }
            EN_T(c == 'a' ? PH_AE : c == 'e' ? PH_EH : c == 'i' ? PH_IH : c == 'o' ? PH_AA : PH_AH, TK_S);
            continue;
        }
        if (c == 't' && nx == 'h') { EN_T(PH_TH, TK_C); i++; continue; }
        if (c == 's' && nx == 'h') { EN_T(PH_SH, TK_C); i++; continue; }
        if (c == 'c' && nx == 'h') { EN_T(PH_CH, TK_C); i++; continue; }
        if (c == 'p' && nx == 'h') { EN_T(PH_F, TK_C); i++; continue; }
        if (c == 'w' && nx == 'h') { EN_T(PH_W, TK_C); i++; continue; }
        if (c == 'c' && nx == 'k') { EN_T(PH_K, TK_C); i++; continue; }
        if (c == 'q' && nx == 'u') { EN_T(PH_K, TK_C); EN_T(PH_W, TK_C); i++; continue; }
        if (c == 'k' && nx == 'n' && !i) continue;
        if (c == 'w' && nx == 'r') continue;
        if (c == 'g' && nx == 'h') { i++; continue; }
        if (c == 'n' && nx == 'g' && !sy_isv(nx2)) { EN_T(PH_NG, TK_C); i++; continue; }
        switch (c) {
        case 'b': EN_T(PH_B, TK_C); break;
        case 'c': EN_T(nx == 'e' || nx == 'i' || nx == 'y' ? PH_S : PH_K, TK_C); break;
        case 'd': EN_T(PH_D, TK_C); break;
        case 'f': EN_T(PH_F, TK_C); break;
        case 'g': EN_T(PH_G, TK_C); break;
        case 'h': if (sy_isv(nx)) EN_T(PH_H, TK_C); break;
        case 'j': EN_T(PH_J, TK_C); break;
        case 'k': EN_T(PH_K, TK_C); break;
        case 'l': if (nx != 'l') EN_T(PH_L, TK_C); break;
        case 'm': EN_T(PH_M, TK_C); break;
        case 'n': EN_T(PH_N, TK_C); break;
        case 'p': EN_T(PH_P, TK_C); break;
        case 'r': EN_T(PH_RA, TK_C); break;
        case 's': EN_T(i && sy_isv(w[i - 1u]) && sy_isv(nx) ? PH_Z : PH_S, TK_C); break;
        case 't': EN_T(PH_T, TK_C); break;
        case 'v': EN_T(PH_V, TK_C); break;
        case 'w': EN_T(PH_W, TK_C); break;
        case 'x': EN_T(PH_K, TK_C); EN_T(PH_S, TK_C); break;
        case 'z': EN_T(PH_Z, TK_C); break;
        default: break;
        }
    }
#undef EN_T
    return m;
}


/* (V10.7.33) phonemes as text ("/" first: the editor's, the NAME screen's): a VOICE EN track sings them (TEXT), and its
 * generator's words (W, S: phoneme codes since the dictionary left the device); the same lyric as English letters sings
 * too (the rules: a project before), otherwise */
static uint32_t render_en(uint32_t lyr, const char *text, int32_t *out, uint32_t max)
{
    static const nt_t NT[] = {{60, 2}, {62, 2}, {64, 1}, {65, 1}, {67, 4}, {65, 2}, {64, 2}, {62, 1}, {60, 3}, {0, 1},
                              {57, 2}, {59, 2}, {60, 4}, {62, 1}, {64, 1}, {67, 2}, {64, 2}, {60, 4}};
    static int32_t o[2 * CTL];
    uint32_t k, i, n = 0, eighth = FS * 60u / 100u / 2u, nn = sizeof NT / sizeof NT[0];
    memset(trk, 0, sizeof trk);
    memset(sing, 0, sizeof sing);
    memset(sing_io, 0, sizeof sing_io);
    formant_nz = 0x2545F491;                             /* (the breath's noise: from its start, a render repeats) */
    host_tracks_init();
    host_preset(&trk[0], eng_voice(), (uint32_t)preset_of("SINGER EN"));
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;   /* (dry: the FX lines carry nothing over) */
    trk[0].p[P_VLANG] = LG_EN;
    lyr_set(0, text);
    trk[0].p[P_VLYR] = (int16_t)lyr;
    for (k = 0; k < 3u * nn; k++) {
        uint32_t len = NT[k % nn].len * eighth, note = NT[k % nn].note;
        if (note)
            trk_note_on(&trk[0], note, 100);
        for (i = 0; i < len; i += CTL) {
            uint32_t j;
            if (note && i + CTL >= len - len / 8u && i < len - len / 8u)
                trk_note_off(&trk[0], note);
            track_render(&trk[0], o, CTL);              /* (the part alone, as vrender: no buses carried over) */
            for (j = 0; j < CTL && n < max; j++)
                out[n++] = o[j];
        }
    }
    return n;
}
static void check_phtext(void)
{
    static const char *const TX[][2] = {
        {"Love me tender, love me sweet; never let me go", "/LAHV MI TEHNDER, LAHV MI SWIT; NEHVER LEHT MI GOUH"},
        {"hello world - - oh", "/HAXLOUH WERLD - - OUH"},
        {"don't stop believing", "/DOUHNT STAAP BIHLIVIHNG"}};
    static const char *const LT[] = {"W1", "W7", "W16", "S1", "S5"};
    static const uint8_t LV[] = {1, 7, 16, LYR_NW + 1u, LYR_NW + 5u};
    static int32_t a[FS * 40], b[FS * 40];
    uint32_t k, j, bad = 0, toks = 0, p = 0, s0, s1, mel;
    char got[64] = "";
    lyr_set(0, "/HAX.LOUH WERLD - LAHV#");
    while (sy_tok(lyr_txt[0], p, &s0, &s1, &mel)) {
        size_t gl = strlen(got);
        snprintf(got + gl, sizeof got - gl, "%s%.*s", toks ? "|" : "", (int)(s1 - s0), lyr_txt[0] + s0);
        toks++;
        p = s1;
    }
    check("V10.7.33 a phoneme lyric's words: \"/\" first, \".\" and \"#\" in them (HAX.LOUH|WERLD|-|LAHV#)",
          toks == 4u && !strcmp(got, "HAX.LOUH|WERLD|-|LAHV#"));
    for (k = 0; k < NELEM(TX); k++) {
        uint32_t na = render_en(0, TX[k][0], a, NELEM(a)), nb = render_en(0, TX[k][1], b, NELEM(b)), za = 0, zb = 0;
        for (j = 0; j < na; j++) za += a[j] != 0;
        for (j = 0; j < nb; j++) zb += b[j] != 0;
        if (za < na / 4u || zb < nb / 4u || (na == nb && !memcmp(a, b, na * sizeof a[0]))) {
            printf("voice:   EN \"%s\": letters %u of %u sounding, phonemes %u of %u\n", TX[k][0], za, na, zb, nb);
            bad++;
        }
    }
    check("V10.7.33 English as letters (a project before: the rules) and as the editor's phonemes: both sing, otherwise", !bad);
    for (k = 0; k < sizeof LV; k++) {
        uint32_t na = render_en(LV[k], "", a, NELEM(a)), za = 0;
        for (j = 0; j < na; j++) za += a[j] != 0;
        if (za < na / 4u) {
            printf("voice:   EN %s: %u of %u samples sounding\n", LT[k], za, na);
            bad++;
        }
    }
    check("V10.7.33 VOICE EN's generator (W1 W7 W16 S1 S5): its phoneme-coded words sing", !bad);
}

/* ------------------------------------------------------------------------------------------ the checks --- */
#define NFFT 4096
/* the power spectrum of y[0..n) (Welch: Hann windows of NFFT, half overlapping), into P[NFFT / 2]; shorter than a
 * window: that one span Hann-windowed, zero-padded to NFFT */
static void spectrum(const double *y, uint32_t n, double *P)
{
    static double re[NFFT], im[NFFT];
    uint32_t k;
    if (jt_hann_power(y, n, P, NFFT, re, im) || !n)
        return;
    for (k = 0; k < NFFT; k++) {
        re[k] = k < n ? y[k] * jt_hann(k, n) : 0;
        im[k] = 0;
    }
    jt_fft(re, im, NFFT, 0);
    for (k = 0; k < NFFT / 2u; k++)
        P[k] = re[k] * re[k] + im[k] * im[k];
}
static double bin_hz(uint32_t k) { return (double)k * FS / NFFT; }
/* the strongest frequency of P (smoothed over +-sm bins) in lo .. hi Hz */
static double peak_in(const double *P, double lo, double hi, uint32_t sm)
{
    uint32_t k, best = 0;
    double bv = -1;
    for (k = (uint32_t)(lo * NFFT / FS); k <= (uint32_t)(hi * NFFT / FS) && k + sm < NFFT / 2u; k++) {
        double s = 0;
        uint32_t j;
        for (j = k - sm; j <= k + sm; j++)
            s += P[j];
        if (s > bv) { bv = s; best = k; }
    }
    return bin_hz(best);
}
static double band(const double *P, double lo, double hi)
{
    uint32_t k;
    double s = 0;
    for (k = (uint32_t)(lo * NFFT / FS); k < (uint32_t)(hi * NFFT / FS) && k < NFFT / 2u; k++)
        s += P[k];
    return s;
}
static double centroid(const double *P)
{
    uint32_t k;
    double s = 0, w = 0;
    for (k = 1; k < NFFT / 2u; k++) {
        s += P[k] * bin_hz(k);
        w += P[k];
    }
    return w > 0 ? s / w : 0;
}
#define rms jt_rms_d

/* track 0 VOICE, preset pi (LANG as it has it, or lang), the text (TEXT); ATK 0, SUS full, a dry voice */
static void vsetup(int32_t pi, uint32_t lang, const char *text)
{
    memset(trk, 0, sizeof trk);
    memset(sing, 0, sizeof sing);
    memset(sing_io, 0, sizeof sing_io);
    formant_nz = 0x2545F491;                             /* (the breath's noise: from its start, a render repeats) */
    host_tracks_init();
    host_preset(&trk[0], eng_voice(), (uint32_t)pi);
    if (lang)
        trk[0].p[P_VLANG] = (int16_t)lang;
    lyr_set(0, text ? text : "");
    {
        static const struct { uint16_t id; int16_t v; } DRY[] = {{P_VLYR, 0}, {P_ATK, 0}, {P_SUS, 127},
                                                                   {P_CHOR, 0}, {P_DLY, 0}, {P_REV, 0}};
        uint32_t k;
        for (k = 0; k < NELEM(DRY); k++)
            trk[0].p[DRY[k].id] = DRY[k].v;
    }
}
/* (voice2) a note as the sequencer starts one, its lead given (sing.c sing_t.lead: its consonants as long as they are
 * written, its vowel on the step): the consonants measured whole; a key's (no lead) sung in ~10 ms */
static void note_led(uint32_t note)
{
    sing[0].lead = 0xC0u | 63u;
    trk_note_on(&trk[0], note, 100);
}
/* n samples of the track's voices (whole blocks rendered) into y (full scale 1.0; NULL: rendered, not kept) */
static void vrender(double *y, uint32_t n)
{
    static int32_t blk[CTL];
    uint32_t done = 0;
    while (done < n) {
        uint32_t k;
        track_render(&trk[0], blk, CTL);
        for (k = 0; k < CTL && done < n; k++, done++)
            if (y != NULL)
                y[done] = blk[k] / 32768.0;
    }
}

static void check_reading(void)
{
    static const char *const ES[][2] = {
        {"corazon", "|k o |r a |s o n"}, {"guerra", "|g e |rr a"}, {"queso", "|k e |s o"}, {"cielo", "|s y e |l o"},
        {"ciudad", "|s y u |d a d"}, {"playa", "|p l a |y a"}, {"estrella", "|e s |t r e |y a"}, {"honra", "|o n |rr a"},
        {"israel", "|i s |rr a |e l"}, {"muy", "|m w i"}, {"hoy", "|o i"}, {"agua", "|a |g w a"}, {"gente", "|x e n |t e"},
        {"jardin", "|x a r |d i n"}, {"extranyo", "|e k s |t r a |ny o"}, {"perro", "|p e |rr o"}, {"pero", "|p e |r o"},
        {"caer", "|k a |e r"}, {"leon", "|l e |o n"}, {"cuidado", "|k w i |d a |d o"}, {"hielo", "|y e |l o"},
        {"chocolate", "|ch o |k o |l a |t e"}, {"vaca", "|b a |k a"}, {"zapato", "|s a |p a |t o"}, {"llave", "|y a |b e"},
        {"atlas", "|a |t l a s"}, {"obstaculo", "|o b s |t a |k u |l o"}, {"aire", "|a i |r e"}, {"auto", "|a u |t o"},
        {"y", "|i"}, {"rey", "|rr e i"}, {"ninyo", "|n i |ny o"}, {"Quiero", "|k y e |r o"}, {"guitarra", "|g i |t a |rr a"},
        {"pinguino", "|p i n |g i |n o"}, {"xilofono", "|k s i |l o |f o |n o"}, {"mi_amor", "|m y a |m o r"},
        {"alrededor", "|a l |rr e |d e |d o r"}, {"pajaro", "|p a |x a |r o"}, {"cantar", "|k a n |t a r"},
    };
    static const char *const JA[][2] = {
        {"konnichiwa", "|k o |N |n i |ch i |w a"}, {"arigatou", "|a |r i |g a |t o |o"}, {"kitto", "|k i |_ t o"},
        {"shinjiru", "|sh i |N |j i |r u"}, {"tsunami", "|ts u |n a |m i"}, {"kyou", "|k y o |o"}, {"fuji", "|f u |j i"},
        {"wo", "|o"}, {"hito", "|h i |t o"}, {"sayonara", "|s a |y o |n a |r a"}, {"si", "|sh i"}, {"ti", "|ch i"},
        {"tu", "|ts u"}, {"hu", "|f u"}, {"chotto", "|ch o |_ t o"}, {"matcha", "|m a |_ ch a"}, {"kanji", "|k a |N |j i"},
        {"onna", "|o |N |n a"}, {"desu", "|d e |s u"}, {"ryokou", "|r y o |k o |o"}, {"Sakura", "|s a |k u |r a"},
    };
    static const char *const EN[][2] = {
        {"/LAHV", "|l ah v"}, {"/MAAIH", "|m aa ih"}, {"/TAAIHM", "|t aa ih m"}, {"/BYUTAXFAXL", "|b y u |t ax |f ax l"},
        {"/WAOTER", "|w ao |t er"}, {"/STRAONG", "|s t r ao ng"}, {"/DHAX", "|dh ax"}, {"/dOUHnt", "|d o uh n t"},
        {"/BI.IHNG", "|b i |ih ng"}, {"/STRIHNG.GER", "|s t r ih ng |g er"}, {"/IHN.GROUHN", "|ih n |g r o uh n"},
    };
    char b[256], what[160];
    uint32_t k, ok = 1, l, n;
    for (k = 0; k < NELEM(ES); k++) {
        g2p_str(LG_ES, ES[k][0], b, sizeof b);
        if (strcmp(b, ES[k][1])) { printf("voice:   ES %-12s %s (want %s)\n", ES[k][0], b, ES[k][1]); ok = 0; }
    }
    snprintf(what, sizeof what, "Spanish by rules: %u words, their phonemes and syllables (seseo, ll y, rr, diphthongs, clusters)",
             (unsigned)NELEM(ES));
    check(what, ok);
    for (k = 0, ok = 1; k < NELEM(JA); k++) {
        g2p_str(LG_JA, JA[k][0], b, sizeof b);
        if (strcmp(b, JA[k][1])) { printf("voice:   JA %-12s %s (want %s)\n", JA[k][0], b, JA[k][1]); ok = 0; }
    }
    snprintf(what, sizeof what, "Japanese romaji -> morae: %u words (moraic n, doubled consonants, shi chi tsu fu, kunrei, ou)",
             (unsigned)NELEM(JA));
    check(what, ok);
    for (k = 0, ok = 1; k < NELEM(EN); k++) {
        g2p_str(LG_EN, EN[k][0], b, sizeof b);
        if (strcmp(b, EN[k][1])) { printf("voice:   EN %-12s %s (want %s)\n", EN[k][0], b, EN[k][1]); ok = 0; }
    }
    check("V10.7.33 English as the editor's phoneme codes (the CMU dictionary's, syllabified): love my time beautiful water "
          "strong the don't being; \".\" where a syllable starts", ok);
    {   /* (V10.7.33) the English dictionary off the device: each of its words as the editor sends it (its code text,
         * tools/gen_lyrics.py ph_text: no "." where sy_phauto finds the syllables) read back exactly (phonemes,
         * syllables), as English; the same text in upper and lower case; without the Bluetooth segment too (no
         * table of English's there now) */
        uint32_t nw, n, ns, i, bad = 0, dots = 0, low = 0, rules = 0;
        uint8_t ph[SW_MAX], ph2[SW_MAX];
        char w[40];
        for (nw = 0; nw < LY_EN_N; nw++) {
            const char *rc = LY_EN_REF_CODE[nw], *rp = LY_EN_REF_PH[nw];
            uint32_t wl;
            char nw2[40];
            snprintf(w, sizeof w, "/%s", rc);           /* (as the editor sends it: upper case) */
            dots += strchr(rc, '.') != 0;
            n = sy_word(LG_EN, nw2, sy_norm_lang(LG_EN, w, (uint32_t)strlen(w), nw2, sizeof nw2), ph, &ns);
            if (n != strlen(rp)) bad++;
            else
                for (i = 0; i < n; i++) bad += ph[i] != ((uint8_t)rp[i] & 0x7Fu);
            for (i = 0; w[i]; i++) w[i] = (char)tolower(w[i]);
            ly_en_ok = 0;
            wl = sy_norm_lang(LG_EN, w, (uint32_t)strlen(w), nw2, sizeof nw2);
            low += sy_word(LG_EN, nw2, wl, ph2, &ns) != n || memcmp(ph, ph2, n);
            ly_en_ok = 1;
            n = sy_word(LG_EN, LY_EN_REF_W[nw], (uint32_t)strlen(LY_EN_REF_W[nw]), ph, &ns);
            {                                            /* (as letters: the letter rules, exactly) */
                uint8_t tc[SW_MAX], tk[SW_MAX];
                uint32_t ns3, m3 = sy_en_rules(LY_EN_REF_W[nw], (uint32_t)strlen(LY_EN_REF_W[nw]), tc, tk);
                m3 = sy_syl(tc, tk, m3, ph2, &ns3);
                rules += n == m3 && ns == ns3 && !memcmp(ph, ph2, n);
            }
        }
        g2p_str(LG_EN, "zorblat", b, sizeof b);
        printf("voice:   %u English words as the editor's codes (%u with \".\"s): %u differ, %u otherwise in lower case "
               "or without the segment; as letters %u sung by the rules (zorblat %s)\n", (unsigned)LY_EN_N, dots, bad,
               low, rules, b);
        snprintf(what, sizeof what, "V10.7.33 English: all %u dictionary words as phoneme codes read back exactly; as letters "
                 "by the rules (a project before)", (unsigned)LY_EN_N);
        check(what, !bad && !low && rules == LY_EN_N && dots < LY_EN_N / 10u && !strcmp(b, "|z aa r |b l ae t"));
    }
    {   /* (V10.7.33) the English letter rules as a table: every dictionary word, every bank word's letters and 200000
         * random words (a .. z, y and e heavy, 1 .. 23 letters) read exactly as the code before it */
        uint8_t ta[SW_MAX], ka[SW_MAX], tb[SW_MAX], kb[SW_MAX];
        uint32_t nw, diff = 0, x = 12345u, k2;
        char w[24];
        for (nw = 0; nw < LY_EN_N + 200000u; nw++) {
            uint32_t wl, na, nb;
            if (nw < LY_EN_N) {
                snprintf(w, sizeof w, "%s", LY_EN_REF_W[nw]);
            } else {
                static const char AL[] = "abcdefghijklmnopqrstuvwxyzaeiouyeghhlnrsw";
                x ^= x << 13; x ^= x >> 17; x ^= x << 5;
                wl = 1u + x % 23u;
                for (k2 = 0; k2 < wl; k2++) { x ^= x << 13; x ^= x >> 17; x ^= x << 5; w[k2] = AL[x % (sizeof AL - 1u)]; }
                w[wl] = 0;
            }
            wl = (uint32_t)strlen(w);
            na = sy_en_rules(w, wl, ta, ka);
            nb = en_rules_ref(w, wl, tb, kb);
            if (na != nb || memcmp(ta, tb, na) || memcmp(ka, kb, na)) {
                if (diff++ < 5u) printf("voice:   EN rules: %s reads otherwise\n", w);
            }
            na = sy_es_tok(w, wl, ta, ka);
            nb = es_tok_ref(w, wl, tb, kb);
            if (na != nb || memcmp(ta, tb, na) || memcmp(ka, kb, na)) {
                if (diff++ < 5u) printf("voice:   ES rules: %s reads otherwise\n", w);
            }
        }
        check("V10.7.33 English and Spanish letter rules as tables: 2095 dictionary words and 200000 random ones as the code before", !diff);
    }
    {   /* (V10.7.33) the codes of every language: each phoneme of a language's list (the NAME screen's, ph_pick) read
         * back as itself from its code, alone and between two vowels */
        uint32_t lg, k, bad = 0, cnt[5] = {0};
        static const uint8_t LGS[5] = {LG_ES, LG_EN, LG_JA, LG_PT, LG_RU};
        for (lg = 0; lg < 5u; lg++)
            for (k = 0; k < 48u; k++) {
                uint8_t ph[SW_MAX], ph1[SW_MAX];
                char c[3], w[12];
                uint32_t ns, n, n1, p, want;
                if (!ph_pick(LGS[lg], k, c)) break;
                for (p = 0, n = k; p < PH_COUNT; p++)  /* (the k-th of the language's codes) */
                    if ((PH_CM[p] & ph_lmask(LGS[lg])) && !n--) break;
                want = p == PH_U && LGS[lg] == LG_JA ? PH_UJ : p == PH_RT && LGS[lg] == LG_EN ? PH_RA : p;
                snprintf(w, sizeof w, "/%s", c);
                w[1] = (char)tolower(w[1]); w[2] = (char)tolower(w[2]);
                n1 = sy_word(LGS[lg], w, (uint32_t)strlen(w), ph1, &ns);
                snprintf(w, sizeof w, "/o.%c%c.o", tolower(c[0]), c[1] ? tolower(c[1]) : '.');
                n = sy_word(LGS[lg], w, (uint32_t)strlen(w), ph, &ns);
                if (n1 != 1u || (ph1[0] & 0x3Fu) != want || n != 3u || (ph[1] & 0x3Fu) != want) {
                    printf("voice:   lang %u code %s: %u, %u phonemes\n", LGS[lg], c, n1, n);
                    bad++;
                }
                cnt[lg]++;
            }
        snprintf(what, sizeof what, "V10.7.33 the languages' phoneme lists (ES %u EN %u JA %u PT %u others %u): each read back "
                 "from its code", cnt[0], cnt[1], cnt[2], cnt[3], cnt[4]);
        check(what, !bad && cnt[1] >= 37u && cnt[0] >= 25u && cnt[2] >= 25u && cnt[4] == 44u);
    }
    {   /* the banks: every word's syllables as gen_lyrics.py counted them (the generator fits phrases by them) */
        static const char *const LN[4] = {"", "ES", "EN", "JA"};
        uint32_t bad_w = 0, tot = 0;
        for (l = LG_ES; l <= LG_JA; l++) {
            const char *const *ref=l==LG_ES?LY_ES_BANK_REF_W:l==LG_EN?LY_EN_BANK_REF_W:LY_JA_BANK_REF_W;
            const ly_bank_t *bk = &LY_BANK[l - 1u];
            uint32_t nwds = l == LG_ES ? LY_ES_NW : l == LG_EN ? LY_EN_NW : LY_JA_NW;
            for (k = 0; k < nwds; k++) {
                uint8_t ph[SW_MAX];
                char nw2[24];
                char word[20];
                const char *w = ly_word(bk, k, word);
                if (l == LG_EN) {                        /* (V10.7.33: its phoneme codes, as the dictionary's) */
                    uint32_t lo = 0, hi = LY_EN_N, i;
                    char rc[24], ww[24];
                    while (lo + 1u < hi) { uint32_t mid = (lo + hi) / 2u; if (strcmp(LY_EN_REF_W[mid], ref[k]) <= 0) lo = mid; else hi = mid; }
                    for (i = 0; LY_EN_REF_CODE[lo][i] && i + 1u < sizeof rc; i++) rc[i] = (char)tolower(LY_EN_REF_CODE[lo][i]);
                    rc[i] = 0;
                    bad_w += strcmp(LY_EN_REF_W[lo], ref[k]) != 0 || strcmp(w, rc) != 0;
                    snprintf(ww, sizeof ww, "/%s", w);
                    (void)sy_word(l, nw2, sy_norm_lang(l, ww, (uint32_t)strlen(ww), nw2, sizeof nw2), ph, &n);
                } else {
                    bad_w += strcmp(w,ref[k])!=0; /* exact text, not only the syllable count */
                    (void)sy_word(l, nw2, sy_norm(w, (uint32_t)strlen(w), nw2, sizeof nw2), ph, &n);
                }
                tot++;
                if (n != (ly_meta(bk,k) & 7u)) {
                    if (bad_w++ < 8u)
                        printf("voice:   %s %s: %u syllables here, %u in the bank\n", LN[l], w, n, ly_meta(bk,k) & 7u);
                }
            }
        }
        snprintf(what, sizeof what, "the banks: all %u words' syllables as the generator counts them (ES %u EN %u JA %u words)",
                 tot, (unsigned)LY_ES_NW, (unsigned)LY_EN_NW, (unsigned)LY_JA_NW);
        check(what, !bad_w);
    }
}

/* the generator's text for a track: nph phrases of W seed / S seed in lang, theme; into out (words, " / " a phrase) */
static uint32_t gen_text(uint32_t lang, uint32_t lyr, uint32_t theme, uint32_t nph, uint32_t pl, char *out, uint32_t max,
                         uint32_t *syl)
{
    static sing_t g;
    uint32_t m = 0, ph, k, all = 1;
    memset(&g, 0, sizeof g);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eng_voice(), 7);
    trk[0].p[P_E1] = (int16_t)(theme & 0x100u ? theme & 127u : theme * 16u + 8u);   /* (0x100: THEME's value) */
    trk[0].p[P_SLEN] = 16;
    for (k = 0; k < 16u; k++) {                          /* pl NOTE steps */
        trk[0].step[k].time = k < pl ? ST_NOTE : ST_REST;
        trk[0].step[k].n = k < pl;
        trk[0].step[k].note[0] = 60;
    }
    out[0] = 0;
    for (ph = 0; ph < nph; ph++) {
        uint32_t s = 0;
        do {
            uint8_t b[SW_MAX];
            char nw[24];
            uint32_t ns, i;
            sy_gen_word(&trk[0], &g, lang, lyr);
            (void)sy_word(lang, nw, sy_norm_lang(lang, g.w, sizeof g.w, nw, sizeof nw), b, &ns);
            s += ns;
            for (i = 0; g.w[i] && m + 2u < max; i++)
                out[m++] = g.w[i];
            if (m + 2u < max)
                out[m++] = ' ';
        } while (g.prem && s < 200u);
        all &= s == pl;
        if (m + 3u < max) { out[m++] = '/'; out[m++] = ' '; }
    }
    out[m] = 0;
    *syl = all;
    return m;
}
static void check_generator(void)
{
    char a[600], b[600], c[600], what[700];
    uint32_t ok, oks, l, s1, s2, s3;
    static const char *const LN[4] = {"", "ES", "EN", "JA"};
    for (l = LG_ES; l <= LG_JA; l++) {
        gen_text(l, 3, 4, 3, 7, a, sizeof a, &s1);
        gen_text(l, 3, 4, 3, 7, b, sizeof b, &s2);
        gen_text(l, 4, 4, 3, 7, c, sizeof c, &s3);
        ok = !strcmp(a, b) && strcmp(a, c) != 0;
        oks = s1 && s2 && s3;
        snprintf(what, sizeof what, "generator %s: W3 the same lyric twice, W4 another; phrases of the pattern's 7 notes", LN[l]);
        check(what, ok && oks);
        printf("voice:   %s W3 ANY: %.160s\n", LN[l], a);
    }
    gen_text(LG_ES, 1, 0, 2, 12, a, sizeof a, &s1);
    gen_text(LG_ES, 1, 7, 2, 12, b, sizeof b, &s2);
    check("generator: another THEME, other words (LOVE against FIRE); 12-note phrases fit", strcmp(a, b) != 0 && s1 && s2);
    printf("voice:   ES W1 LOVE: %.160s\n", a);
    printf("voice:   ES W1 FIRE: %.160s\n", b);
    gen_text(LG_JA, LYR_NW + 2u, 4, 2, 9, a, sizeof a, &s1);
    gen_text(LG_EN, LYR_NW + 1u, 4, 1, 9, b, sizeof b, &s2);
    check("scat S2 JA / S1 EN: syllables of the language, phrases fit", s1 && s2 && a[0] && b[0]);
    printf("voice:   JA S2: %.120s\n", a);
    printf("voice:   EN S1: %.120s\n", b);
    {   /* every word of W a real one: in the bank (or the vocable) */
        uint32_t k, w, bad_w = 0;
        for (l = LG_ES; l <= LG_JA; l++)
            for (k = 1; k <= 4u; k++) {
                char *p, *q;
                gen_text(l, k, 4, 4, 11, a, sizeof a, &s1);
                for (p = a; *p; p = q) {
                    const ly_bank_t *bk = &LY_BANK[l - 1u];
                    uint32_t nwds = l == LG_ES ? LY_ES_NW : l == LG_EN ? LY_EN_NW : LY_JA_NW, found = 0, n;
                    while (*p == ' ' || *p == '/') p++;
                    for (q = p; *q && *q != ' '; q++)
                        ;
                    n = (uint32_t)(q - p);
                    if (!n) break;
                    for (w = 0; w < nwds && !found; w++) {
                        char word[20];
                        const char *t = ly_word(bk, w, word);
                        found = strlen(t) == n && (!strncmp(t, p, n) ||
                                                   (l == LG_ES && !strncmp(t, p, n - 1u) && t[n - 1u] == 'o' && p[n - 1u] == 'a'));
                    }
                    found |= (n == 2u && (!strncmp(p, "la", 2) || !strncmp(p, "ra", 2))) || (n == 3u && !strncmp(p, "ouh", 3));
                    bad_w += !found;
                }
            }
        check("generator: every word of W1..W4 in ES EN JA a bank word (the adjective's -a after a feminine noun)", !bad_w);
    }
}

/* the steady part of a held syllable (text) at note 60 (formants as the table: SHIFT 0, key tracking at C4) */
static void sustain(int32_t pi, uint32_t lang, const char *text, int16_t breath, double *y, uint32_t n)
{
    vsetup(pi, lang, text);
    trk[0].p[P_E3] = 0;
    if (breath >= 0)
        trk[0].p[P_E5] = breath;
    trk_note_on(&trk[0], 60, 100);
    vrender(0, FS / 4);                                  /* past the onset */
    vrender(y, n);
}
static void check_sound(void)
{
    static double y[FS * 2], P[NFFT / 2];
    static const struct { const char *t; double f1lo, f1hi, f2lo, f2hi; } V[] = {
        {"a", 640, 880, 1120, 1480}, {"e", 400, 570, 1650, 2080}, {"i", 240, 380, 2000, 2500}, {"o", 420, 590, 780, 1040},
        {"u", 270, 420, 660, 960},
    };
    char what[200];
    uint32_t k, ok = 1, n = FS;
    int32_t pi = preset_of("CANTOR ES");
    for (k = 0; k < NELEM(V); k++) {                     /* whispered: the noise draws the cascade's response */
        double f1, f2;
        sustain(pi, LG_ES, V[k].t, 127, y, n);
        spectrum(y, n, P);
        f1 = peak_in(P, 200, 950, 4);
        f2 = peak_in(P, f1 + 250, 2800, 4);
        printf("voice:   vowel %s: F1 %4.0f F2 %4.0f Hz\n", V[k].t, f1, f2);
        ok &= f1 >= V[k].f1lo && f1 <= V[k].f1hi && f2 >= V[k].f2lo && f2 <= V[k].f2hi;
    }
    check("vowels a e i o u: F1 / F2 of the sound in their ranges (Spanish, adult male, Quilis / Hillenbrand)", ok);
    {   /* sung (voiced) a: its spectral envelope's F1 near the table too */
        double f1;
        sustain(pi, LG_ES, "a", 0, y, n);
        spectrum(y, n, P);
        f1 = peak_in(P, 400, 1000, 1);
        ok = f1 > 600 && f1 < 900;
        snprintf(what, sizeof what, "voiced a at C4: the strongest harmonic near F1 (%.0f Hz)", f1);
        check(what, ok);
    }
    {   /* fricatives: s high, sh lower, f weak and broad */
        double cs, ps, csh, psh, rs, rsh, rf, rv;
        vsetup(pi, LG_ES, "sa");
        note_led(60);
        vrender(y, FS / 2);
        spectrum(y + (uint32_t)(0.015 * FS), (uint32_t)(0.05 * FS), P);
        cs = centroid(P);
        ps = peak_in(P, 1500, 9000, 6);
        rs = rms(y, (uint32_t)(0.015 * FS), (uint32_t)(0.065 * FS));
        rv = rms(y, (uint32_t)(0.2 * FS), (uint32_t)(0.4 * FS));
        vsetup(preset_of("UTA JA"), LG_JA, "sha");
        trk[0].p[P_E3] = 0;
        note_led(60);
        vrender(y, FS / 2);
        spectrum(y + (uint32_t)(0.015 * FS), (uint32_t)(0.06 * FS), P);
        csh = centroid(P);
        psh = peak_in(P, 1500, 9000, 6);
        rsh = rms(y, (uint32_t)(0.015 * FS), (uint32_t)(0.07 * FS));
        vsetup(pi, LG_ES, "fa");
        note_led(60);
        vrender(y, FS / 2);
        rf = rms(y, (uint32_t)(0.015 * FS), (uint32_t)(0.065 * FS));
        printf("voice:   s: centroid %.0f Hz, peak %.0f Hz, %.1f dB of the vowel; sh: centroid %.0f, peak %.0f, %.1f dB; f %.1f dB\n",
               cs, ps, 20 * log10(rs / rv), csh, psh, 20 * log10(rsh / rv), 20 * log10(rf / rv));
        check("fricatives: s noise 4.5 .. 8 kHz, sh 2.2 .. 3.6 kHz, both 8 .. 24 dB under the vowel, f weaker than s",
              ps > 4500 && ps < 8000 && cs > 4000 && psh > 2200 && psh < 3600 && csh < cs && rs < rv / 2.5 && rs > rv / 16 &&
              rsh < rv / 2.5 && rsh > rv / 16 && rf < rs);
    }
    {   /* plosives: a closure (silence), a burst, the vowel */
        uint32_t i, cl = 0, bu = 0, frame = FS / 1000u;
        double rv, rmax = 0;
        vsetup(pi, LG_ES, "ta");
        note_led(60);
        vrender(y, FS / 2);
        rv = rms(y, (uint32_t)(0.2 * FS), (uint32_t)(0.4 * FS));
        for (i = 0; i + frame < (uint32_t)(0.12 * FS); i += frame) {
            double r = rms(y, i, i + frame);
            if (!bu && r < rv / 20)
                cl++;                                    /* the closure: ms near silence */
            if (cl >= 25u && !bu && r > rv / 6)
                bu = i / frame;                          /* the burst after it */
            if (r > rmax) rmax = r;
        }
        snprintf(what, sizeof what, "plosive t: a closure (%u ms under -26 dB), then the burst at %u ms, then the vowel", cl, bu);
        check(what, cl >= 25u && cl <= 70u && bu >= 25u && bu <= 80u);
    }
    {   /* no click: legato syllables (vowels, nasals, liquids), the sample-to-sample jump never far past the steady one */
        static const char *const T[] = {"a e i o u a", "mama nana lala", "ya wa ya wa"};
        uint32_t t, i, worst_ok = 1;
        for (t = 0; t < NELEM(T); t++) {
            double dmax = 0, dsteady = 0;
            vsetup(preset_of("CANTOR ES"), LG_ES, T[t]);
            trk[0].p[P_VOICE] = V_LEGATO;
            for (k = 0; k < 6u; k++) {
                static double z[FS / 4];
                trk_note_on(&trk[0], 60 + (k & 1u) * 2u, 100);
                if (k) trk_note_off(&trk[0], 60 + ((k - 1u) & 1u) * 2u);
                vrender(z, FS / 4);
                for (i = 2; i < FS / 4; i++) {
                    double d2 = fabs(z[i] - 2 * z[i - 1] + z[i - 2]);
                    if (i < FS / 8) { if (d2 > dmax) dmax = d2; }
                    else if (d2 > dsteady) dsteady = d2;
                }
            }
            printf("voice:   \"%s\": the largest second difference across the changes %.4f, steady %.4f\n", T[t], dmax, dsteady);
            worst_ok &= dmax < dsteady * 2.0;
        }
        check("no click at syllable changes (legato vowels, nasals, liquids, glides): the jumps as the steady sound's", worst_ok);
    }
}

/* ------------------------------------------------------------------------------------- (0.9.8) the singer --- */
/* notes nt on track 0 as the sequencer gates them (7/8 of each, or held into the next with leg), the voice sum into y
 * (n at most), each block's syllable count into nsy (if nsy) and the first voice's pitch as sung (1/4096 st) into pq (if
 * pq); returns the samples */
typedef struct { uint8_t note, len, leg; } snt_t;   /* len: 1/8 beats at 100 BPM */
static uint32_t sing_notes(const snt_t *nt, uint32_t nn, double *y, uint32_t n, uint16_t *nsy, int32_t *pq)
{
    static int32_t b[CTL];
    uint32_t k, i, f = 0, eighth = FS * 60u / 100u / 2u, prev = 0;
    for (k = 0; k <= nn && f + CTL <= n; k++) {
        uint32_t len = k < nn ? nt[k].len * eighth : FS / 2u, note = k < nn ? nt[k].note : 0, leg = k < nn && nt[k].leg;
        if (note)
            trk_note_on(&trk[0], note, 100);
        if (prev)
            trk_note_off(&trk[0], prev);
        prev = 0;
        for (i = 0; i < len && f + CTL <= n; i += CTL, f += CTL) {
            uint32_t j;
            if (note && !leg && i + CTL >= len - len / 8u && i < len - len / 8u)
                trk_note_off(&trk[0], note);
            track_render(&trk[0], b, CTL);
            for (j = 0; j < CTL; j++)
                y[f + j] = b[j] / 32768.0;
            if (nsy)
                nsy[f / CTL] = sing[0].nsy;
            if (pq)
                pq[f / CTL] = trk[0].v[0].active ? glide_sung(&trk[0].v[0]) : 0;
        }
        if (leg)
            prev = note;
    }
    return f;
}
static int cmp_dbl(const void *a, const void *b)
{
    double x = *(const double *)a, z = *(const double *)b;
    return x < z ? -1 : x > z;
}
/* F0 (Hz) of y[a .. a + 40 ms) by autocorrelation in 150 .. 400 Hz (0: unvoiced) */
static double f0_at(const double *y, uint32_t a)
{
    uint32_t w = FS / 25u, lag, best = 0;
    double bv = 0, e = 0, r0, r1, r2;
    for (lag = 0; lag < w; lag++)
        e += y[a + lag] * y[a + lag];
    for (lag = FS / 400u; lag <= FS / 150u; lag++) {
        double s = 0;
        uint32_t i;
        for (i = 0; i < w; i++)
            s += y[a + i] * y[a + i + lag];
        if (s > bv) { bv = s; best = lag; }
    }
    if (!best || bv < 0.3 * e)
        return 0;
    {   /* parabolic peak */
        uint32_t i;
        r0 = r1 = r2 = 0;
        for (i = 0; i < w; i++) {
            r0 += y[a + i] * y[a + i + best - 1u];
            r1 += y[a + i] * y[a + i + best];
            r2 += y[a + i] * y[a + i + best + 1u];
        }
        return FS / (best + 0.5 * (r0 - r2) / (r0 - 2 * r1 + r2));
    }
}
static void check_life(void);
static void check_singer(void)
{
    static double y[FS * 8], P[NFFT / 2], ctx[FS / 10];
    static uint16_t nsy[FS * 8 / CTL];
    static int32_t pq[FS * 8 / CTL];
    char what[240];
    int32_t pi = preset_of("CANTOR ES");
    uint32_t i, k, n;
    {   /* s: its noise comes in over >= 6 ms (10 .. 90 % of its 1 ms RMS; 0.9.7: ~2 ms, a third of it a ms), its level
         * 14 .. 24 dB under the vowel (0.9.7: 14.6), its band ~6 kHz (0.9.7: 7.1) with little past 12 kHz (band-passed) */
        double env[60], top, rv, rs, cs, hi, mid, step = 0;
        uint32_t t10 = 0, t90 = 0;
        vsetup(pi, LG_ES, "sa");
        note_led(60);
        vrender(y, FS / 2);
        for (k = 0; k < 60u; k++)
            env[k] = rms(y, k * FS / 1000u, (k + 1u) * FS / 1000u);
        for (k = 20, top = 0; k < 60u; k++)
            top += env[k] / 40.0;
        for (k = 0; k < 60u && env[k] < 0.1 * top; k++)
            ;
        t10 = k;
        for (; k < 60u && env[k] < 0.9 * top; k++)
            ;
        t90 = k;
        for (k = 0; k < 30u; k++) {                      /* the steepest rise a ms, of the 3 ms average */
            double a0 = k >= 3u ? (env[k - 3u] + env[k - 2u] + env[k - 1u]) / 3.0 : 0, a1 = (env[k] + (k ? env[k - 1u] : 0) +
                                                                                           (k > 1u ? env[k - 2u] : 0)) / 3.0;
            if ((a1 - a0) / 3.0 / top > step)
                step = (a1 - a0) / 3.0 / top;
        }
        rv = rms(y, (uint32_t)(0.2 * FS), (uint32_t)(0.4 * FS));
        rs = rms(y, (uint32_t)(0.015 * FS), (uint32_t)(0.065 * FS));
        spectrum(y + (uint32_t)(0.015 * FS), (uint32_t)(0.05 * FS), P);
        cs = centroid(P);
        mid = band(P, 4000, 8000);
        hi = band(P, 12000, 20000);
        printf("voice:   s: rise %u ms (10 .. 90 %%), at most %.0f %% of its level a ms, %.1f dB of the vowel, centroid %.0f Hz, "
               "12 .. 20 kHz %.1f dB under 4 .. 8 kHz\n", t90 - t10, step * 100, 20 * log10(rs / rv), cs, 10 * log10(mid / hi));
        snprintf(what, sizeof what, "soft s: in over %u ms (>= 6), at most %.0f %% of it a ms (<= 15), %.1f dB under the vowel "
                 "(14 .. 24), ~6 kHz, no fizz", t90 - t10, step * 100, -20 * log10(rs / rv));
        check(what, t90 - t10 >= 6u && step <= 0.15 && rs < rv / 5.0 && rs > rv / 16.0 && cs > 5000 && cs < 7500 &&
                    mid > hi * 20.0);
    }
    {   /* sh: lower, 2 .. 4 kHz, no harsher onset than s */
        double cs, lo, hi;
        vsetup(preset_of("UTA JA"), LG_JA, "sha");
        trk[0].p[P_E3] = 0;
        note_led(60);
        vrender(y, FS / 2);
        spectrum(y + (uint32_t)(0.02 * FS), (uint32_t)(0.05 * FS), P);
        cs = centroid(P);
        lo = band(P, 2000, 4000);
        hi = band(P, 5000, 10000);
        snprintf(what, sizeof what, "soft sh: centroid %.0f Hz (2.5 .. 4 k), 2 .. 4 kHz %.1f dB over 5 .. 10 kHz", cs,
                 10 * log10(lo / hi));
        check(what, cs > 2500 && cs < 4000 && lo > hi * 3.0);
    }
    {   /* joins: "sus suenyos son sonidos suaves", a note a syllable, gated as the sequencer does (7/8): at each syllable
         * change the largest second difference within 3 ms against the 99th percentile of the 20 ms around (outside it)
         * (0.9.7: up to 2.6); between two syllables (the gap, the consonant) the level never under -25 dB of the vowels
         * (0.9.7: -48 dB, cut off by the gate) */
        static const snt_t NT[] = {{62, 2, 0}, {64, 2, 0}, {65, 3, 0}, {67, 3, 0}, {65, 2, 0}, {64, 2, 0}, {62, 4, 0},
                                   {64, 2, 0}, {62, 6, 0}};
        static const snt_t NL[] = {{60, 2, 0}, {62, 2, 0}, {64, 2, 0}, {65, 2, 0}, {67, 4, 0}};
        double worst = 0, dip = 0;
        uint32_t nj = 0;
        vsetup(pi, LG_ES, "sus suenyos son sonidos suaves");
        trk[0].p[P_ATK] = ENG_FORMANT.presets[pi].env[0];   /* (the preset's envelope, as played) */
        trk[0].p[P_SUS] = ENG_FORMANT.presets[pi].env[2];
        n = sing_notes(NT, NELEM(NT), y, FS * 8u, nsy, 0);
        for (k = 1; k < n / CTL; k++) {
            uint32_t c = k * CTL, w3 = FS * 3u / 1000u, w20 = FS / 50u, m = 0;
            double near = 0, p99;
            if (nsy[k] == nsy[k - 1u] || c < w20 + 2u || c + w20 + 2u >= n)
                continue;
            for (i = c - w20; i < c + w20; i++) {
                double d2 = fabs(y[i] - 2 * y[i - 1u] + y[i - 2u]);
                if (i + w3 >= c && i < c + w3) { if (d2 > near) near = d2; }
                else ctx[m++] = d2;
            }
            qsort(ctx, m, sizeof ctx[0], cmp_dbl);
            p99 = ctx[m * 99u / 100u];
            if (near / p99 > worst)
                worst = near / p99;
            nj++;
        }
        vsetup(pi, LG_ES, "la le li lo lu");                /* a syllable's vowel into the next note's l */
        trk[0].p[P_ATK] = ENG_FORMANT.presets[pi].env[0];
        trk[0].p[P_SUS] = ENG_FORMANT.presets[pi].env[2];
        n = sing_notes(NL, NELEM(NL), y, FS * 6u, nsy, 0);
        {
            double rv = rms(y, (uint32_t)(0.3 * FS), (uint32_t)(0.5 * FS)), lo = 1e9;
            for (i = (uint32_t)(0.45 * FS); i + FS / 200u < (uint32_t)(2.4 * FS); i += FS / 400u) {
                double r = rms(y, i, i + FS / 200u);
                if (r < lo) lo = r;
            }
            dip = 20 * log10(lo / rv);
        }
        snprintf(what, sizeof what, "syllable joins: no click (%u joins of sus suenyos son sonidos suaves, the worst %.2f x the "
                 "sound around, <= 1.6), la le li lo lu at most %.1f dB down between notes (>= -28)", nj, worst, dip);
        check(what, nj >= 8u && worst <= 1.6 && dip >= -28.0);   /* (voice3: the l a liquid's dip, ~-5 dB more) */
    }
    {   /* the glide as sung: LEGATO, GLIDE 36 TIME (1 + 36 * 36 / 8 = 163 blocks), A3 -> E4 held over, "ah -" (one vowel):
         * the pitch the render plays (glide_sung) block by block: monotonic, there in 163 +- 2 blocks, an S-curve (at a
         * quarter of its time <= 20 % of the way, linear 25; at three quarters >= 80 %), no step past 1/8 st a block; the
         * formants: the vowel's targets (sing_t f) as they were, F1 as sounding (key tracking) moving smoothly (< 0.4 % a
         * block), no jump; with GLIDE 0 a legato word glides a little (~80 blocks) */
        static const snt_t GM[] = {{57, 4, 1}, {64, 4, 0}};
        uint32_t a = 0, b = 0, mono = 1, bigstep = 0, okf = 1;
        int32_t f1a = 0, f1b = 0;
        double q25, q75, jmax = 0;
        vsetup(preset_of("CANTORA ES"), LG_ES, "ah -");
        trk[0].p[P_GLIDE] = 36;
        trk[0].p[P_GLMODE] = 1;
        trk[0].p[P_E3] = 0;
        n = sing_notes(GM, NELEM(GM), y, FS * 3u, nsy, pq);
        for (k = 1; k < n / CTL && !a; k++)
            if (pq[k] != pq[k - 1u] && pq[k - 1u])
                a = k - 1u;                              /* the last block on A3 */
        for (k = a; k < n / CTL && pq[k] != 64 * 16 * 256; k++)
            ;
        b = k;                                           /* the first on E4 */
        for (k = a + 1u; k <= b; k++) {
            mono &= pq[k] >= pq[k - 1u];
            bigstep |= pq[k] - pq[k - 1u] > 2 * 256 * (int32_t)CTL / 32;   /* (1/8 st a block of 32) */
        }
        q25 = (pq[a + (b - a) / 4u] - pq[a]) / (double)(pq[b] - pq[a]);
        q75 = (pq[a + 3u * (b - a) / 4u] - pq[a]) / (double)(pq[b] - pq[a]);
        for (k = a; k <= b + 4u; k++) {                  /* F1 as it sounds, per block (eng_formant.c: f x the ratio) */
            const sing_t *s = &sing[0];
            int32_t p16 = pq[k] >> 8, r = (int32_t)fmt_ratio(trk[0].p[P_E3] * 16 + ((FMT_FOLOW * (p16 - 60 * 16) * 516) >> 16));
            int32_t f1 = (int32_t)(((int64_t)s->f[0] * r) >> 16);
            (void)f1;
            if (k == a) f1a = r;
            if (k == b) f1b = r;
            if (k > a) {
                int32_t r0 = (int32_t)fmt_ratio(trk[0].p[P_E3] * 16 + ((FMT_FOLOW * ((pq[k - 1u] >> 8) - 60 * 16) * 516) >> 16));
                double j = fabs((double)r / r0 - 1.0);
                if (j > jmax) jmax = j;
            }
        }
        {   /* the vowel's targets held over the glide (a melisma: no syllable moves) */
            vsetup(preset_of("CANTORA ES"), LG_ES, "ah -");
            trk[0].p[P_GLIDE] = 36;
            trk[0].p[P_GLMODE] = 1;
            trk_note_on(&trk[0], 57, 100);
            vrender(0, FS / 2);
            {
                int16_t f0[3] = {sing[0].f[0], sing[0].f[1], sing[0].f[2]};
                trk_note_on(&trk[0], 64, 100);
                trk_note_off(&trk[0], 57);
                for (k = 0; k < 200u * 32u / CTL; k++) {   /* (~145 ms: the glide and after) */
                    vrender(0, CTL);
                    for (i = 0; i < 3u; i++)
                        okf &= abs(sing[0].f[i] - f0[i]) <= 2 + f0[i] / 8;   /* (voice3: a held vowel moves a little) */
                }
            }
        }
        printf("voice:   glide A3 -> E4 (TIME 36): %u blocks, at 1/4 of its time %.1f %% of the way, at 3/4 %.1f %%, F1's key "
               "tracking x%.3f, at most %.2f %% a block\n", b - a, q25 * 100, q75 * 100, (double)f1b / f1a, jmax * 100);
        snprintf(what, sizeof what, "glide as sung: an S-curve (%u blocks of %u, 163 +- 2 of 32; %.0f %% at 1/4 of its time, %.0f %% at 3/4), "
                 "monotonic, smooth; formants continuous across it", b - a, (unsigned)CTL, q25 * 100, q75 * 100);
        check(what, a && b - a >= 161u * 32u / CTL && b - a <= 165u * 32u / CTL && mono && !bigstep && q25 <= 0.20 && q25 >= 0.08 && q75 >= 0.80 &&
                    q75 <= 0.92 && jmax < 0.004 * CTL / 32 && okf && f1b > f1a);
        {   /* GLIDE 0, words, legato: a singer's ~60 ms */
            static const snt_t GW[] = {{60, 4, 1}, {64, 4, 0}};
            vsetup(preset_of("CANTOR ES"), LG_ES, "la la");
            n = sing_notes(GW, NELEM(GW), y, FS * 3u, nsy, pq);
            for (k = 1, a = 0; k < n / CTL && !a; k++)
                if (pq[k] != pq[k - 1u] && pq[k - 1u])
                    a = k - 1u;
            for (k = a; k < n / CTL && pq[k] != 64 * 16 * 256; k++)
                ;
            snprintf(what, sizeof what, "a legato word with GLIDE 0 slides as sung: %u blocks of %u (~80 of 32, ~60 ms), a syllable on it",
                     k - a, (unsigned)CTL);
            check(what, a && k - a >= 76u * 32u / CTL && k - a <= 84u * 32u / CTL && nsy[k] == 2u);
        }
    }
    check_life();
}

/* (voice3) the pitch of y (FS) every 10 ms from a into c[] (cents re C4, 0 unvoiced): n frames */
static uint32_t cents_of(const double *y, uint32_t a, uint32_t n, double *c)
{
    uint32_t k;
    for (k = 0; k < n; k++) {
        double f = f0_at(y, a + k * FS / 100u);
        c[k] = f > 0 ? 1200.0 * log2(f / 261.6256) : 0;
    }
    return n;
}
/* the vibrato's swing in c[a .. b) (10 ms frames): the cents less their 210 ms moving mean (the wander, the tuning gone),
 * its peak-to-peak / 2 (the 90th percentile of |x|) and its rate (zero crossings) */
static double vib_of(const double *c, uint32_t a, uint32_t b, double *rate)
{
    static double x[400], ax[400];
    uint32_t k, j, n = 0, zc = 0;
    for (k = a; k < b && n < 400u; k++, n++) {
        double m = 0;
        for (j = k - 10u; j <= k + 10u; j++) m += c[j];
        x[n] = c[k] - m / 21.0;
        ax[n] = fabs(x[n]);
        zc += n && (x[n] > 0) != (x[n - 1u] > 0);
    }
    qsort(ax, n, sizeof ax[0], cmp_dbl);
    *rate = zc / 2.0 / (n / 100.0);
    return n ? ax[n * 9u / 10u] : 0;
}
/* (voice3) the singer's pitch, after a male voice measured: a held note's vibrato (none in its first 0.15 s, then 10 ..
 * 45 ct at 4.5 .. 6.5 Hz), none on a key let go after 0.3 s, none on the sequencer's short notes (1/8 at 120 BPM),
 * there on its long ones (3 steps of 1/8 at 70 BPM); a note from rest scooping (its first 20 ms up to 80 ct off) and on
 * its note by 0.25 s (within the wander's +-30 ct); a legato note landing (there in 60 .. 200 ms, past it 0 .. 50 ct);
 * ROBOT EN (BUZZ at its top) as played; the same twice */
static void check_life(void)
{
    static double y[FS * 4], c[400];
    char what[240];
    uint32_t k, r;
    double early, late, rate, rr;
    {   /* a key held 2.4 s, CANTOR ES "a" (no breath: a clean F0) */
        vsetup(preset_of("CANTOR ES"), LG_ES, "a");
        trk[0].p[P_E5] = 0;
        trk_note_on(&trk[0], 60, 100);
        vrender(y, (uint32_t)(2.4 * FS));
        cents_of(y, 0, 235, c);
        early = vib_of(c, 15, 35, &rr);
        late = vib_of(c, 80, 225, &rate);
        printf("voice:   CANTOR ES held C4: swing %.1f ct in 0.15 .. 0.35 s, %.1f ct at %.1f Hz from 0.8 s\n", early, late, rate);
        snprintf(what, sizeof what, "vibrato on a held note: none at first (%.1f ct, <= 12: the scoop, the wander), then %.0f ct at %.1f Hz (15 .. 45, 4.5 .. 6.5)",
                 early, late, rate);
        check(what, early < 12.0 && late > 15.0 && late < 45.0 && rate > 4.5 && rate < 6.5);
        {   /* the scoop: a note from rest starts off its pitch, then is on it */
            double o0 = fabs(c[1]), m = 0;
            for (k = 22; k < 28u; k++) m += c[k] / 6.0;
            snprintf(what, sizeof what, "a note from rest: %.0f ct off at its start (<= 80), %+.0f ct at 0.22 .. 0.28 s (+-30)", o0, m);
            check(what, o0 <= 80.0 && fabs(m) <= 30.0);
        }
    }
    {   /* a key let go after 0.3 s: no vibrato in it */
        vsetup(preset_of("CANTOR ES"), LG_ES, "a a");
        trk[0].p[P_E5] = 0;
        trk_note_on(&trk[0], 60, 100);
        vrender(y, (uint32_t)(0.32 * FS));
        trk_note_off(&trk[0], 60);
        vrender(y + (uint32_t)(0.32 * FS), FS / 4);
        cents_of(y, 0, 34, c);
        early = vib_of(c, 12, 26, &rr);
        snprintf(what, sizeof what, "a key let go after 0.3 s: no vibrato (%.1f ct, <= 12)", early);
        check(what, early <= 12.0);
    }
    for (r = 0; r < 2u; r++) {   /* the sequencer: short notes, then long ones */
        static int32_t o[2 * CTL];
        uint32_t n = 0, i, period, nn = r ? 12u : 16u, frames;
        double mx = 0, mn = 1e9;
        vsetup(preset_of("CANTOR ES"), LG_ES, "a a a a a a a a a a a a a a a a");
        trk[0].p[P_E5] = 0;
        for (k = 1; k < NPART; k++) trk[k].p[P_LEVEL] = 0;
        trk[0].p[P_SLEN] = (int16_t)nn;
        trk[0].p[P_SDIV] = 1;
        for (k = 0; k < nn; k++) {
            step_t *s = &trk[0].step[k];
            s->time = r && k % 3u ? ST_TIE : ST_NOTE; s->n = !(r && k % 3u); s->note[0] = 60; s->vel = 100;
        }
        song.g[G_BPM] = (int16_t)(r ? 70 : 120);
        period = div_samples(1u);
        frames = period * nn;
        seq_start();
        while (n + CTL <= frames && n + CTL <= NELEM(y)) {
            mix_block(o, CTL);
            for (i = 0; i < CTL; i++) y[n + i] = (o[2 * i] + o[2 * i + 1]) / 65536.0;
            n += CTL;
        }
        seq_stop();
        for (k = 1; k + 1u < nn / (r ? 3u : 1u); k++) {   /* each note's middle (its vowel), but the first and the last */
            uint32_t a = (k * (r ? 3u : 1u) * period) / (FS / 100u), len = (r ? 3u : 1u) * period / (FS / 100u);
            double v;
            cents_of(y, 0, a + len, c);
            v = r ? vib_of(c, a + len / 2u, a + len - 12u, &rr) : vib_of(c, a + 4u, a + len - 4u, &rr);
            if (v > mx) mx = v;
            if (v < mn) mn = v;
        }
        snprintf(what, sizeof what, r ? "sequenced long notes (1.3 s): a vibrato (%.1f .. %.1f ct, >= 18)" :
                 "sequenced short notes (1/8 at 120 BPM): no vibrato (%.1f .. %.1f ct, <= 14)", mn, mx);
        check(what, r ? mn >= 18.0 : mx <= 14.0);
    }
    {   /* legato C4 -> E4 (GLIDE 0): the landing, as sung */
        static const snt_t GW[] = {{60, 4, 1}, {64, 4, 0}};
        static uint16_t nsy[FS * 3 / CTL];
        double t10 = -1, t90 = -1, over = 0;
        uint32_t a = (uint32_t)(4 * FS * 60u / 100u / 2u) / (FS / 100u);
        vsetup(preset_of("CANTOR ES"), LG_ES, "la la");
        trk[0].p[P_E5] = 0;
        sing_notes(GW, NELEM(GW), y, FS * 3u, nsy, 0);
        cents_of(y, 0, a + 40u, c);
        for (k = a; k < a + 40u; k++) {
            if (t10 < 0 && c[k] > 40) t10 = (k - a) * 10.0;
            if (t90 < 0 && c[k] > 360) t90 = (k - a) * 10.0;
            if (c[k] - 400 > over) over = c[k] - 400;
        }
        snprintf(what, sizeof what, "a legato note lands as sung: 10 .. 90 %% in %.0f ms (40 .. 200), %.0f ct past it (<= 60)",
                 t90 - t10, over);
        check(what, t10 >= 0 && t90 > t10 && t90 - t10 >= 40 && t90 - t10 <= 200 && over <= 60);
    }
    {   /* ROBOT EN: as played */
        double lo = 1e9, hi = -1e9;
        vsetup(preset_of("ROBOT EN"), LG_ES, "a");
        trk[0].p[P_E5] = 0;
        trk_note_on(&trk[0], 60, 100);
        vrender(y, (uint32_t)(2.4 * FS));
        cents_of(y, 0, 230, c);
        for (k = 20; k < 230u; k++) {
            if (c[k] < lo) lo = c[k];
            if (c[k] > hi) hi = c[k];
        }
        snprintf(what, sizeof what, "ROBOT EN (BUZZ at its top): no vibrato, no scoop, no wander (%+.1f .. %+.1f ct)", lo, hi);
        check(what, hi - lo < 2.0);
    }
    {   /* the same twice: a legato phrase with its draws, from silence */
        static double z[FS * 2];
        static const snt_t PH[] = {{60, 3, 1}, {62, 2, 0}, {64, 4, 0}, {60, 2, 0}};
        uint32_t n1, n2;
        vsetup(preset_of("CANTOR ES"), LG_ES, "la noche es azul");
        n1 = sing_notes(PH, NELEM(PH), y, FS * 2u, 0, 0);
        vsetup(preset_of("WORD CHOIR"), LG_ES, "sus suenyos");   /* (another singer between) */
        sing_notes(PH, NELEM(PH), z, FS * 2u, 0, 0);
        vsetup(preset_of("CANTOR ES"), LG_ES, "la noche es azul");
        n2 = sing_notes(PH, NELEM(PH), z, FS * 2u, 0, 0);
        check("the singer's draws (scoop, wander, vibrato, creak) the same each time a phrase is sung from silence",
              n1 == n2 && n1 > FS && !memcmp(y, z, n1 * sizeof y[0]));
    }
}

/* every word preset: sounds, no DC, ends, the same twice; at the corners no clipping */
static void check_presets(void)
{
    static double y[FS * 3];
    uint32_t k, ok = 1, okdc = 1, oke = 1, okr = 1, okc = 1;
    double worst = 0, peak = 0;
    for (k = getenv("VDC") ? 0u : 7u; k < ENG_FORMANT.npresets; k++) {   /* (VDC: the vowel presets too, for comparison) */
        double m = 0, r;
        uint32_t i, j, live = 0;
        if (!getenv("VDC") && !(ENG_FORMANT.presets[k].xp & 3u))
            continue;                                    /* (V10.7.44: TRACT's vowel presets come after the words') */
        vsetup((int32_t)k, 0, "la sol mar canta");
        trk[0].p[P_VLYR] = ENG_FORMANT.presets[k].xp >> 2;
        for (j = 0; j < 4u; j++) {
            trk_note_on(&trk[0], 60 + j * 2u, 100);
            vrender(y + j * (FS / 2), FS / 2);
            trk_note_off(&trk[0], 60 + j * 2u);
        }
        vrender(y + 2 * FS, FS);
        for (i = 0; i < 2u * FS; i++) m += y[i];
        m /= 2.0 * FS;
        r = rms(y, 0, 2u * FS);
        ok &= r > 0.01;
        if (fabs(m) / VOICE_FS * 32768.0 > worst) worst = fabs(m) / VOICE_FS * 32768.0;
        if (getenv("VDC"))
            printf("voice:   DC %s %.3f %%\n", ENG_FORMANT.presets[k].name, fabs(m) / VOICE_FS * 32768.0 * 100.0);
        okdc &= fabs(m) < 0.015 * VOICE_FS / 32768.0;
        for (i = 0; i < NVOICE; i++) live += trk[0].v[i].active;
        oke &= !live;
        {   /* the same twice */
            static double z[FS];
            vsetup((int32_t)k, 0, "la sol mar canta");
            trk[0].p[P_VLYR] = ENG_FORMANT.presets[k].xp >> 2;
            trk_note_on(&trk[0], 60, 100);
            vrender(z, FS / 2);
            okr &= !memcmp(z, y, (FS / 2) * sizeof z[0]);
        }
    }
    check("every word preset sounds", ok);
    {
        char what[160];
        snprintf(what, sizeof what, "no DC: worst |mean| %.2f %% of a voice's full scale (the vowel presets: up to 3.2 %%, CHANT)",
                 worst * 100.0);
        check(what, okdc);
    }
    check("every word preset's voices end after its release", oke);
    check("a phrase from silence renders the same samples again (every word preset)", okr);
    {   /* the corners: 4 voices, velocity 127, LEVEL 127, BUZZ / Q / BREATH at their ends, fricatives and plosives */
        static int32_t o[2 * CTL];
        static const int16_t C[4][3] = {{127, 127, 0}, {0, 127, 127}, {127, 0, 0}, {127, 127, 127}};
        uint32_t c, i, j;
        for (c = 0; c < 4u; c++) {
            vsetup(preset_of("WORD CHOIR"), LG_ES, "sha cha ta pa ka ssss");
            trk[0].p[P_E4] = C[c][0]; trk[0].p[P_E6] = C[c][1]; trk[0].p[P_E5] = C[c][2];
            trk[0].p[P_LEVEL] = 127;
            trk[0].p[P_VOICE] = V_POLY;
            for (j = 0; j < 4u; j++)
                trk_note_on(&trk[0], 48 + j * 7u, 127);
            for (i = 0; i < FS * 2u; i += CTL) {
                mix_block(o, CTL);
                for (j = 0; j < 2u * CTL; j++)
                    if (abs(o[j]) > peak) peak = abs(o[j]);
                if (i % (FS / 3u) < CTL)
                    for (j = 0; j < 4u; j++) { trk_note_off(&trk[0], 48 + j * 7u); trk_note_on(&trk[0], 48 + j * 7u, 127); }
            }
            okc &= peak < 32767;
        }
        {
            char what[160];
            snprintf(what, sizeof what, "no clipping at the corners (4 voices, velocity 127, LEVEL 127): mix peak %.0f", peak);
            check(what, okc);
        }
    }
}

/* the lyric as notes play it: a note the next syllable, a TIE (no note) holds it, "-" holds the vowel, POS, empty TEXT */
static void check_lyric(void)
{
    static const char *const W[4] = {"la", "le", "li", "lo"};
    uint32_t k, ok = 1;
    vsetup(preset_of("CANTOR ES"), LG_ES, "la le, li lo! ma");
    for (k = 0; k < 4u; k++) {
        const sing_t *s = sing_view(&trk[0]);
        trk_note_on(&trk[0], 60, 100);
        vrender(0, FS / 5);
        ok &= s->nsy == k + 1u && !strcmp(s->w, W[k]) && s->ph[s->nu] == (k == 0 ? PH_A : k == 1 ? PH_E : k == 2 ? PH_I : PH_O);
        trk_note_off(&trk[0], 60);
        vrender(0, FS / 10);
    }
    check("each note sings the next syllable (la le li lo; the punctuation skipped)", ok);
    {   /* a held note (a TIE) holds the syllable */
        const sing_t *s = sing_view(&trk[0]);
        trk_note_on(&trk[0], 62, 100);
        vrender(0, FS);
        ok = s->nsy == 5u && !strcmp(s->w, "ma") && s->i == s->nu;
        trk_note_off(&trk[0], 62);
        vrender(0, FS / 10);
        trk_note_on(&trk[0], 62, 100);                  /* the text wraps round */
        vrender(0, FS / 10);
        ok &= !strcmp(s->w, "la") && s->nsy == 6u;
        check("a held note holds its vowel (no syllable moves on); the text starts over at its end", ok);
    }
    {   /* "-": the vowel again, no consonant */
        const sing_t *s;
        vsetup(preset_of("CANTOR ES"), LG_ES, "sol - mar");
        s = sing_view(&trk[0]);
        trk_note_on(&trk[0], 60, 100); vrender(0, FS / 5); trk_note_off(&trk[0], 60); vrender(0, FS / 50);
        trk_note_on(&trk[0], 62, 100); vrender(0, FS / 50);
        ok = s->nsy == 2u && s->ph[s->nu] == PH_O && s->i == s->nu;
        trk_note_off(&trk[0], 62); vrender(0, FS / 5);
        ok &= s->i > s->nu;                              /* the release: sol's l at last */
        trk_note_on(&trk[0], 64, 100); vrender(0, FS / 5);
        ok &= !strcmp(s->w, "mar") && s->nsy == 3u;
        check("\"-\" holds the vowel one more note (sol - mar: o, o, then the l at the release, then mar)", ok);
    }
    {   /* POS: a syllable of the lyric, as it changes */
        const sing_t *s;
        vsetup(preset_of("CANTOR ES"), LG_ES, "uno dos tres cuatro");
        s = sing_view(&trk[0]);
        trk[0].p[P_E0] = 4;                              /* the 4th syllable: tres */
        trk_note_on(&trk[0], 60, 100); vrender(0, FS / 10); trk_note_off(&trk[0], 60); vrender(0, FS / 20);
        ok = !strcmp(s->w, "tres");
        trk_note_on(&trk[0], 60, 100); vrender(0, FS / 10); trk_note_off(&trk[0], 60); vrender(0, FS / 20);
        ok &= !strcmp(s->w, "cuatro") && s->wsy == 1u;   /* (POS held: on from there) */
        trk[0].p[P_E0] = 0;
        trk_note_on(&trk[0], 60, 100); vrender(0, FS / 10); trk_note_off(&trk[0], 60); vrender(0, FS / 20);
        ok &= !strcmp(s->w, "cuatro") && s->wsy == 2u;
        trk[0].p[P_E0] = 1;
        trk_note_on(&trk[0], 60, 100); vrender(0, FS / 10);
        ok &= !strcmp(s->w, "uno") && s->wsy == 1u;
        check("POS: a note sings that syllable of the lyric (as POS changes: a lock, automation), then on from it", ok);
    }
    {   /* an empty TEXT sings the dice's words; LANG OFF after words: the vowels as fresh */
        static double y1[FS / 2], y2[FS / 2];
        const sing_t *s;
        vsetup(preset_of("CANTOR ES"), LG_ES, "");
        s = sing_view(&trk[0]);
        trk_note_on(&trk[0], 60, 100); vrender(0, FS / 10);
        ok = s->w[0] && s->nsy == 1u;
        check("an empty TEXT sings W1 (the dice) rather than nothing", ok);
        vsetup(preset_of("VOX LEAD"), 0, "");
        trk_note_on(&trk[0], 60, 100);
        vrender(y1, FS / 2);
        vsetup(preset_of("CANTOR ES"), LG_ES, "sha la");
        trk_note_on(&trk[0], 60, 100); vrender(0, FS / 3); trk_note_off(&trk[0], 60); vrender(0, FS);
        trk[0].p[P_VLANG] = 0;
        host_preset(&trk[0], eng_voice(), (uint32_t)preset_of("VOX LEAD"));
        trk[0].p[P_ATK] = 0; trk[0].p[P_SUS] = 127; trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
        trk[0].v[0].active = 0; trk[0].v[0].env = trk[0].v[0].env_out = 0; trk[0].v[0].stage = 0;
        formant_nz = 0x2545F491;
        trk_note_on(&trk[0], 60, 100);
        vrender(y2, FS / 2);
        check("LANG OFF after singing words: the vowels bit for bit as from a fresh track", !memcmp(y1, y2, sizeof y1));
    }
    {   /* LOOP: with the sequencer, the lyric from its start every pattern loop (LOOP 1), on through it (FREE) */
        uint32_t loops, okl = 1, okf = 1, i;
        static int32_t o[2 * CTL];
        for (loops = 0; loops < 2u; loops++) {
            const sing_t *s;
            uint16_t first[3];
            vsetup(preset_of("CANTOR ES"), LG_ES, "la le li lo lu ma me mi mo mu");
            s = sing_view(&trk[0]);
            trk[0].p[P_E7] = loops ? 0 : 30;              /* LOOP 1 (30: the second name), FREE */
            trk[0].p[P_SLEN] = 4;
            trk[0].p[P_SDIV] = 2;
            for (k = 0; k < 4u; k++) {
                trk[0].step[k].time = ST_NOTE; trk[0].step[k].n = 1; trk[0].step[k].note[0] = 60; trk[0].step[k].vel = 100;
            }
            song.g[G_BPM] = 120;                          /* (1/16 at 120: a step 125 ms, the loop 0.5 s) */
            seq_start();
            for (i = 0; i < FS / 16u; i += CTL)           /* into the first step */
                mix_block(o, CTL);
            for (k = 0; k < 3u; k++) {                    /* three loops: the syllables at each loop's first step */
                first[k] = s->nsy;
                for (i = 0; i < FS / 2u; i += CTL)
                    mix_block(o, CTL);
            }
            seq_stop();
            printf("voice:   LOOP %s: syllables at the loops' first steps %u %u %u\n", loops ? "FREE" : "1", first[0], first[1],
                   first[2]);
            if (!loops) okl = first[0] == 1u && first[1] == 1u && first[2] == 1u;
            else okf = first[0] == 1u && first[1] == 5u && first[2] == 9u;
        }
        check("LOOP 1: the lyric from its start every loop of the pattern; FREE: on through it", okl && okf);
    }
}

/* (voice2) on time: the vowel on the note's time. Sequenced (130 BPM, 1/8 and 1/16, each step a syllable): the singer
 * reaches the vowel within -16 .. +3 ms of the step (the lead its consonants took, read a note ahead), and the vowel is
 * heard (its 350 .. 2000 Hz band at -6 dB of the held vowel's) within +-16 ms of it; a key (no lead): the vowel within
 * 14 ms of the key (the consonant squeezed, its hiss over the vowel); V10.7.22: 40 .. 120 ms late, both */
typedef struct { double b0, b1, b2, a1, a2, z1, z2; } bq_t;
static void bq_set(bq_t *q, int hp, double f)            /* a Butterworth 2-pole high- or low-pass */
{
    double w = 2 * M_PI * f / FS, c = cos(w), al = sin(w) / (2 * 0.7071), a0 = 1 + al;
    q->b0 = q->b2 = (hp ? (1 + c) : (1 - c)) / 2 / a0;
    q->b1 = (hp ? -(1 + c) : (1 - c)) / a0;
    q->a1 = -2 * c / a0; q->a2 = (1 - al) / a0; q->z1 = q->z2 = 0;
}
static double bq(bq_t *q, double x)
{
    double y = q->b0 * x + q->z1;
    q->z1 = q->b1 * x - q->a1 * y + q->z2;
    q->z2 = q->b2 * x - q->a2 * y;
    return y;
}
/* the vowel heard: y's 350 .. 2000 Hz band (4 poles each side), its RMS over 4 ms each ms; in [a, b) samples, the
 * vowel's level the most after mid, the least before it, then the first rise to -6 dB of the vowel's: samples */
static double band_on(const double *y, uint32_t a, uint32_t mid, uint32_t b)
{
    static double f[FS * 3];
    bq_t q[4];
    double ref = 0, mn = 1e30;
    uint32_t i, j, am = a, W = FS / 250u, H = FS / 1000u;
    bq_set(&q[0], 1, 350); bq_set(&q[1], 1, 350); bq_set(&q[2], 0, 2000); bq_set(&q[3], 0, 2000);
    for (i = 0; i < b; i++)
        f[i] = bq(&q[3], bq(&q[2], bq(&q[1], bq(&q[0], y[i]))));
    for (i = a; i + W < b; i += H) {
        double e = 0;
        for (j = 0; j < W; j++) e += f[i + j] * f[i + j];
        f[i] = e;                                        /* (the window's energy at its start, read ahead of it) */
    }
    for (i = mid; i + W < b; i += H)
        if (f[i] > ref) ref = f[i];
    for (i = a; i <= mid; i += H)
        if (f[i] < mn) { mn = f[i]; am = i; }
    for (i = am; i + W < b && f[i] < ref * 0.25; i += H)
        ;
    return (double)i + W / 2.0;
}
static void check_ontime(void)
{
    static const char *const SY[] = {"pa", "ta", "ka", "ba", "sa", "fa", "cha", "vai", "ma", "la", "ra", "tra"};
    static double y[FS * 3];
    static int32_t o[2 * CTL];
    char what[240];
    double nlo = 1e9, nhi = -1e9, alo = 1e9, ahi = -1e9, klo = 1e9, khi = -1e9;
    uint32_t k, d, i, j;
    for (k = 0; k < NELEM(SY); k++)
        for (d = 1; d <= 2u; d++) {                      /* 1/8, 1/16 */
            char txt[64];
            uint32_t period, n = 0, nuc[12] = {0};
            snprintf(txt, sizeof txt, "%s %s %s %s %s %s %s %s %s", SY[k], SY[k], SY[k], SY[k], SY[k], SY[k], SY[k], SY[k], SY[k]);
            vsetup(preset_of("CANTOR ES"), LG_ES, txt);
            trk[0].p[P_ATK] = ENG_FORMANT.presets[preset_of("CANTOR ES")].env[0];
            for (j = 1; j < NPART; j++) trk[j].p[P_LEVEL] = 0;
            trk[0].p[P_SLEN] = 8;
            trk[0].p[P_SDIV] = (int16_t)d;
            for (j = 0; j < 8u; j++) {
                trk[0].step[j].time = ST_NOTE; trk[0].step[j].n = 1; trk[0].step[j].note[0] = 60; trk[0].step[j].vel = 100;
            }
            song.g[G_BPM] = 130;
            period = div_samples(d);
            seq_start();
            while (n + CTL <= period * 8u && n + CTL <= NELEM(y)) {
                mix_block(o, CTL);
                for (i = 0; i < CTL; i++) y[n + i] = (o[2 * i] + o[2 * i + 1]) / 65536.0;
                n += CTL;
                if (sing[0].nsy < 12u && !nuc[sing[0].nsy] && sing[0].np && sing[0].i == sing[0].nu) nuc[sing[0].nsy] = n;
            }
            seq_stop();
            for (j = 2; j < 7u; j++) {                   /* (step 0 has no lead: PLAY) */
                double g = (double)(j - 1u) * period, t = ((double)nuc[j] - g) * 1000.0 / FS;
                double a = (band_on(y, (uint32_t)g - period / 2u, (uint32_t)g + FS / 25u, (uint32_t)g + period - FS / 50u) - g) *
                           1000.0 / FS;
                if (t < nlo) nlo = t;
                if (t > nhi) nhi = t;
                if (k < 8u && a < alo) alo = a;          /* (heard: not l r tr, their liquid heard as the vowel's start) */
                if (k < 8u && a > ahi) ahi = a;
            }
        }
    for (k = 0; k < NELEM(SY); k++) {                    /* keys: from silence and after a note */
        for (d = 0; d < 2u; d++) {
            double t = -1;
            char txt[64];
            snprintf(txt, sizeof txt, "%s%s", d ? "a " : "", SY[k]);
            vsetup(preset_of("CANTOR ES"), LG_ES, txt);
            if (d) {
                trk_note_on(&trk[0], 60, 100); vrender(0, FS / 4); trk_note_off(&trk[0], 60); vrender(0, FS / 20);
            }
            trk_note_on(&trk[0], 62, 100);
            for (i = 0; i < FS / 10u && t < 0; i += CTL) {
                vrender(0, CTL);
                if (sing[0].nsy == d + 1u && sing[0].i == sing[0].nu) t = (i + CTL) * 1000.0 / FS;
            }
            if (t < klo) klo = t;
            if (t > khi) khi = t;
        }
    }
    printf("voice:   on time: sequenced vowel reached %.1f .. %.1f ms of the step, heard %.1f .. %.1f; keys %.1f .. %.1f ms\n",
           nlo, nhi, alo, ahi, klo, khi);
    snprintf(what, sizeof what, "on time: a sequenced syllable's vowel on its step (reached %.0f .. %.0f ms, heard %.0f .. %.0f; "
             "-16 .. 3, +-16), a key's within %.0f ms (<= 14)", nlo, nhi, alo, ahi, khi);
    check(what, nlo >= -16.0 && nhi <= 3.0 && alo >= -16.0 && ahi <= 16.0 && klo >= 0 && khi <= 14.0);
}

static void check_cost(void)
{
    static double y[FS];
    uint64_t t0, t1, t2, t3;
    uint32_t j;
    vsetup(preset_of("VOX LEAD"), 0, "");
    trk[0].p[P_VOICE] = V_POLY;
    for (j = 0; j < 4u; j++) trk_note_on(&trk[0], 60 + j * 3u, 100);
    t0 = now_ns();
    vrender(y, FS);
    t1 = now_ns();
    vsetup(preset_of("CANTOR ES"), LG_ES, "la noche es azul y el mar canta para ti");
    trk[0].p[P_VOICE] = V_POLY;
    for (j = 0; j < 4u; j++) trk_note_on(&trk[0], 60 + j * 3u, 100);
    t2 = now_ns();
    vrender(y, FS);
    t3 = now_ns();
    printf("voice: cost, host ns per sample and voice (4 voices; information): vowels %.1f, words %.1f\n",
           (double)(t1 - t0) / FS / 4.0, (double)(t3 - t2) / FS / 4.0);
    check("cost: a singing voice (words) at most 1.6 x a vowel voice (V10.7.22: ~1.25; voice2 adds no work a sample)",
          (double)(t3 - t2) <= 1.6 * (double)(t1 - t0));
}

/* (V10.6y, alex.knip) the level: an open vowel ("AH", LANG OFF and sung) under full scale, as loud as the other engines
 * (it was ~+9 dB over them, a third of an A's samples in the soft knee); CONS (EDIT > WORDS KNOB 3, P_VCONS): 127 (every
 * project and preset of before) 100 %, the consonants 50 .. 400 % as long */
static uint32_t cons_raw(uint32_t pct)                   /* the stored value nearest pct */
{
    uint32_t v, best = 127;
    for (v = 0; v < 128u; v++)
        if (abs((int)(vcons_q6((int32_t)v) * 100u / 64u) - (int)pct) < abs((int)(vcons_q6((int32_t)best) * 100u / 64u) - (int)pct))
            best = v;
    return best;
}
static uint32_t cons_blocks(uint32_t raw, uint32_t ph)  /* "sa" at CONS raw: the blocks phoneme ph sounds */
{
    uint32_t b, n = 0;
    vsetup(preset_of("CANTOR ES"), LG_ES, "sa");
    trk[0].p[P_VCONS] = (int16_t)raw;
    note_led(60);
    for (b = 0; b < FS / CTL; b++) {
        vrender(0, CTL);
        n += sing_phon(&sing[0]) == ph && sing[0].i < sing[0].nu;
    }
    return n;
}
static void check_level_cons(void)
{
    static double y[FS];
    static const uint8_t NT[3] = {48, 60, 72}, VL[2] = {100, 127};
    char what[200], v[8];
    const char *u;
    double pk = 0, lo = 0, hi = -99, ra;
    uint32_t k, j, i, n100, n300, n50;
    for (k = 0; k < 6u; k++)
        for (j = 0; j < 2u; j++) {
            double s = 0, r;
            vsetup(preset_of(k & 1u ? "CANTOR ES" : "INIT VOWEL"), k & 1u ? LG_ES : 0u, k & 1u ? "a" : "");
            trk[0].p[P_ATK] = 2;
            trk_note_on(&trk[0], NT[k >> 1], VL[j]);
            vrender(y, FS);
            for (i = 0; i < FS; i++) { s += y[i] * y[i]; if (fabs(y[i]) > pk) pk = fabs(y[i]); }
            r = 10 * log10(s / FS);
            if (r < lo || lo == 0) lo = r;
            if (r > hi) hi = r;
        }
    vsetup(0, 0, "");
    host_preset(&trk[0], 0, 0);                          /* (ANALOG's first, as a reference) */
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk_note_on(&trk[0], 60, 100);
    vrender(y, FS);
    for (i = 0, ra = 0; i < FS; i++) ra += y[i] * y[i];
    ra = 10 * log10(ra / FS);
    snprintf(what, sizeof what, "level: an open A (vowels, sung) C3 .. C5 peaks %.2f (under full scale), RMS %.1f .. %.1f dB (ANALOG %.1f)",
             pk, lo, hi, ra);
    check(what, pk < 0.95 && hi < ra + 3 && lo > ra - 13);   /* (voice3: a held note fades ~4 dB, as sung) */
    vsetup(preset_of("CANTOR ES"), LG_ES, "sa");
    {
        const param_desc_t *d = track_desc(&trk[0], P_VCONS);
        uint32_t ok = d && str_eq(d->label, "CONS") && trk[0].p[P_VCONS] == 127 && vcons_q6(127) == 64u;
        param_format(d, 127, v, &u);
        ok &= str_eq(v, "100") && str_eq(u, "%");
        param_format(d, param_turn(d, 127, -200), v, &u);
        ok &= str_eq(v, "50");
        param_format(d, param_turn(d, 127, 200), v, &u);
        ok &= str_eq(v, "400");
        ok &= param_turn(d, 127, 1) != 127 && vcons_q6(param_turn(d, 127, 1)) > 64u && vcons_q6(param_turn(d, 127, -1)) < 64u;
        for (k = 0; k < NELEM(PAGES); k++)
            if (str_eq(PAGES[k].title, "WORDS"))
                ok &= PAGES[k].id[2] == P_VCONS;
        ok &= track_desc(&trk[0], P_VCONS) != track_desc(&(track_t){.eng_req = ENGI_DRUM}, P_VCONS);
        check("CONS: on WORDS' KNOB 3, 127 (projects and presets of before) 100 %, turned 50 .. 400 %; DRUM's lane level else", ok);
    }
    n100 = cons_blocks(127, PH_S);
    n300 = cons_blocks(cons_raw(200), PH_S);            /* (voice2: 200 %, was 300: a led note's onset at most ~180 ms) */
    n50 = cons_blocks(cons_raw(50), PH_S);
    snprintf(what, sizeof what, "CONS: the s of \"sa\" %u blocks at 100 %%, %u at 200 %%, %u at 50 %%", n100, n300, n50);
    check(what, n100 > 4u && n300 >= n100 * 3u / 2u && n300 <= n100 * 5u / 2u && n50 * 2u >= n100 - 2u && n50 * 2u <= n100 + 2u);
}

/* --------------------------------------------------------------------------------------------------- demos --- */
static void demos(const char *dir)
{
    /* Spanish: 14 syllables (la-no-che-es-a-zul-y-el-mar-can-ta-pa-ra-ti) on a G major line */
    static const nt_t ES[] = {{67, 2}, {69, 2}, {71, 4}, {69, 2}, {67, 2}, {64, 4}, {62, 2}, {64, 2}, {67, 6}, {0, 2},
                              {67, 2}, {69, 2}, {71, 2}, {72, 2}, {71, 4}, {67, 8}};
    /* English: "you are my sunshine, my only sunshine" (you-are-my-sun-shine-my-on-ly-sun-shine) */
    static const nt_t EN[] = {{55, 2}, {60, 2}, {62, 2}, {64, 6}, {64, 4}, {0, 2}, {64, 2}, {62, 2}, {64, 2}, {60, 6},
                              {60, 6}};
    /* Japanese: "sakura sakura yayoi no sora wa" (sa-ku-ra sa-ku-ra ya-yo-i no so-ra wa) */
    static const nt_t JA[] = {{69, 3}, {69, 3}, {71, 6}, {69, 3}, {69, 3}, {71, 6}, {69, 3}, {71, 3}, {72, 3}, {71, 3},
                              {69, 3}, {71, 2}, {69, 2}, {65, 6}};
    /* the dice: 16 notes */
    static const nt_t RN[] = {{62, 2}, {64, 2}, {65, 2}, {67, 2}, {69, 3}, {67, 1}, {65, 2}, {64, 4}, {0, 2}, {62, 2},
                              {64, 2}, {65, 2}, {69, 2}, {67, 3}, {65, 1}, {64, 2}, {62, 6}};
    uint32_t k;
    char nm[64];
    demo_wav(dir, "es_phrase", preset_of("CANTOR ES"), "la noche es azul y el mar canta para ti", ES, NELEM(ES));
    demo_wav(dir, "es_phrase_cantora", preset_of("CANTORA ES"), "la noche es azul y el mar canta para ti", ES, NELEM(ES));
    demo_wav(dir, "en_phrase", preset_of("SINGER EN"), "you are my sunshine my only sunshine", EN, NELEM(EN));
    demo_wav(dir, "ja_phrase", preset_of("UTA JA"), "sakura sakura yayoi no sora wa", JA, NELEM(JA));
    demo_wav(dir, "rnd_es_W1", preset_of("CANTOR ES"), "", RN, NELEM(RN));
    demo_wav(dir, "rnd_en_W3", preset_of("SINGER EN"), "", RN, NELEM(RN));
    demo_wav(dir, "rnd_ja_W4", preset_of("UTA JA"), "", RN, NELEM(RN));
    demo_wav(dir, "rnd_en_scat_S1", preset_of("SCAT EN"), "", RN, NELEM(RN));
    for (k = 7; k < ENG_FORMANT.npresets; k++) {         /* every word preset, its own dice */
        uint32_t i;
        snprintf(nm, sizeof nm, "preset_%02u_", k);
        for (i = 0; ENG_FORMANT.presets[k].name[i] && i < 20u; i++)
            nm[10 + i] = ENG_FORMANT.presets[k].name[i] == ' ' ? '_' : ENG_FORMANT.presets[k].name[i];
        nm[10 + i] = 0;
        demo_wav(dir, nm, (int32_t)k, "", RN, NELEM(RN));
    }
    printf("voice: demos in %s\n", dir);
}

/* The original POS walk is the reference, including its 64-word limit. Check every knob position and the phoneme
 * cache as well as the singer: a faster jump must leave the next syllable and generated phrase unchanged. */
static void seek_reference(const track_t *t, sing_t *s, uint32_t lang, uint32_t lyr, uint32_t k)
{
    uint32_t w;
    sy_reset(s);
    for (w = 0; w < 64u && k; w++) {
        if (s->wsy >= s->wns && !sy_word_next(t, s, lang, lyr)) return;
        {
            uint32_t d = (uint32_t)(s->wns - s->wsy) < k ? (uint32_t)(s->wns - s->wsy) : k;
            s->wsy = (uint8_t)(s->wsy + d);
            k -= d;
            s->nsy = (uint16_t)(s->nsy + d);
        }
    }
}
static void check_seek_cycles(void)
{
    static const char *const TEXT[] = {
        "love me now we go home", "uno dos tres cuatro", "a", "-", "- a -", "a - - b",
        "strength through the night", "... ???", "_ _ x _", "konnichiwa sekai", "", "a - a - a - a",
        "a b c d e f g h i j k l m n o p q r s t u v w x y z a b c d e f"};
    uint32_t ti, lang, tx, pos, lyr, errors = 0, count = 0;
    host_tracks_init();
    for (ti = 0; ti < NTRK; ti++) {
        track_t *t = &trk[ti];
        host_preset(t, eng_voice(), 0);
        for (lang = LG_ES; lang <= LG_JA; lang++)
            for (tx = 0; tx < NELEM(TEXT); tx++) {
                lyr_set(ti, TEXT[tx]);
                for (pos = 0; pos < 127u; pos++) {
                    sing_t a, b;
                    sw_t wa, wb;
                    memset(&a, 0, sizeof a);
                    a.rng = 17u + ti; a.phr = 9; a.prem = 3; a.wsy = a.wns = 2;
                    b = a;
                    memset(&sing_w[ti], 0, sizeof sing_w[ti]);
                    seek_reference(t, &a, lang, 0, pos); wa = sing_w[ti];
                    memset(&sing_w[ti], 0, sizeof sing_w[ti]);
                    sy_seek(t, &b, lang, 0, pos); wb = sing_w[ti];
                    errors += memcmp(&a, &b, sizeof a) != 0 || memcmp(&wa, &wb, sizeof wa) != 0;
                    count++;
                }
            }
    }
    /* Generated lyrics, including empty TEXT's generated fallback, retain the original walk. */
    lyr_set(0, "");
    for (lang = LG_ES; lang <= LG_JA; lang++)
        for (lyr = 0; lyr < LYR_N; lyr++)
            for (pos = 0; pos < 127u; pos++) {
                sing_t a, b;
                sw_t wa, wb;
                memset(&a, 0, sizeof a); a.rng = pos + 1u; b = a;
                memset(&sing_w[0], 0, sizeof sing_w[0]);
                seek_reference(&trk[0], &a, lang, lyr, pos); wa = sing_w[0];
                memset(&sing_w[0], 0, sizeof sing_w[0]);
                sy_seek(&trk[0], &b, lang, lyr, pos); wb = sing_w[0];
                errors += memcmp(&a, &b, sizeof a) != 0 || memcmp(&wa, &wb, sizeof wa) != 0;
                count++;
            }
    printf("voice: POS equivalence: %u cases, %u differences\n", count, errors);
    check("POS cycle skipping preserves the original singer and phoneme state at every position", errors == 0);
}

/* (p1, main 0.9.16) THEME: a name a detent (the knob, params.c param_turn), and swept while a note is held the voice goes on as
 * it was, bit for bit: THEME is the next words', never the sound (voice.c engine_block dipped at every value: crackle);
 * the other named values (LOFI WAV#, NOISE LEN) a name a detent too, a new name still a dip */
static void check_theme(void)
{
    static double a[FS], b[FS];
    const param_desc_t *d = &FMT_WDESC[1];
    uint32_t i, k, dips = 0, names = 1, ok = 1;
    int32_t v = 0, w;
    char nm[8], prev[8];
    const char *u;
    for (i = 0; i < 2u; i++) {
        vsetup(0, LG_ES, "laaa");
        trk[0].p[P_E1] = 0;
        trk_note_on(&trk[0], 60, 100);
        for (k = 0; k < FS; k += CTL) {
            if (i && k >= FS / 4)
                trk[0].p[P_E1] = (int16_t)((k / CTL) % 128u);   /* a value a block: swept back and forth */
            engine_block(&trk[0]);
            dips += trk[0].xf_dip;
            vrender(i ? b + k : a + k, CTL);
        }
    }
    for (k = 0; k < FS; k++)
        ok &= a[k] == b[k];
    check("THEME swept every block over a held note: no dip, the voice bit for bit as without it", ok && !dips);
    param_format(d, 0, prev, &u);
    for (k = 0; k < 7u; k++) {                           /* a detent: the next name, onto its middle */
        w = param_turn(d, v, 1);
        param_format(d, w, nm, &u);
        names += strcmp(nm, prev) != 0;
        ok &= w == (int32_t)(16u * (k + 1u) + 8u);
        memcpy(prev, nm, sizeof prev);
        v = w;
    }
    ok &= names == 8u && param_turn(d, v, 1) == 120 && param_turn(d, 64, 1) == 88 && param_turn(d, 64, -1) == 56 &&
          param_turn(d, 127, -2) == 88;
    check("THEME: LOVE .. FIRE in 7 detents, each onto its name's middle (stored 0..127 as before)", ok);
    ok = param_turn(&FMT_WDESC[7], 0, 1) == 38 && param_turn(&ENG_WEST.edit[3], 0, 3) == 3 &&
         param_turn(&FMT_WDESC[0], 5, 1) == 6;       /* (LOOP 5 names; LEN 8; POS no names: a value; V10.6d: LOFI's WAV# gone with its chips) */
    check("  the other named values a name a detent (LOOP; V10.7.36 WEST RATIO, an enum: a ratio a detent); POS a value a detent", ok);
    {                                                    /* (0.9.17) between the detents: the blend */
        static char t8[2048], tv[2048];
        uint32_t s1, l, same = 1, diff = 1;
        for (l = LG_ES; l <= LG_JA; l++) {
            gen_text(l, 1, 0, 6, 9, t8, sizeof t8, &s1);
            for (k = 0; k <= 8u; k += 4u) {              /* a name's low half: its words, bit for bit (projects) */
                gen_text(l, 1, 0x100u + k, 6, 9, tv, sizeof tv, &s1);
                same &= !strcmp(t8, tv);
            }
            gen_text(l, 1, 0x100u + 15u, 6, 9, tv, sizeof tv, &s1);
            diff &= strcmp(t8, tv) != 0 && s1;           /* the top: the next theme's words in it, phrases whole */
            gen_text(l, 1, 1, 6, 9, t8, sizeof t8, &s1);
            gen_text(l, 1, 0x100u + 16u, 6, 9, tv, sizeof tv, &s1);
            same &= !strcmp(t8, tv);
        }
        check("  THEME between detents: 0..8 of a name its words as at the detent, 15 a blend with the next", same && diff);
    }
}

int main(int argc, char **argv)
{
    if (argc > 2 && !strcmp(argv[1], "--g2p")) {
        uint32_t lang = !strcmp(argv[2], "JA") ? LG_JA : !strcmp(argv[2], "EN") ? LG_EN : !strcmp(argv[2], "PT") ? LG_PT : LG_ES;
        int a;
        char b[256];
        for (a = 3; a < argc; a++) {
            g2p_str(lang, argv[a], b, sizeof b);
            printf("%-16s %s\n", argv[a], b);
        }
        return 0;
    }
    check_reading();
    check_generator();
    check_sound();
    check_singer();
    check_presets();
    check_lyric();
    check_theme();
    check_level_cons();
    check_ontime();
    check_cost();
    if (argc > 1)
        demos(argv[1]);
    check_phtext();                                      /* (V10.7.33; after the demos: they render as before) */
    check_seek_cycles();
    printf("voice: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}

