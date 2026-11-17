/* SPDX-License-Identifier: GPL-3.0-only */
/* A tester reports tracks beginning on different bars after live overdubbing.
 * Check shared transport phase separately from where a live take is written. */
#define UI_TEST_NO_MAIN
#include "ui_test.c"
static unsigned aligned(void)
{
    for (unsigned k = 1; k < trk_live; k++)
        if (trk[k].seq_idx != trk[0].seq_idx || trk[k].seq_pos != trk[0].seq_pos) return 0;
    return 1;
}
int main(void)
{
    unsigned bad = 0, ok = 1, k, b;
    static int32_t out[2 * CTL];
    ui_power_on(); trks_put(1); cin_bars = 0;
    song.g[G_CLOCK] = 0; song.g[G_BPM] = 120; song.g[G_SWING] = 0;
    for (k = 0; k < 6; k++) {
        set_engine_of(&trk[k], ENGI_ANALOG); trk[k].engine = ENGI_ANALOG;
        track_defaults_steps(&trk[k]);
        for (b = 0; b < NSTEP; b++) step_clear(&trk[k].step[b]);
        trk[k].p[P_SLEN] = 64; trk[k].p[P_SDIV] = 2; trk[k].seq_fit = 0;
        trk[k].p[P_SSWING] = 0; trk[k].p[P_AMODE] = 0; trk[k].p[P_MUTE] = 0;
    }
    transport_req = 1; mix_block(out, CTL);
    ok &= song.playing && aligned() && trk[0].seq_idx == 0;
    for (k = 0; k < 3; k++) {
        unsigned target = 16u * k, timeout = 0;
        while ((trk[0].seq_idx != target || trk[0].seq_pos < div_samples(2) / 4u) && timeout++ < 20000u) {
            mix_block(out, CTL); ok &= aligned();
        }
        ok &= timeout < 20000u;
        song.sel = k;
        unsigned idx = trk[k].seq_idx, pos = trk[k].seq_pos;
        rec_tap();
        ok &= trk[k].seq_idx == idx && trk[k].seq_pos == pos && aligned();
        key_down(white(7));
        for (b = 0; b < 4; b++) { mix_block(out, CTL); ok &= aligned(); }
        key_up(white(7)); rec_tap();
        ok &= trk[k].step[target].time == ST_NOTE && trk[k].step[target].n > 0;
        printf("record phase: track %u written at step %u, transport still aligned\n", k + 1, target + 1);
    }
    bad += check("live REC arming and takes in bars 1/2/3 never offset the six track clocks", ok);
    transport_req = 2; mix_block(out, CTL);
    transport_req = 1; mix_block(out, CTL);
    bad += check("STOP/PLAY restarts every track on step one; recorded rests retain their global position",
                 aligned() && trk[0].seq_idx == 0 && trk[1].step[0].time == ST_REST && trk[2].step[0].time == ST_REST);
    return !!bad;
}
