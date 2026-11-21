/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void edit_reset(void)
{
    ui_power_on();
    preview_req = preview_left = preview_mask = 0;
    panic_all = panic_req = 0;
    fm1_in.notes = 0;
    mi_r = mi_w;
    memset(&pf, 0, sizeof pf);
    song.sel = 0;
    song.master_q12 = 4096;
    song.seq_mode = 1;
    go_page(GR_ROLL);
    TSEL->p[P_VOICE] = V_POLY;
    TSEL->p[P_SLEN] = 16;
    TSEL->step[1] = (step_t){.time=ST_NOTE,.n=4,.note={48,52,55,59},.vel=100};
    TSEL->step[2] = (step_t){.time=ST_NOTE,.n=1,.note={65},.vel=100};
}
static uint64_t preview_energy;
static void render_preview(unsigned count)
{
    int32_t out[2 * CTL];
    while (count--) {
        mix_block(out, CTL);
        for (unsigned i=0;i<2*CTL;i++) preview_energy += abs(out[i]);
    }
}
static unsigned note_gate(unsigned n)
{
    unsigned i;
    for (i=0;i<NVOICE;i++) if (TSEL->v[i].active && TSEL->v[i].gate && TSEL->v[i].note==n) return 1;
    return 0;
}
int main(void)
{
    int bad=0;
    step_t before[NSTEP];
    edit_reset(); memcpy(before,TSEL->step,sizeof before);
    turn(EN_K1,1);
    bad+=check("cursor publishes preview without main-loop voice mutation",preview_req && !voices_busy());
    render_preview(1);
    bad+=check("stopped selection auditions the four-note chord",note_gate(48)&&note_gate(52)&&note_gate(55)&&note_gate(59));
    preview_energy=0;render_preview(FS/CTL);
    bad+=check("preview produces audible samples",preview_energy>0);
    bad+=check("preview expires without stuck voices or recorded edits",!voices_busy()&&!song.rec&&!memcmp(before,TSEL->step,sizeof before));

    edit_reset();cursor_set(1);cursor_set(2);render_preview(1);
    bad+=check("fast cursor motion coalesces to the newest selected note",note_gate(65)&&!note_gate(48));
    cursor_set(1);render_preview(15);
    bad+=check("previous preview fades before a new chord is admitted",note_gate(48)&&!note_gate(65));
    ui.pr_focus=2;ui_preview();render_preview(15);
    bad+=check("NOTE focus auditions only the selected chord tone",note_gate(52)&&!note_gate(48)&&!note_gate(55));

    edit_reset();trk_note_on(&trk[1],40,100);unsigned age=trk[1].v[0].age;
    cursor_set(1);render_preview(1);
    bad+=check("preview refuses to steal another part's held note",!preview_mask&&trk[1].v[0].age==age&&trk[1].v[0].gate);
    edit_reset();cursor_set(2);render_preview(1);trk_note_on(TSEL,65,100);render_preview(FS/CTL);
    bad+=check("preview expiry never releases a later live retrigger",note_gate(65));

    edit_reset();cursor_set(1);song.rec=1;render_preview(1);
    bad+=check("record arming cancels a pending audition",!preview_left&&!voices_busy());
    edit_reset();cursor_set(1);render_preview(1);transport_req=1;render_preview(1);
    bad+=check("PLAY cancels preview before transport starts",song.playing&&!preview_left);
    edit_reset();cursor_set(1);render_preview(1);ui.home=1;ui_input();render_preview(1);
    bad+=check("leaving the note editor cancels audition",!preview_left&&!gates());

    edit_reset();set_engine(ENGI_DRUM);render_preview(20);
    TSEL->step[1]=(step_t){.time=ST_NOTE,.hit=0x0f,.vel=100};ui.lane=2;cursor_set(1);render_preview(1);
    bad+=check("drum cursor previews the selected lane only",note_gate(DRUM_LANE_NOTE[2])&&!note_gate(DRUM_LANE_NOTE[0]));

    edit_reset();prefs_rev=4;ui_prefs|=PREF_LARGE;prefs_update();   /* (V10.7.73, the owner: LARGE off, CLICK REC, COUNT-IN 1 BAR, once) */
    bad+=check("settings migrate once: LARGE off, CLICK REC, COUNT-IN 1 BAR",prefs_rev==6&&!(ui_prefs&PREF_LARGE)&&rp_get(RP_CLICK)==1u&&rp_get(RP_COUNTIN)==1u);
    ui_prefs&=~PREF_LARGE;prefs_update();
    bad+=check("later compact-type choice is respected",!(ui_prefs&PREF_LARGE));
    bad+=test_rec_layer();
    return bad!=0;
}
