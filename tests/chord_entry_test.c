/* POLY through the actual VOICES knob and keyboard/step-entry path. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    static const unsigned keys[] = {7, 11, 14, 18};
    int bad = 0;
    for (unsigned selected = 0; selected < NTRK; selected++) {
        ui_power_on(); trks_put(1);
        unsigned kit = (selected + 1) % NTRK;
        track_select(kit); set_engine(ENGI_DRUM);
        track_select(selected); set_engine(ENGI_ANALOG);
        int32_t out[2 * CTL];
        for (unsigned b = 0; b < 16; b++) mix_block(out, CTL);
        bad += check("engine changes complete before playing", TSEL->engine == ENGI_ANALOG && trk[kit].engine == ENGI_DRUM);
        sys_load = LOAD_FULL; voice_headroom = load_cores2 = 1;
        load_tight = guard_cap = 0;
        load_avg_q8 = load_all_q8 = load_pred_q8 = 0;
        memset(mc_tk, 0, sizeof mc_tk); memset(mc_nv, 0, sizeof mc_nv);
        TSEL->p[P_VOICE] = V_LEGATO;
        nav_put(SEC_EDIT, "VOICES"); frame();
        turn(EN_K1, -2); frame();
        bad += check("VOICES knob selects POLY on this channel", TSEL->p[P_VOICE] == V_POLY);
        TSEL->p[P_SUS] = 127; TSEL->p[P_CHRD] = 0;
        TSEL->p[P_VCAP] = trk[kit].p[P_DDST] = 0;
        track_defaults_steps(TSEL); go_page(GR_ROLL); cursor_set(0); frame();
        song.rec = 1u << selected;
        for (unsigned l = 0; l < DV_NLANE; l++) trk_note_on(&trk[kit], DRUM_LANE_NOTE[l], 110);
        for (unsigned j = 0; j < 4; j++) { key_down(keys[j]); frame(); }
        unsigned held = 0, hits = 0;
        for (unsigned i = 0; i < NVOICE; i++) {
            held += TSEL->v[i].active && TSEL->v[i].gate && TSEL->v[i].stage != 4;
            hits += trk[kit].v[i].active && trk[kit].v[i].stage != 4;
        }
        printf("chord entry: selected=%u held=%u kit=%u recorded=%u\n", selected + 1, held, hits, TSEL->step[0].n);
        bad += check("four keyboard notes sound beside the complete kit", held == 4 && hits == DV_NLANE);
        bad += check("step entry retains all four chord notes", TSEL->step[0].n == 4);
        for (unsigned j = 0; j < 4; j++) key_up(keys[j]);
        frame(); song.rec = 0;
    }
    return bad;
}
