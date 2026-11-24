/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The generator's sounds and mix audited (docs/GENERATOR-STUDY.es.md): N seeds a genre, each role's engines and the
 * parameters GEN leaves on its track (means), the tempo and the swing.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_audit tests/gen_audit.c -lm
 *   build/host/gen_audit [SEEDS [GENRE]] */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static const char *RLN[6] = {"DRUM", "BASS", "PAD", "LEAD", "ARP", "TEX"};
int main(int argc, char **argv)
{
    static const int ID[] = {P_LEVEL, P_PAN, P_FCUT, P_FRES, P_ATK, P_DEC, P_SUS, P_REL, P_REV, P_DLY, P_DIST, P_CHOR, P_GLIDE, P_DETUNE, P_E4};
    static const char *IN[] = {"LEVEL", "PAN", "FCUT", "FRES", "ATK", "DEC", "SUS", "REL", "REV", "DLY", "DIST", "CHOR", "GLIDE", "DETUN", "E5"};
    uint32_t seeds = argc > 1 ? (uint32_t)atoi(argv[1]) : 16, only = argc > 2 ? (uint32_t)atoi(argv[2]) : 0, g, s, k, r, i;
    ui_power_on(); go_home(); frames(4);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    for (g = only ? only : 1; g <= (only ? only : NGENRE + 1u); g++) {
        double sum[6][16]; uint32_t n[6], eng[6][NENGINES], bl = 999, bh = 0; int sw = 0;
        memset(sum, 0, sizeof sum); memset(n, 0, sizeof n); memset(eng, 0, sizeof eng);
        for (s = 1; s <= seeds; s++) {
            gp.genre = (uint8_t)g; gp.seed = (uint16_t)(s * 37u); gp.energy = 50; gp.form = 0;
            for (k = 0; k < GP_N; k++) gp.amt[k] = 60;   /* (every part on: its sound heard) */
            gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30;
            gen_go();
            if ((uint32_t)song.g[G_BPM] < bl) bl = (uint32_t)song.g[G_BPM];
            if ((uint32_t)song.g[G_BPM] > bh) bh = (uint32_t)song.g[G_BPM];
            sw = song.g[G_SWING];
            for (k = 0; k < NTRK; k++) {
                r = gs.role[k];
                if (!gs.act[r] || (r == RL_TEX && k != gs.trk_of[RL_TEX])) continue;
                n[r]++; eng[r][trk[k].eng_req % NENGINES]++;
                for (i = 0; i < 15u; i++) sum[r][i] += trk[k].p[ID[i]];
            }
        }
        printf("\n### %s  BPM %u..%u  swing %d  master: REV type %d  DLY time %d\n", g <= NGENRE ? GENRE[g - 1].name : "EXPLORE", bl, bh, sw, song.g[G_RTYPE], song.g[G_DTIME]);
        printf("%-5s %-22s", "role", "engines"); for (i = 0; i < 15u; i++) printf(" %5s", IN[i]); printf("\n");
        for (r = 0; r < 6u; r++) {
            char eb[64] = ""; if (!n[r]) continue;
            for (k = 0; k < NENGINES; k++) if (eng[r][k]) { char t[24]; sprintf(t, "%s%u ", ENGINES[k]->name, eng[r][k]); if (strlen(eb) + strlen(t) < 60) strcat(eb, t); }
            printf("%-5s %-22s", RLN[r], eb);
            for (i = 0; i < 15u; i++) printf(" %5.0f", sum[r][i] / n[r]);
            printf("\n");
        }
    }
    return 0;
}
