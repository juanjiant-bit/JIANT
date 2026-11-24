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
int main(void)
{
    int bad = 0, ok;
    uint32_t k, s0[NTRK], mel = 0;
    ui_power_on();
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
        uint32_t ha, hb, hx, hy;
        uint16_t a, b2, mx;
        transport_req = 2; blk();                       /* (stop) */
        gp.entry = 0; gp.key = 0; gp.sig = 0;
        gen_press(); frames(64);
        a = gp.seed; b2 = gp.seed2; mx = gp.mix;
        ok = mx != 0u && str_len(gp.last) > 6u && gp.last[str_len(gp.last) - 5u] == '+';
        gen_make();                                     /* (the piece as made, its CPU fit aside) */
        hx = steps_sig(&trk[0]) ^ steps_sig(&trk[1]) * 3u ^ steps_sig(&trk[2]) * 7u;
        gp.mix = 0; gp.seed = a; gen_make(); ha = steps_sig(&trk[0]) ^ steps_sig(&trk[1]) * 3u ^ steps_sig(&trk[2]) * 7u;
        gp.seed = b2; gen_make(); hb = steps_sig(&trk[0]) ^ steps_sig(&trk[1]) * 3u ^ steps_sig(&trk[2]) * 7u;
        gp.seed = a; gp.seed2 = b2; gp.mix = mx; gen_make(); hy = steps_sig(&trk[0]) ^ steps_sig(&trk[1]) * 3u ^ steps_sig(&trk[2]) * 7u;
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
        bars(30);
        ok &= song.playing && dice_print() == h0 && !grd.rn;      /* (its 31st bar: still the piece) */
        bars(3); blk();
        ok &= song.playing && grd.rn == 1u && gp.seed != sd0 && dice_print() != h0;
        h0 = dice_print();
        for (n = 0; n < 40u && grd.rn < 2u; n++) bars(1);
        blk();
        ok &= song.playing && grd.rn == 2u && n >= 31u && dice_print() != h0;
        if (!ok) printf("  radio: playing %u rn %u seed %u (was %u)\n", (unsigned)song.playing, grd.rn, gp.seed, sd0);
        bad += check("RADIO: off nothing; on, the next piece after the loop's 32 bars, playing on, twice", ok);
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
    }
    printf(bad ? "gen-entry: %d FAILED\n" : "gen-entry: all ok\n", bad);
    return bad != 0;
}
