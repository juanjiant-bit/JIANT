/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.33, renders/v33a: removed before release) A VOICE track singing a lyric to a fixed tune, dry, into a WAV:
 *   render_voice OUT.wav ES|EN PRESET "LYRIC"
 * Built against tests/hostsim.c of either tree (the same calls): V10.7.32 (the English dictionary on the device) for
 * "before", this branch for "after" (the lyric as the editor sends it: web/lyrics.js's phonemes). See LEEME.txt */
#define main hostsim_main
#include "hostsim.c"
#undef main
static uint32_t eng_of(const engine_t *en)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == en) return e;
    return 0;
}
int main(int argc, char **argv)
{
    static const struct { uint8_t note, len; } NT[] = {{57, 2}, {60, 2}, {62, 2}, {64, 3}, {62, 1}, {60, 2}, {57, 4},
        {64, 2}, {65, 2}, {67, 3}, {65, 1}, {64, 2}, {62, 2}, {60, 6}};   /* (14 notes, ~8 s) */
    static int32_t o[2 * CTL];
    uint32_t k, i, frames = 0, eighth = FS * 60u / 92u / 2u, pi = 0, nn = sizeof NT / sizeof NT[0];
    FILE *f;
    if (argc < 5 || !(f = fopen(argv[1], "wb"))) {
        fprintf(stderr, "render_voice OUT.wav ES|EN PRESET LYRIC\n");
        return 2;
    }
    for (k = 0; k < ENG_FORMANT.npresets; k++)
        if (str_eq(ENG_FORMANT.presets[k].name, argv[3])) pi = k;
    host_tracks_init();
    host_preset(&trk[0], eng_of(&ENG_FORMANT), pi);
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
    trk[0].p[P_VLANG] = !strcmp(argv[2], "EN") ? LG_EN : LG_ES;
    trk[0].p[P_VLYR] = 0;
    lyr_set(0, argv[4]);
    wav_hdr(f, 0);
    for (k = 0; k < nn; k++) {
        uint32_t len = NT[k].len * eighth, note = NT[k].note;
        if (note) trk_note_on(&trk[0], note, 100);
        for (i = 0; i < len; i += CTL) {
            uint32_t j;
            if (note && i + CTL >= len - len / 8u && i < len - len / 8u) trk_note_off(&trk[0], note);
            track_render(&trk[0], o, CTL);
            for (j = 0; j < CTL; j++, frames++) wav_put(f, o[j] >> 1, o[j] >> 1);
        }
    }
    for (i = 0; i < FS / 4u; i += CTL) {
        uint32_t j;
        track_render(&trk[0], o, CTL);
        for (j = 0; j < CTL; j++, frames++) wav_put(f, o[j] >> 1, o[j] >> 1);
    }
    fseek(f, 0, SEEK_SET);
    wav_hdr(f, frames);
    fclose(f);
    printf("%s: %u frames\n", argv[1], frames);
    return 0;
}
