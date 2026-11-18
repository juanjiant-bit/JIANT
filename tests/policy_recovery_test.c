/* Same music and injected deadline readings; compares admission, not device CPU. */
#define main hostsim_main
#include <hostsim.c>
#undef main
static int32_t out[2*CTL];
static unsigned tick;
static void run(unsigned blocks,unsigned pct) {
 for(unsigned b=0;b<blocks;b++) {
  mix_block(out,CTL);
  for(unsigned p=0;p<NTRK;p++) {
   unsigned n=0;for(unsigned i=0;i<NVOICE;i++)n+=trk[p].v[i].active;
   mc_tk[p]=n*700;
#ifdef P_VCAP
   mc_nv[p]=n;
#endif
#ifdef MC_PART_TICKS
   if(n)mc_measure(&trk[p],n*700,n);
#endif
  }
  if(++tick%(HALF_FRAMES/CTL)==0)load_half(pct,pct,1);
 }
}
static unsigned count(unsigned t) {unsigned n=0;for(unsigned i=0;i<NVOICE;i++)n+=trk[t].v[i].active&&trk[t].v[i].stage!=4;return n;}
int main(int argc,char**argv) {
 unsigned scenario=argc>1?atoi(argv[1]):0;
 unsigned tracks=argc>2?atoi(argv[2]):6;
 host_tracks_init();trk_live=tracks;sys_load=LOAD_FULL;voice_headroom=load_cores2=mc_host=1;
 load_tight=guard_cap=0;load_avg_q8=load_all_q8=load_pred_q8=0;
 for(unsigned t=0;t<NTRK;t++) {
  host_preset(&trk[t],scenario&&t==0?ENGI_DRUM:ENGI_ANALOG,0);
  trk[t].p[P_VOICE]=scenario&&t==1?V_POLY:V_MONO;
  trk[t].p[P_SUS]=127;trk[t].p[P_REL]=5;
 }
 run(256,30);
 if(!scenario) {
  for(unsigned t=0;t<tracks;t++)trk_note_on(&trk[t],60+t,100);
  run(32,50);
 } else if(scenario==3) {
  for(unsigned l=0;l<8;l++)trk_note_on(&trk[0],DRUM_LANE_NOTE[l],100);
  const unsigned chord[]={48,52,55,59};
  for(unsigned j=0;j<4;j++)trk_note_on(&trk[1],chord[j],100);
  for(unsigned t=2;t<tracks;t++)trk_note_on(&trk[t],60+t,100);
 } else {
  trk_note_on(&trk[0],36,100);
  trk_note_on(&trk[2],60,100);trk_note_on(&trk[3],64,100);
  if(scenario==2)run(128,95);
  run(4000,65);
  const unsigned kit[]={36,38,39,42},chord[]={48,52,55,59};
  for(unsigned j=0;j<4;j++)trk_note_on(&trk[0],kit[j],100);
  for(unsigned j=0;j<4;j++)trk_note_on(&trk[1],chord[j],100);
 }
 printf("case=%u CPU=%u cap=%u guard=%u tracks=",scenario,load_all_q8>>8,voice_cap(),load_tight);
 for(unsigned t=0;t<6;t++)printf("%s%u",t?",":"",count(t));
 printf(" fades=%u\n",voice_kills);
 unsigned bad=0;
 for(unsigned t=0;t<tracks;t++) {
  unsigned want=!scenario?1:scenario==3?(t==0?8:t==1?4:1):t<2?4:t<4?1:0;
  bad+=count(t)!=want;
 }
 return !!bad;
}
