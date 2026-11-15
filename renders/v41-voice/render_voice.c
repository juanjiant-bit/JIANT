/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.43, renders/v41-voice: removed before release) A VOICE track singing a lyric to a fixed tune, dry, into a WAV,
 * played by the keys (live) or by the sequencer (seq):
 *   render_voice OUT.wav PRESET live|seq "LYRIC" [LANG]
 * Built against tests/hostsim.c of either tree (the same calls): V10.7.23 (841f7dd9) for "antes", V10.7.40 (58c9d1bc)
 * and this branch for "despues". The whole mix (mix_block: the keys, the sequencer, the master), the sends at 0, LIFE 0.
 * The tune: 7 notes (A3 C4 D4 E4 D4 C4 A3, eighths 2 2 2 3 1 2 4 at 92 BPM), twice. Written mono ((L + R) / 2), 16 bits.
 * BAL=0 (the master's BALANCE off: V10.7.23 had none), ORG=v (this branch: TONE's ORGANIC). See LEEME.txt */
#define main hostsim_main
#include "hostsim.c"
#undef main
static void mono_hdr(FILE *f, uint32_t frames)
{
    uint32_t v;
    uint16_t a = 1, ch = 1, ba = 2, bits = 16;
    uint32_t sr = FS, br = FS * 2u;
    fwrite("RIFF", 1, 4, f); v = 36 + frames * 2u; fwrite(&v, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v = 16; fwrite(&v, 4, 1, f);
    fwrite(&a, 2, 1, f); fwrite(&ch, 2, 1, f); fwrite(&sr, 4, 1, f); fwrite(&br, 4, 1, f);
    fwrite(&ba, 2, 1, f); fwrite(&bits, 2, 1, f);
    fwrite("data", 1, 4, f); v = frames * 2u; fwrite(&v, 4, 1, f);
}
static uint32_t eng_of(const engine_t *en)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == en) return e;
    return 0;
}
int main(int argc, char **argv)
{
    static const struct { uint8_t note, len; } NT[] = {{57, 2}, {60, 2}, {62, 2}, {64, 3}, {62, 1}, {60, 2}, {57, 4}};
    static int32_t o[2 * CTL];
    uint32_t k, i, pi = 0, nn = sizeof NT / sizeof NT[0], seq, st = 0, frames = 0, total, sixteenth;
    FILE *f;
    if (argc < 5 || !(f = fopen(argv[1], "wb"))) {
        fprintf(stderr, "render_voice OUT.wav PRESET live|seq LYRIC [LANG]\n");
        return 2;
    }
    seq = !strcmp(argv[3], "seq");
    for (k = 0; k < ENG_FORMANT.npresets; k++)
        if (str_eq(ENG_FORMANT.presets[k].name, argv[2])) pi = k;
    host_tracks_init();
    song.g[G_BPM] = 92;
    host_preset(&trk[0], eng_of(&ENG_FORMANT), pi);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    if (argc > 5)
        trk[0].p[P_VLANG] = !strcmp(argv[5], "EN") ? LG_EN : !strcmp(argv[5], "JA") ? LG_JA : LG_ES;
    trk[0].p[P_VLYR] = 0;
#ifdef P_VORG
    if (getenv("ORG"))
        trk[0].p[P_VORG] = (int16_t)atoi(getenv("ORG"));
#endif
    lyr_set(0, argv[4]);
    for (k = 1; k < NTRK; k++)
        trk[k].p[P_MUTE] = 1;
    sixteenth = FS * 60u / 92u / 4u;
    for (k = 0; k < 2u * nn; k++) {                 /* the pattern: a note, then its ties (32 steps) */
        uint32_t n = NT[k % nn].len * 2u, j;
        uint8_t u = NT[k % nn].note;
        for (j = 0; j < n && st < NSTEP; j++)
            put_step(&trk[0], st++, j ? 0u : 1u, &u, j ? ST_TIE : ST_NOTE, 0);
    }
    trk[0].p[P_SLEN] = (int16_t)st;
    total = st * sixteenth + FS / 2u;
    if (seq)
        transport_req = 1;
    mono_hdr(f, 0);
    for (frames = 0; frames < total; frames += CTL) {
        if (!seq) {                                 /* the keys: each note held 7/8 of its length (key 7 = C4) */
            uint32_t t = frames, a = 0, n = 0;
            for (k = 0; k < 2u * nn; k++) {
                uint32_t len = NT[k % nn].len * 2u * sixteenth;
                if (t >= a && t < a + len - len / 8u)
                    n = 1u << (NT[k % nn].note - 53u);
                a += len;
            }
            fm1_in.notes = n;
        } else if (frames + CTL >= st * sixteenth && frames < st * sixteenth) {
            transport_req = 2;                      /* the end: stop (the last note's release) */
        }
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            int32_t m = (o[2 * i] + o[2 * i + 1]) / 2;
            int16_t q = (int16_t)(m > 32767 ? 32767 : m < -32768 ? -32768 : m);
            fwrite(&q, 2, 1, f);
        }
    }
    fseek(f, 0, SEEK_SET);
    mono_hdr(f, frames);
    fclose(f);
    printf("%s: %u frames\n", argv[1], frames);
    return 0;
}
