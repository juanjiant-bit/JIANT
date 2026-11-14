/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.33, renders/v33a: removed before release) An ANALOG bass riff through its track (track_render, then DIST and
 * CRUSH as fx.c's mix runs them), with the bit reducer at 70: built against V10.7.32 with -DOLD (DIST 70, TYPE CRUSH) and against
 * this branch without it (DIST 0, EDIT > PLAY 3 CRUSH 70). render_crush OUT.wav PRESET. See LEEME.txt */
#define main hostsim_main
#include "hostsim.c"
#undef main
int main(int argc, char **argv)
{
    static const uint8_t RIFF[16] = {36, 36, 48, 36, 39, 36, 43, 36, 36, 36, 48, 46, 43, 41, 39, 41};
    static int32_t o[2 * CTL];
    uint32_t k, i, frames = 0, step = FS * 60u / 110u / 4u;
    FILE *f;
    if (argc < 2 || !(f = fopen(argv[1], "wb"))) return 2;
    host_tracks_init();
    host_preset(&trk[0], 0, argc > 2 ? (uint32_t)atoi(argv[2]) : 2u);   /* (ANALOG 02 SQR BASS) */
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
#ifdef OLD
    trk[0].p[P_DIST] = 70;
    trk[0].p[P_DTYPE] = 3;                               /* (V10.7.32: CRUSH) */
#else
    trk[0].p[P_DIST] = 0;
    cru_v[0] = 70;                                       /* (V10.7.33: EDIT > PLAY 3, CRUSH) */
#endif
    wav_hdr(f, 0);
    for (k = 0; k < 64u; k++) {
        trk_note_on(&trk[0], RIFF[k % 16u], 100);
        for (i = 0; i < step; i += CTL) {
            uint32_t j;
            if (i + CTL >= step / 2u && i < step / 2u) trk_note_off(&trk[0], RIFF[k % 16u]);
            track_render(&trk[0], o, CTL);
            track_dist(&trk[0], o, CTL);                 /* (the part's inserts, as fx.c's mix runs them) */
#ifndef OLD
            track_crush(&trk[0], o, CTL);
#endif
            for (j = 0; j < CTL; j++, frames++) wav_put(f, o[j] >> 1, o[j] >> 1);
        }
    }
    fseek(f, 0, SEEK_SET);
    wav_hdr(f, frames);
    fclose(f);
    printf("%s: %u frames\n", argv[1], frames);
    return 0;
}
