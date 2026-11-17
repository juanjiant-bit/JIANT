/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducible whole-mixer punch-in profile / audio differential.
 * cc -O2 -w -Wno-implicit-function-declaration -Itests -Ibuild/gen -Ifirmware/src
 *    tests/punch_cpu.c -lm -o build/host/punch-cpu
 * Add -DPUNCH_CALLGRIND and Valgrind's include path to measure the middle 256 blocks.
 * Arguments: effect bit mask, live tracks (4/6), transition mode (0/1).
 * Compare this same source compiled against the previous release's tests/headers.
 * Host lacks the device's timer: reset its speculative load before pressing FX;
 * this profiles work, not a hardware CPU percentage or deadline guarantee. */
#define UI_TEST_NO_MAIN
#include <ui_test.c>
#ifdef PUNCH_CALLGRIND
#include <valgrind/callgrind.h>
#else
#define CALLGRIND_ZERO_STATS
#define CALLGRIND_START_INSTRUMENTATION
#define CALLGRIND_DUMP_STATS_AT(x)
#define CALLGRIND_STOP_INSTRUMENTATION
#endif
int main(int argc, char **argv)
{
    static int32_t out[2 * CTL];
    unsigned bits = argc > 1 ? strtoul(argv[1], 0, 0) : 0;
    unsigned tracks = argc > 2 ? atoi(argv[2]) : 4;
    unsigned transitions = argc > 3 ? atoi(argv[3]) : 0, b, j;
    uint64_t hash = 1469598103934665603ull;
    ui_power_on(); trks_put(tracks == 6);
    cpu_test_song(); song.master_q12 = 4096;
    transport_req = 1; ui_input();
    for (b = 0; b < 1024; b++) mix_block(out, CTL);
    load_all_q8 = load_pred_q8 = 0;
    perf_held = bits;
    if (!transitions) for (b = 0; b < 512; b++) mix_block(out, CTL);
    CALLGRIND_ZERO_STATS; CALLGRIND_START_INSTRUMENTATION;
    for (b = 0; b < (transitions ? 1536u : 256u); b++) {
        if (transitions) {
            perf_held = b < 64 || (b >= 256 && b < 640) ? bits : 0;
            perf_k[0] = b >= 300 && b < 400 ? -60 : b >= 400 && b < 500 ? 70 : 0;
            perf_k[1] = b >= 500 && b < 590 ? 60 : 0;
            perf_k[2] = b >= 650 && b < 710 ? 50 : 0;
            /* Same simulated spare CPU in reference and candidate. */
            load_all_q8 = load_pred_q8 = 0;
        }
        mix_block(out, CTL);
        for (j = 0; j < 2 * CTL; j++) { hash ^= (uint32_t)out[j]; hash *= 1099511628211ull; }
    }
    CALLGRIND_DUMP_STATS_AT("punch"); CALLGRIND_STOP_INSTRUMENTATION;
    printf("bits=%u tracks=%u transitions=%u hash=%016llx act=%u mode=%u\n",
           bits, trk_live, transitions, (unsigned long long)hash, pf.act, pf.mode);
    return 0;
}
