/* SPDX-License-Identifier: GPL-3.0-only
 * Four single-note lines with overlapping releases. A new note should reuse
 * that track's released voice before fading another track's audible tail. */
#define main hostsim_main
#include "hostsim.c"
#undef main
int main(int argc,char **argv)
{
    uint32_t budget=argc>1?(uint32_t)atoi(argv[1]):8u;
    uint32_t p,b,kills; int32_t out[2*CTL]; voice_t *other=0;
    host_tracks_init(); voice_headroom=budget==12; sys_load=LOAD_FULL;
    if(budget!=8&&budget!=12) return 4;
    for(p=0;p<4;p++) {
        host_preset(&trk[p],p&1?ENGI_VOICE:ENGI_FM6,0);
        trk[p].p[P_VOICE]=V_POLY; trk[p].p[P_REL]=100;
        trk_note_on(&trk[p],48+p,100);
    }
    for(b=0;b<128;b++) mix_block(out,CTL);
    for(p=0;p<4;p++) {
        trk_note_off(&trk[p],48+p);
        if(budget==12) {
            trk_note_on(&trk[p],54+p,100);
            trk_note_off(&trk[p],54+p);
        }
        trk_note_on(&trk[p],60+p,100);
    }
    for(p=0;p<NVOICE;p++) if(trk[0].v[p].active&&trk[0].v[p].note==48) other=&trk[0].v[p];
    if(!other||voices_busy()!=budget) return 2;
    trk_note_off(&trk[3],63); kills=voice_kills;
    trk_note_on(&trk[3],67,100);
    printf("voice-fairness: busy=%u cross_track_fades=%u oldest_other_tail_stage=%u\n",
           voices_busy(),voice_kills-kills,other->stage);
    if(voice_kills!=kills||other->stage==4||voices_busy()!=budget) return 1;
    for(p=0;p<3;p++) {
        uint32_t i,held=0;
        for(i=0;i<NVOICE;i++)held+=trk[p].v[i].active&&trk[p].v[i].gate&&trk[p].v[i].note==60+p;
        if(held!=1) return 3;
    }
    printf("voice-fairness: other tracks retain their lead and release PASS\n");
    /* If this part has no released slot, the normal global fallback must
     * still admit a new note and protect the other tracks' held leads. */
    for(p=0;p<NVOICE;p++) if(trk[3].v[p].active&&trk[3].v[p].stage!=4u) trk[3].v[p].gate=1;
    kills=voice_kills; trk_note_on(&trk[3],71,100);
    if(voice_kills!=kills+1||other->stage!=4||voices_busy()!=budget) return 5;
    printf("voice-fairness: global fallback remains available when no own release exists PASS\n");
    return 0;
}
