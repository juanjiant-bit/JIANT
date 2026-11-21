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
    printf(bad ? "harmony: %d FAILED\n" : "harmony: all ok\n", bad);
    return bad != 0;
}
