/* SPDX-License-Identifier: GPL-3.0-only */
#define main hostsim_main
#include "hostsim.c"
#undef main
static unsigned bad;
static void ck(const char *name,int ok){printf("adaptive: %s %s\n",name,ok?"ok":"FAIL");bad+=!ok;}
static void fresh(unsigned engine)
{
    memset(trk,0,sizeof trk);memset(&song,0,sizeof song);host_tracks_init();
    trk_live=6;sys_load=LOAD_FULL;voice_headroom=1;load_cores2=1;
    load_tight=guard_cap=guard_run=vh_wait=vh_held=vh_calm=frz_on=0;
    load_avg_q8=load_all_q8=load_pred_q8=0;load_fx_q8=0;
    memset(mc_tk,0,sizeof mc_tk);memset(mc_vt,0,sizeof mc_vt);memset(mc_nv,0,sizeof mc_nv);memset(vh_c,0,sizeof vh_c);
    for(unsigned p=0;p<6;p++) {
        host_preset(&trk[p],engine,0);trk[p].p[P_VOICE]=V_POLY;
        trk[p].p[P_SUS]=127;trk[p].p[P_VCAP]=0;trk[p].p[P_MUTE]=0;
    }
}
static void native(unsigned engine)
{
    static int32_t out[2*CTL];unsigned ok=1;
    fresh(engine);trk_note_on(&trk[0],60,100);
    for(unsigned b=0;b<16;b++)mix_block(out,CTL);
    track_t *t=&trk[0];voice_t *v=&t->v[0],v0=*v;
    drw_vc_t *d=&drw_v[drw_part(t)][0],d0=*d,full;
    vmod_t m={0};m.inc=pitch_inc(60*16);m.pitch16=60*16;
    m.amp0=m.amp1=16000;m.envq15=32767;m.shape=64<<8;
    ENGINES[engine]->render(t,v,out,CTL,&m);full=*d;
    *v=v0;*d=d0;m.rate_shift=1;
    ENGINES[engine]->render(t,v,out,CTL/2,&m);
    for(unsigned i=0;i<DRW_NP;i++)ok&=d->ph[i]==full.ph[i];
    ok&=d->perc==full.perc&&d->clk==full.clk;
    ck(engine==ENGI_ADD?"ADD keeps partial pitch and decay age at half rate":"WHEEL keeps partial pitch, percussion and click duration at half rate",ok);
    *v=v0;*d=d0;m.rate_shift=0;
    ENGINES[engine]->render(t,v,out,CTL/2,&m);
    ok=0;for(unsigned i=0;i<DRW_NP;i++)ok|=d->ph[i]!=full.ph[i];
    ck("a half-length event block is not mistaken for downsampling",ok);
}
static void seam(unsigned engine)
{
    static int32_t out[2*CTL];fresh(engine);
    track_t *t=&trk[0];t->p[P_E0]=0;t->p[P_E4]=0;t->p[P_E5]=0;
    t->p[P_E6]=engine==ENGI_ADD?127:0;t->p[P_E7]=0;
    if(engine==ENGI_ADD)t->p[P_E3]=0;
    else {t->p[P_E1]=t->p[P_E2]=t->p[P_E3]=0;t->p[P_WDUST]=127;}
    trk_note_on(t,60,100);for(unsigned b=0;b<32;b++)mix_block(out,CTL);
    voice_t *v=&t->v[0];vmod_t m={0};m.inc=pitch_inc(60*16);m.pitch16=60*16;
    m.amp0=m.amp1=16000;m.envq15=32767;m.shape=64<<8;
    int32_t prev=0,peak=0,jump=0;vh_c[0]=0;
    for(unsigned b=0;b<80;b++) {
        memset(out,0,sizeof out);
        if(b==20)vh_c[0]=(vh_c[0]&~7)|VH_IN;
        if(b==60)vh_c[0]=(vh_c[0]&~7)|VH_OUT;
        if(VH_ST(vh_c[0]))voice_half(t,ENGINES[engine],v,out,CTL,&m,0);
        else {ENGINES[engine]->render(t,v,out,CTL,&m);vh_c[0]=out[CTL-1]*8;}
        for(unsigned i=0;i<CTL;i++) {
            int32_t a=abs(out[i]),d=abs(out[i]-prev);if(a>peak)peak=a;
            if(b>=18&&b<=62&&d>jump)jump=d;prev=out[i];
        }
    }
    printf("adaptive: seam engine=%u peak=%d largest-step=%d\n",engine,peak,jump);
    ck("steady tone has no impulse at rate entry or exit",peak>100&&jump<peak/8);
}
int main(void)
{
    static int32_t out[2*CTL];unsigned ok=1;
    native(ENGI_ADD);native(ENGI_WHEEL);seam(ENGI_ADD);seam(ENGI_WHEEL);
    fresh(ENGI_ANALOG);vh_held=1;trk_note_on(&trk[0],60,100);mix_block(out,CTL);
    ck("new notes inherit reduced quality on their first complete render block",VH_ST(vh_c[0])==VH_ON);
    fresh(ENGI_ANALOG);
    for(unsigned p=0;p<2;p++)for(unsigned i=0;i<4;i++)trk_note_on(&trk[p],48+p*12+i*3,100);
    load_all_q8=load_pred_q8=68u<<8;
    for(unsigned b=0;b<16;b++)mix_block(out,CTL);
    ck("sustained six-track pressure selects cheaper rendering before the 78% ceiling",vh_held!=0&&!load_tight);
    unsigned mask=vh_held,kills=voice_kills;
    for(unsigned p=0;p<6;p++) {mc_measure(&trk[p],5000,4);}
    load_all_q8=load_pred_q8=50u<<8;
    for(unsigned b=0;b<1024;b++)voice_soft();
    ck("saving itself cannot immediately switch expensive held chords back to full rate",(vh_held&mask)==mask);
    load_all_q8=load_pred_q8=10u<<8;memset(mc_tk,0,sizeof mc_tk);memset(mc_vt,0,sizeof mc_vt);
    for(unsigned b=0;b<64;b++)voice_soft();
    ck("a brief lull cannot oscillate render quality",(vh_held&mask)==mask);
    for(unsigned b=0;b<1024;b++)voice_soft();
    ck("sustained full-rate headroom restores normal quality",vh_held==0);
    for(unsigned p=0;p<2;p++)for(unsigned i=0;i<4;i++)ok&=trk[p].v[i].active&&trk[p].v[i].gate&&trk[p].v[i].stage!=4;
    ck("quality transitions retain all eight held chord notes",ok&&voice_kills==kills);
    fresh(ENGI_ANALOG);trk_note_on(&trk[0],48,100);mc_measure(&trk[0],2000,1);
    unsigned cost=load_cost_q8(&trk[0]);trk[0].v[1].active=1;
    ck("queued voices cannot dilute a previous per-voice CPU measurement",load_cost_q8(&trk[0])==cost);
    fresh(ENGI_ANALOG);mc_measure(&trk[0],8000,8);
    cost=load_cost_q8(&trk[0]);mc_measure(&trk[0],1000,1);
    ck("matched normalization keeps an eight-to-one voice transition stable",load_cost_q8(&trk[0])==cost);
    mc_measure(&trk[0],80,0);
    ck("silence retains the last valid per-voice measurement",load_cost_q8(&trk[0])==cost);
    trk[0].engine=ENGI_FM6;
    ck("engine switch cannot reuse the previous engine's measured cost",load_cost_q8(&trk[0])!=cost);
    mc_measure(&trk[0],2000,1);cost=load_cost_q8(&trk[0]);
    ck("new engine's first measurement replaces the previous estimate",mc_vt[0]==2000&&mc_ve[0]==ENGI_FM6);
    mc_vt[0]=0xffffffffu;
    ck("extreme timing cannot wrap into free admission",load_cost_q8(&trk[0])==(100u<<8));
    fresh(ENGI_ANALOG);load_all_q8=load_pred_q8=72u<<8;
    trk_note_on(&trk[0],48,100);ck("empty channels cannot consume hypothetical CPU",voices_busy()==1);
    for(unsigned p=1;p<6;p++)trk[p].p[P_MUTE]=1;
    trk_note_on(&trk[0],52,100);ck("muting empty channels does not change measured admission",voices_busy()==2);
    printf("adaptive rate: %s\n",bad?"FAILED":"all passed");return !!bad;
}
