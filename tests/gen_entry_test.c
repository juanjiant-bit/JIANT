// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.7.66, Hollusion's ideas) GEN while it plays: the piece waits for the bar it plays to end (OCT+ mid-bar changes
 * nothing yet); KEY KEEP keeps the first melodic track's key and mode; ENTRY BUILD brings the new piece in a track a bar
 * (the kit first), the others the piece before until their bar, all of it the new piece in the end. Built and run by CI
 * (voice-user-check.yml) */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void blk(void) { static int32_t o[2 * CTL]; mix_block(o, CTL); frames(32); }   /* (transport_req: the next block) */
static void bars(uint32_t n)                            /* play on until n more bars started */
{
    static int32_t o[2 * CTL];
    uint32_t b0 = bar_n, i;
    for (i = 0; i < 200000u && bar_n - b0 < n; i++) { mix_block(o, CTL); if (!(i & 7u)) frame(); }
    frames(64);
}
static uint32_t xsig(void)
{
    uint32_t h = 2166136261u, k, i;
    for (k = 0; k < NTRK; k++) {
        h = (h ^ steps_sig(&trk[k])) * 16777619u;
        for (i = 0; i < sizeof trk[k].p; i++) h = (h ^ ((const uint8_t *)trk[k].p)[i]) * 16777619u;
    }
    for (i = 0; i < sizeof song.g; i++) h = (h ^ ((const uint8_t *)song.g)[i]) * 16777619u;
    return h;
}
int main(void)
{
    int bad = 0, ok, mid_ok = 1;
    uint32_t k, s0[NTRK], mel = 0;
    ui_power_on();
    {   /* (V10.8.40) MOOD MID: the pieces V10.8.39's, byte for byte (from power-on: the tracks' values and steps, the
         * song's, the kit; a checksum of 8 seeds in EXPLORE, RANDOM and HOUSE; V10.8.39's own, the same tool) */
        static const uint32_t MID[3] = {0xee6fe401u, 0x8fd7e736u, 0x5a231927u};   /* (V10.8.51: a genre's headroom, GTRIM; V10.8.48: the envelopes by role, the filter types; V10.8.47: JAZZ's hats; V10.8.46 recorded again: a part never still; V10.8.45: the envelopes' rules; V10.8.42 too; V10.8.41: the generator rewritten genre by genre:
         * this version's own pieces, recorded with GEN_MID=1; a change to the generator records them again) */
        uint32_t md, hs = 0, r, i;
        for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, "HOUSE")) hs = r;
        for (md = 0; md < 3u; md++) {
            uint32_t h = 2166136261u;
            for (r = 0; r < 8u; r++) {
                gp.genre = (uint8_t)(md == 0u ? GEN_EXPLORE : md == 1u ? 0u : hs);
                gp.form = 0; gp.seed = (uint16_t)(100u + r * 37u); grd.on = 0; grd.mood = 0;
                gen_make();
                for (k = 0; k < NTRK; k++)
                    for (i = 0; i < sizeof trk[k].p + sizeof trk[k].step; i++)
                        h = (h ^ (i < sizeof trk[k].p ? ((const uint8_t *)trk[k].p)[i] : ((const uint8_t *)trk[k].step)[i - sizeof trk[k].p])) * 16777619u;
                for (i = 0; i < sizeof song.g; i++) h = (h ^ ((const uint8_t *)song.g)[i]) * 16777619u;
                for (i = 0; i < sizeof dx_kit; i++) h = (h ^ ((const uint8_t *)dx_kit)[i]) * 16777619u;
            }
            if (getenv("GEN_MID")) printf("  MID[%u] 0x%08xu\n", md, h);
            mid_ok &= h == MID[md];
        }
        ui_power_on();
    }
    gp.genre = 1; gp.form = 0; gp.key = 0; gp.entry = 0;
    gen_press(); frames(64);                             /* a piece to start from, stopped: made at once */
    for (k = 0; k < NTRK; k++) s0[k] = steps_sig(&trk[k]);
    transport_req = 1; blk();
    bars(1);
    {   /* mid-bar: asked, nothing yet; the bar's end: the new piece */
        static int32_t o[2 * CTL];
        uint32_t i;
        for (i = 0; i < 40u; i++) mix_block(o, CTL);
        gen_press(); frames(64);
        ok = gen_wait && steps_sig(&trk[0]) == s0[0];
        bars(1);
        { static int32_t o2[2 * CTL]; mix_block(o2, CTL); frames(32); }   /* (it plays again from the next block) */
        for (k = 0, i = 0; k < trk_live; k++) i |= steps_sig(&trk[k]) != s0[k];
        ok &= !gen_wait && i && song.playing;
        bad += check("GEN while it plays: OCT+ waits for the bar's end, then the new piece (it plays on)", ok);
    }
    {   /* KEY KEEP */
        uint32_t r;
        transport_req = 1; blk();       /* (stop) */
        for (mel = 0; mel < trk_live && ENGINES[trk[mel].eng_req]->oneshot; mel++) {}
        trk[mel].p[P_ROOT] = 5; trk[mel].p[P_SCALE] = 1;   /* (F, a mode of seven) */
        gp.key = 1;
        for (r = 0, ok = 1; r < 4u; r++) {
            gen_press(); frames(64);
            for (k = 0; k < trk_live; k++)
                if (!ENGINES[trk[k].eng_req]->oneshot) ok &= trk[k].p[P_ROOT] % 12 == 5 && trk[k].p[P_SCALE] == 1;
        }
        gp.key = 0;
        bad += check("KEY KEEP: four new pieces, every melodic track in F and the same mode", ok);
    }
    {   /* ENTRY BUILD */
        uint32_t nb = 0, n0;
        gp.entry = 1;
        transport_req = 1; blk();       /* (play) */
        bars(1);
        for (k = 0; k < NTRK; k++) s0[k] = steps_sig(&trk[k]);
        gen_press(); frames(64);
        bars(1); blk();                                 /* (the piece made at the bar's end, playing again) */
        ok = gst_old != 0;
        for (k = 0; k < trk_live; k++)
            if (!((gst_old >> k) & 1u)) ok &= ENGINES[trk[k].eng_req]->oneshot || k == 0u;   /* (the kit first) */
            else ok &= steps_sig(&trk[k]) == s0[k];     /* (the others: the piece before) */
        n0 = (uint32_t)__builtin_popcount(gst_old);
        while (gst_old && nb < 12u) { bars(1); nb++; }
        ok &= !gst_old && nb == n0 && song.playing;
        gp.entry = 0;
        bad += check("ENTRY BUILD: the kit first, a track a bar, the others the piece before until theirs", ok);
        if (!ok) printf("  n0 %u nb %u old %x\n", n0, nb, gst_old);
    }
    {   /* (V10.7.66, the owner: "combinations of two seeds") a piece two seeds crossed: named A+B, not A's alone nor B's,
         * the same three again the same piece */
#define XSIG xsig()   /* (V10.8.41: every track's steps and values and the song's: a mix can fall on parts that write no step) */
        uint32_t ha, hb, hx, hy;
        uint16_t a, b2, mx;
        transport_req = 2; blk();                       /* (stop) */
        gp.entry = 0; gp.key = 0; gp.sig = 0;
        gen_press(); frames(64);
        a = gp.seed; b2 = gp.seed2; mx = gp.mix;
        ok = mx != 0u && str_len(gp.last) > 6u && gp.last[str_len(gp.last) - 5u] == '+';
        gen_make();                                     /* (the piece as made, its CPU fit aside) */
        hx = XSIG;
        gp.mix = 0; gp.seed = a; gen_make(); ha = XSIG;
        gp.seed = b2; gen_make(); hb = XSIG;
        gp.seed = a; gp.seed2 = b2; gp.mix = mx; gen_make(); hy = XSIG;
        ok &= hx != ha && hx != hb && hx == hy;
        ok &= (a > b2 ? a - b2 : b2 - a) >= 1500u;      /* (V10.7.67: the two far apart) */
        if (!ok) printf("  cross: %s mix %x s2 %u  %x %x %x %x\n", gp.last, mx, b2, hx, ha, hb, hy);
        bad += check("GEN: two seeds crossed (A+B, neither alone, the same three the same piece)", ok);
    }
    {   /* (V10.8.39, the owner) RADIO: a loop's 32 bars (over 110 BPM) played, the next piece made on the bar line and
         * played on, and again; off: the piece stays. MOOD: RANDOM's genres slower CALM than DANCE, the middle the piece
         * as ever; a DANCE radio's tempo the nearest its genre has to the piece before */
        uint32_t h0, r, n, sc = 0, sd = 0;
        uint16_t sd0;
        gp.genre = 1; gp.form = 0; gp.entry = 0; gp.key = 0; gp.sig = 0; gp.mix = 0;
        gen_press(); frames(64);
        h0 = dice_print(); sd0 = gp.seed;
        transport_req = 1; blk();
        bars(40);
        ok = song.playing && dice_print() == h0 && !grd.rn;       /* (off: nothing) */
        transport_req = 2; blk();
        grd.on = 1;
        transport_req = 1; blk();
        bars(14);                                        /* (V10.8.45, the owner: half: 16 bars over 110 BPM, were 32) */
        ok &= song.playing && dice_print() == h0 && !grd.rn;      /* (its 15th bar: still the piece) */
        bars(3); blk();
        ok &= song.playing && grd.rn == 1u && gp.seed != sd0 && dice_print() != h0;
        h0 = dice_print();
        for (n = 0; n < 40u && grd.rn < 2u; n++) bars(1);
        blk();
        ok &= song.playing && grd.rn == 2u && n >= 15u && dice_print() != h0;
        if (!ok) printf("  radio: playing %u rn %u seed %u (was %u)\n", (unsigned)song.playing, grd.rn, gp.seed, sd0);
        bad += check("RADIO: off nothing; on, the next piece after the loop's 16 bars, playing on, twice", ok);
        {   /* (V10.8.43, BYTE's) a STOP right on the piece's last bar stops: no next piece made, nothing playing (it
             * made the next piece and played it) */
            uint32_t rn;
            for (n = 0; n < 40u && (bar_n & 15u) != 15u; n++) bars(1);
            frames(8);
            h0 = dice_print(); rn = grd.rn; sd0 = gp.seed;
            ok = song.playing && grd.arm;
            transport_req = 2; blk(); frames(200);
            ok &= !song.playing && !grd.arm && grd.rn == rn && gp.seed == sd0 && dice_print() == h0;
            frames(500);
            ok &= !song.playing && dice_print() == h0;
            if (!ok) printf("  radio stop: playing %u arm %u rn %u (%u) bar %u\n", (unsigned)song.playing, grd.arm, grd.rn, rn, (unsigned)bar_n);
            bad += check("RADIO: a STOP on the loop's last bar stops (no next piece, nothing playing)", ok);
        }
        gp.form = 2; gp.sig = 0;                         /* (a song: its END the next piece's cue) */
        transport_req = 2; blk();
        gen_press(); frames(64);
        ok = chain_config.count > 0u && !grd.rn;
        h0 = dice_print();
        if (live.mode) chain_play_ui(); else transport_req = 1;
        blk();
        for (n = 0; n < 400u && !grd.rn; n++) { bars(1); song_poll(); }   /* (the next row staged: the main loop's) */
        blk();
        ok &= grd.rn == 1u && song.playing && dice_print() != h0;
        if (!ok) printf("  radio song: rows %u bars waited %u playing %u rn %u\n", chain_config.count, n, (unsigned)song.playing, grd.rn);
        bad += check("RADIO: a song's last row ended, the next song made and playing", ok);
        {   /* (V10.8.43) the song's last bar, STOP: the song stops, no next song (its own END alone goes on: chain.fin) */
            uint32_t rn;
            for (n = 0; n < 400u && !(chain.running && chain.row + 1u >= chain.config.count && chain.remaining <= 1u); n++) { bars(1); song_poll(); }
            frames(64);
            sd0 = gp.seed; rn = grd.rn;
            ok = song.playing && grd.arm;
            transport_req = 2; blk(); song_poll(); frames(500); song_poll(); frames(64);
            ok &= !song.playing && !grd.arm && grd.rn == rn && gp.seed == sd0;
            if (!ok) printf("  song stop: playing %u arm %u rn %u (%u) row %u/%u rem %u fin %u\n", (unsigned)song.playing, grd.arm, grd.rn, rn, chain.row, chain.config.count, chain.remaining, chain.fin);
            bad += check("RADIO: a STOP on a song's last bar stops (no next song)", ok);
        }
        transport_req = 2; blk();
        grd.on = 0; gp.form = 0; gp.genre = 0;
        for (r = 0; r < 24u; r++) {
            gp.seed = (uint16_t)(100u + r * 37u);
            grd.mood = -50; gen_make(); sc += (uint32_t)song.g[G_BPM];
            grd.mood = 50; gen_make(); sd += (uint32_t)song.g[G_BPM];
        }
        grd.mood = 0;
        ok = sd > sc + 24u * 20u;
        printf("  MOOD: RANDOM's mean tempo CALM %u, DANCE %u BPM\n", sc / 24u, sd / 24u);
        bad += check("MOOD: RANDOM's pieces over 20 BPM slower CALM than DANCE", ok);
        grd.on = 1; grd.mood = 50; gp.genre = 0;
        for (r = 0, ok = 1; r < 12u; r++) {
            int32_t b0 = song.g[G_BPM];
            gp.seed = (uint16_t)(900u + r * 53u);
            grd.rn = 1;
            gen_make();
            ok &= song.g[G_BPM] == clamp(b0, gs.g->bpm_lo, gs.g->bpm_hi);
        }
        grd.on = 0; grd.mood = 0; grd.rn = 0;
        bad += check("MOOD DANCE on the radio: each piece's tempo the nearest its genre has to the one before", ok);
        {   /* (V10.8.40, the owner: "CALM changes nothing audible, everything still has drums") MOOD in every GENRE mode
             * (EXPLORE, RANDOM, HOUSE), 24 seeds each (MID: main's first lines). CALM -50: no sound of the kit but the closed
             * hat left (every other lane's level 0), that one down, the kit shorter and its filter closing; -25: the
             * kick and the rest under MID's, still there. DANCE +50: every piece a kick that sounds, on every beat, its
             * level whole, the drum track over MID's; +25: on the bar's first and third beats */
            uint32_t md, hs = 0, i, l, okc = 1, okh = 1, okd = 1, okq = 1;
            for (r = 1; r <= NGENRE; r++) if (str_eq(GENRE[r - 1u].name, "HOUSE")) hs = r;
            for (md = 0; md < 3u; md++) {
                for (r = 0; r < 24u; r++) {
                    track_t *t;
                    int16_t lv0, e3, fc, ln[8];
                    uint32_t len, kb = 0, k2 = 0, k1 = 0;
                    gp.genre = (uint8_t)(md == 0u ? GEN_EXPLORE : md == 1u ? 0u : hs);
                    gp.form = 0; gp.seed = (uint16_t)(100u + r * 37u); grd.on = 0;
                    grd.mood = 0; gen_make();
                    t = &trk[gs.trk_of[RL_DRUM]];
                    lv0 = t->p[P_LEVEL]; e3 = t->p[P_E3]; fc = t->p[P_FCUT];
                    for (l = 0; l < 8u; l++) ln[l] = t->p[P_LN0 + l];
                    if (gs.act[RL_DRUM]) {
                        grd.mood = -25; gen_make(); t = &trk[gs.trk_of[RL_DRUM]];
                        okq &= !gs.act[RL_DRUM] || (t->p[P_LN0] < ln[0] && t->p[P_LN0] > 0 && (md == 1u || (t->p[P_LN1] < ln[1] && t->p[P_E3] < e3 && t->p[P_FCUT] < fc)));   /* (V10.8.41: RANDOM's
                                                         * genre moves with the mood: its kick's level alone compared) */
                        grd.mood = -50; gen_make(); t = &trk[gs.trk_of[RL_DRUM]];
                        if (gs.act[RL_DRUM]) {
                            for (l = 0; l < 8u; l++) okc &= l == DV_HATC ? t->p[P_LN0 + l] <= 80 : t->p[P_LN0 + l] == 0;
                            okc &= t->p[P_E3] < 40 && t->p[P_FCUT] <= 80 && t->p[P_LEVEL] < 100;
                        }
                    }
                    grd.mood = 50; gen_make(); t = &trk[gs.trk_of[RL_DRUM]];
                    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
                    for (i = 0, k1 = 0; i < len; i += 4u) { kb += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); if (!(i & 15u)) k1 += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); }
                    okd &= gs.act[RL_DRUM] && (ghalf() ? k1 == (len + 15u) / 16u : kb == (len + 3u) / 4u) && t->p[P_LN0] == 127 && (md == 1u || t->p[P_LEVEL] > lv0);
                    grd.mood = 25; gen_make(); t = &trk[gs.trk_of[RL_DRUM]];
                    len = (uint32_t)clamp(t->p[P_SLEN], 1, NSTEP);
                    for (i = 0, k1 = 0; i < len; i += 8u) { k2 += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); if (!(i & 15u)) k1 += t->step[i].time == ST_NOTE && (t->step[i].hit & 1u); }
                    okh &= gs.act[RL_DRUM] && (ghalf() ? k1 == (len + 15u) / 16u : k2 == (len + 7u) / 8u);
                }
            }
            grd.mood = 0; gp.genre = 0;
            bad += check("MOOD MID: EXPLORE's, RANDOM's and HOUSE's pieces the recorded ones byte for byte", mid_ok);
            bad += check("MOOD CALM -25: the kick, the snare, the kit's decay and filter under MID's, still there", okq);
            bad += check("MOOD CALM -50: every piece's kit silent but a low, short, dark closed hat (EXPLORE, RANDOM, HOUSE)", okc);
            bad += check("MOOD DANCE +25: every piece its drums, the kick on the bar's first and third beats (a half-time's, a break's: its one)", okh);
            bad += check("MOOD DANCE +50: a kick on every beat (a half-time's, a break's: the bar's one), its level whole, the drums over MID's", okd);
        }
    }
    printf(bad ? "gen-entry: %d FAILED\n" : "gen-entry: all ok\n", bad);
    return bad != 0;
}
