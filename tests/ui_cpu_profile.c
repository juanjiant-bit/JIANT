/* SPDX-License-Identifier: GPL-3.0-only */
/* Host drawing work includes the screen harness' layout hooks and LCD stub;
 * no SPI/interrupt timing. Never compare these counts to FM-1 deadline %. */
#define ANIM_TEST_MAIN animation_reference_main
#include "anim_test.c"
#undef main
#include <valgrind/callgrind.h>
int main(void)
{
    static const char *screens[]={"home_macros","home_trk6","save_timeline_loop","roll_playing","v6_fm6_op","vx_kit_hits","gen"};
    rep=fopen("/dev/null","w");al_off=1;
    for(unsigned s=0;s<sizeof screens/sizeof screens[0];s++)for(unsigned mode=0;mode<3;mode++) {
        unsigned step=mode==0?15:mode==1?50:100;
        prep(scr_id(screens[s]));song.playing=1;
        load_all_q8=load_avg_q8=(mode==0?40:mode==1?65:80)<<8;ui_hold_poll();
        for(unsigned i=0;i<20;i++)at(fm1_ms+step);
        char name[96];snprintf(name,sizeof name,"ui/%s/%ums",screens[s],step);
        sent=0;clock_t begin=clock();
        CALLGRIND_ZERO_STATS;CALLGRIND_START_INSTRUMENTATION;
        for(unsigned i=0;i<60;i++) {
            for(unsigned t=0;t<NTRK;t++)trk[t].peak=2000+(i*37+t*911)%5000;
            at(fm1_ms+step);
        }
        CALLGRIND_DUMP_STATS_AT(name);CALLGRIND_STOP_INSTRUMENTATION;
        printf("%s frames=60 pixels_per_frame=%llu host_us_per_frame=%.1f\n",name,(unsigned long long)sent/60,1e6*(clock()-begin)/CLOCKS_PER_SEC/60);
    }
    return 0;
}
