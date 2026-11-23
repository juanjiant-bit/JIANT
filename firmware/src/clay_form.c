/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.21) DRONE CLAY's composer, from JIANT BYTE (GPL, its eng_form.c of 93e9eb1: FORM's gestures' machinery, the
 * FORM engine itself not here: FM never had it). Included by engines.c (before mod.c, which reads clay_v). */
/* DRONE CLAY's composer (was the FORM engine, BYTE 0.17; the owner: "out as an engine, a sequencer of the voice the
 * track has: endless timbres"). Four modulators, each a SHAPE out of five tables (TABLES, HITS, GESTURES, NOTES, FM),
 * its EUCLID ("--": none), AMOUNT and DECAY, on the track's STEP; a HIT is a bundle of moves (the kick: down, louder),
 * a NOTE a real note on the track's voice (seq.c clay_tick). What each modulator adds now (form_st.mv) moves the DEST it
 * is routed to through the matrix (mod.c mod_begin: the track's own parameters, any engine). The gestures' machinery
 * and the stored shapes' coding (FORM_DS) are FORM's */
/* (BYTE's fvm.c: the hash and FUSE's euclid, the two DRONE CLAY reads) */
static inline uint32_t fvm_hash(uint32_t h)
{
    h *= 0x9E3779B1u;
    h ^= h >> 15;
    h *= 0x85EBCA77u;
    return h ^ (h >> 13);
}
static const uint8_t FVM_EU[16][2]={{3,8},{5,8},{2,8},{3,16},{5,16},{7,16},{9,16},{4,16},{11,16},{5,12},{7,12},{4,12},
                                    {3,6},{2,6},{6,16},{13,16}};
static uint32_t fvm_eu(uint32_t s, uint32_t seed)
{
    uint32_t h = fvm_hash(seed * 0x9E3779B1u + (s >> 6) * 0x85EBCA77u), i, g[2];
    for (i = 0; i < 2u; i++) {
        uint32_t e = (h >> (i * 8u)) & 15u, k = FVM_EU[e][0], n = FVM_EU[e][1], r = (h >> (16u + i * 4u)) & 15u;
        g[i] = ((s + r) * k) % n < k;
    }
    switch (h >> 30) {
    case 0: return g[0] | g[1];
    case 1: return (g[0] & g[1]) | ((s & 15u) == 0u);
    case 2: return g[0] ^ g[1];
    default: return (s >> 4) & 1u ? g[1] : g[0];
    }
}
enum{FD_PITCH,FD_FM,FD_FOLD,FD_NOISE,FD_SHAPE,FD_LEVEL,FD_RATIO,FD_N};
static const char *const FORM_SHAPE[]={
 "SINE","S&H","DRIFT","SPRING","STAIRS","BOUNCE","EXP UP","LOG DN","CHAOS","SWEEP","RND GATE","TRIPLET",
 "KICK","SNARE","HAT","OPEN HAT","CLAP","RIM","TOM","ZAP","BITHAT","NOISE DN","SWELL","CRASH",
 "RAMP UP","RAMP DN","TRI","GATE","ENV UP","ENV DN","RISER","STUTTER","STUT UP","DOWN","UP","REST",
 "ARP UP","ARP DN","ARP UPDN","ARP RND","CHORD m","CHORD M","SUS4","m7","CLUSTER","+OCT","+5TH","GLIDE DN",
 "FM x1/2","FM x1","FM x1.5","FM x2","FM x3","FM x5","FM x7",0};
#define FORM_NSH 55u
/* (BYTE, RAM) DRONE CLAY's SHAPE knob: the same names, the HITS first (clay_sh), a table in the flash */
static const char *const CLAY_SHN[]={"KICK","SNARE","HAT","OPEN HAT","CLAP","RIM","TOM","ZAP","BITHAT","NOISE DN","SWELL","CRASH","SINE","S&H","DRIFT","SPRING","STAIRS","BOUNCE","EXP UP","LOG DN","CHAOS","SWEEP","RND GATE","TRIPLET","RAMP UP","RAMP DN","TRI","GATE","ENV UP","ENV DN","RISER","STUTTER","STUT UP","DOWN","UP","REST","ARP UP","ARP DN","ARP UPDN","ARP RND","CHORD m","CHORD M","SUS4","m7","CLUSTER","+OCT","+5TH","GLIDE DN","FM x1/2","FM x1","FM x1.5","FM x2","FM x3","FM x5","FM x7",0};
enum{FPG_TAB,FPG_HIT,FPG_GES,FPG_NOTE,FPG_FM};
enum{FS_ENVDN=29,FS_REST=35};                          /* (BYTE's S_ENVDN, S_REST: its S_* names the USB's registers here) */
/* (a stored parameter is -64 .. 127: 192 values) a modulator's two slots: SHAPE with its DEST (the TABLES and GESTURES
 * but REST: 23 x 7 DESTs, then the 31 shapes with no DEST: REST, the HITS, the NOTES, FM but x1), and its EUCLID with
 * its RATE (25 x 7). Inside, as one: DEST + 8 x SHAPE + 512 x RATE (form_vds) */
#define FORM_DSC(d,s) (((s)<12u)?(uint32_t)(s)*7u+(uint32_t)(d):((s)>=24u&&(s)<35u)?((uint32_t)(s)-12u)*7u+(uint32_t)(d):\
 161u+((s)==35u?0u:(s)<24u?(uint32_t)(s)-11u:(s)<48u?(uint32_t)(s)-23u:(uint32_t)(s)-23u-((s)>49u)))
#define FORM_DS(d,s) ((int32_t)FORM_DSC(d,s)-64)
#define FORM_EUV(e,r) ((int32_t)(e)+25*(int32_t)(r)-64)
#define FORM_DSMAX (512*7-1)
/* a modulator's step: 2^e a beat (e -2 .. 3: 1/1 .. 1/32), its RATE's or FORM's */
static int32_t form_mexpo(const int16_t*p,int32_t ds){int32_t r=ds>>9;return r?r-3:clamp(p[0],0,3);}   /* (p: clay_sp, its STEP at 0) */
static const char *const FORM_EU[]={"--","FUSE","1/4","2/4","3/4","1/8","2/8","3/8","5/8","7/8","2/5","3/5","3/7","4/7","2/9","4/9","5/9",
 "5/12","7/12","4/11","5/13","3/16","5/16","7/16","9/16",0};
static const uint8_t FORM_EUK[25][2]={{0,0},{0,0},{1,4},{2,4},{3,4},{1,8},{2,8},{3,8},{5,8},{7,8},{2,5},{3,5},{3,7},{4,7},{2,9},{4,9},{5,9},
 {5,12},{7,12},{4,11},{5,13},{3,16},{5,16},{7,16},{9,16}};
#define FORM_NEU 25u
static const char *const FORM_RATE[]={"1/4","1/8","1/16","1/32",0};
/* (BYTE, RAM) the same four in DRONE CLAY's compact slots (clay_sp: 0 STEP, 1 SEED, then 4 a modulator) */
static const uint8_t FORM_MC[4][4]={{2,3,4,5},{6,7,8,9},{10,11,12,13},{14,15,16,17}};
#define CLAY_SPN 18u
static uint32_t form_vds_x(const int16_t*p,const uint8_t (*mf)[4],uint32_t m){
 uint32_t c=(uint32_t)clamp(p[mf[m][0]]+64,0,191),e=(uint32_t)clamp(p[mf[m][1]]+64,0,174),s,d=0,j;
 if(c<161u){s=c/7u;d=c%7u;if(s>=12u)s+=12u;}
 else{j=c-161u;s=!j?35u:j<13u?11u+j:j<25u?23u+j:j==25u?48u:24u+j;}
 return d+8u*s+512u*(e/25u);
}
/* ---- the gestures: a key's or a euclid's, alive until their envelope is out ---- */
typedef struct{uint8_t sh,mod,fl,vel,key,spd;uint16_t len;uint32_t t0,rel;} form_g_t;   /* fl: 1 held 2 latched 4 inverted 8 alive */
#define FORM_G 6u
typedef struct{int32_t lastm[4];int16_t mv[4];uint32_t clk;uint8_t modes;form_g_t g[FORM_G];} form_st_t;   /* (FM: only DRONE CLAY's; FORM's keys and hold not here) */
static form_st_t form_st[NTRK];
static uint8_t form_lit[NTRK];                           /* (the panel: the modulators that struck this step, bit m) */
/* modulator m's hit at step s ("--": none, FUSE: two drawn from the seed and joined) */
static int form_hit(const int16_t*p,uint32_t m,uint32_t s,uint32_t seed)
{
 uint32_t e=(uint32_t)clamp(p[FORM_MC[m][1]]+64,0,174)%25u;
 if(!e)return 0;
 if(e==1u)return (int)fvm_eu(s,seed+m*37u);
 uint32_t k=FORM_EUK[e][0],n=FORM_EUK[e][1],q=s%n;   /* (the owner: on the step, with the others: a hit on the
                                                         * cycle's first step, no rotation by the seed) */
 (void)m;
 return (q*k)%n<k;
}
/* 2^(-x/256), Q15 */
static int32_t form_fexp(uint32_t x){if(x>=15u*256u)return 0;uint32_t u=x&255u;int32_t m=32768-(int32_t)((u*(21512u-((u*5128u)>>8)))>>8);m>>=x>>8;return m>32767?32767:m;}
static uint32_t form_isqrt(uint32_t x){uint32_t r=0,b=1u<<30;while(b>x)b>>=2;while(b){if(x>=r+b){x-=r+b;r=(r>>1)+b;}else r>>=1;b>>=2;}return r;}
static int form_oneshot(uint32_t sh){return sh/12u==FPG_HIT||sh==FS_ENVDN||sh==3u||sh==5u;}
static void form_spawn(form_st_t*st,uint32_t sh,uint32_t mod,uint32_t vel,uint32_t fl,uint32_t len,uint32_t key)
{
 uint32_t i,b=0;
 for(i=0;i<FORM_G;i++){form_g_t*g=&st->g[i];if(!(g->fl&8u)){b=i;break;}
  if(!(g->fl&3u)&&(st->g[b].fl&3u||g->t0<st->g[b].t0))b=i;}
 form_g_t*g=&st->g[b];
 g->sh=(uint8_t)sh;g->mod=(uint8_t)mod;g->vel=(uint8_t)vel;g->key=(uint8_t)key;g->len=(uint16_t)len;
 g->fl=(uint8_t)(fl|8u|((st->modes&8u)?4u:0u));g->spd=(uint8_t)(st->modes&2u?1u:st->modes&4u?2u:0u);g->t0=st->clk;g->rel=0;
}
/* a shape's value at tau (steps, Q8), Q15 signed: the TABLES' and the GESTURES' */
static int32_t form_shape(uint32_t sh,uint32_t tau,uint32_t seed)
{
 uint32_t g=sh%12u;
 if(sh/12u==FPG_GES)switch(g){
  case 0:return (int32_t)((tau&1023u)<<5);
  case 1:return 32767-(int32_t)((tau&1023u)<<5);
  case 2:{int32_t x=(int32_t)((tau&1023u)<<6);return x<32768?x:65535-x;}
  case 3:return (tau&511u)<256u?32767:0;
  case 4:return tau>=1024u?32767:(int32_t)(tau<<5);
  case 5:return form_fexp((tau*332u)>>8);
  case 6:{int32_t r=tau>=4096u?32767:(int32_t)(tau<<3);return r*r>>15;}
  case 7:return (tau&127u)<64u?32767:0;
  case 8:{uint32_t q=(tau*tau)>>13;int32_t a=9830+(int32_t)(tau<<4);return (q&127u)<64u?(a>32767?32767:a):0;}
  case 9:return -32767;
  default:return 32767;
 }
 switch(g){
  case 0:return sine_i(tau<<22);
  case 1:return (int32_t)(fvm_hash(seed+(tau>>8))&65535u)-32768;
  case 2:{uint32_t i=tau>>10,f=tau&1023u;int32_t a=(int32_t)(fvm_hash(seed+i)&65535u)-32768,b=(int32_t)(fvm_hash(seed+i+1u)&65535u)-32768;return a+(b-a)*(int32_t)f/1024;}
  case 3:return form_fexp((tau*185u)>>8)*sine_i(tau*(3u<<22)+0x40000000u)>>15;
  case 4:return (int32_t)((tau>>10)&3u)*10922;
  case 5:{int32_t s=sine_i((tau*tau)<<9);return (s<0?-s:s)*form_fexp((tau*92u)>>8)>>15;}
  case 6:{int32_t r=tau>=2340u?32767:(int32_t)(tau*14u);return (r*r>>15)*r>>15;}
  case 7:{uint32_t s=tau>=3276u?32767u:tau*10u;return 32767-(int32_t)form_isqrt(s<<15);}
  case 8:return (int32_t)(fvm_hash(seed*7u+(tau>>7))&32767u);
  case 9:return (int32_t)((tau&16383u)<<1);
  case 10:return (fvm_hash(seed+(tau>>7))&3u)?0:32767;
  default:return ((tau*3u)&1023u)<512u?32767:0;
 }
}
/* a HIT's envelope (Q15) and its moves: lift, octaves (Q12), level, shape, fm, ratio (Q12 oct), noise, crush */
static int32_t form_henv(uint32_t g,uint32_t tau)
{
 switch(g){
  case 0:case 3:return form_fexp((tau*591u)>>8);
  case 1:{int32_t a=tau>=7u?32767:(int32_t)(tau*5120u);return form_fexp((tau*1477u)>>8)*a>>15;}
  case 2:{int32_t a=tau>=5u?32767:(int32_t)(tau*7680u);return form_fexp((tau*5170u)>>8)*a>>15;}
  case 4:return tau<77u?(((tau*10u)>>8)&1u?6554:32767):form_fexp(((tau-77u)*1108u)>>8);
  case 5:return form_fexp((tau*4800u)>>8);
  case 6:case 7:return form_fexp((tau*923u)>>8);
  case 8:return form_fexp((tau*2954u)>>8);
  case 9:return form_fexp((tau*258u)>>8);
  case 10:{int32_t r=tau>=2048u?32767:(int32_t)(tau<<4);return r*r>>15;}
  default:return form_fexp((tau*166u)>>8);
 }
}
static const int16_t FORM_HIT[12][8]={
 /* lift   oct    lvl    shape   fm    ratio  noise  crush */
 {32767,-6144,13107,-32767,   0,    0,    0,    0},   /* KICK (its octaves falling: form_eval) */
 {32767, 1229, 9830,-21845,3120, 1434,29491,    0},   /* SNARE */
 {32767,19661,    0,-10923,6760, 3645, 6554,    0},   /* HAT */
 {32767,19661,    0,-10923,6760, 3645, 6554,    0},   /* OPEN HAT */
 {32767,10650, 6554,-10923,1040,    0,32767,    0},   /* CLAP */
 {32767, 9830, 3277,-21845,2080,    0,    0,    0},   /* RIM */
 {32767,-2458,13107,-32767,   0,    0,    0,    0},   /* TOM */
 {32767,-4096, 9830,-10923,   0,-1638,    0,    0},   /* ZAP */
 {32767,15565, 3277,     0,   0,    0,    0,18022},   /* BITHAT */
 {    0,    0,    0,     0,   0,    0,29491,    0},   /* NOISE DN */
 {    0,    0,    0,     0,   0,    0,29491,    0},   /* SWELL */
 {32767,18842,    0,-10923,7800, 3645,13107,    0}};  /* CRASH */
static const int8_t FORM_ARP[3][6]={{0,3,7,12,0,0},{12,7,3,0,0,0},{0,3,7,12,7,3}};
static const int8_t FORM_CH[5][4]={{0,3,7,0},{0,4,7,0},{0,5,7,0},{0,3,7,10},{0,1,2,3}};
static const int16_t FORM_RAT[7]={-8192,-4096,-1700,0,2396,5415,7403};
enum{FV_OCT,FV_FM,FV_FOLD,FV_CRUSH,FV_NOISE,FV_SHAPE,FV_LEVEL,FV_RATIO,FV_LIFT,FV_SEM,FV_ESUM,FV_N};
/* every gesture alive, summed into the eight (and the lift, the notes' semitones) */
static void form_eval(form_st_t*st,const int16_t*p,int32_t*D,uint32_t at)
{
 uint32_t i;int32_t M[4]={0,0,0,0};                  /* (the panel: what each modulator adds now) */
 for(i=0;i<FORM_G;i++){
  form_g_t*g=&st->g[i];if(!(g->fl&8u))continue;
  uint32_t m=g->mod&3u,raw=(st->clk-g->t0)>>8,sh=g->sh,pg=sh/12u,k=sh%12u;
  int32_t ds=(int32_t)form_vds_x(p,FORM_MC,m),dst=ds&7,amt=clamp(p[FORM_MC[m][2]],0,127),dec=clamp(p[FORM_MC[m][3]],0,127),e,xe=form_mexpo(p,ds)-2;
  uint32_t tau=xe>=0?raw<<xe:raw>>(-xe);                 /* (its time in its own RATE's steps) */
  tau=g->spd==1u?tau<<1:g->spd==2u?tau>>1:tau;
  if(form_oneshot(sh)){if(tau>=2048u){g->fl=0;continue;}e=32767;}
  else if((g->fl&3u)||(g->len&&tau<g->len))e=32767;
  else{if(!g->rel)g->rel=st->clk|1u;uint32_t rt=(st->clk-g->rel)>>8,T=64u+(uint32_t)dec*32u;e=form_fexp(rt*369u/T);if(e<200){g->fl=0;continue;}}
  int32_t gn=amt*g->vel*2;if(g->fl&1u)gn+=gn*(int32_t)at/127;
  int32_t a=mul64_sh15(e,gn);
  if(pg==FPG_HIT){
   const int16_t*h=FORM_HIT[k];int32_t A=form_henv(k,tau)*gn>>15,oct=h[1],fm=h[4];
   if(k==0)oct=-6144+(9011*form_fexp((tau*3324u)>>8)>>15);
   else if(k==1)oct=1229+(3277*form_fexp((tau*5170u)>>8)>>15);
   else if(k==6)oct=-2458+(4915*form_fexp((tau*1847u)>>8)>>15);
   else if(k==7){int32_t x=form_fexp((tau*1477u)>>8);oct=-4096+(24576*x>>15);fm=5200*x>>15;}
   D[FV_LIFT]+=h[0]*A>>15;D[FV_OCT]+=oct*A>>15;D[FV_LEVEL]+=h[2]*A>>15;D[FV_SHAPE]+=h[3]*A>>15;
   D[FV_FM]+=fm*A>>15;D[FV_RATIO]+=h[5]*A>>15;D[FV_NOISE]+=h[6]*A>>15;D[FV_CRUSH]+=h[7]*A>>15;
   M[m]+=A;continue;}
  if(pg==FPG_NOTE){
   int32_t sem,pl=0;
   if(k<3u){sem=FORM_ARP[k][(tau>>8)%(k==2u?6u:4u)];pl=1;}
   else if(k==3u){static const int8_t R[7]={0,3,5,7,10,12,15};sem=R[fvm_hash(g->t0+(tau>>8))%7u];pl=1;}
   else if(k<9u){const int8_t*c=FORM_CH[k-4u];sem=c[(tau>>6)%(k==7u||k==8u?4u:3u)];}
   else if(k==9u)sem=12;else if(k==10u)sem=7;else sem=-(int32_t)(tau>=512u?12:tau*12u/512u);
   D[FV_LIFT]+=a;D[FV_SEM]+=mul64_sh15(sem*341,a);D[FV_ESUM]+=a;
   if(pl){int32_t q=32767-form_fexp(((tau&255u)*923u)>>8);D[FV_LEVEL]-=(27852*q>>15)*a>>15;}
   M[m]+=sem*a/12;continue;}
  if(pg==FPG_FM){if(k<7u){D[FV_RATIO]+=FORM_RAT[k]*e>>15;D[FV_FM]+=5200*a>>15;}M[m]+=a;continue;}
  if(sh==FS_REST){D[FV_OCT]-=a;M[m]-=a;continue;}
  int32_t v=form_shape(sh,tau,g->t0);if(g->fl&4u)v=-v;
  int32_t va=v*a>>15;M[m]+=va;
  switch(dst){
   case FD_PITCH:D[FV_OCT]+=va>>2;if(va>0)D[FV_LIFT]+=va;break;
   case FD_FM:D[FV_FM]+=va*15600>>15;break;
   case FD_NOISE:D[FV_NOISE]+=va+(va>>1);break;
   case FD_RATIO:D[FV_RATIO]+=va>>3;break;
   case FD_FOLD:D[FV_FOLD]+=va;break;
   case FD_SHAPE:D[FV_SHAPE]+=va;break;
   default:D[FV_LEVEL]+=va;break;
  }
 }
 for(i=0;i<4u;i++)st->mv[i]=(int16_t)clamp(M[i]>>8,-1024,1024);
}
/* ---- (BYTE) DRONE CLAY: a track's settings (ui pages SEQ > DRONE CLAY, CLAY MOD 1..4, CLAY DECAY, CLAY TIME; the
 * project's 'C' 'Y' block). ON, the drone's OCTave (C0 .. C3 on the track's ROOT), its RATE (HOLD: held; else struck
 * again every 4 BAR .. 1/16), RUN (SONG: while the transport plays; FREE: always); each modulator's DEST (the matrix's,
 * CLAY_MD), SHAPE (the knob's order: the HITS first, CLAY_SH), EUCLID, AMOUNT; their DECAYs; the STEP and the SEED */
enum { CL_ON, CL_OCT, CL_RATE, CL_RUN, CL_M0, CL_DEC = CL_M0 + 16, CL_STEP = CL_DEC + 4, CL_SEED, CLAY_NF };
#define CL_MF(m, f) (CL_M0 + 4u * (m) + (f))        /* f: 0 DEST, 1 SHAPE, 2 EUCLID, 3 AMOUNT */
static int16_t clay[NTRK][CLAY_NF];   /* (FM: in the bss, zeroed at power-on) */
static int16_t clay_v[NTRK][4];                       /* what each modulator adds now (Q15): mod.c's per-block source */
static uint8_t clay_dn[NTRK], clay_nn[NTRK];          /* the drone's note sounding, a NOTE gesture's (0: none) */
static uint32_t clay_lr[NTRK];                        /* the drone's last strike (RATE's period count) */
static int clay_on(uint32_t tr) { return tr < NTRK && clay[tr][CL_ON]; }
/* the knob's SHAPE order -> FORM's shape: the HITS (12..23) first, then the TABLES (0..11), then the rest */
static uint32_t clay_sh(int32_t i) { uint32_t k = (uint32_t)clamp(i, 0, FORM_NSH - 1); return k < 12u ? k + 12u : k < 24u ? k - 12u : k; }
/* the stored settings as FORM's slots (form_hit, form_eval, form_vds read p[]): into sp */
static void clay_fill(const int16_t *c, int16_t *sp)
{
    uint32_t m;
    sp[0] = (int16_t)clamp(c[CL_STEP], 0, 3);
    sp[1] = c[CL_SEED];
    for (m = 0; m < 4u; m++) {
        uint32_t s = clay_sh(c[CL_MF(m, 1)]);
        sp[FORM_MC[m][0]] = (int16_t)FORM_DS(0u, s);
        sp[FORM_MC[m][1]] = (int16_t)FORM_EUV(clamp(c[CL_MF(m, 2)], 0, FORM_NEU - 1), 0);
        sp[FORM_MC[m][2]] = c[CL_MF(m, 3)];
        sp[FORM_MC[m][3]] = c[CL_DEC + m];
    }
}
/* a new DRONE CLAY: a drone on C1 held, a kick on the cutoff, an arp's notes, a slow triangle on the sound's KNOB 2, a
 * hat on its shape (the knobs as any FORM preset's) */
static void clay_defaults(int16_t *c)
{
    static const int8_t D[CLAY_NF] = {1, 1, 0, 0,   2, 0, 2, 100,   0, 36, 7, 90,   13, 26, 2, 60,   3, 2, 22, 70,
                                   10, 14, 30, 10,   2, 7};
    uint32_t i;
    for (i = 0; i < CLAY_NF; i++) c[i] = D[i];
}

