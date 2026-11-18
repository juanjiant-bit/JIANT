/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducible host work, NOT FM-1 CPU percentages. One process per family.
 * Engine sections: dry channel, four held notes (DRUM-X eight lanes retriggered
 * every 16 blocks), default preset. FX/seq/mix reference sections: cpu_vg.c.
 * Compile with Callgrind headers and -O2; see performance report for commands. */
#define CPU_VG_MAIN reference_cpu_main
#include "cpu_vg.c"
#undef main
int main(int argc, char **argv)
{
    if(argc>1)return reference_cpu_main();
    (void)argv;
    for(unsigned e=0;e<NENGINES;e++)if(eng_ok(e)&&e!=ENGI_INPUT) {
        vg_quiet();trk_live=6;sys_load=LOAD_FULL;voice_headroom=load_cores2=1;
        load_tight=guard_cap=vh_held=0;load_all_q8=load_pred_q8=load_avg_q8=0;
        memset(vh_c,0,sizeof vh_c);memset(mc_tk,0,sizeof mc_tk);memset(mc_nv,0,sizeof mc_nv);
        vg_part(0,e,0,e==ENGI_DRUM?8:4);
        trk[0].p[P_CHOR]=trk[0].p[P_DLY]=trk[0].p[P_REV]=0;
        for(unsigned b=0;b<32;b++)mix_block(vo,CTL);
        unsigned total=0,minimum=8,maximum=0;
        char name[80];snprintf(name,sizeof name,"channel/%u-%s",e,ENGINES[e]->name);
        CALLGRIND_ZERO_STATS;CALLGRIND_START_INSTRUMENTATION;
        for(unsigned b=0;b<NB;b++) {
            if(e==ENGI_DRUM && !(b%16))for(unsigned l=0;l<8;l++)trk_note_on(&trk[0],DRUM_LANE_NOTE[l],100);
            mix_part(&trk[0],CTL);
            unsigned nv=0;for(unsigned j=0;j<NVOICE;j++)nv+=trk[0].v[j].active;
            total+=nv;if(nv<minimum)minimum=nv;if(nv>maximum)maximum=nv;
        }
        CALLGRIND_DUMP_STATS_AT(name);CALLGRIND_STOP_INSTRUMENTATION;
        printf("%s blocks=%u samples=%u voices_min=%u max=%u avg=%.2f\n",name,NB,NB*CTL,minimum,maximum,(double)total/NB);
    }
    return 0;
}
