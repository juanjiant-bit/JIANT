/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * JIANT (FELUCCA TONIC): the song as SLOOP 2.4.1 plays it (isod89/sloop-fm1, GPL-3.0: arranger.h, arranger_scene.c),
 * rebuilt on Felucca's sequencer. */
/* The song (docs/TONIC-SONG-PLAN.md, 4a / 4b). Up to CHAIN_ROWS rows, each a section (one of the four saved projects,
 * A..D) and the bars it plays (1..64, 4/4 bars of 1/16 steps). A section is the whole project: the four tracks' sounds
 * (engine, parameters, FM6 patch), their steps and the motion; the song's tempo and globals stay (not the sections').
 *   main loop  chain_prepare (PLAY on SONG, the editor's SONG start): keeps the music as it is (song_keep), decodes the
 *              first row's section into sec_stage and arms; song_poll: the next row's section into sec_stage, a bar
 *              at least before it is due; when the song has ended (or was stopped), the kept music comes back
 *   audio ISR  chain_start (with PLAY): the staged section in, every track from step 0; chain_tick each block: the bar
 *              clock (16 steps of 1/16 at the tempo, the remainder carried), and at the end of a row's bars the next
 *              row's section in, from step 0 (seq_tick starts each track the carried samples into its first step).
 *              A section the main loop has not staged in time: the row plays its section another bar
 * The last row's end stops the transport. The editable music is the song's while it plays (STOP TO EDIT).
 * This file: the state and the rows; song_play.c (included at the end of seq.c): the ISR's part; song_main.c
 * (included by project.c): the main loop's. */
static chain_config_t chain_config;
typedef struct {                      /* a track of a section, as the ISR puts it in */
    int16_t p[P_COUNT];
    uint8_t engine, preset;
    step_t step[NSTEP];
    uint8_t fm6[FP_SIZE + 1u];        /* (unpacked: eng_fm6.c fm6_set_patch) */
} sec_trk_t;
static struct {
    sec_trk_t t[NTRK];
    motion_store_t motion;
    volatile uint8_t ready;           /* main: staged for row `row`; ISR: taken (0) */
    uint8_t row;
} sec_stage __attribute__((section(".pool")));
static struct {
    chain_config_t config;
    volatile uint8_t armed, running, row, remaining;   /* remaining: the bars of the row still to play */
    volatile uint8_t ended;           /* ISR: the song ended or stopped (song_poll brings the music back) */
    volatile uint8_t applied;         /* ISR: a section went in (song_poll: the FM6 SLOTs, the screen) */
    uint8_t rec, kept;
    uint32_t carry;                   /* samples past the bar line at the end of the block: the first step's */
    uint32_t bar_pos;                 /* samples into the bar */
} chain;

static void chain_start(void);                   /* song_play.c (end of seq.c) */
static void chain_stop(void);
static void chain_tick(uint32_t n);
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);   /* seq.c (motion.c uses it) */

static void chain_defaults(chain_config_t *c)
{
    uint32_t i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < CHAIN_ROWS; i++)
        c->row[i].bars = 1;
}
static int chain_valid(const chain_config_t *c)
{
    uint32_t i;
    if (c->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < c->count; i++)
        if (c->row[i].slot >= 4u || !c->row[i].bars || c->row[i].bars > CHAIN_BARS)
            return 0;
    return 1;
}
static const step_t *seq_steps(const track_t *t) { return t->step; }   /* (a section's steps are the track's) */

