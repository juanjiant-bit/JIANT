// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.48, the owner: "everything lacks decay and release, it is all at sustain 100"; "the leads: decay, little sustain,
 * a lot of release"; "the chords all filtered the same: the other filter types, bands above all") a study's tool (host):
 * every genre and EXPLORE x 24 seeds x MOOD -50 / 0 / +50, LOOP form: a role's envelope (ATK DEC SUS REL), its share of
 * parts with SUS over 80 % (102 of 127), its filter types (LP HP BP COMB COMB-), then a genre a line (the mood 0's).
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src tests/gen_env48.c -lm -o build/host/gen_env48 */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
static const char *const RN[6] = {"DRUM", "BASS", "PAD", "LEAD", "ARP", "TEX"};
int main(void)
{
    static const int8_t MOOD[3] = {-50, 0, 50};
    double env[6][4] = {{0}}, ge[NGENRE + 1][6][4];
    uint32_t n[6] = {0}, hi[6] = {0}, ft[6][6] = {{0}}, gn[NGENRE + 1][6];
    uint32_t g, s, m, k, r, i;
    memset(ge, 0, sizeof ge); memset(gn, 0, sizeof gn);
    ui_power_on();
    for (g = 0; g <= NGENRE; g++)
        for (m = 0; m < 3u; m++)
            for (s = 0; s < 24u; s++) {
                gp.genre = (uint8_t)(g == NGENRE ? GEN_EXPLORE : g + 1u);
                gp.form = 0; gp.seed = (uint16_t)(300u + s * 91u);
                grd.on = 0; grd.mood = MOOD[m];
                gen_make();
                for (k = 0; k < NTRK; k++) {
                    const track_t *t = &trk[k];
                    r = gs.role[k];
                    if (k >= trk_live || r > 5u || r == RL_DRUM || !gs.act[r]) continue;
                    for (i = 0; i < 4u; i++) env[r][i] += t->p[P_ATK + i];
                    n[r]++; hi[r] += t->p[P_SUS] > 101;
                    ft[r][(uint32_t)t->p[P_FTYPE] < 5u ? (uint32_t)t->p[P_FTYPE] : 5u]++;
                    if (MOOD[m] == 0) { for (i = 0; i < 4u; i++) ge[g][r][i] += t->p[P_ATK + i]; gn[g][r]++; }
                }
            }
    printf("role   ATK DEC SUS REL  SUS>80%%   LP  HP  BP COMB COMB- (%% of the role's parts)\n");
    for (r = 1; r < 6u; r++) {
        if (!n[r]) continue;
        printf("%-5s %4.0f%4.0f%4.0f%4.0f   %5.1f%%  ", RN[r], env[r][0] / n[r], env[r][1] / n[r], env[r][2] / n[r], env[r][3] / n[r], 100.0 * hi[r] / n[r]);
        for (i = 0; i < 5u; i++) printf("%4.0f", 100.0 * ft[r][i] / n[r]);
        printf("\n");
    }
    printf("\n%-15s %-15s %-15s %-15s %-15s %-15s\n", "genre (mood 0)", "BASS a/d/s/r", "PAD", "LEAD", "ARP", "TEX");
    for (g = 0; g <= NGENRE; g++) {
        printf("%-15s", g == NGENRE ? "EXPLORE" : GENRE[g].name);
        for (r = 1; r < 6u; r++)
            if (gn[g][r]) printf(" %3.0f/%3.0f/%3.0f/%3.0f", ge[g][r][0] / gn[g][r], ge[g][r][1] / gn[g][r], ge[g][r][2] / gn[g][r], ge[g][r][3] / gn[g][r]);
            else printf(" %-15s", "-");
        printf("\n");
    }
    return 0;
}
