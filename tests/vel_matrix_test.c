/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.22, the owner) EDIT > VOICE 2's VEL is only how much velocity reaches the sound: the matrix's VEL follows the
 * velocity as played (seq.c tvel, vel_note; voice.c vel_sound).
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src tests/vel_matrix_test.c -lm
 * 1. the mix of a fixed play (keys / MIDI in, steps with an ACCENT, ANALOG, FM6 POLY and UNISON) bit for bit as
 *    the sources before it (VEL_HASH_*, measured on that tree: -DVEL_HASH_ONLY prints them): VEL 127 with a VEL route,
 *    VEL 64 and 0 with none (what VEL does to the sound itself unchanged);
 * 2. VEL 0: notes struck 30 and 120 start at the same velocity (100), the matrix's VEL reads 30 and 120 (per voice and
 *    per track), a VEL -> CUT route opens the filter by it; a step's own velocity and its ACCENT (127) too. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define VEL_HASH_127R 0xd4355752u
#define VEL_HASH_64 0xd408d383u
#define VEL_HASH_0 0xdb036bc8u

static int bad;
static void ck(const char *what, int ok)
{
    printf("vel: %-100s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void setup(int tv, int route)
{
    uint32_t i;
    host_tracks_init();
    host_preset(&trk[0], ENGI_ANALOG, 1);
    host_preset(&trk[1], ENGI_FM6, 0);
    host_preset(&trk[2], ENGI_FM6, 1);
    trk[2].p[P_VOICE] = V_UNISON;
    for (i = 0; i < 3u; i++) {
        uint32_t k;
        trk[i].p[P_TVEL] = (int16_t)tv;
        for (k = 0; k < 12u; k++)                   /* (the presets' own slots off: FM6 1 has VEL -> CUT) */
            trk[i].p[P_M1SRC + k] = 0;
    }
    if (route) {
        trk[0].p[P_M1SRC] = MS_VEL; trk[0].p[P_M1DST] = MD_CUT; trk[0].p[P_M1AMT] = 63;   /* per voice */
        trk[1].p[P_M1SRC] = MS_VEL; trk[1].p[P_M1DST] = MD_PAN; trk[1].p[P_M1AMT] = 40;   /* per track */
    }
}
static uint32_t run(int tv, int route)
{
    static const uint8_t V[6] = {30, 120, 64, 127, 1, 96};
    uint32_t b, i, h = 2166136261u;
    int32_t o[2 * CTL];
    step_t st;
    setup(tv, route);
    memset(&st, 0, sizeof st);
    for (b = 0; b < FS * 3u / CTL; b++) {
        uint32_t k = b / 80u, t = k % 3u;
        if (b % 80u == 0u && k < 24u) {
            if (k % 4u == 3u) {                         /* a step: its own velocity, every other an ACCENT */
                st.vel = V[k % 6u];
                st.flags = (uint8_t)(k & 4u ? SF_ACCENT : 0u);
                seq_note_on(&trk[t], 48u + k, STEP_VEL(&trk[t], &st));
            } else {
                input_on(&trk[t], 48u + k, V[k % 6u]);   /* (the keys, MIDI in) */
            }
        }
        if (b % 80u == 60u && k < 24u)
            trk_note_off(&trk[t], 48u + k);
        mix_block(o, CTL);
        for (i = 0; i < 2u * CTL; i++)
            h = (h ^ (uint32_t)o[i]) * 16777619u;
    }
    return h;
}

int main(void)
{
    char what[160];
    uint32_t h127 = run(127, 1), h64 = run(64, 0), h0 = run(0, 0);
#ifdef VEL_HASH_ONLY
    printf("vel hash 127r %08x 64 %08x 0 %08x\n", h127, h64, h0);
    return 0;
#endif
    snprintf(what, sizeof what, "VEL 127, a VEL route: the mix bit for bit as before (%08x)", h127);
    ck(what, h127 == VEL_HASH_127R);
    snprintf(what, sizeof what, "VEL 64 / 0, no route: the sound as before, FM6 POLY and UNISON too (%08x %08x)", h64, h0);
    ck(what, h64 == VEL_HASH_64 && h0 == VEL_HASH_0);
    {
        int32_t s30, s120;
        uint32_t v30, v120, m30, m120;
        setup(0, 1);
        input_on(&trk[0], 60, 30);
        v30 = trk[0].v[trk[0].m_vi].vel; m30 = trk[0].v[trk[0].m_vi].mvel;
        s30 = mod_tsrc(&trk[0], MS_VEL, 0);
        trk_note_off(&trk[0], 60);
        input_on(&trk[0], 62, 120);
        v120 = trk[0].v[trk[0].m_vi].vel; m120 = trk[0].v[trk[0].m_vi].mvel;
        s120 = mod_tsrc(&trk[0], MS_VEL, 0);
        snprintf(what, sizeof what, "VEL 0: struck 30 and 120 both start at %u / %u; the matrix's VEL reads %u / %u (per voice), %d / %d",
                 v30, v120, m30, m120, (int)s30, (int)s120);
        ck(what, v30 == 100u && v120 == 100u && m30 == 30u && m120 == 120u && s30 == 30 * 258 && s120 == 120 * 258);
    }
    {
        int32_t o[2 * CTL];
        int32_t cut[2];
        uint32_t j;
        for (j = 0; j < 2u; j++) {
            uint32_t b;
            setup(0, 1);
            trk[0].p[P_M1SRC] = MS_VEL; trk[0].p[P_M1DST] = MD_CUT; trk[0].p[P_M1AMT] = 63;
            input_on(&trk[0], 60, j ? 120u : 30u);
            for (b = 0; b < 4u; b++) mix_block(o, CTL);
            mod_begin(&trk[0]);
            {
                voice_t *v = &trk[0].v[trk[0].m_vi];
                vmod_t m;
                memset(&m, 0, sizeof m);
                mod_voice(&trk[0], v, &m, 0);
                cut[j] = m.cutoff;
            }
            mod_end(&trk[0]);
        }
        snprintf(what, sizeof what, "VEL 0, VEL -> CUT 63: the filter opened by the velocity played (%d for 30, %d for 120)", (int)cut[0], (int)cut[1]);
        ck(what, cut[0] == (30 * 258 * 63) >> 7 && cut[1] == (120 * 258 * 63) >> 7);
    }
    {
        step_t st;
        uint32_t a, m;
        setup(0, 0);
        memset(&st, 0, sizeof st);
        st.vel = 30;
        seq_note_on(&trk[0], 60, STEP_VEL(&trk[0], &st));
        m = trk[0].m_vel; a = trk[0].v[trk[0].m_vi].vel;
        st.flags = SF_ACCENT;
        seq_note_on(&trk[0], 64, STEP_VEL(&trk[0], &st));
        snprintf(what, sizeof what, "VEL 0, a step of 30: plays 100, the matrix 30; its ACCENT plays 127, the matrix 127 (%u %u, %u %u)",
                 a, m, trk[0].v[trk[0].m_vi].vel, trk[0].m_vel);
        ck(what, a == 100u && m == 30u && trk[0].v[trk[0].m_vi].vel == 127u && trk[0].m_vel == 127u);
    }
    printf("vel: %s\n", bad ? "FAILED" : "all ok");
    return bad != 0;
}
