/* SPDX-License-Identifier: GPL-3.0-only */
/* Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
static int test_song_save_ui(void)
{
    int bad = 0, ok;
    uint32_t target;
    char a[24], b[24];
    ui_power_on();
    song_cur = 0;
    memset(&song_idx, 0, sizeof song_idx);
    memset(&chain, 0, sizeof chain);
    live.cur = -1;
    name_close();
    nav_put(SEC_SAVE, "PROJECTS"); frame();          /* (V10.7.20: the SAVE SONG AS page is gone; PROJECTS does its job) */
    ok = cur_page()->graph == GR_SLOTS && save_level() == SV_LIB && !ui.act && song_save_target == song_cur &&
         !nav_put(SEC_SAVE, "SAVE SONG AS");
    {
        uint32_t i;
        for (i = 0; i < NPAGES; i++) ok &= strcmp(PAGES[i].title, "SAVE SONG AS") != 0;
    }
    bad += check("PROJECTS is the save page (no SAVE SONG AS page), this project picked first", ok);
    target = song_save_target;
    turn(EN_K1, 1); frame();
    ok = song_save_target == target + 1u && song_cur == 0 && live.cur == -1;
    turn(EN_K1, 40); frame();
    ok &= song_save_target == target + 2u && song_cur == 0;          /* (a detent a project) */
    turn(EN_K1, -1); frame();
    ok &= song_save_target == target + 1u && song_cur == 0;
    bad += check("destination knob does not open another song or change the source", ok);
    song_save_loop = 1;                              /* (left from a save before: SAVE starts at SONG again) */
    turn(EN_K4, 1); press(B_OCTUP); frame();
    ok = name_on() && nm.kind == NK_SONG_SAVE && nm.slot == song_save_target && !song_save_loop;
    nm_title(a);
    ok &= strstr(a, "SAVE SONG TO PROJECT") != 0;
    press(B_OCTDN); frame();
    ok &= !name_on() && !ui.confirm && song_cur == 0;
    bad += check("empty destination opens naming; cancel keeps the source", ok);
    turn(EN_K4, 1); press(B_OCTUP); frame();         /* NAME: KNOB 3 right LOOP, left SONG; nothing else moves */
    ok = name_on() && !song_save_loop;
    turn(EN_K3, 1); frame();
    nm_title(a);
    ok &= song_save_loop && strstr(a, "SAVE LOOP TO PROJECT") != 0 && name_on() && song_cur == 0 && live.cur == -1;
    turn(EN_K3, 1); frame();
    ok &= song_save_loop == 1;
    turn(EN_K3, -1); frame();
    nm_title(a);
    ok &= !song_save_loop && strstr(a, "SAVE SONG TO PROJECT") != 0;
    press(B_OCTDN); frame();
    ok &= !name_on();
    bad += check("NAME for a song save: KNOB 3 switches SONG / LOOP (its title follows), cancel writes nothing", ok);
    song_idx.rows[song_save_target].count = 1;
    turn(EN_K4, 1); press(B_OCTUP); frame();
    ok = ui.confirm == CF_OVR_SONG && !name_on();
    confirm_text(a, b);
    ok &= strstr(a, "OVERWRITE PROJECT") != 0 && !strcmp(b, "NEXT: K3 SONG OR LOOP");
    press(B_OCTDN); frame();
    ok &= !ui.confirm && !name_on();
    bad += check("occupied destination asks to overwrite (SONG or LOOP comes on NAME); cancel writes nothing", ok);
    turn(EN_K4, 1); press(B_OCTUP); frame();
    press(B_OCTUP); frame();
    ok = name_on() && nm.kind == NK_SONG_SAVE;
    turn(EN_K3, 1); frame();
    ok &= song_save_loop;                            /* (a LOOP over a whole song: the owner's use; what it writes:
                                                      * tests/song_draft_test.c, on the flash model) */
    press(B_OCTUP); frame();
    ok &= name_on() && strstr(ui.msg, "NO FLASH") != 0;  /* ui_test has no flash: never a false SAVED */
    press(B_OCTDN); frame();
    bad += check("overwrite confirmation leads to naming; failed save keeps the name open", ok);
    song_idx.rows[song_save_target].count = 0;
    turn(EN_K1, -40); frame();
    ok = song_save_target == song_cur;
    turn(EN_K4, 1); press(B_OCTUP); frame();
    bad += check("current project is available as an explicit save destination", ok && (name_on() || ui.confirm == CF_OVR_SONG));
    if (name_on()) { press(B_OCTDN); frame(); }
    else if (ui.confirm) { press(B_OCTDN); frame(); }
    song_save_target = 2;
    song.playing = 1;
    turn(EN_K4, 1); press(B_OCTUP); frame();
    bad += check("playing refuses before naming or overwrite confirmation", !name_on() && !ui.confirm && strstr(ui.msg, "STOP TO SAVE") != 0);
    song.playing = 0;
    ui.force = 1;                                    /* (V10.7.23: COLORS, the one skin) */
    frame();
    ui_power_on();                                   /* LOAD and ERASE act on the picked project */
    nav_put(SEC_SAVE, "PROJECTS"); frame();
    ok = song_save_target == song_cur;
    turn(EN_K1, 1); frame();
    target = song_save_target;
    turn(EN_K2, 1); frame();
    ok &= ui.act == 2u && act_ready() && target == song_cur + 1u;
    turn(EN_K3, 1); press(B_OCTUP); frame();
    ok &= ui.confirm == CF_ERASE_SONG && ui.confirm_trk == target && song_cur == 0;
    press(B_OCTDN); frame();
    ok &= !ui.confirm && song_cur == 0 && song_save_target == target;
    bad += check("PROJECTS: LOAD ready for the picked project; ERASE asks for it and can be cancelled, nothing opened", ok);
    ui_power_on();
    song_draft = 1; song_save_target = song_cur;
    nav_put(SEC_SAVE, "PROJECTS"); frame();
    target = song_cur;
    turn(EN_K1, 1); frame();
    bad += check("PROJECTS destination browsing preserves the playing draft buffers",
                 song_draft && song_cur == target && song_save_target == target + 1u);
    turn(EN_K2, 1); frame();
    bad += check("PROJECTS LOAD availability depends on transport, not the draft's selected part", act_ready());
    turn(EN_K3, 1); press(B_OCTUP); frame();
    bad += check("PROJECTS ERASE targets the selected saved project and can be cancelled",
                 ui.confirm == CF_ERASE_SONG && ui.confirm_trk == song_save_target);
    press(B_OCTDN); frame();
    song.playing = 1;
    turn(EN_K3, 1); press(B_OCTUP); frame();
    bad += check("PROJECTS ERASE refuses during playback without discarding the draft",
                 !ui.confirm && song_draft && strstr(ui.msg, "STOP TO ERASE") != 0);
    song.playing = 0;
    song_draft = 0;
    return bad;
}
