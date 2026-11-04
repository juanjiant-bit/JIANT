/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 JIANT port: Copyright (C) 2026 Israel Peña (pedeplei1281) */
/* SMK-37: the free take's UI half (the audio half and what it does: smk_take.c). Included by ui_input.c. */

static int smk_project_empty(void)                     /* no track has a note or a hit */
{
    uint32_t i, k;
    for (i = 0; i < NTRK; i++)
        for (k = 0; k < NSTEP; k++) {
            const step_t *s = &trk[i].step[k];
            if (s->time == ST_NOTE && (s->n || s->hit))
                return 0;
        }
    return 1;
}

/* REC tapped (rec_tap, past its own checks): a take closed or cancelled, or one armed; -1 = not the take's (as ever) */
static int smk_ft_rec_tap(uint32_t bit)
{
    if (ft_on) {                                        /* REC on the "1" after the last bar: the loop */
        ft_close_req = 1;
        return 1;
    }
    if (ft_arm) {                                       /* armed, no note yet: REC again disarms */
        ft_arm = FT_NONE;
        song.rec &= (uint8_t)~bit;
        ui_message("REC OFF");
        ui.force = 1;
        return 1;
    }
    if (song.playing || seq_counting() || transport_req == 1u || song.seq_mode || song.g[G_CLOCK] || (song.rec & bit))
        return -1;                                      /* playing, a SEQ page, an external clock, disarming: as ever */
    if (!smk_project_empty() && cin_bars)
        return -1;                                      /* (notes there and a COUNT-IN: REC counts in, as ever) */
    song.rec |= (uint8_t)bit;                           /* (armed before the take is: seq.c ft_note_on reads it) */
    ft_arm = smk_project_empty() ? FT_ARM_FREE : FT_ARM_NOTE;
    ui_message(ft_arm == FT_ARM_FREE ? "PLAY, REC CLOSES THE LOOP" : "THE FIRST NOTE STARTS");
    ui.force = 1;
    return 1;
}

/* PLAY: a take running is dropped (1: PLAY does nothing else); armed, PLAY starts as ever (recording at the tempo set) */
static int smk_ft_play(void)
{
    if (ft_on) {
        ft_drop_req = 1;
        return 1;
    }
    ft_arm = FT_NONE;
    return 0;
}

/* each frame: what the take says on the top bar */
static void smk_ft_poll(void)
{
    static uint32_t said = 0xFFFFFFFFu;
    uint32_t r = ft_res;
    char a[24], b[8];
    if (r) {
        ft_res = 0;
        if (r == 0xFFFFu)
            ui_message("NO LOOP: TOO SHORT OR LONG");
        else if (r == 0xFFFEu)
            ui_message("TAKE DROPPED");
        else {
            fmt_int(a, (int32_t)(r >> 8));
            str_cpy(a + str_len(a), (r >> 8) == 1u ? " BAR  " : " BARS  ", 8);
            fmt_int(b, (int32_t)((r & 0xFFu) + 39u));
            str_cpy(a + str_len(a), b, 8);
            ui_say(a, " BPM");
        }
        ui.force = 1;
    }
    if (ft_on) {
        uint32_t s = ft_t * CTL / FS;
        if (s != said) {
            said = s;
            fmt_int(b, (int32_t)s);
            str_cpy(a, "FREE TAKE ", sizeof a);
            str_cpy(a + str_len(a), b, 8);
            ui_say(a, " S  REC: LOOP");
        }
    } else {
        said = 0xFFFFFFFFu;
    }
}
