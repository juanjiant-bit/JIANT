/* Six-track music: a complete DRUM-X kit, tetrad and four mono lines.
 * Checks drum onsets too: retaining melodies alone is not a musical pass. */
#define main hostsim_main
#include <hostsim.c>
#undef main
static unsigned failed, engine = ENGI_ANALOG;
#ifdef KIT_CALLGRIND
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_ZERO_STATS
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_DUMP_STATS_AT(x)
#define CALLGRIND_STOP_INSTRUMENTATION
#endif
static int live_note(unsigned t,unsigned n) {
    for(unsigned i=0;i<NVOICE;i++)if(trk[t].v[i].active&&trk[t].v[i].stage!=4&&trk[t].v[i].note==n)return 1;
    return 0;
}
static void setup_music(unsigned cap) {
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);memset(&chain,0,sizeof chain);
    memset(drum_kit,0,sizeof drum_kit);
    host_tracks_init();trk_live=6;sys_load=LOAD_FULL;voice_headroom=load_cores2=1;
    ev_off=guard_cap=load_tight=guard_run=vh_held=vh_wait=vh_calm=frz_on=0;
    load_fx_q8=perf_held=perf_allowed=perf_denied=0;memset(vh_c,0,sizeof vh_c);load_avg_q8=load_all_q8=load_pred_q8=30u<<8;
    memset(mc_tk,0,sizeof mc_tk);memset(mc_nv,0,sizeof mc_nv);
    for(unsigned t=0;t<6;t++) {
        host_preset(&trk[t],t==0?ENGI_DRUM:engine,0);
        trk[t].p[P_VOICE]=t==1?V_POLY:V_MONO;
        trk[t].p[P_VCAP]=0;trk[t].p[P_DDST]=t?0:cap;
        trk[t].p[P_SUS]=127;trk[t].p[P_REL]=50;
    }
}
static void check_note(unsigned t,unsigned note,const char *tag) {
    if(!live_note(t,note)){printf("FAIL %s: track=%u note=%u\n",tag,t+1,note);failed++;}
}
int main(int argc,char **argv) {
    if(argc>1)engine=atoi(argv[1]);
    unsigned pressure=argc>2?atoi(argv[2]):68;
    static int32_t out[CTL*2];
    static unsigned chord[]={48,52,55,59};
    setup_music(0);
    for(unsigned j=0;j<4;j++)trk_note_on(&trk[1],chord[j],100);
    for(unsigned t=2;t<6;t++)trk_note_on(&trk[t],60+t,100);
    for(unsigned l=0;l<DV_NLANE;l++)trk_note_on(&trk[0],DRUM_LANE_NOTE[l],110);
    printf("AUTO kit cap=%u global=%u voices=%u\n",trk_nvoice(&trk[0]),voice_cap(),voices_busy());
    for(unsigned l=0;l<DV_NLANE;l++)check_note(0,DRUM_LANE_NOTE[l],"whole-kit onset");
    for(unsigned j=0;j<4;j++)check_note(1,chord[j],"chord onset");
    for(unsigned t=2;t<6;t++)check_note(t,60+t,"mono onset");
    for(unsigned b=0;b<8;b++) {mix_block(out,CTL);load_half(40,40,1);}
    for(unsigned l=0;l<3;l++)check_note(0,DRUM_LANE_NOTE[l],"core hit rendered");
    unsigned before=failed, kills=voice_kills, peak=0;
    CALLGRIND_ZERO_STATS;CALLGRIND_START_INSTRUMENTATION;
    for(unsigned step=0;step<64;step++) {
        unsigned transpose=(step%3)*2;
        for(unsigned j=0;j<NVOICE;j++)if(trk[1].v[j].gate)trk_note_off(&trk[1],trk[1].v[j].note);
        for(unsigned j=0;j<4;j++)trk_note_on(&trk[1],chord[j]+transpose,100);
        for(unsigned t=2;t<6;t++) {
            trk_note_off(&trk[t],trk[t].v[0].note);
            trk_note_on(&trk[t],60+t+transpose,100);
        }
        ev_off=step%3?17:0; /* off-grid hits as well as simultaneous block-edge hits */
        for(unsigned l=0;l<DV_NLANE;l++) {
            trk_note_on(&trk[0],DRUM_LANE_NOTE[l],110);
            check_note(0,DRUM_LANE_NOTE[l],"repeated kit onset");
        }
        ev_off=0;
        for(unsigned b=0;b<16;b++) {
            mix_block(out,CTL);load_half(pressure,pressure,1);
            for(unsigned j=0;j<4;j++)check_note(1,chord[j]+transpose,"sustained chord");
            for(unsigned t=2;t<6;t++)check_note(t,60+t+transpose,"sustained mono");
            if(!b)for(unsigned l=0;l<3;l++)check_note(0,DRUM_LANE_NOTE[l],"rendered core onset");
            if(voices_load()>peak)peak=voices_load();
            for(unsigned j=0;j<CTL*2;j++)if(out[j]<-32768||out[j]>32767)failed++;
        }
    }
    CALLGRIND_DUMP_STATS_AT("kit-six");CALLGRIND_STOP_INSTRUMENTATION;
    printf("engine=%u pressure=%u missing=%u peak=%u fades=%u half_mask=%u\n",engine,pressure,failed-before,peak,voice_kills-kills,vh_held);
    if(peak>16 || voice_kills!=kills)failed++;
    setup_music(8); /* explicit four-voice kit: core hits must win over auxiliary hits */
    static unsigned burst[]={0,1,2,3,5};
    for(unsigned l=4;l<8;l++)trk_note_on(&trk[0],DRUM_LANE_NOTE[l],110);
    ev_off=1;
    for(unsigned j=0;j<5;j++)trk_note_on(&trk[0],DRUM_LANE_NOTE[burst[j]],110);
    ev_off=0;
    for(unsigned l=0;l<3;l++)check_note(0,DRUM_LANE_NOTE[l],"pending core hit");
    mix_block(out,CTL);
    for(unsigned l=0;l<3;l++)check_note(0,DRUM_LANE_NOTE[l],"pending core after render");
    setup_music(5); /* explicit one-voice kit cannot replace kick with a hat */
    trk_note_on(&trk[0],36,100);trk_note_on(&trk[0],42,100);
    check_note(0,36,"tiny cap protects kick");
    setup_music(0);trk_note_on(&trk[0],42,100);
    ev_off=17;trk_note_on(&trk[0],44,100);trk_note_on(&trk[0],42,100);ev_off=0;
    unsigned slots=0;for(unsigned j=0;j<NVOICE;j++)slots+=trk[0].v[j].active;
    if(slots!=1){printf("FAIL alternate GM hats allocated %u slots\n",slots);failed++;}
    /* Request downsampling during admission, but never grant fictional CPU credit. */
    setup_music(0);trk_note_on(&trk[1],48,100);
    load_all_q8=load_pred_q8=79u<<8;
    trk_note_on(&trk[1],52,100);
    if(live_note(1,52)){puts("FAIL overload admission bypassed");failed++;}
    if(ENGINES[engine]->half && !(vh_held&2u)){puts("FAIL quality request waited for next control pass");failed++;}
    printf("kit continuity: %u failures\n",failed);
    return !!failed;
}
