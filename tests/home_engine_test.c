/* HOME quick controls: real UI input, selected-track isolation, locks and automation. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0, ok = 1;
    uint32_t e, k, t;
    ui_power_on();
    trk_live = NTRK;
    for (e = 0; e < NENGINES; e++) {
        if (!eng_ok(e)) continue;
        for (t = 0; t < NTRK; t++) {
            track_select(t);
            set_engine(e);
            nav_go(SEC_HOME, HE_ENGINE, 0);
            frame();
            for (k = 0; k < 4u; k++) {
                uint32_t id = home_engine_id(k), j, c;
                int16_t before[NTRK][P_COUNT];
                uint8_t macros[4];
                const param_desc_t *d = track_desc(TSEL, id);
                memcpy(macros, macro_v, sizeof macros);
                for (j = 0; j < NTRK; j++) memcpy(before[j], trk[j].p, sizeof before[j]);
                int32_t dir = TSEL->p[id] == d->max ? -1 : 1;
                turn(EN_K1 + k, dir); frame();
                ok &= TSEL->p[id] != before[t][id] && !memcmp(macros, macro_v, sizeof macros);
                for (j = 0; j < NTRK; j++) if (j != t) ok &= !memcmp(before[j], trk[j].p, sizeof before[j]);
                for (c = 0; c < k; c++) ok &= home_engine_id(c) != id;
                ui.plk_src = 0;
                ok &= lock_id(k) == id;
                if (!k) ok &= !strncmp(ui.col[0], "FILTER|", 7);
            }
        }
    }
    bad += check("HOME: all active engines, all six channels, four distinct controls, correct locks; other channels/macros untouched", ok);
    ui_power_on(); set_engine(ENGI_ANALOG);
    TSEL->p[P_E4] = 64;
    song.rec = 1; seq_start(); frames(8);
    for (k = 0; k < 6; k++) { turn(EN_K1, -1); frames(4); }
    ok = TSEL->p[P_E4] < 64;
    for (k = 0, e = 0; k < motion.count; k++) e += MOTION_ID(&motion.event[k]) == P_E4;
    bad += check("HOME FILTER records the selected engine's cutoff automation", ok && e);
    stop_transport(); song.rec = 0;
    int16_t cutoff = TSEL->p[P_E4];
    btn_down(B_EDIT); frame(); turn(EN_K1, -4);
    ok = TSEL->p[P_E4] != cutoff;
    btn_up(B_EDIT); frames(64);
    bad += check("HOME EDIT-held momentary control returns to its original value", ok && TSEL->p[P_E4] == cutoff);
    ui_power_on();
    btn_down(B_LFO); frames(30);
    uint8_t mv = macro_v[0];
    turn(EN_K1, 4); frame();
    ok = macro_v[0] != mv;
    btn_up(B_LFO); frame();
    bad += check("LFO retains access to MACROS after replacing HOME", ok);
    const char *shots = getenv("HOME_LIVE_SHOTS");
    if (shots) {
        ui_power_on(); set_engine(ENGI_ANALOG); frame(); screen_save(shots, "home-engine");
        track_select(1); set_engine(ENGI_FM6); frame(); screen_save(shots, "home-fm6");
        for (t = 0; t < NTRK; t++) my_steps(&trk[t]);
        nav_go(SEC_GLO, 0, 0); frame(); screen_save(shots, "live");
        live_knob(0, 2); live_knob(1, -2); live_knob(2, 3); live_knob(3, -3); frame();
        screen_save(shots, "live-modified");
        nav_put(SEC_GLO, "KEY FX"); frame(); screen_save(shots, "live-keys");
    }
    return bad;
}
