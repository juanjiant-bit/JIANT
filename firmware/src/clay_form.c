/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (V10.8.21) DRONE CLAY's composer, from JIANT BYTE (GPL, its eng_form.c of 93e9eb1: FORM's gestures' machinery, the
 * FORM engine itself not here: FM never had it); (V10.8.27) BYTE's 1adbf0a6 + d7f32b11: each modulator its own RATE on
 * the bar's grid, the HITs a drum on any engine, FM's seven a sine at their ratio. Included by engines.c (before mod.c,
 * which reads clay_vk and the HITs' bundle). */
/* DRONE CLAY's composer (was the FORM engine, BYTE 0.17; the owner: "out as an engine, a sequencer of the voice the
 * track has: endless timbres"). Four modulators, each a SHAPE out of five tables (TABLES, HITS, GESTURES, NOTES, FM),
 * its EUCLID ("--": none), AMOUNT, DECAY and RATE; a HIT the track's voice as a drum (its pitch, noise, CUTOFF),
 * a NOTE a real note on the track's voice (seq.c clay_tick). What each modulator adds now (form_st.mv) moves the DEST it
 * is routed to through the matrix (mod.c mod_begin: the track's own parameters, any engine). The gestures' machinery
 * is FORM's */
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
/* (BYTE's 1adbf0a6) DRONE CLAY's EUCLID: the straight ones first (on the bar's grid), then the triplets' 12, then the
 * polyrhythms; "--" none, FUSE two drawn from the SEED and joined, 1/1 every step */
static const char *const CLAY_EU[]={"--","FUSE","1/1","1/4","2/4","3/4","1/8","2/8","3/8","5/8","7/8","3/16","5/16","7/16","9/16",
 "5/12","7/12","2/5","3/5","3/7","4/7","2/9","4/9","5/9","4/11","5/13",0};
static const uint8_t CLAY_EUK[26][2]={{0,0},{0,0},{1,1},{1,4},{2,4},{3,4},{1,8},{2,8},{3,8},{5,8},{7,8},{3,16},{5,16},{7,16},{9,16},
 {5,12},{7,12},{2,5},{3,5},{3,7},{4,7},{2,9},{4,9},{5,9},{4,11},{5,13}};
#define CLAY_NEU 26u
/* (a first DRONE CLAY's EUCLID: FORM's order, 25) -> this order */
static const uint8_t CLAY_EU_OLD[25]={0,1,3,4,5,6,7,8,9,10,17,18,19,20,21,22,23,15,16,24,25,11,12,13,14};
/* (BYTE, the owner: "more resolutions, not polyrhythmic, and faster") DRONE CLAY's resolutions, each a modulator's RATE
 * (its euclid's steps, its shapes' time) and the drone's (struck again each): straight first, then dotted, triplets,
 * the polyrhythms last. num steps a bar in den bars (the bar 4 beats); a step's index is the bar's (never out of
 * phase), more than one a bar: spb a bar, the last one cut short when they do not fill it (the dotted). rl: 2^32 / its
 * length (Q16 beats), its gestures' time */
typedef struct { uint8_t num, den, spb; uint32_t rl; } clay_res_t;
#define CLAY_RL(num, den) (uint32_t)(0xFFFFFFFFu / (262144u * (den) / (num)))
#define CR(num, den, spb) {num, den, spb, CLAY_RL(num, den)}
static const clay_res_t CLAY_RES[25] = {
    CR(64, 1, 64), CR(32, 1, 32), CR(16, 1, 16), CR(8, 1, 8), CR(4, 1, 4), CR(2, 1, 2), CR(1, 1, 1), CR(1, 2, 1), CR(1, 4, 1),
    CR(64, 3, 22), CR(32, 3, 11), CR(16, 3, 6), CR(8, 3, 3), CR(4, 3, 2),
    CR(48, 1, 48), CR(24, 1, 24), CR(12, 1, 12), CR(6, 1, 6), CR(3, 1, 3),
    CR(5, 1, 5), CR(10, 1, 10), CR(20, 1, 20), CR(7, 1, 7), CR(14, 1, 14), CR(28, 1, 28)};
#undef CR
#define CLAY_NRES 25u
static const char *const CLAY_RESN[]={"HOLD","1/64","1/32","1/16","1/8","1/4","1/2","1BAR","2BAR","4BAR",
 "1/32D","1/16D","1/8D","1/4D","1/2D","1/32T","1/16T","1/8T","1/4T","1/2T","1/5","1/10","1/20","1/7","1/14","1/28",0};
/* the step of resolution r at the time e (Q16 beats from the bar's start of PLAY) */
static uint32_t clay_step(uint32_t e, uint32_t r)
{
    const clay_res_t *q = &CLAY_RES[r % CLAY_NRES];
    uint32_t bar = e >> 18, pib = e & 262143u;
    if (q->num <= q->den)
        return q->den == 1u ? bar : q->den == 2u ? bar >> 1 : bar >> 2;
    return bar * q->spb + (q->den == 1u ? (pib * q->num) >> 18 : pib * q->num / ((uint32_t)q->den << 18));
}
/* ---- the gestures: a key's or a euclid's, alive until their envelope is out ---- */
typedef struct{uint8_t sh,mod,fl,vel,key,spd;uint16_t len;uint32_t t0,rel;} form_g_t;   /* fl: 1 held 2 latched 4 inverted 8 alive */
#define FORM_G 6u
/* the HITs' bundle this block, for mod.c clay_mod (written by clay_tick): pitch (1/16 semitones), gain (Q15), CUTOFF
 * (Q15 of its range); on: any */
typedef struct { int16_t pit, gain, cut, on; } clay_hb_t;
/* a track's composer: each modulator's last step (lastm, its low 16 bits: a change is a new step), what each adds now
 * (mv), the gestures' clock (clk, Q18 beats); a HIT's drone strike (rt, its velocity), a DRUM track's pad sounding (pn)
 * and the blocks until it is let go (pt); the bundle (hb). (FM: only DRONE CLAY's; FORM's keys not here) */
typedef struct{int16_t lastm[4],mv[4];uint32_t clk;clay_hb_t hb;uint8_t modes,rt,pn,pt;form_g_t g[FORM_G];} form_st_t;
static form_st_t form_st[NTRK];
static uint8_t form_lit[NTRK];                           /* (the panel: the modulators that struck this step, bit m) */
/* modulator m's hit at step s on its EUCLID e (CLAY_EU; "--": none, FUSE: two drawn from the seed and joined) */
static int form_hit(uint32_t e,uint32_t m,uint32_t s,uint32_t seed)
{
 e%=CLAY_NEU;
 if(!e)return 0;
 if(e==1u)return (int)fvm_eu(s,seed+m*37u);
 uint32_t k=CLAY_EUK[e][0],n=CLAY_EUK[e][1],q=s%n;   /* (the owner: on the step, with the others: a hit on the
                                                         * cycle's first step, no rotation by the seed) */
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
/* a HIT's envelope (Q15) at tau (sixteenths, Q8) */
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
/* (BYTE's 1adbf0a6, the owner: "KICK, SNARE .. don't work on ANALOG") a HIT on any engine: the track's own voice moved
 * as a drum, a bundle beside its DEST: its pitch swept from p0 to p1 (MIDI notes) at pace pr (the kick's fall, 0 none),
 * thrown at random each block over nz semitones (the noise of a snare, a hat: the pitch's, any engine's), the track's
 * CUTOFF moved (cut, % of its range), the voice gated by it (clay.c: the drone ducked by the AMOUNT between hits); on a
 * DRUM track the kit's own pad instead (gm, a GM note) */
typedef struct { uint8_t p0, p1; uint16_t pr; uint8_t nz; int8_t cut; uint8_t gm, pad; } clay_hit_t;
static const clay_hit_t CLAY_HIT[12] = {
    {55, 31, 3324, 0, -20, 36, 0},  /* KICK: G3 down to G1 in ~10 ms, darker */
    {55, 50, 5170, 14, 40, 38, 0},  /* SNARE: a short body and its rattle */
    {100, 100, 0, 24, 60, 42, 0},   /* HAT */
    {100, 100, 0, 24, 60, 46, 0},   /* OPEN HAT */
    {76, 76, 0, 18, 40, 39, 0},     /* CLAP */
    {79, 76, 4800, 0, 30, 37, 0},   /* RIM */
    {50, 43, 1847, 0, 0, 45, 0},    /* TOM */
    {96, 36, 1477, 0, 30, 50, 0},   /* ZAP */
    {108, 108, 0, 30, 60, 42, 0},   /* BITHAT */
    {84, 84, 0, 30, 30, 39, 0},     /* NOISE DN */
    {84, 84, 0, 30, 30, 49, 0},     /* SWELL */
    {103, 103, 0, 24, 60, 49, 0}};  /* CRASH */
static const int8_t FORM_ARP[3][6]={{0,3,7,12,0,0},{12,7,3,0,0,0},{0,3,7,12,7,3}};
static const int8_t FORM_CH[5][4]={{0,3,7,0},{0,4,7,0},{0,5,7,0},{0,3,7,10},{0,1,2,3}};
/* what the gestures give the track this block: the NOTEs' semitones (sem, weighted by esum), the HITs' bundle: the
 * loudest one's pitch (hp, MIDI Q4) and its level (ha), their sum (hg, Q15), noise (hn, semitones Q15) and CUTOFF (hc) */
typedef struct { int32_t sem, esum, hp, ha, hg, hn, hc; } clay_out_t;
/* ---- (BYTE) DRONE CLAY: a track's settings (ui pages SEQ > DRONE CLAY, CLAY MOD, CLAY RHYTHM, CLAY TIME; the project's
 * 'C' 'L' block). ON, the drone's OCTave (C0 .. C3 on the track's ROOT), its RATE (HOLD: held; else struck again each
 * CLAY_RES step), RUN (SONG: while the transport plays; FREE: always), PITCH (the drone and its notes, -24 .. +36
 * semitones), the SEED; each modulator's DEST (the matrix's, CLAY_MD), SHAPE (the knob's order: the HITS first,
 * CLAY_SH), EUCLID (CLAY_EU), AMOUNT, DECAY and its own RATE (CLAY_RES) */
enum { CL_ON, CL_OCT, CL_RATE, CL_RUN, CL_PIT, CL_SEED, CL_M0, CLAY_NF = CL_M0 + 24 };
#define CL_MF(m, f) (CL_M0 + 6u * (m) + (f))        /* f: 0 DEST, 1 SHAPE, 2 EUCLID, 3 AMOUNT, 4 DECAY, 5 RATE */
#define CL_SEL CLAY_NF                               /* (the pages' MOD: which modulator their knobs edit; not stored) */
#define CL_PITB 24                                   /* (PITCH stored biased in the project: 0 .. 60) */
typedef int8_t clay_t;                               /* (a setting: -24 .. 127) */
static clay_t clay[NTRK][52];                        /* (FM: in the bss, zeroed at power-on; a row CLAY_NF used: the room
                                                         * V10.8.21's int16 rows had, so that the RAM after it stays where it
                                                         * was: the image's code reaches it as before) */
static int16_t clay_v[NTRK][4] __attribute__((used));   /* (V10.8.21's, kept in place, unused: as above) */
int16_t clay_ui[CLAY_NF];                            /* (the pages' knobs: the selected track's, as int16, clay.c clay_desc;
                                                         * not static: after the RAM's others, as the new ones) */
int16_t clay_km[NTRK];                               /* (the pages' MOD: the modulator CLAY MOD / CLAY RHYTHM edit) */
int8_t clay_kr[NTRK];                                /* (V10.8.31, BYTE's keys) a degree key's semitones over the ROOT */
/* what modulator k of track tr adds now (Q15): mod.c's per-block source, the panel's */
static int32_t clay_vk(uint32_t tr, uint32_t k) { return clamp(form_st[tr % NTRK].mv[k & 3u] * 256, -32767, 32767); }
static uint8_t clay_dn[NTRK], clay_nn[NTRK];          /* the drone's note sounding, a NOTE gesture's (0: none) */
static uint32_t clay_lr[NTRK];                        /* the drone's last strike (RATE's step, its low 16 bits) */
static int clay_on(uint32_t tr) { return tr < NTRK && clay[tr][CL_ON]; }
/* the knob's SHAPE order -> FORM's shape: the HITS (12..23) first, then the TABLES (0..11), then the rest */
static uint32_t clay_sh(int32_t i) { uint32_t k = (uint32_t)clamp(i, 0, FORM_NSH - 1); return k < 12u ? k + 12u : k < 24u ? k - 12u : k; }
static int clay_is_hit(const clay_t *c, uint32_t m) { return clay_sh(c[CL_MF(m, 1u)]) / 12u == FPG_HIT; }
/* every gesture alive, summed: what each modulator adds now (st->mv), the NOTEs' and the HITs' (o). Each its time in
 * its modulator's RATE's steps; a HIT's in sixteenths, whatever its RATE (a kick is a kick), DECAY its length */
static void clay_eval(form_st_t*st,const clay_t*c,clay_out_t*o,uint32_t at)
{
 uint32_t i;int32_t M[4]={0,0,0,0};
 o->sem=o->esum=o->hp=o->ha=o->hg=o->hn=o->hc=0;
 for(i=0;i<FORM_G;i++){
  form_g_t*g=&st->g[i];if(!(g->fl&8u))continue;
  uint32_t m=g->mod&3u,sh=g->sh,pg=sh/12u,k=sh%12u,d16=(st->clk-g->t0)>>2;
  int32_t amt=clamp(c[CL_MF(m,3u)],0,127),dec=clamp(c[CL_MF(m,4u)],0,127),e;
  uint32_t tau=(uint32_t)(umul64(d16,CLAY_RES[(uint32_t)clamp(c[CL_MF(m,5u)],0,CLAY_NRES-1)].rl)>>24);   /* (its RATE's steps, Q8) */
  int32_t gn=amt*g->vel*2;if(g->fl&1u)gn+=gn*(int32_t)at/127;
  tau=g->spd==1u?tau<<1:g->spd==2u?tau>>1:tau;
  if(pg==FPG_HIT){
   uint32_t th=d16>>6;                                /* (sixteenths, Q8; DECAY: 0 x1.5 as fast .. 64 x1 .. 127 half) */
   if(th>=8192u){g->fl=0;continue;}
   th=th*(384u-2u*(uint32_t)dec)>>8;th=g->spd==1u?th<<1:g->spd==2u?th>>1:th;
   if(th>=2048u){g->fl=0;continue;}
   const clay_hit_t*h=&CLAY_HIT[k];int32_t A=form_henv(k,th)*gn>>15;
   o->hg+=A;o->hn+=(int32_t)h->nz*A;o->hc+=(int32_t)h->cut*A;
   if(A>o->ha){o->ha=A;o->hp=(int32_t)h->p1*16+(((int32_t)h->p0-(int32_t)h->p1)*16*(h->pr?form_fexp((th*h->pr)>>8):32767)>>15);}
   M[m]+=A;continue;}
  if(form_oneshot(sh)){if(tau>=2048u){g->fl=0;continue;}e=32767;}
  else if((g->fl&3u)||(g->len&&tau<g->len))e=32767;
  else{if(!g->rel)g->rel=st->clk|1u;uint32_t rt=(st->clk-g->rel)>>8,T=64u+(uint32_t)dec*32u;e=form_fexp(rt*369u/T);if(e<200){g->fl=0;continue;}}
  int32_t a=e*gn>>15;
  if(pg==FPG_NOTE){
   int32_t sem;
   if(k<3u)sem=FORM_ARP[k][(tau>>8)%(k==2u?6u:4u)];
   else if(k==3u){static const int8_t R[7]={0,3,5,7,10,12,15};sem=R[fvm_hash(g->t0+(tau>>8))%7u];}
   else if(k<9u){const int8_t*ch=FORM_CH[k-4u];sem=ch[(tau>>6)%(k==7u||k==8u?4u:3u)];}
   else if(k==9u)sem=12;else if(k==10u)sem=7;else sem=-(int32_t)(tau>=512u?12:tau*12u/512u);
   o->sem+=sem*341*a>>15;o->esum+=a;
   M[m]+=sem*a/12;continue;}
  if(pg==FPG_FM){                                     /* (BYTE's review: the seven sounded alike) a sine on DEST at its ratio
                                                         * of the RATE's step: x1/2 once in two steps .. x7 seven times a step */
   static const uint8_t R2[7]={1,2,3,4,6,10,14};int32_t v=sine_i(tau*(uint32_t)R2[k%7u]<<23);if(g->fl&4u)v=-v;
   M[m]+=v*a>>15;continue;}
  if(sh==FS_REST){M[m]-=a;continue;}
  int32_t v=form_shape(sh,tau,g->t0);if(g->fl&4u)v=-v;
  M[m]+=v*a>>15;
 }
 for(i=0;i<4u;i++)st->mv[i]=(int16_t)clamp(M[i]>>8,-1024,1024);
}
/* a new DRONE CLAY: a drone on C1 held, a kick on the cutoff at 1/16 on the beat, an arp's notes, a slow triangle on
 * the sound's KNOB 2, a hat on its shape (DEST SHAPE EUCLID AMOUNT DECAY RATE each) */
static void clay_defaults(clay_t *c)
{
    static const int8_t D[CLAY_NF] = {1, 1, 0, 0, 0, 7,   2, 0, 3, 100, 10, 2,   0, 36, 8, 90, 14, 2,
                                      13, 26, 3, 60, 30, 2,   3, 2, 12, 70, 10, 2};
    uint32_t i;
    for (i = 0; i < CLAY_NF; i++) c[i] = D[i];
}
