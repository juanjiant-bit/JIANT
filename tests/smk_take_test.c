/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT on the M-Vave SMK-37 (BOARD_SMK37): Israel Peña (pedeplei1281) */
/* SMK-37 free take (firmware/src/smk_take.c) on the host, the sequencer stood in for: the loop's length gives 1, 2 or 4
 * bars at the tempo nearest the one set (within 3 % it is kept), the notes go to the nearest 1/16 step with their
 * lengths as TIEs, every track takes the loop's length, a take too short fits nothing, PLAY drops a take, and on a
 * project with notes the first note starts the transport and is kept for step 1. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FS 44100u
#define CTL 32u
#define NTRK 6u
#define NSTEP 64u
enum { ST_NOTE, ST_TIE, ST_REST };
#define SF_ACCENT 1u
enum { P_SLEN, P_SDIV, P_AMODE, P_COUNT };
enum { G_BPM, G_COUNT };
typedef struct { uint8_t note[4], n, time, flags, vel, hit, acc; } step_t;
typedef struct { int16_t p[P_COUNT]; step_t step[NSTEP]; uint8_t seq_active; } track_t;
static struct { uint8_t sel, rec, playing; int16_t g[G_COUNT]; } song;
static const struct { int16_t min, max; } GP[G_COUNT] = {{40, 240}};
static track_t trk[NTRK];
static uint8_t cin_note[NTRK][4], cin_vel[NTRK][4], cin_n[NTRK], cin_up[NTRK], cin_oct[NTRK][4], cin_flush;
static int starts;
static uint32_t trk_index(const track_t *t) { return (uint32_t)(t - trk); }
static int drum_track(const track_t *t) { (void)t; return 0; }
static int seq_counting(void) { return 0; }
static void step_clear(step_t *st) { memset(st, 0, sizeof *st); st->time = ST_REST; }
static void rec_drum(track_t *t, step_t *s, uint32_t note, uint32_t vel, int32_t mic) { (void)t; (void)s; (void)note; (void)vel; (void)mic; }
static void seq_start(void) { song.playing = 1; starts++; }
static struct { uint32_t buttons; } fm1_in;            /* (REC is bit 0 here: ft_btn_mask) */
static uint32_t fm1_ms;
#include "../firmware/src/smk_take.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("%-78s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
static void reset(int bpm)
{
    memset(trk, 0, sizeof trk);
    for (uint32_t i = 0; i < NTRK; i++) {
        trk[i].p[P_SLEN] = 16;
        for (uint32_t k = 0; k < NSTEP; k++)
            step_clear(&trk[i].step[k]);
    }
    song.sel = 2;
    song.rec = 1u << 2;
    song.playing = 0;
    song.g[G_BPM] = (int16_t)bpm;
    ft_on = 0;
    ft_res = 0;
    starts = 0;
}
static void blocks(uint32_t n) { while (n--) ft_block(); }
static uint32_t secs(double s) { return (uint32_t)(s * FS / CTL + 0.5); }

/* a free take of `quarters` quarter notes at `bpm`, each held half a beat, closed on the next downbeat */
static void take(double bpm, uint32_t quarters)
{
    double beat = 60.0 / bpm;
    uint32_t q;
    ft_arm = FT_ARM_FREE;
    for (q = 0; q < quarters; q++) {
        ft_note_on(&trk[2], 60u + q % 12u, 100);
        blocks(secs(beat / 2));
        ft_note_off(&trk[2], 60u + q % 12u);
        blocks(secs(beat / 2));
    }
    ft_btn_mask = 1u;                                   /* REC pressed on the downbeat, held 150 ms, let go */
    fm1_in.buttons = 1u;
    ft_block();
    blocks(secs(0.15));
    fm1_in.buttons = 0;
    blocks(2);
}

int main(void)
{
    uint32_t k, ok;
    reset(120);
    take(120.0, 8);                                     /* 2 bars at 120 */
    check("8 quarters at 120 BPM: 2 bars, 120 BPM", ft_res == (2u << 8 | (120u - 39u)) && song.g[G_BPM] == 120);
    for (k = 0, ok = 1; k < NTRK; k++)
        ok &= trk[k].p[P_SLEN] == 32 && trk[k].p[P_SDIV] == 2;
    check("... every track 32 steps of 1/16", ok);
    for (k = 0, ok = 1; k < 32u; k++) {
        const step_t *s = &trk[2].step[k];
        ok &= k % 4u == 0u ? s->time == ST_NOTE && s->n == 1u && s->note[0] == 60u + k / 4u :
              k % 4u == 1u ? s->time == ST_TIE : s->time == ST_REST;
    }
    check("... a note on every 4th step, held a step longer (a TIE), rests between", ok);
    check("... the loop plays at once, REC off", starts == 1 && song.playing && !song.rec);
    check("... closed on REC's press, not its release 150 ms later; the UI drops that press's tap",
          ft_swallow == 1u && trk[2].p[P_SLEN] == 32);
    ft_swallow = 0;

    reset(120);
    take(123.0, 8);
    check("2 bars at 123 BPM (2.5 % off 120): the tempo set kept, 120", (ft_res >> 8) == 2u && song.g[G_BPM] == 120);

    reset(120);
    take(92.0, 8);
    check("2 bars at 92 BPM: 2 bars at 92 (46 or 184 further from 120)", (ft_res >> 8) == 2u && song.g[G_BPM] == 92);

    reset(100);
    take(50.0, 4);                                      /* 4 quarters at 50 (4.8 s): 1 bar at 50 or 2 bars at 100 */
    check("4 quarters at 50 BPM, 100 set: read as 2 bars at 100", (ft_res >> 8) == 2u && song.g[G_BPM] == 100);
    reset(120);
    take(200.0, 4);                                     /* 1.2 s: 1 bar at 200 (2 bars would be 400) */
    check("4 quarters at 200 BPM: 1 bar at 200 (2 bars: past 240)", (ft_res >> 8) == 1u && song.g[G_BPM] == 200);

    reset(120);
    ft_arm = FT_ARM_FREE;
    ft_note_on(&trk[2], 60, 100);
    blocks(secs(0.2));
    ft_close_req = 1;
    ft_block();
    check("a take of 0.2 s: no loop, nothing changed, REC off", ft_res == 0xFFFFu && trk[2].p[P_SLEN] == 16 &&
          !song.playing && !song.rec);

    reset(120);
    ft_arm = FT_ARM_FREE;
    ft_note_on(&trk[2], 60, 100);
    blocks(secs(1.0));
    ft_drop_req = 1;
    ft_block();
    check("PLAY during a take: dropped, nothing written", ft_res == 0xFFFEu && !ft_on && trk[2].step[0].time == ST_REST);

    reset(120);
    ft_arm = FT_ARM_FREE;
    ft_note_on(&trk[1], 60, 100);
    check("a note on another track than the selected one: no take", !ft_on && ft_arm == FT_ARM_FREE);

    reset(120);
    ft_arm = FT_ARM_FREE;
    ft_note_on(&trk[2], 60, 100);
    blocks(FT_BLOCKS + 5u);
    check("a take past 24 s closes by itself", !ft_on && ft_res != 0u);

    reset(120);
    ft_arm = FT_ARM_NOTE;
    k = (uint32_t)ft_note_on(&trk[2], 64, 90);
    ok = ft_note_on(&trk[2], 67, 90);                   /* (its chord's next note, the same block) */
    check("a project with notes: the first note starts the transport, kept for step 1 with its chord",
          k && ok && starts == 1 && song.playing && cin_flush && cin_n[2] == 2u && cin_note[2][0] == 64u &&
          cin_note[2][1] == 67u && cin_vel[2][0] == 90u);
    printf("%s\n", bad ? "SMK TAKE TEST FAILED" : "smk take test passed");
    return bad != 0;
}
