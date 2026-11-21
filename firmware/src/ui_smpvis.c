/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (ports/, FELUCCA_HOST_POWER only) SAMPLE's and GRAIN's picture (ui_engvis.c vx_draw): the slot's name over its
 * waveform (min / max a column), the region lit (SAMPLE: START .. END, its slices; GRAIN: POS .. REGION), the voices'
 * playheads, GRAIN's grains as dots; EMPTY for an empty slot, LIVE IN / LIVE MST and the last seconds of what comes in
 * for GRAIN's LIVE; RECORDING (ARMED) while the recorder works for the track. Reads the slot as the audio leaves it
 * (the UI runs on the audio's thread in ports/core); nothing in the audio path */
static void vx_smp(const track_t *t, const int32_t *A)
{
    const engine_t *e = trk_engp(t);
    uint32_t ti = (uint32_t)(t - trk) % NPART, gr = e == &ENG_GRAIN, slot = (uint32_t)clamp(t->p[P_E0], 0, 15), k;
    uint32_t src = gr ? (uint32_t)clamp(t->p[P_GXSRC], 0, GS_N - 1) : GS_SLOT;
    const smp_obj_t *o = src == GS_SLOT ? smp_get(slot) : 0;
    int32_t x0 = A[0] + 6, w = A[2] - 12, y0 = A[1] + 30, h = A[3] - 50, ym = y0 + h / 2, c;   /* (under the page's
                                                         * name and the slot's, over the sound's: ui_lzedit.c) */
    double n = o ? (double)o->frames : (double)SMP_LIVEF, ra = 0.0, rb = n;
    char b[40];
    if (w < 16 || h < 16)
        return;
    if (src != GS_SLOT)
        str_cpy(b, src == GS_IN ? "LIVE IN" : "LIVE MST", sizeof b);
    else if (!o)
        str_cpy(b, "EMPTY", sizeof b);
    else {
        b[0] = (char)('1' + (slot >= 9u ? 0 : slot));
        if (slot >= 9u) {
            b[0] = '1';
            b[1] = (char)('0' + slot - 9u);
            b[2] = 0;
        } else {
            b[1] = 0;
        }
        str_cpy(b + str_len(b), " ", sizeof b - str_len(b));
        str_cpy(b + str_len(b), o->name, sizeof b - str_len(b));
    }
    if (smp_rec.owner == ti && smp_rec.st != SR_IDLE)
        str_cpy(b, smp_rec.st == SR_REC ? "RECORDING" : "ARMED", sizeof b);
    cv_clip_text(x0 + 4, A[1] + 18 - AF_X_CAP_Y, &AF_X, b, smp_rec.owner == ti && smp_rec.st != SR_IDLE ? T_THEME : T_TEXT, T_SURF, w);
    if (o && !gr) {                                   /* SAMPLE: START .. END */
        double s = n * clamp(t->p[P_E1], 0, 127) / 128.0;
        ra = s;
        rb = s + (n - s) * clamp(t->p[P_E2], 1, 127) / 127.0;
    } else if (o) {                                   /* GRAIN: POS .. REGION */
        double s = n * clamp(t->p[P_E1], 0, 127) / 128.0;
        ra = s;
        rb = s + (n - s) * clamp(t->p[P_GXREG], 1, 127) / 127.0;
    }
    cv_rect(x0, ym, w, 1, T_MID);
    for (c = 0; c < w; c++) {                         /* the waveform: min / max of each column (64 reads at most) */
        double f0 = n * c / w, f1 = n * (c + 1) / w;
        int32_t lo = 0, hi = 0, j, step = (int32_t)((f1 - f0) / 64.0) + 1;
        for (j = (int32_t)f0; j < (int32_t)f1; j += step) {
            int32_t v;
            if (o)
                v = (o->work[0][j] + o->work[1][j]) / 2;
            else {
                const int16_t *q = smp_live_at(src == GS_IN ? 0u : 1u, (uint32_t)(SMP_LIVEF - 1 - j));
                v = (q[0] + q[1]) / 2;
            }
            lo = v < lo ? v : lo;
            hi = v > hi ? v : hi;
        }
        lo = lo * (h / 2) / 32768;
        hi = hi * (h / 2) / 32768;
        if (hi > lo || hi > 0)
            cv_rect(x0 + c, ym - hi, 1, hi - lo + 1, (f0 >= ra && f0 < rb) ? T_THEME : T_MID);
    }
    if (o && !gr && t->p[P_E3] == SM_SLICE) {         /* the slices */
        uint32_t ns = smp_slices(t);
        for (k = 1; k < ns; k++)
            cv_rect(x0 + (int32_t)((ra + (rb - ra) * k / ns) * w / n), y0, 1, 3, T_TEXT);
    }
    for (k = 0; k < NVOICE; k++) {                    /* the playheads */
        const voice_t *v = &t->v[k];
        double p;
        if (!v->active)
            continue;
        if (!gr) {
            if (!smp_v[ti][k].live)
                continue;
            p = smp_v[ti][k].pos;
        } else {
            uint32_t g;
            p = gr_v[ti][k].ph - (o ? 0.0 : (double)smp_live_w - n);
            for (g = 0; g < GR_N; g++)                /* the grains */
                if ((gr_v[ti][k].live >> g) & 1u) {
                    double q = gr_v[ti][k].g[g].pos - (o ? 0.0 : (double)smp_live_w - n);
                    int32_t gy = y0 + (int32_t)((g * 37u) % (uint32_t)(h > 2 ? h - 2 : 1));
                    if (q >= 0.0 && q < n)
                        cv_rect(x0 + (int32_t)(q * w / n), gy, 2, 2, T_TEXT);
                }
        }
        if (p >= 0.0 && p < n)
            cv_rect(x0 + (int32_t)(p * w / n), y0, 1, h, T_TEXT);
    }
}
