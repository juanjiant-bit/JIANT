/* SPDX-License-Identifier: GPL-3.0-only */
/* Musical admission regression: two tetrads, three drum hits and three melodies.
 * Synthetic CPU measurements exercise admission; the profile counts host work,
 * not device deadline percentages. Compile against V48 to reproduce note loss. */
#define main hostsim_main
#include <hostsim.c>
#undef main
#ifdef PHRASE_CALLGRIND
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_ZERO_STATS
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_DUMP_STATS_AT(x)
#define CALLGRIND_STOP_INSTRUMENTATION
#endif
static unsigned failed;
static void expect(const char *what, int ok)
{
    if (!ok) { fprintf(stderr,"six phrase: FAIL %s\n",what); failed++; }
}
static unsigned has(unsigned p, unsigned note)
{
    for(unsigned i=0;i<NVOICE;i++) {
        voice_t *v=&trk[p].v[i];
        if(v->active&&v->stage!=4&&v->gate&&v->note==note)return 1;
    }
    return 0;
}
static void setup(unsigned engine)
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);memset(&chain,0,sizeof chain);
    host_tracks_init();trk_live=6;sys_load=LOAD_FULL;voice_headroom=1;load_cores2=1;
    load_tight=guard_cap=guard_run=0;load_avg_q8=load_all_q8=load_pred_q8=40u<<8;
    load_fx_q8=perf_allowed=perf_denied=perf_held=0;memset(mc_tk,0,sizeof mc_tk);
    for(unsigned p=0;p<6;p++) {
        host_preset(&trk[p],p==2?ENGI_DRUM:engine,0);
        trk[p].p[P_VOICE]=V_POLY;trk[p].p[P_VCAP]=0;trk[p].p[P_DDST]=0;
        trk[p].p[P_SUS]=127;trk[p].p[P_REL]=55;trk[p].p[P_MUTE]=0;
    }
    song.master_q12=4096;
}
int main(int argc,char **argv)
{
    unsigned engine=argc>1?atoi(argv[1]):ENGI_ANALOG, quiet=argc>2;
    unsigned target=argc>3?68u:40u;
    static int32_t out[2*CTL];static const unsigned chord[]={0,4,7,11};
    setup(engine);
    for(unsigned p=0;p<2;p++)for(unsigned i=0;i<4;i++)trk_note_on(&trk[p],48+p*12+chord[i],90);
    for(unsigned i=0;i<3;i++)trk_note_on(&trk[2],DRUM_LANE_NOTE[i],100);
    for(unsigned p=3;p<6;p++)trk_note_on(&trk[p],72+p,95);
    if(!quiet)expect("two full tetrads, kit and three leads need 14 simultaneous voices",voices_busy()==14);
    printf("initial engine=%u voices=%u cap=%u\n",engine,voices_busy(),voice_cap());
    for(unsigned b=0;b<64;b++){mix_block(out,CTL);load_half(target,target,1);}
    unsigned kills=voice_kills,missing=0,held_missing=0,checks=0,max=0;
    CALLGRIND_ZERO_STATS;CALLGRIND_START_INSTRUMENTATION;
    for(unsigned step=0;step<32;step++) {
        unsigned root=48+(step&1u)*2u;
        for(unsigned p=0;p<2;p++) {
            for(unsigned i=0;i<NVOICE;i++)if(trk[p].v[i].gate)trk_note_off(&trk[p],trk[p].v[i].note);
            for(unsigned i=0;i<4;i++)trk_note_on(&trk[p],root+p*12+chord[i],90);
        }
        for(unsigned i=0;i<3;i++)trk_note_on(&trk[2],DRUM_LANE_NOTE[i],100);
        for(unsigned p=3;p<6;p++) {
            for(unsigned i=0;i<NVOICE;i++)if(trk[p].v[i].gate)trk_note_off(&trk[p],trk[p].v[i].note);
            trk_note_on(&trk[p],72+p+(step%7),95);
        }
        for(unsigned p=0;p<2;p++)for(unsigned i=0;i<4;i++){missing+=!has(p,root+p*12+chord[i]);checks++;}
        for(unsigned p=3;p<6;p++){missing+=!has(p,72+p+step%7);checks++;}
        for(unsigned b=0;b<16;b++) {
            mix_block(out,CTL);load_half(target,target,1);
            unsigned count=voices_load();if(count>max)max=count;
            for(unsigned p=0;p<2;p++)for(unsigned i=0;i<4;i++)held_missing+=!has(p,root+p*12+chord[i]);
            for(unsigned p=3;p<6;p++)held_missing+=!has(p,72+p+step%7);
            for(unsigned j=0;j<2*CTL;j++)if(!quiet)expect("bounded output",out[j]>=-32768&&out[j]<=32767);
        }
    }
    CALLGRIND_DUMP_STATS_AT("six-phrase");CALLGRIND_STOP_INSTRUMENTATION;
    printf("phrase notes=%u missing=%u held_missing=%u peak_voices=%u fades=%u half_mask=%u\n",checks,missing,held_missing,max,voice_kills-kills,vh_held);
    {
        expect("every chord and melody note survives changing phrases",missing==0);
        expect("held chord and melody notes survive every rendered block",held_missing==0);
        expect("all rendered tails stay inside the shared ceiling",max<=16);
    }
    return failed?1:0;
}
