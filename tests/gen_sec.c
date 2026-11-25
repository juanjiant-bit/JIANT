// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.45, the owner: "a section lasts more than twice what it should; loops that do not evolve and come back the
 * same; envelopes") a song's rows measured, a genre a line (SONG, 4 seeds): the rows' seconds (mean, the longest), the
 * rows that are a row before them again (section, mutes, sources, variations, the scene's macros), the automation
 * records a piece; then the envelopes a role (ATK DEC SUS REL, the envelope's share of the filter), every genre.
 * A study's tool (host): cc -O1 -w -Ibuild/gen -Ifirmware/src tests/gen_sec.c -lm -o build/host/gen_sec */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    static const char *const RN[6] = {"DRUM", "BASS", "PAD", "LEAD", "ARP", "TEX"};
    double env[6][5] = {{0}}, tsec = 0, tmax = 0, tsame = 0, trow = 0, tmot = 0;
    uint32_t envn[6] = {0}, g, r, k;
    ui_power_on();
    printf("%-16s %5s %7s %7s %6s %7s\n", "genre", "BPM", "row s", "max s", "same%", "motion");
    for (g = 0; g <= NGENRE; g++) {
        double sec = 0, mx = 0, mot = 0, bpm = 0; uint32_t rows = 0, same = 0;
        for (r = 0; r < 4u; r++) {
            uint32_t i, j;
            gp.genre = (uint8_t)(g == NGENRE ? GEN_EXPLORE : g + 1u);
            gp.form = 2; gp.seed = (uint16_t)(300u + r * 91u);
            grd.on = 0; grd.mood = 0;
            gen_make();
            bpm += song.g[G_BPM];
            mot += motion.count;
            for (i = 0; i < chain_config.count; i++) {
                double s = chain_config.row[i].bars * 240.0 / song.g[G_BPM];
                const scene_t *a = &song_idx.scene[song_cur][i];
                sec += s; rows++; if (s > mx) mx = s;
                for (j = 0; j < i; j++) {
                    const scene_t *b = &song_idx.scene[song_cur][j];
                    if (chain_config.row[j].slot == chain_config.row[i].slot && a->mute == b->mute && a->on == b->on &&
                        song_idx.tsrc[song_cur][i] == song_idx.tsrc[song_cur][j] && song_idx.tvar[song_cur][i] == song_idx.tvar[song_cur][j] &&
                        !memcmp(a->mac, b->mac, sizeof a->mac)) { same++; break; }
                }
            }
            for (k = 0; k < NTRK; k++) {
                uint32_t role = gs.role[k];
                if (k >= trk_live || role > 5u || !gs.act[role]) continue;
                env[role][0] += trk[k].p[P_ATK]; env[role][1] += trk[k].p[P_DEC]; env[role][2] += trk[k].p[P_SUS];
                env[role][3] += trk[k].p[P_REL]; env[role][4] += trk[k].p[P_ED_FLT]; envn[role]++;
            }
        }
        printf("%-16s %5.0f %7.1f %7.1f %6.0f %7.1f\n", g == NGENRE ? "EXPLORE" : GENRE[g].name, bpm / 4, rows ? sec / rows : 0, mx,
               rows ? 100.0 * same / rows : 0, mot / 4);
        tsec += sec; trow += rows; tsame += same; tmot += mot; if (mx > tmax) tmax = mx;
    }
    printf("%-16s %5s %7.1f %7.1f %6.0f %7.1f\n\n", "ALL", "", tsec / trow, tmax, 100.0 * tsame / trow, tmot / 4 / (NGENRE + 1));
    printf("%-6s %5s %5s %5s %5s %7s\n", "role", "ATK", "DEC", "SUS", "REL", "ENV>FLT");
    for (r = 0; r < 6u; r++) if (envn[r]) printf("%-6s %5.0f %5.0f %5.0f %5.0f %7.0f\n", RN[r], env[r][0] / envn[r], env[r][1] / envn[r], env[r][2] / envn[r], env[r][3] / envn[r], env[r][4] / envn[r]);
    return 0;
}
