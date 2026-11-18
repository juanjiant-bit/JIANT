/* V53: reference-version features through real panel controls, plus the
 * per-block SHIFT cache's boundaries. No device-CPU claim from host tests. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static int sounds_test(void)
{
    int bad = 0, ok = 1;
    uint32_t i, p, e;
    int32_t out[2 * CTL];
    static step_t keep[NTRK][NSTEP];
    ui_power_on(); trks_put(1); go_home(); frame();
    for (i = 0; i < HE_SOUNDS; i++) { turn(EN_SELECT, 1); frame(); }
    bad += check("HOME > SOUNDS reachable with SELECT, engine and preset controls", home_sounds() &&
        !strncmp(ui.col[0], "ENG|", 4) && !strncmp(ui.col[1], "No.|", 4));
    for (p = 0; p < NTRK; p++) {
        trk[p].step[0] = (step_t){{(uint8_t)(48 + p)}, 1, ST_NOTE, 0, 100};
        memcpy(keep[p], trk[p].step, sizeof keep[p]);
    }
    for (p = 0; p < NTRK; p++) {
        if (p) { turn(EN_ALGO, 1); frame(); }
        e = p < 4u ? ENGI_DRUM : ENGI_ANALOG;
        for (i = 0; i < NENG_SHOWN && TSEL->eng_req != e; i++) { turn(EN_K1, 1); frame(); }
        for (i = 0; i < 32; i++) mix_block(out, CTL);
        ok &= song.sel == p && home_sounds() && TSEL->engine == e;
        for (i = 0; i <= p; i++) ok &= trk[i].eng_req == (i < 4u ? ENGI_DRUM : ENGI_ANALOG);
    }
    for (p = 0; p < NTRK; p++) ok &= !memcmp(keep[p], trk[p].step, sizeof keep[p]);
    bad += check("SOUNDS: four parallel DRUM-X plus two ANALOG; selecting engines preserves all sequences", ok);
    int16_t before = trk[4].p[P_E0];
    uint8_t pr = TSEL->preset;
    turn(EN_K2, 1); frame();
    bad += check("SOUNDS preset changes only the selected channel", TSEL->preset != pr && trk[4].p[P_E0] == before &&
                 home_sounds() && TSEL->eng_req == ENGI_ANALOG);
    TSEL->p[P_VOICE] = V_POLY; TSEL->p[P_VCAP] = 5;
    turn(EN_K3, 1); turn(EN_K4, 1); frame();
    bad += check("unused SOUNDS knobs preserve the four-note POLY setup", TSEL->p[P_VOICE] == V_POLY && TSEL->p[P_VCAP] == 5);
    track_select(0); frame();
    uint32_t filter0 = favorites.filter;
    uint8_t type0 = list_type;
    favorites.filter = 1; list_type = PT_N;
    e = TSEL->eng_req; pr = TSEL->preset;
    turn(EN_K2, 1); frame();
    bad += check("SOUNDS factory browse ignores and preserves the browser's USER/favorite filter",
                 TSEL->eng_req == e && TSEL->preset != pr && favorites.filter == 1 && list_type == PT_N);
    favorites.filter = filter0; list_type = type0;
    track_select(5); frame();
    up_store(3, "MINE"); up_load(3); frame();
    e = TSEL->eng_req;
    turn(EN_K2, 1); frame();
    bad += check("SOUNDS stays on the user's engine when browsing from a saved user sound", TSEL->eng_req == e && !TSEL->user);
    const char *dir = getenv("REFERENCE_SHOTS");
    if (dir) { ui.force = 1; frame(); screen_save(dir, "home-sounds-drums"); }
    return bad;
}

static int cache_test(void)
{
    int bad = 0, ok = 1;
    uint32_t p, pass, len, i;
    ui_power_on();
    for (p = 0; p < NTRK; p++) {
        track_t *t = &trk[p];
        memset(t->step, 0, sizeof t->step);
        t->step[0].comp = CO_MAKE(CA_SHIFT, CC_FIRST);
        t->step[0].carg = 64; /* +1 on first pass only */
        for (pass = 0; pass < 4u; pass++) for (len = 1; len <= NSTEP; len++) {
            t->p[P_SLEN] = (int16_t)len;
            shift_prepare_pass(t, len, pass);
            for (i = 0; i < len; i++) {
                const step_t *s = shift_step_pass(t, i, pass);
                int match = pass || len == 1 ? s == &t->step[i] :
                      i == 0 ? s->time == ST_REST : s == &t->step[i == 1 ? 0 : i];
                if (ok && !match) printf("cache mismatch track=%u pass=%u len=%u i=%u rotate=%d src=%ld\n",p,pass,len,i,t->p[P_SOFS],(long)(s-t->step));
                ok &= match;
            }
            uint8_t map[NSTEP]; memcpy(map, shift_map[pass & 1u][p], sizeof map);
            shift_prepare_pass(t, len, pass);
            ok &= !memcmp(map, shift_map[pass & 1u][p], sizeof map);
        }
        shift_prepare_pass(t, NSTEP, 65535u);
        ok &= shift_step_pass(t, 1, 65535u) == &t->step[1];
    }
    for (len = 1; len <= NSTEP; len++) for (int rot = -32; rot <= 32; rot++) {
        trk[0].p[P_SLEN] = (int16_t)len; trk[0].p[P_SOFS] = (int16_t)rot;
        for (i = 0; i < 256; i++)
            ok &= seq_src(&trk[0], i) == (uint32_t)(((int)(i % len) - rot % (int)len + (int)len) % (int)len);
    }
    bad += check("SHIFT cache: all tracks, lengths, alternating passes, cache hit and saturated pass", ok);
    seq_start();
    ok = 1;
    for (p = 0; p < NTRK; p++) ok &= !shift_stamp[0][p] && !shift_stamp[1][p];
    bad += check("transport restart invalidates every SHIFT cache", ok);
    return bad;
}

static int acid_test(void)
{
    int bad = 0, ok = 1;
    uint32_t a = NGENRE, i, seed, acc = 0, slide = 0;
    ui_power_on(); trks_put(1);
    bad += check("ACID HOUSE appended; previous HOUSE and HIP HOP IDs stable", !strcmp(GENRE[a - 1].name, "ACID HOUSE") &&
        !strcmp(GENRE[3].name, "HOUSE") && !strcmp(GENRE[26].name, "HIP HOP"));
    nav_go(SEC_HOME, HE_GEN, 0); frame();
    gp.genre = (uint8_t)(a - 1u); turn(EN_K1, 1); frame();
    bad += check("GEN knob exposes ACID HOUSE directly before EXPLORE", gp.genre == a && gen_genre_next(a, 1) == GEN_EXPLORE);
    for (seed = 1; seed <= 24; seed++) {
        gp.genre = (uint8_t)a; gp.seed = seed; gp.energy = 50; gp.form = 0;
        for (i = 0; i < GP_N; i++) gp.amt[i] = GA_AUTO;
        gen_go();
        ok &= gs.ba == BA_ACID && trk[1].eng_req == ENGI_ANALOG && trk[1].p[P_VOICE] == V_LEGATO;
        for (i = 0; i < (uint32_t)trk[1].p[P_SLEN]; i++) {
            acc += !!(trk[1].step[i].flags & SF_ACCENT);
            slide += !!(trk[1].step[i].flags & SF_SLIDE);
        }
    }
    bad += check("ACID HOUSE generates its 303 bass with legato, accents and slides across seeds", ok && acc && slide);
    return bad;
}
int main(void)
{
    int bad = sounds_test() + cache_test() + acid_test();
    return bad;
}
