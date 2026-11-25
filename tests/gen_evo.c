// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.46, the owner: "every part its timbre and modulations, parameters that evolve; the envelopes") a study's tool
 * (host): a genre a line, LOOP form, 6 seeds: the movers a melodic part (its LFO's destinations with an amount, its AUTO
 * PAN, its envelope on the filter or the shape), the parts with none, then the envelopes a role (ATK DEC SUS REL, the
 * envelope's share of the filter).
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src tests/gen_evo.c -lm -o build/host/gen_evo */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
int main(void)
{
    uint32_t g, r, k;
    ui_power_on();
    printf("%-15s %4s %4s | %-15s %-15s %-15s %-15s\n", "genre", "mov", "none", "BASS a/d/s/r>f", "PAD", "LEAD", "ARP");
    for (g = 0; g <= NGENRE; g++) {
        double mv = 0, env[6][5] = {{0}}; uint32_t n = 0, none = 0, en[6] = {0};
        for (r = 0; r < 6u; r++) {
            gp.genre = (uint8_t)(g == NGENRE ? GEN_EXPLORE : g + 1u);
            gp.form = 0; gp.seed = (uint16_t)(300u + r * 91u);
            grd.on = 0; grd.mood = 0;
            gen_make();
            for (k = 0; k < NTRK; k++) {
                const track_t *t = &trk[k];
                uint32_t role = gs.role[k], m;
                if (k >= trk_live || role > 5u || role == RL_DRUM || !gs.act[role]) continue;
                m = (t->p[P_LD_FLT] != 0) + (t->p[P_LD_SHP] != 0) + (t->p[P_LD_AMP] != 0) + (t->p[P_LD_PIT] != 0) + (t->p[P_APDEP] != 0);
                mv += m; n++; none += !m;
                env[role][0] += t->p[P_ATK]; env[role][1] += t->p[P_DEC]; env[role][2] += t->p[P_SUS]; env[role][3] += t->p[P_REL];
                env[role][4] += t->p[P_ED_FLT]; en[role]++;
            }
        }
        printf("%-15s %4.1f %3.0f%% |", g == NGENRE ? "EXPLORE" : GENRE[g].name, n ? mv / n : 0, n ? 100.0 * none / n : 0);
        for (r = 1; r < 5u; r++)
            if (en[r]) printf(" %2.0f/%2.0f/%3.0f/%3.0f>%2.0f", env[r][0] / en[r], env[r][1] / en[r], env[r][2] / en[r], env[r][3] / en[r], env[r][4] / en[r]);
            else printf(" %-15s", "-");
        printf("\n");
    }
    return 0;
}
