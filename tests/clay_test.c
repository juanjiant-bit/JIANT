// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.21) DRONE CLAY (seq.c clay.c, JIANT BYTE's): the drone on and off, RATE's strikes, a modulator moving its
 * destination through mod_begin, the 'C' 'Y' block saved and loaded, an older project loading with it off.
 *   cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests tests/clay_test.c -lm */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void blocks(uint32_t n)
{
    int32_t o[2 * CTL];
    uint32_t i;
    for (i = 0; i < n; i++) {
        mix_block(o, CTL);
        if (!(i & 15u)) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); song_poll(); }
    }
}
static int voice_on(const track_t *t, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < NVOICE; k++)
        if (t->v[k].active && t->v[k].gate && t->v[k].note == note) return 1;
    return 0;
}
int main(void)
{
    int bad = 0, ok;
    const uint32_t tr = 1;
    track_t *t = &trk[tr];
    uint32_t dn, k, strikes = 0, last;
    ui_power_on();
    ok = !clay[tr][CL_ON] && !clay_dn[tr];
    bad += check("clay: off at power-on, no drone", ok);

    clay_defaults(clay[tr]);
    clay[tr][CL_ON] = 1; clay[tr][CL_RUN] = 1; clay[tr][CL_RATE] = 0;   /* FREE: runs stopped; HOLD */
    blocks(200);
    dn = 12u * (uint32_t)(clay[tr][CL_OCT] + 1) + (uint32_t)clamp(t->p[P_ROOT], 0, 11);
    ok = clay_dn[tr] == dn && voice_on(t, dn);
    bad += check("clay ON (FREE): the drone sounds on the track's voice, C(OCT+1) on the ROOT", ok);

    clay[tr][CL_ON] = 0;
    blocks(40);
    ok = !clay_dn[tr] && !clay_nn[tr] && !clay_v[tr][0] && !clay_v[tr][1] && !clay_v[tr][2] && !clay_v[tr][3];
    bad += check("clay OFF: the drone and its gestures end, the modulators at 0", ok);

    clay[tr][CL_ON] = 1; clay[tr][CL_RATE] = 7;          /* 1/16: struck again each sixteenth */
    last = clay_lr[tr];
    for (k = 0; k < 400u; k++) { blocks(4); if (clay_lr[tr] != last) { strikes++; last = clay_lr[tr]; } }
    bad += check("clay RATE 1/16: the drone struck again and again", strikes >= 8u);

    {   /* (BYTE's ff242c4) one voice (MONO): a key's note takes it, the drone back after */
        uint32_t n0;
        clay[tr][CL_RATE] = 0;
        t->p[P_VOICE] = V_MONO;
        blocks(40);
        n0 = clay_dn[tr];
        seq_note_on(t, 72u, 100u);
        blocks(40);
        ok = n0 && !clay_dn[tr] && voice_on(t, 72u);
        seq_note_off(t, 72u);
        blocks(40);
        ok &= clay_dn[tr] == n0;
        t->p[P_VOICE] = V_POLY; clay[tr][CL_RATE] = 7;
        bad += check("clay on MONO: a key's note takes the voice, the drone back after it", ok);
    }
    {   /* a modulator on DIST (CLAY_MD 6): mod_begin moves the track's parameter by what it adds, mod_end puts it back */
        uint32_t id = mod_param(MD_DIST), moved = 0;
        int16_t keep;
        for (k = 0; k < 4u; k++) { clay[tr][CL_MF(k, 0u)] = 0; clay[tr][CL_MF(k, 3u)] = 0; }
        clay[tr][CL_MF(0u, 0u)] = 6; clay[tr][CL_MF(0u, 3u)] = 127; clay[tr][CL_MF(0u, 2u)] = 2;
        t->p[id] = 64;
        keep = t->p[id];
        for (k = 0; k < 600u && !moved; k++) {
            blocks(2);
            if (clay_v[tr][0]) {
                macro_master();
                if (mod_begin(t)) { moved = t->p[id] != keep; mod_end(t); }
            }
        }
        ok = moved && t->p[id] == keep;
        bad += check("clay MOD 1 on DIST: mod_begin moves it, mod_end restores the stored value", ok);
    }

    {   /* saved and loaded: the 'C' 'Y' block, the track's 26 settings back */
        static project_t q, r;
        static project_store_t st;
        int16_t was[CLAY_NF];
        memcpy(was, clay[tr], sizeof was);
        project_capture(&q);
        ok = proj_pack(&st, &q);
        memset(&r, 0, sizeof r);
        ok &= proj_unpack_c(&r, st.raw) != 0;
        for (k = 0; k < CLAY_NF; k++) ok &= r.clay[tr][k] == (uint8_t)clamp(was[k], 0, 127);
        for (k = 0; k < NTRK; k++) ok &= k == tr || !r.clay[k][CL_ON];
        bad += check("clay saved and loaded: its 26 settings back, the other tracks off", ok);
        clay[tr][CL_ON] = 0;
        blocks(20);
        project_capture(&q);                             /* no track ON: no 'C' 'Y' block, as an older project */
        ok = proj_pack(&st, &q);
        memset(&r, 0xFF, sizeof r.clay);
        ok &= proj_unpack_c(&r, st.raw) != 0;
        for (k = 0; k < NTRK; k++) ok &= !r.clay[k][CL_ON];
        bad += check("a project without the block (older): DRONE CLAY off on every track", ok);
    }
    printf(bad ? "clay: %d FAILED\n" : "clay: all passed\n", bad);
    return bad != 0;
}
