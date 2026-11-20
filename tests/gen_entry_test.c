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
    printf(bad ? "gen-entry: %d FAILED\n" : "gen-entry: all ok\n", bad);
    return bad != 0;
}
