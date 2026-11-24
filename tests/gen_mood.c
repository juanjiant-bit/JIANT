// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.40, the owner: "MOOD is far too weak") what MOOD does to a piece, measured: 24 seeds a GENRE mode (EXPLORE,
 * RANDOM, HOUSE) at -50 -25 0 +25 +50: the drum hits a bar that sound (a lane's level over 8), the pieces with a kick
 * sounding, the kit's DECAY and filter, the tempo, and the drums against the other parts (fx.c's stems over 2 bars, dB).
 * A study's tool (host): cc -O1 -w -Ibuild/gen -Ifirmware/src tests/gen_mood.c -lm -o build/host/gen_mood */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    static const int8_t MOODS[5] = {-50, -25, 0, 25, 50};
    static const char *const MODE[3] = {"EXPLORE", "RANDOM", "HOUSE"};
    uint32_t md, mi, r, hs = 0;
    ui_power_on();
    for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, "HOUSE")) hs = r;
    printf("%-8s %5s %9s %6s %6s %6s %6s %12s\n", "mode", "MOOD", "hits/bar", "kick%", "DECAY", "FCUT", "BPM", "drums-parts dB");
    for (md = 0; md < 3u; md++)
        for (mi = 0; mi < 5u; mi++) {
            double hits = 0, dec = 0, cut = 0, bpm = 0, db = 0;
            uint32_t kick = 0, ndb = 0;
            for (r = 0; r < 24u; r++) {
                uint32_t dr, i, l, len, h = 0, k = 0, b, nb;
                double ed = 0, eo = 0;
                track_t *t;
                gp.genre = (uint8_t)(md == 0u ? GEN_EXPLORE : md == 1u ? 0u : hs);
                gp.form = 0; gp.seed = (uint16_t)(100u + r * 37u);
                grd.on = 0; grd.mood = MOODS[mi];
                gen_make();
                dr = gs.trk_of[RL_DRUM]; t = &trk[dr];
                bpm += song.g[G_BPM];
                if (!gs.act[RL_DRUM] && MOODS[mi] > 0) printf("  no drums: %s seed %u genre %s\n", MODE[md], gp.seed, gs.gi < NGENRE ? GENRE[gs.gi].name : "-");
                if (gs.act[RL_DRUM]) {
                    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
                    for (i = 0; i < len; i++)
                        if (t->step[i].time == ST_NOTE)
                            for (l = 0; l < 8u; l++)
                                if (((t->step[i].hit >> l) & 1u) && t->p[P_LN0 + l] > 8) { h++; k |= l == DV_KICK; }
                    hits += (double)h * 16.0 / len; kick += k;
                    if (!k && MOODS[mi] > 0) printf("  no kick: %s seed %u genre %s len %u\n", MODE[md], gp.seed, gs.gi < NGENRE ? GENRE[gs.gi].name : "-", len);
                    dec += t->p[P_E3]; cut += t->p[P_FCUT];
                }
                nb = (uint32_t)(2u * 4u * 60u * FS / (uint32_t)song.g[G_BPM] / CTL);
                song.master_q12 = 4096;
                transport_req = 1;
                for (b = 0; b < nb; b++) {
                    int32_t o[2 * CTL];
                    uint32_t p;
                    if (b % 32u == 0u) { ui_input(); song_poll(); host_ticks += 20000u; fm1_ms += 20u; }
                    fx_stems = 1;
                    mix_block(o, CTL);
                    for (i = 0; i < CTL; i++)
                        for (p = 0; p < NPART; p++) {
                            double e = (double)stem_buf[p][i] * stem_buf[p][i];
                            if (p == dr && gs.act[RL_DRUM]) ed += e; else eo += e;
                        }
                }
                fx_stems = 0;
                transport_req = 2;
                { int32_t o[2 * CTL]; ui_input(); mix_block(o, CTL); }
                if (eo > 0) { db += 10.0 * log10((ed + 1.0) / eo); ndb++; }
            }
            printf("%-8s %+5d %9.1f %6.0f %6.0f %6.0f %6.0f %12.1f\n", MODE[md], MOODS[mi], hits / 24.0, kick * 100.0 / 24.0,
                   dec / 24.0, cut / 24.0, bpm / 24.0, ndb ? db / ndb : 0.0);
        }
    grd.mood = 0;
    return 0;
}
