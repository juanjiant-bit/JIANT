/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.8.45, the owner: "the arp is not quantized, it should be on the 1/16 at least, not to play out of phase") a key
 * into a track's ARP while the transport plays, at 18 moments of a 1/16, four RATEs, three tempos: every step the arp
 * takes afterwards on its RATE's grid of the transport (to the sample), its first one a 1/16 away at most or at once
 * when the grid's step is in this block or 20 ms back; stopped: at once, as ever */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    static const uint32_t tempos[] = {90, 121, 133}, rates[] = {2, 1, 3, 5};   /* 1/16 1/8 1/32 1/16T */
    uint32_t bad = 0, off = 0, late = 0, n_on = 0;
    for (uint32_t b = 0; b < NELEM(tempos); b++)
        for (uint32_t r = 0; r < NELEM(rates); r++)
            for (uint32_t m = 0; m < 18u; m++) {
                track_t *t = &trk[0];
                uint32_t per, i, started = 0, wait = 0, idx;
                ui_power_on();
                song.g[G_BPM] = (int16_t)tempos[b]; song.g[G_CLOCK] = 0;
                t->p[P_AMODE] = 1; t->p[P_ARATE] = (int16_t)rates[r]; t->p[P_APROB] = 127; t->p[P_ASWING] = 0;
                transport_req = 1;
                events_block(CTL);
                per = div_samples(rates[r]);
                for (i = 0; i < 40u + m * (div_samples(2) / 18u / CTL + 1u); i++) events_block(seq_time(CTL));
                arp_add(t, 60);
                idx = t->arp_idx;
                for (i = 0; i < 4000u; i++) {
                    events_block(seq_time(CTL));
                    if (t->arp_idx != idx) {             /* a step taken in this block */
                        uint64_t at = (uint64_t)clk_n * div_samples(2) + clk_pos - t->arp_pos;
                        idx = t->arp_idx; n_on++;
                        if (at % per) off++;
                        if (!started) { started = 1; if (wait * CTL > per + CTL) late++; }
                    } else if (!started) wait++;
                }
                if (!started) bad++;
            }
    bad += check("ARP while playing: every step on its RATE's grid of the transport, three tempos, four RATEs, 18 moments", !off && n_on > 1000u);
    bad += check("  its first step a RATE's step away at most", !late);
    {   /* stopped: at once */
        track_t *t = &trk[0];
        ui_power_on();
        t->p[P_AMODE] = 1; t->p[P_APROB] = 127;
        events_block(CTL); events_block(CTL);
        arp_add(t, 60);
        events_block(CTL);
        bad += check("ARP stopped: the first note in the key's block", t->arp_idx == 0u && t->arp_note == 60);
    }
    {   /* (the owner: "in REC it has to be quantized") armed, REC Q 0 (as played), keys off the grid: every recorded
         * step on its place (MICRO 0), one note a step, none doubled or missing over the loop's end */
        uint32_t offs = 0, miss = 0, dbl = 0, m;
        for (m = 0; m < 6u; m++) {
            track_t *t = &trk[0];
            uint32_t i, n = 0;
            ui_power_on();
            song.g[G_BPM] = 121; song.g[G_CLOCK] = 0; song.sel = 0;
            for (i = 0; i < NSTEP; i++) step_clear(&t->step[i]);
            t->p[P_SLEN] = 16; t->p[P_SDIV] = 2; t->p[P_HRQ] = 0; t->p[P_QUANT] = QN_OFF;
            t->p[P_AMODE] = 1; t->p[P_ARATE] = 2; t->p[P_APROB] = 127; t->p[P_ASWING] = 0; t->p[P_AGATE] = 60;
            song.rec = 1;
            transport_req = 1;
            events_block(CTL);
            for (i = 0; i < 30u + m * 17u; i++) events_block(seq_time(CTL));
            arp_add(t, 60); arp_add(t, 64);
            for (i = 0; i < 20u * div_samples(2) / CTL; i++) events_block(seq_time(CTL));   /* (over the loop's end) */
            arp_remove(t, 60); arp_remove(t, 64);
            for (i = 0; i < 16u; i++) {
                const step_t *s = &t->step[i];
                if (s->time != ST_NOTE || !s->n) { miss++; continue; }
                n++;
                if (ST_MICRO(s)) offs++;
                if (s->n != 1u) dbl++;
            }
        }
        bad += check("ARP recorded (REC Q as played, keys off the grid): every step its note, on its place, one a step", !offs && !miss && !dbl);
        if (offs || miss || dbl) printf("  off %u missing %u doubled %u\n", offs, miss, dbl);
    }
    printf(bad ? "arp-grid: %u FAILED (off %u, late %u of %u)\n" : "arp-grid: all ok\n", bad, off, late, n_on);
    return bad ? 1 : 0;
}
