/* SPDX-License-Identifier: GPL-3.0-only
 * New synthesized recordings for the web factory bank, released as CC0-1.0.
 * Build: cc -O2 -w -Ibuild/gen -Ifirmware/src tools/gen_factory_samples.c -lm -o build/host/gen_factory_samples
 * Run: build/host/gen_factory_samples > web/factory-samples.json
 * No audio or tables are added to the firmware image.
 */
#define main hostsim_main
#include "../tests/hostsim.c"
#undef main
struct sound { const char *name; int en,note,rate,n; };
static const struct sound sounds[] = {
 {"KICK",10,36,8000,880},{"SNARE",10,38,8000,720},{"HAT",10,42,8000,440},
 {"CLAP",10,39,8000,800},{"TOM",10,45,8000,720},{"AAH",5,60,4000,960},
 {"BIRD",14,72,4000,880},{"PIANO",-1,60,4000,1000},{"FLUTE",-2,60,4000,1200},
 {"VINYL",-3,60,2000,700},{"BASS",0,48,4000,1000},{"CHORD",7,60,4000,1000}
};
int main(void)
{
 uint32_t k,j,b,i; int32_t raw[24000], out[CTL]; double pcm[1600];
 host_tracks_init();
 printf("{\"format\":\"jiant-factory-pcm\",\"version\":1,\"license\":\"CC0-1.0\",\"samples\":[\n");
 for(k=0;k<NELEM(sounds);k++) {
  const struct sound *s=&sounds[k]; uint32_t frames=(uint32_t)((uint64_t)s->n*FS/s->rate), seed=0x1234567u;
  double peak=0,lp=0;
  if(s->en>=0) {
   memset(trk,0,sizeof trk); host_tracks_init(); host_preset(&trk[0],s->en,0);
   trk[0].p[P_VOICE]=V_POLY; trk[0].p[P_ATK]=0; trk[0].p[P_SUS]=127; trk[0].p[P_DRIFT]=127;
   if(s->en==5) trk[0].p[P_VLANG]=0;
   if(s->en==7) trk[0].p[P_E0]=15;
   trk_note_on(&trk[0],s->note,100);
   if(s->en==7) {trk_note_on(&trk[0],64,90);trk_note_on(&trk[0],67,90);}
   for(b=0;b<frames;b+=CTL) {
    track_render(&trk[0],out,CTL);
    for(i=0;i<CTL && b+i<frames;i++) raw[b+i]=out[i];
   }
  } else for(j=0;j<frames;j++) {
   double t=(double)j/FS, f=261.6255653, y=0;
   if(s->en==-1) for(i=1;i<=8;i++) y+=sin(2*M_PI*f*i*t*(1+.0003*i))*exp(-(5.0+2.0*i)*t)/(i*i);
   if(s->en==-2) y=(sin(2*M_PI*f*t)+.18*sin(4*M_PI*f*t)+.06*sin(6*M_PI*f*t))*fmin(1,t/.02)*exp(-t);
   if(s->en==-3) { seed=seed*1664525u+1013904223u;lp+=(((int32_t)(seed>>16)-32768)/32768.0-lp)*.1;y=lp+((seed&4095u)<3?.6:0); }
   raw[j]=(int32_t)(y*24000);
  }
  for(j=0;j<(uint32_t)s->n;j++) {
   uint32_t a=(uint32_t)((uint64_t)j*frames/s->n),z=(uint32_t)((uint64_t)(j+1)*frames/s->n);
   double v=0;for(i=a;i<z;i++)v+=raw[i];v/=z-a;
   v*=fmin(1,(double)j/(s->rate*.002))*fmin(1,(double)(s->n-1-j)/(s->rate*.008));
   pcm[j]=v;if(fabs(v)>peak)peak=fabs(v);
  }
  if(peak<1) {fprintf(stderr,"silent factory sample: %s\n",s->name);return 1;}
  printf("%s{\"name\":\"%s\",\"rate\":%d,\"pcm\":[",k?",\n":"",s->name,s->rate);
  for(j=0;j<(uint32_t)s->n;j++) printf("%s%d",j?",":"",(int)lrint(pcm[j]*29000/peak));
  printf("]}");
 }
 printf("\n]}\n"); return 0;
}
