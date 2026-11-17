/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the real overload transitions with four tracks and every preset. */
#define main hostsim_main
#include "hostsim.c"
#undef main

int main(void)
{
    uint32_t tracks, e, pr, p, b, cases = 0;
    int32_t out[2 * CTL];
    for (tracks = 4; tracks <= 6; tracks += 2) for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        for (pr = 0; pr < ENGINES[e]->npresets; pr++) {
            memset(trk, 0, sizeof trk);
            memset(&song, 0, sizeof song);
            memset(&chain, 0, sizeof chain);
            host_tracks_init(); trk_live = tracks;
            load_fx_q8 = perf_allowed = perf_denied = perf_held = 0;
            for (p = 0; p < tracks; p++) {
                host_preset(&trk[p], e, pr);
                trk[p].p[P_VOICE] = V_POLY;
                trk_note_on(&trk[p], 48 + p * 3, 100);
            }
            for (b = 0; b < 384; b++) {
                if (!(b & 1)) load_half(b < 192 ? 90 : 40, b < 192 ? 98 : 45, 1);
                if (!(b & 31)) for (p = 0; p < tracks; p++) {
                    trk_note_off(&trk[p], 48 + p * 3);
                    trk_note_on(&trk[p], 48 + p * 3, 100);
                }
                perf_held = (b & 64u) ? PF_BIT(PF_D16) | PF_BIT(PF_REV) | PF_BIT(PF_WOB) : 0;
                mix_block(out, CTL);
                for (p = 0; p < 2u * CTL; p++) if (out[p] < -32768 || out[p] > 32767) return 1;
            }
            cases++;
        }
    }
    printf("overload stress: %u four/six-track presets with FX, overload/recovery PASS\n", cases);
    return 0;
}
