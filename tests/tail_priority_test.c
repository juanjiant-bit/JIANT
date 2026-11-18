/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Allocation study inspired by Mello-D/yehnah a26c786, voice.c:
 * quietest released tail first; retain JIANT's one-shot and bass protection.
 * Synthetic levels make the ordering deterministic; the final scene uses real
 * note admission. Host headroom is injected, not an FM-1 CPU measurement. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static unsigned failures;
static void ck(const char *name, int ok)
{
    printf("tail-priority: %s %s\n", name, ok ? "PASS" : "FAIL");
    failures += !ok;
}
static void reset_scene(void)
{
    host_tracks_init();
    for (unsigned p=0;p<NTRK;p++) {
        host_preset(&trk[p], ENGI_ANALOG, 0);
        memset(trk[p].v,0,sizeof trk[p].v);
        trk[p].p[P_VOICE]=V_POLY;
    }
    sys_load=LOAD_FULL;voice_headroom=1;load_cores2=1;
    load_tight=guard_cap=0;load_avg_q8=load_pred_q8=0;
    memset(mc_tk,0,sizeof mc_tk);
}
static void tail(unsigned p, unsigned i, unsigned age, int level)
{
    voice_t *v=&trk[p].v[i];
    memset(v,0,sizeof *v);v->active=1;v->stage=3;v->age=age;
    v->env_out=level;v->note=(uint8_t)(48+i);
}
static int chosen(unsigned p, unsigned i, int soft)
{
    track_t *owner=0;
    unsigned slot=voice_victim(&trk[5],soft,&owner);
    return slot==i && owner==&trk[p];
}
static unsigned held(unsigned p)
{
    unsigned n=0;
    for (unsigned i=0;i<NVOICE;i++)
        n+=trk[p].v[i].active && trk[p].v[i].stage!=4 && voice_held(&trk[p],&trk[p].v[i]);
    return n;
}
int main(void)
{
    reset_scene();tail(0,0,1,24000);tail(1,0,2,300);
    ck("reuse a quiet recent tail instead of a loud older tail",chosen(1,0,0));
    trk[0].v[0].env_out=300;
    ck("equal levels retain oldest-first ordering",chosen(0,0,0));
    trk[0].engine=ENGI_FM6;trk[0].v[0].env_out=0;trk[0].v[0].s[0]=24000;
    ck("FM6 uses its measured peak rather than an unused envelope",chosen(1,0,0));
    trk[0].v[0].s[0]=100;
    ck("a genuinely quiet FM6 release can be reused",chosen(0,0,0));

    reset_scene();tail(0,0,1,1);trk[0].p[P_VOICE]=V_MONO;trk[0].v[0].gate=1;
    tail(1,0,2,1);trk[1].v[0].gate=1;tail(2,0,3,20000);
    ck("held mono and poly bass stay protected",chosen(2,0,0));
    trk[2].v[0].stage=4;
    track_t *owner=0;
    ck("already fading voices are not taken again",voice_victim(&trk[5],0,&owner)==NVOICE);

    reset_scene();trk[0].engine=ENGI_DRUM;
    for(unsigned i=0;i<4;i++){tail(0,i,i+1,1);trk[0].v[i].s[0]=(int32_t)i;}
    tail(1,0,5,10000);
    ck("key-up drum hits stay ahead of even a loud synth release",chosen(1,0,0));
    trk[1].v[0].active=0;
    ck("kick snare and clap remain protected under pressure",chosen(0,3,0));
    tail(5,0,1,1);tail(2,0,10,500);
    ck("soft selection skips its requesting track",chosen(2,0,1));

    reset_scene();host_drums(&trk[0]);trk[0].p[P_VOICE]=V_POLY;
    for(unsigned p=2;p<NTRK;p++)trk[p].p[P_VOICE]=V_MONO;
    for(unsigned i=0;i<4;i++) {
        trk_note_on(&trk[0],DRUM_LANE_NOTE[i],100);
        trk_note_on(&trk[1],48+i*4,100);
        trk_note_on(&trk[i+2],36+i*7,100);
    }
    ck("injected headroom admits drums plus chord plus four mono tracks",
       voices_busy()==12 && held(0)==4 && held(1)==4 && held(2)==1 && held(3)==1 && held(4)==1 && held(5)==1);
    printf("tail-priority: %u failures\n",failures);
    return failures!=0;
}
