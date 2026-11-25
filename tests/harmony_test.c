// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.66, the owner) SCL > HARMONY: a progression every melodic sequence follows. A track repeating C plays the
 * progression's roots (in its scale), chord by chord on LENGTH; SKIP's track does not move; a kit never; OFF as before;
 * the project keeps it ('H' 'M'). Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0, ok;
    project_t p;
    ui_power_on();
    trk[1].p[P_ROOT] = 0; trk[1].p[P_SCALE] = 1;      /* (C major) */
    song.g[G_HPROG] = 1 + PR_50S; song.g[G_HLEN] = 2; song.g[G_HEVO] = 0; song.g[G_HSKIP] = 0;
    gen_tick();
    ok = hm_n == 4u;
    bar_n = 0; beat_n = 0; ok &= hm_shift(&trk[1]) == 0;   /* I */
    bar_n = 1; ok &= hm_shift(&trk[1]) == 5;                /* vi */
    bar_n = 2; ok &= hm_shift(&trk[1]) == 3;                /* IV */
    bar_n = 3; beat_n = 3; ok &= hm_shift(&trk[1]) == 4;    /* V (the bar's last beat still V) */
    bar_n = 4; beat_n = 0; ok &= hm_shift(&trk[1]) == 0;    /* round */
    ok &= scale_steps(&trk[1], 60, hm_shift(&trk[1])) == 60;
    bar_n = 1; ok &= scale_steps(&trk[1], 60, hm_shift(&trk[1])) == 69;   /* (C up six scale steps: A) */
    bad += check("HARMONY 50S: I vi IV V a bar each; C follows as C A F G", ok);
    song.g[G_HLEN] = 0; bar_n = 0; beat_n = 1;
    ok = hm_shift(&trk[1]) == 5;                        /* (1/4: a chord a beat) */
    song.g[G_HSKIP] = 2;                                /* T2 skips */
    ok &= hm_shift(&trk[1]) == 0 && hm_shift(&trk[0]) == 5;
    song.g[G_HSKIP] = 0; song.g[G_HPROG] = 0;
    ok &= hm_shift(&trk[1]) == 0;
    bad += check("HARMONY: LENGTH 1/4 a chord a beat; SKIP T2 stays; OFF nothing", ok);
    song.g[G_HPROG] = 1 + PR_JAZZ2; song.g[G_HLEN] = 3; song.g[G_HEVO] = 90; song.g[G_HSKIP] = 3;
    project_capture(&p);
    ok = !project_save(1);
    song.g[G_HPROG] = 0; song.g[G_HLEN] = 2; song.g[G_HEVO] = 0; song.g[G_HSKIP] = 0;
    project_load(1); frames(4);
    ok &= song.g[G_HPROG] == 1 + PR_JAZZ2 && song.g[G_HLEN] == 3 && song.g[G_HEVO] == 90 && song.g[G_HSKIP] == 3;
    bad += check("HARMONY kept with the project", ok);
    {   /* SCL > HARMONY: its page and knobs */
        song.g[G_HPROG] = 0;
        go_title("HARMONY"); frames(32);
        turn(EN_K1, 3); frames(32);
        ok = str_eq(cur_page()->title, "HARMONY") && song.g[G_HPROG] == 3;
        bad += check("SCL > HARMONY: the page, KNOB 1 the progression", ok);
    }
    {   /* (V10.8.43, BYTE's; the owner: "GEN must not touch HARMONY") throws alternately with HARMONY off (the piece its
         * own progression, as ever) and on (its four values kept; the melodic parts on the key's tonic chord, every
         * section's too: HARMONY supplies the movement, not twice) */
        uint32_t k, i, prog = 0;
        ok = 1;
        for (k = 0; k < 96u; k++) {
            ui_power_on();
            gp.genre = (uint8_t)(1u + (k >> 1) % NGENRE); gp.seed = (uint16_t)(100u + (k >> 1)); gp.energy = 60; gp.form = (uint8_t)((k >> 1) % 3u);
            gp.complex = 50; gp.swing = GA_AUTO; gp.human = 30; gp.mix = 0;
            for (i = 0; i < GP_N; ++i) gp.amt[i] = GA_AUTO;
            song.g[G_HPROG] = k & 1u ? 3 : 0; song.g[G_HLEN] = 3; song.g[G_HEVO] = 40; song.g[G_HSKIP] = 2;
            gen_go();
            if (k & 1u) {
                ok &= song.g[G_HPROG] == 3 && song.g[G_HLEN] == 3 && song.g[G_HEVO] == 40 && song.g[G_HSKIP] == 2 &&
                      gs.pn == 1u && gs.pdeg[0] == 0u;
                if (!(gs.pn == 1u && gs.pdeg[0] == 0u)) printf("  on: %s seed %u form %u pn %u deg %u\n", gs.g->name, gp.seed, gp.form, gs.pn, gs.pdeg[0]);
            } else {
                ok &= song.g[G_HPROG] == 0;
                prog += gs.pn > 1u;
            }
        }
        if (prog < 24u) printf("  off: a progression in %u of 48\n", prog);
        bad += check("GEN leaves SCL > HARMONY alone: on, kept and the piece on the tonic chord; off, its own progression", ok && prog >= 24u);
    }
    printf(bad ? "harmony: %d FAILED\n" : "harmony: all ok\n", bad);
    return bad != 0;
}
