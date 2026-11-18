/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN
#include "ui_test.c"
static void reset6(void)
{
    ui_power_on(); trks_put(1); voice_headroom=load_tight=guard_cap=0;
    load_avg_q8=load_all_q8=load_pred_q8=load_fx_q8=0; load_cores2=1;
    memset(mc_tk,0,sizeof mc_tk);memset(mc_nv,0,sizeof mc_nv); perf_denied=perf_allowed=0;
    for(unsigned k=0;k<6;k++) {
        set_engine_of(&trk[k],ENGI_ANALOG);trk[k].engine=ENGI_ANALOG;
        trk[k].p[P_VOICE]=V_POLY;trk[k].p[P_VCAP]=0;trk[k].p[P_MUTE]=0;
    }
}
static unsigned sounding(unsigned k)
{
    unsigned n=0;for(unsigned i=0;i<NVOICE;i++)n+=trk[k].v[i].active;return n;
}
int main(void)
{
    unsigned bad=0,ok=1,k,i,b;static int32_t out[2*CTL];
    reset6();
    ok=trk_nvoice(&trk[0])==4;
    trk[0].p[P_VCAP]=2;ok&=trk_nvoice(&trk[0])==1;trk[0].p[P_VCAP]=0;
    trk_live=4;ok&=trk_nvoice(&trk[0])==4;trk_live=6;
    set_engine_of(&trk[0],ENGI_DRUM);trk[0].engine=ENGI_DRUM;
    ok&=trk_nvoice(&trk[0])==4;
    bad+=check("six-track AUTO keeps four voices per engine, lower user cap respected",ok);
    reset6();trks_put(0);ok=1;
    for(k=0;k<NENGINES;k++)if(eng_ok(k)) {
        trk[0].engine=k;trk[0].p[P_VCAP]=0;trk[0].p[P_DDST]=0;
        unsigned cap=ENGINES[k]->poly?ENGINES[k]->poly:NVOICE;
        ok&=trk_nvoice(&trk[0])==(cap<4?cap:4);
    }
    bad+=check("every engine AUTO uses at most four voices before six-track protection",ok);
    reset6();ok=1;
    for(k=0;k<6;k++)for(i=0;i<8;i++) {
        trk_note_on(&trk[k],48+i*2,100);ok&=voices_load()<=voice_cap();
    }
    for(k=0;k<6;k++)ok&=sounding(k)>=1&&sounding(k)<=4;
    bad+=check("six chords reserve a voice per channel without exceeding the shared budget",ok);
    reset6();ok=1;
    for(k=0;k<6;k++) {set_engine_of(&trk[k],ENGI_DRUM);trk[k].engine=ENGI_DRUM;}
    for(k=0;k<6;k++)for(i=0;i<8;i++) {
        trk_note_on(&trk[k],DRUM_LANE_NOTE[i],100);ok&=voices_load()<=voice_cap();
    }
    for(k=0;k<6;k++)ok&=sounding(k)>=1&&sounding(k)<=4;
    bad+=check("six kits cannot bypass the global budget with protected kick/snare/clap hits",ok);
    reset6();trk_note_on(&trk[0],48,100);
    load_all_q8=load_pred_q8=79u<<8;
    trk_note_on(&trk[0],60,100);
    bad+=check("no fallback admits an extra six-track voice above the CPU prediction",sounding(0)==1);
    reset6();trk_note_on(&trk[0],48,100);trk_note_on(&trk[0],60,100);
    load_tight=1;mc_tk[0]=12000;mc_tk[1]=200;
    for(b=0;b<4;b++)voice_soft();
    bad+=check("CPU pressure does not shrink a channel cap or fade its held chord",
        trk_nvoice(&trk[0])==4&&trk[0].v[0].stage!=4&&trk[0].v[1].stage!=4);
    reset6();load_all_q8=load_pred_q8=40u<<8;
    unsigned d=PF_BIT(PF_D16),r=PF_BIT(PF_REV),w=PF_BIT(PF_WOB),mask=d|r|w;
    unsigned got=perf_budget(mask);
    ok=__builtin_popcount(got)==2&&perf_denied;
    unsigned blocked=perf_denied;mask&=~(got&-got);
    ok&=!(perf_budget(mask)&blocked);
    perf_budget(mask&~blocked);ok&=(perf_budget(mask)&blocked)!=0;
    bad+=check("at most two six-track audio punches; rejected presses require release before retry",ok);
    load_all_q8=load_pred_q8=75u<<8;
    ok=__builtin_popcount(perf_budget(mask))<=1;
    load_all_q8=load_pred_q8=82u<<8;ok&=perf_budget(mask)==0;
    trk_live=4;ok&=perf_budget(d|r|w)==0;
    perf_budget(0);load_all_q8=load_pred_q8=40u<<8;
    ok&=__builtin_popcount(perf_budget(d|r|w))==2;
    bad+=check("FX shed before the deadline in both four- and six-track modes",ok);
    reset6();
    trk_live=4;trk[0].p[P_VCAP]=9;for(i=0;i<6;i++)trk_note_on(&trk[0],48+i,100);
    trk_live=6;trk[0].p[P_VCAP]=0;for(b=0;b<4;b++)voice_soft();ok=1;
    for(i=4;i<NVOICE;i++)if(trk[0].v[i].active)ok&=trk[0].v[i].stage==4;
    unsigned fading=sounding(0);ok&=voices_load()==fading;
    bad+=check("explicit channel-cap reduction fades surplus voices and keeps charging their CPU",ok);
    reset6();trk_live=4;set_engine_of(&trk[0],ENGI_DRUM);trk[0].engine=ENGI_DRUM;trk[0].p[P_DDST]=12;
    for(i=0;i<6;i++)trk_note_on(&trk[0],DRUM_LANE_NOTE[i],100);
    trk_live=6;trk[0].p[P_DDST]=0;for(b=0;b<4;b++)voice_soft();
    trk_note_on(&trk[0],DRUM_LANE_NOTE[5],100);ok=1;
    for(i=4;i<NVOICE;i++)if(trk[0].v[i].active)ok&=trk[0].v[i].stage==4;
    bad+=check("a kit lane outside the new cap cannot reactivate a fading surplus slot",ok);
    reset6();trk_note_on(&trk[0],48,100);trk_note_on(&trk[0],60,100);
    trk_note_on(&trk[1],64,100);trk_note_off(&trk[1],64);
    trk[1].v[0].env_out=1;load_all_q8=load_pred_q8=79u<<8;
    track_t *victim=0;unsigned slot=load_victim(&trk[0],&victim);
    bad+=check("overload cannot replace a held chord or another channel's tail",slot==NVOICE);
    reset6();load_all_q8=load_pred_q8=60u<<8;
    perf_budget(d);unsigned debt=load_fx_q8;
    trk_note_on(&trk[0],48,100);
    bad+=check("voice prediction preserves newly admitted FX cost until measurements catch up",debt==(10u<<8)&&load_pred_q8>=(70u<<8));
    reset6();load_all_q8=40u<<8;load_pred_q8=200u<<8;
    bad+=check("stale repeated-note debt cannot permanently refuse a punch with spare CPU",perf_budget(d)==d&&load_pred_q8==(50u<<8));
    reset6();ok=pfm_get(0,15)==PF_DECP&&pfm_get(0,26)==PF_DECM&&pfm_get(0,24)==PF_RVA;
    for(k=0;k<6;k++)ok&=pfm_get(0,16+k)==PA_M1+k;
    pfm.map[0][15]=PF_COMB+1;ok&=pfm_get(0,15)==PF_COMB;
    bad+=check("default punch map: six mutes, DEC+/DEC-, REV ALL; saved custom assignment preserved",ok);
    reset6();ok=1;song.playing=0;
    for(k=0;k<NENGINES;k++)if(eng_ok(k)) {
        trk[0].engine=k;trk[0].p[P_DEC]=60;trk[0].p[P_REL]=40;trk[0].p[P_E3]=64;
        pfx_block(CTL,PF_BIT(PF_DECM));
        ok&=k==ENGI_DRUM?trk[0].p[P_E3]<64:trk[0].p[P_DEC]<60&&trk[0].p[P_REL]<40;
        pfx_end();ok&=trk[0].p[P_DEC]==60&&trk[0].p[P_REL]==40&&trk[0].p[P_E3]==64;
        pfx_block(CTL,0);pfx_end();pfx_block(CTL,PF_BIT(PF_DECP));
        ok&=k==ENGI_DRUM?trk[0].p[P_E3]>64:trk[0].p[P_DEC]>60&&trk[0].p[P_REL]>40;
        pfx_end();ok&=trk[0].p[P_DEC]==60&&trk[0].p[P_REL]==40&&trk[0].p[P_E3]==64;
        pfx_block(CTL,0);pfx_end();
    }
    bad+=check("decay punches shorten/lengthen each engine and restore every stored parameter",ok);
    trk[0].engine=ENGI_FM6;trk[0].p[P_DEC]=trk[0].p[P_REL]=0;
    pfx_block(CTL,PF_BIT(PF_DECM));fm6_sync(&trk[0]);ok=trk[0].p[P_DEC]<0&&trk[0].p[P_REL]<0;
    pfx_end();ok&=trk[0].p[P_DEC]==0&&trk[0].p[P_REL]==0;
    bad+=check("FM6 DEC- can shorten a native patch even with its envelope offsets at zero",ok);
    pfx_block(CTL,0);pfx_end();load_all_q8=71u<<8;
    pfx_block(CTL,PF_BIT(PF_DECP));
    bad+=check("DEC+ cannot extend tails when spare CPU is exhausted",trk[0].p[P_DEC]==0&&trk[0].p[P_REL]==0);pfx_end();
    reset6();song.master_q12=4096;cpu_test_song();transport_req=1;ui_input();ok=trk_live==6;
    for(b=0;b<1024;b++) {
        if(!(b&31))perf_held=(b&64)?mask:0;
        if(!(b&3))load_half(b<512?80:40,b<512?84:45,1);
        mix_block(out,CTL);
        for(i=0;i<2*CTL;i++)ok&=out[i]>=-32768&&out[i]<=32767;
    }
    bad+=check("six-track sequencer and FX transitions remain bounded under simulated load",ok);
    printf("six budget: %s\n",bad?"FAILED":"all ok");return !!bad;
}
