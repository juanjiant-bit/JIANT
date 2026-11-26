/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (studio's RADIO page) web_gen_resound / web_gen_resound_undo on the native build of the emulator's glue: a GEN
 * song and a project of the player's get new sounds, every track's steps and the stored parts' steps kept, the parts'
 * sounds the same new ones, the same seed the same sounds, UNDO the sounds back, it still sounds */
#include "felucca_web.c"
static int bad;
static void ck(const char *what, int ok) { printf("%-64s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static float peak;
static void run(uint32_t blocks)
{
    while (blocks--) {
        uint32_t i;
        web_render(512);
        for (i = 0; i < 512u; i++) if (out_l[i] > peak) peak = out_l[i]; else if (-out_l[i] > peak) peak = -out_l[i];
    }
}
static uint32_t snd_of(const track_t *t)               /* a track's sound: its engine, preset, its sound's parameters */
{
    uint32_t h = t->eng_req * 131u + t->preset, i;
    for (i = 0; i < P_COUNT; i++) if (!param_kept(i)) h = h * 31u + (uint16_t)t->p[i];
    return h;
}
static uint32_t snd_all(void) { uint32_t h = 7, k; for (k = 0; k < trk_live; k++) h = h * 1000003u + snd_of(&trk[k]); return h; }
static uint32_t steps_all(void) { uint32_t h = 7, k; for (k = 0; k < trk_live; k++) h = h * 1000003u + steps_sig(&trk[k]); return h; }
static uint32_t slot_steps(uint32_t s, uint32_t *snd)  /* a stored part's steps (and its sounds) */
{
    uint32_t h = 7, k, i;
    *snd = 0;
    if (!proj_import(&proj_scratch, &proj_slot[s], sizeof proj_slot[s])) return 0;
    for (k = 0; k < NTRK; k++) {
        for (i = 0; i < NSTEP; i++) h = h * 31u + proj_scratch.t[k].step[i].time * 7u + proj_scratch.t[k].step[i].note[0];
        *snd = *snd * 31u + proj_scratch.t[k].engine * 131u + proj_scratch.t[k].preset + (uint16_t)proj_scratch.t[k].p[P_E0];
    }
    return h;
}
int main(void)
{
    uint32_t st0, sn0, sn1, sn2, s, used = 0, kept = 0, moved = 0, ss[4], sv[4], v;
    web_nor_erase();
    web_boot();
    run(40);
    web_gen(1u + 3u, 1791, 60, 2);                       /* a SONG of the fourth genre */
    run(8);
    st0 = steps_all(); sn0 = snd_all();
    for (s = 0; s < 4u; s++) ss[s] = slot_steps(s, &sv[s]);
    ck("a GEN song: a draft, its parts stored", song_draft && ss[0]);
    ck("RESOUND 1234: done", web_gen_resound(1234) == 0);
    ck("  the steps as they were", steps_all() == st0);
    sn1 = snd_all();
    ck("  the sounds new", sn1 != sn0);
    for (s = 0; s < 4u; s++) {
        uint32_t h = slot_steps(s, &v);
        if (!ss[s]) continue;
        used++; kept += h == ss[s]; moved += v != sv[s];
    }
    printf("  parts stored %u, their steps kept %u, their sounds new %u\n", used, kept, moved);
    ck("  every stored part's steps kept, their sounds new", used && kept == used && moved == used);
    ck("  the seed reported", web_gen_resound_seed() == 1234u);
    peak = 0; web_gen_cmd(4, 1); run(200);
    ck("  it sounds (playing)", peak > 0.01f && web_gen_info()[7] == 1);
    web_gen_cmd(4, 0); run(30);
    ck("UNDO: the sounds back", web_gen_resound_undo() == 0 && snd_all() == sn0 && steps_all() == st0);
    ck("  the parts' sounds back", slot_steps(0, &v) == ss[0] && v == sv[0]);
    ck("  undo again: nothing", web_gen_resound_undo() == 1 && web_gen_resound_seed() == 0xFFFFFFFFu);
    web_gen_resound(1234);
    sn2 = snd_all();
    ck("the same seed again: the same sounds", sn2 == sn1);
    web_gen_resound_undo();
    web_gen_resound(4321);
    ck("another seed: other sounds", snd_all() != sn1 && steps_all() == st0);
    web_gen_cmd(4, 1); run(20);
    ck("RESOUND while it plays: it plays on", web_gen_resound(99) == 0 && (run(20), web_gen_info()[7] == 1));
    web_gen_cmd(4, 0); run(30);
    /* a project of the player's: the heavy song (FM6 PAD, TRIO, VOICE chords, DRUM), no draft */
    web_nor_erase();
    web_boot();
    run(20);
    web_test_heavy();
    run(4);
    st0 = steps_all(); sn0 = snd_all();
    song_draft = 0;                                      /* (the native build's statics outlive web_boot: a project loaded ends it) */
    ck("the player's song (no draft): RESOUND 77 done", !song_draft && web_gen_resound(77) == 0);
    ck("  its steps as they were, its sounds new", steps_all() == st0 && snd_all() != sn0);
    peak = 0; web_gen_cmd(4, 1); run(200);
    ck("  it sounds", peak > 0.01f);
    printf(bad ? "resound-web: %d FAILED\n" : "resound-web: all ok\n", bad);
    return bad != 0;
}
