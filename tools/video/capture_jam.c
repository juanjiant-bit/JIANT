// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.2) the song mode's screens for the owner's check page: SEQ > STEP's sequence card, JAM FREE / REC / LOCK (the
 * player's over the take, YOU) / a track's REC and its OCT- UNDO, TAKE (the change's bar zoomed under the take). A GEN piece in four parts, played as a jam. Writes NAME.ppm into DIR.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/jamshots/cap tools/video/capture_jam.c -lm
 *   build/jamshots/cap build/jamshots */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static const char *dir;
static void run(uint32_t blocks)
{
    int32_t o[2 * CTL];
    uint32_t i;
    for (i = 0; i < blocks; i++) {
        mix_block(o, CTL);
        if (!(i & 15u)) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); song_poll(); ui_draw(); }
    }
}
static void until_step(uint32_t s)                        /* the playhead to the take's (or the song's) 16th s */
{
    int32_t o[2 * CTL];
    uint32_t i, len = jam_len() ? jam_len() : 16u;
    for (i = 0; i < 400000u && (gl_step_at(0) - (jam_lock ? jam_base : 0u)) % len != s; i++) {
        mix_block(o, CTL);
        if (!(i & 15u)) { host_ticks += 16000u; fm1_ms += 16u; ui_input(); song_poll(); ui_draw(); }
    }
}
static void shot(const char *name)
{
    char p[512];
    FILE *f;
    uint32_t i;
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    ui.force = 1; ui_draw();
    snprintf(p, sizeof p, "%s/%s.ppm", dir, name);
    f = fopen(p, "wb");
    fprintf(f, "P6 240 240 255\n");
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        uint8_t px[3] = {(uint8_t)((c >> 11) * 255u / 31u), (uint8_t)(((c >> 5) & 63u) * 255u / 63u), (uint8_t)((c & 31u) * 255u / 31u)};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    fprintf(stderr, "%s\n", name);
}
int main(int argc, char **argv)
{
    uint32_t k;
    dir = argc > 1 ? argv[1] : "build/jamshots";
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    song.master_q12 = 4096;
    gp.genre = 1; gp.seed = 7; gp.energy = 70; gp.form = 1;   /* a piece in parts A..D */
    gen_go();
    ui.confirm = 0; frames(30);
    memset(clip_now, 0, sizeof clip_now);
    favorites.factory[15][31] = (uint8_t)((3u ^ 1u) << 2);  /* CAMBIO 1/4 */

    song.sel = 1; go_title("STEP"); frames(4);
    transport_req = 1; ui_input(); run(3000);
    turn(EN_K1, 1); frame();                                /* T2 to B, waiting for the quarter */
    shot("seq_step");
    run(4000);

    nav_put(SEC_HOME, "JAM"); frames(4);
    jam_switch(3, 2); run(2000);
    jam_switch(4, 1); frame();
    shot("jam_free");

    favorites.factory[15][31] = (uint8_t)((1u ^ 1u) << 2);  /* CAMBIO 1/16 */
    press(B_REC); frame();
    for (k = 0; k < 400000u && live.srec != 2u; k++) run(1);
    until_step(3); jam_switch(1, 2);
    until_step(6); jam_switch(1, 0);
    until_step(9); jam_switch(2, 3);
    until_step(12); jam_switch(0, 1);
    until_step(14); jam_switch(1, 3);
    run(6000);
    until_step(4); jam_switch(3, 0);
    until_step(8); jam_switch(1, 1);
    until_step(10); jam_switch(5, 2);
    shot("jam_rec");
    run(9000);
    press(B_REC); frame(); run(30000);
    transport_req = 2; ui_input(); run(200);

    turn(EN_K4, 1); frame(); song_poll(); frames(8);
    transport_req = 1; ui_input();
    until_step(20);
    shot("jam_lock");
    jam_switch(3, 3); run(1500);
    shot("jam_lock_vos");

    track_select(2); frame();
    press(B_REC); frame();
    for (k = 0; k < 400000u && !(jam_tr & 8u); k++) run(1);
    run(5000); jam_switch(2, 1); run(4000);
    shot("jam_rec_track");
    for (k = 0; k < 400000u && jam_tr; k++) run(1);
    run(200); shot("jam_undo");
    transport_req = 2; ui_input(); run(200);

    jam_lock = 0;
    nav_put(SEC_HOME, "TAKE"); frames(4);
    for (take_i = 0; take_i < jam_end() && !(JE_K(live.jam[take_i]) == 1u && JE_POS(live.jam[take_i])); take_i++) {}
    frames(4);
    shot("take");
    return 0;
}
