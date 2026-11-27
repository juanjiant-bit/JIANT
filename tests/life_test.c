// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.62) LIFE (mod.c life_block, HOME > LIFE): all 0 runs nothing (no route, no follower asked); each knob moves the
 * tracks by the others' sound, audibly (PUMP ducks, FILTER and TIMBRE change the sound, FX sends more to the buses); a
 * project keeps it ('L' 'F') and one without it writes no block. Built and run by CI (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static double bright, side;                     /* the last render's: its slope's energy, its L - R's (over its own) */
static double rms_song(uint32_t blocks, double *diff, const int32_t *ref)
{
    static int32_t o[2 * CTL];
    double e = 0, d = 0, sd = 0, pl = 0;
    uint32_t b, i;
    (void)diff; (void)ref;
    for (b = 0; b < blocks; b++) {
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++) {
            double l = o[2u * i], r = o[2u * i + 1u];
            e += l * l + r * r;
            d += (l - pl) * (l - pl);
            sd += (l - r) * (l - r);
            pl = l;
        }
    }
    bright = e > 0 ? d / e : 0;
    side = e > 0 ? sd / e : 0;
    return e;
}
static int32_t o_sil[2 * CTL];
static void duet(void)                          /* T2 and T3 a held note each, the others silent */
{
    uint32_t k;
    ui_power_on();
    for (k = 0; k < NTRK; k++) if (k != 1u && k != 2u) trk[k].p[P_MUTE] = 1;
    song.master_q12 = 2048;
    for (k = 0; k < 3000u; k++) mix_block(o_sil, CTL);   /* (the buses' tails out, the same seed: each run alike) */
    rng_state = 0x1234567u;
    trk_note_on(&trk[1], 36, 120);
    trk_note_on(&trk[2], 48, 100);
}
static void kit_duet(void)                      /* T1 a kit (a kick each 1/4 s), T3 a held note, the others silent */
{
    uint32_t k;
    ui_power_on();
    set_engine_of(&trk[0], ENGI_DRUM); trk[0].engine = trk[0].eng_req;
    for (k = 0; k < NTRK; k++) if (k != 0u && k != 2u) trk[k].p[P_MUTE] = 1;
    song.master_q12 = 2048;
    for (k = 0; k < 3000u; k++) mix_block(o_sil, CTL);
    rng_state = 0x1234567u;
    trk_note_on(&trk[2], 48, 100);
}
static double kick_song(uint32_t blocks, double *kit)   /* T3's follower summed (T1's in *kit) */
{
    static int32_t o[2 * CTL];
    double e = 0, k = 0;
    uint32_t b;
    for (b = 0; b < blocks; b++) {
        if (b % 375u == 0u) trk_note_on(&trk[0], 36, 127);
        if (b % 375u == 40u) trk_note_off(&trk[0], 36);
        mix_block(o, CTL);
        e += track_env[2];
        k += track_env[0];
    }
    *kit = k;
    return e;
}
static double kit_energy(int g)                  /* (V10.7.63) a kick on a kit alone at GAIN g */
{
    static int32_t o[2 * CTL];
    double e = 0;
    uint32_t b, j;
    ui_power_on();
    set_engine_of(&trk[0], ENGI_DRUM); trk[0].engine = trk[0].eng_req;
    for (b = 1; b < NTRK; b++) trk[b].p[P_MUTE] = 1;
    dx_gain = (int16_t)g; frames(64); song.master_q12 = 2048;
    trk_note_on(&trk[0], 36, 100);
    for (b = 0; b < 600u; b++) {
        mix_block(o, CTL);
        for (j = 0; j < 2u * CTL; j++) e += (double)o[j] * o[j];
    }
    return e;
}
int main(void)
{
    int bad = 0, ok, q;
    double e0, e1;
    static const char *const NM[4] = {"FILTER", "TIMBRE", "STEREO", "FX"};
    ok = !life_on && !life_v[0] && !life_v[1] && !life_v[2] && !life_v[3];
    bad += check("LIFE: off at power-on (nothing runs)", ok);
    {   /* (the yardstick: the voices' free phases move a render a few %: four runs, their spread) */
        double ea, br0 = 0, sd0 = 0, lo = 1e30, hi = 0;
        e0 = 0;
        for (q = 0; q < 4; q++) {
            duet();
            ea = rms_song(600, 0, 0);
            e0 += ea / 4.0; br0 += bright / 4.0; sd0 += side / 4.0;
            lo = ea < lo ? ea : lo; hi = ea > hi ? ea : hi;
        }
        printf("  off: energy %.1f%% run to run, slope %.4f, side %.4f\n", 100.0 * (hi - lo) / e0, br0, sd0);
        bad += check("LIFE: off, no follower asked for it", !life_on && !(track_follow_mask & 6u));
        for (q = 0; q < 4; q++) {
            char what[120];
            double r;
            {
                double br1 = 0, sd1 = 0, ex;
                int u;
                e1 = 0;
                for (u = 0; u < 4; u++) {               /* (four runs, as the yardstick's) */
                    duet();
                    life_v[q] = 100;
                    ex = rms_song(600, 0, 0);
                    e1 += ex / 4.0; br1 += bright / 4.0; sd1 += side / 4.0;
                }
                r = q == 0 ? br1 / br0 : q >= 2 ? sd1 / sd0 : e1 / e0;   /* (V10.8.64: STEREO, the side too) */
            }   /* (FILTER: brighter; FX: wider; TIMBRE: its
                                                         * shape, the energy) */
            snprintf(what, sizeof what, "LIFE %s 100: the tracks move each other (%s x%.2f)", NM[q],
                     q == 0 ? "slope" : q >= 2 ? "side" : "energy", r);
            bad += check(what, life_on && fabs(r - 1.0) > 0.1);
            life_v[q] = 0;
        }
    }
    /* (V10.8.64) PUMP gone: STEREO (above, the side wider) */
    {   /* (V10.7.63) SCENE led by a track: T1's sequence (C4, then F4 at step 8) plays T2's CHORD (its chord follows each
         * note); the keys stay T1's own; a stop lets T2's notes go */
        uint32_t i, k, c, got0 = 0, got1 = 0;
        ui_power_on();
        for (i = 0; i < NTRK; i++) {
            track_t *t = &trk[i];
            track_defaults_steps(t);
            t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_SSWING] = 0; t->p[P_SGATE] = 50;
            if (i > 1u) t->p[P_MUTE] = 1;
            for (k = 0; k < 16u; k++) t->step[k] = (step_t){{0}, 0, ST_REST, 0, 100};
        }
        for (i = 0; i < 2u; i++) { set_engine_of(&trk[i], ENGI_ANALOG); trk[i].engine = trk[i].eng_req; trk[i].p[P_SCALE] = 1; trk[i].p[P_ROOT] = 0; }
        trk[0].step[0] = (step_t){{60u}, 1, ST_NOTE, 0, 100};
        trk[0].step[8] = (step_t){{65u}, 1, ST_NOTE, 0, 100};
        scn_role[0] = SC_KEEP; scn_role[1] = SC_CHORD; trk[1].p[P_AMODE] = 0; trk[1].p[P_VOICE] = V_POLY;
        scn_on = 2;                                     /* (T1 leads) */
        song.g[G_BPM] = 120; song.g[G_SWING] = 0;
        seq_start(); song.playing = 1;
        for (c = 0; c < 66150u; c += CTL) {             /* (1.5 s at 120: step 8 at 1 s, both notes) */
            events_block(CTL);
            for (i = 0; i < NVOICE; i++)
                if (trk[1].v[i].active && trk[1].v[i].stage < 3u) {
                    got0 |= trk[1].v[i].note == 60u || trk[1].v[i].note == 64u || trk[1].v[i].note == 67u;
                    got1 |= trk[1].v[i].note == 65u || trk[1].v[i].note == 69u || trk[1].v[i].note == 72u;
                }
        }
        seq_stop(); song.playing = 0;
        events_block(CTL);
        for (i = 0, k = 0; i < NVOICE; i++) k += trk[1].v[i].active && trk[1].v[i].stage < 3u;
        ok = got0 && got1 && !k && !scn_fn[1][0];
        if (!ok) printf("  scene follow: C %u F %u held after stop %u\n", got0, got1, k);
        bad += check("SCENE led by T1's sequence: T2's CHORD follows its notes (C, then F); a stop lets it go", ok);
        scn_on = 0; memset(scn_role, 0, sizeof scn_role);
    }
    {   /* (V10.7.63) the chord SETS (SCL > CHORD past POWER): each degree its colour, in the key (C major) */
        track_t *t = &trk[0];
        uint8_t o[KB_CH + 1];
        uint32_t n1, n2, n3;
        ui_power_on();
        set_engine_of(t, ENGI_ANALOG); t->engine = t->eng_req; t->p[P_VOICE] = V_POLY; t->p[P_SCALE] = 1; t->p[P_ROOT] = 0;
        t->p[P_CVOI] = 0; chord_nlast[0] = 0;
        t->p[P_CHRD] = CD_SET0;                         /* POP */
        n1 = chord_build(t, 60, o);
        ok = n1 == 4u && o[0] == 60 && o[1] == 64 && o[2] == 67 && o[3] == 74;   /* I: ADD9 */
        n2 = chord_build(t, 62, o);
        ok &= n2 == 4u && o[0] == 62 && o[1] == 65 && o[2] == 69 && o[3] == 72;  /* ii: 7TH */
        t->p[P_CHRD] = CD_SET0 + 1;                     /* JAZZ */
        n3 = chord_build(t, 67, o);
        ok &= n3 == 4u && o[0] == 67 && o[1] == 71 && o[2] == 77 && o[3] == 81;  /* V: 9TH (no fifth) */
        t->p[P_CHRD] = CD_SET0 + 4;                     /* QUART */
        ok &= chord_build(t, 64, o) == 3u && o[0] == 64 && o[1] == 69 && o[2] == 74;   /* iii: fourths */
        bad += check("CHORD SETS: POP I add9, ii 7th; JAZZ V 9th; QUART fourths, all in the key", ok);
        t->p[P_CHRD] = 0;
    }
    {   /* (V10.7.63) a kit's GAIN (KIT 2 KNOB 4): the kit louder (its DRIVE off the panel, still playing), kept with the
         * project ('K' 'G') */
        project_t p;
        double e0g, e1g;
        (void)kit_energy(0);                            /* (V10.8.64: the kit once first, as PUMP's kit test left it) */
        e0g = kit_energy(0); e1g = kit_energy(12);
        ok = e1g > e0g * 3.0;                           /* (+12 dB: x16 in power, the knee taking its top) */
        dx_gain = 7;
        project_capture(&p);
        ok &= p.dx_gain == 7u && !project_save(1);
        dx_gain = 0;
        project_load(1); frames(4);
        ok &= dx_gain == 7;
        if (!ok) printf("  gain: energy %g -> %g, loaded %d\n", e0g, e1g, dx_gain);
        bad += check("DRUM-X GAIN: +12 dB the kit louder; saved with the project and back", ok);
        dx_gain = 0;
    }
    {   /* (V10.7.64, the owner: GAIN in the engines, DIST the SENDS' only) a track's GAIN: louder on its level, kept
         * ('T' 'G'); the pages: FILTER TYPE CUT RES IMAGE, PLAY POLY GLIDE UNISON VEL, ANALOG's MORE its GAIN */
        project_t p;
        static int32_t o[2 * CTL];
        double e[2] = {0, 0};
        uint32_t b, j, r, fi = NPAGES, pi = NPAGES;
        for (r = 0; r < 2u; r++) {
            ui_power_on();
            for (b = 1; b < NTRK; b++) trk[b].p[P_MUTE] = 1;
            trk_gain[0] = (int16_t)(r ? 12 : 0); frames(64); song.master_q12 = 1024;
            trk_note_on(&trk[0], 60, 100);
            for (b = 0; b < 400u; b++) { mix_block(o, CTL); for (j = 0; j < 2u * CTL; j++) e[r] += (double)o[j] * o[j]; }
            trk_note_off(&trk[0], 60);
        }
        ok = e[1] > e[0] * 4.0;
        trk_gain[0] = 0; trk_gain[2] = 9;
        project_capture(&p);
        ok &= p.tgain[2] == 9u && !project_save(1);
        trk_gain[2] = 0;
        project_load(1); frames(4);
        ok &= trk_gain[2] == 9 && !trk_gain[0];
        trk_gain[2] = 0;
        for (j = 0; j < NPAGES; j++) {
            if (str_eq(PAGES[j].title, "FILTER") && PAGES[j].id[1] == P_FCUT) fi = j;
            if (str_eq(PAGES[j].title, "PLAY")) pi = j;
        }
        ok &= fi < NPAGES && PAGES[fi].id[3] == P_SIMG && pi < NPAGES && PAGES[pi].id[2] == P_DETUNE && PAGES[pi].id[3] == P_TVEL;
        if (!ok) printf("  track gain: energy %g -> %g; tg %u / %d %d; fi %u pi %u\n", e[0], e[1], p.tgain[2], trk_gain[2], trk_gain[0], fi, pi);
        bad += check("GAIN in the engines: a track +12 dB louder, saved and back; FILTER's IMAGE, PLAY's UNISON VEL", ok);
    }
    {   /* a project: LIFE off writes no block; on, 'L' 'F' kept and back after a load */
        uint32_t n0, n1;
        project_t p;
        ui_power_on();
        project_capture(&p);
        n0 = proj_packed_size(&p);
        life_v[0] = 40; life_v[2] = 77;
        project_capture(&p);
        n1 = proj_packed_size(&p);
        ok = n1 == n0 + 2u + sizeof p.lf && !project_save(1);
        if (!ok) printf("  sizes %u %u\n", n0, n1);
        life_v[0] = life_v[2] = 0;
        project_load(1); frames(4);
        ok &= life_v[0] == 40 && life_v[1] == 0 && life_v[2] == 77 && life_v[3] == 0;
        bad += check("LIFE: saved with the project and loaded back; off writes no block", ok);
    }
    printf(bad ? "life: %d FAILED\n" : "life: all ok\n", bad);
    return bad != 0;
}
