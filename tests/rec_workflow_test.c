/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(void)
{
    int bad = 0;
    uint32_t before, after, other;
    char label[12];
    ui_power_on();
    song.sel = 0;
    my_steps(&trk[0]); my_steps(&trk[1]);
    before = steps_sig(&trk[0]); other = steps_sig(&trk[1]);
    press(B_REC);
    bad += check("arming REC captures the music before the take", undo_ok && song.rec == 1);
    trk[0].step[0].note[0] = 77; after = steps_sig(&trk[0]);
    ly_white(B_REC, 2);
    bad += check("REC+A3 restores the take; shortcut key is not recorded and REC disarms",
                 steps_sig(&trk[0]) == before && steps_sig(&trk[1]) == other && !song.rec && !gates());
    ly_white(B_REC, 2);
    bad += check("REC+A3 again redoes the same take", steps_sig(&trk[0]) == after && !song.rec);
    ly_white(B_GLO, GLO_UNDO);
    bad += check("original GLO+G4 shares the same undo/redo snapshot", steps_sig(&trk[0]) == before);

    ui_power_on(); undo_ok = 0;
    ly_white(B_REC, 2);
    bad += check("empty undo reports NOTHING TO UNDO and never arms REC", msg_is("NOTHING TO UNDO") && !song.rec);
    undo_mark(); trk[0].step[0].note[0] = 70; after = steps_sig(&trk[0]);
    chain.running = 1;
    ly_white(B_REC, 2);
    bad += check("song-chain guard refuses undo without changing notes", msg_is("STOP TO EDIT") && steps_sig(&trk[0]) == after);
    chain.running = 0;

    ui_power_on(); song.sel = 2;
    trk[2].p[P_HRQ] = 127; trk[1].p[P_HRQ] = 64;
    ly_white(B_REC, 3);
    bad += check("REC+B3 turns recording quantization off on selected track only",
                 !trk[2].p[P_HRQ] && trk[1].p[P_HRQ] == 64 && !song.rec && !gates());
    ly_white(B_REC, 3);
    bad += check("REC+B3 toggles hard quantization back to 100 percent", trk[2].p[P_HRQ] == 127 && !song.rec);
    rec_down(); frames(512);
    turn(EN_K2, -8);
    bad += check("REC+K2 exposes partial recording quantization", trk[2].p[P_HRQ] > 0 && trk[2].p[P_HRQ] < 127);
    turn(EN_K2, -1000);
    bad += check("REC+K2 reaches fully unquantized recording", !trk[2].p[P_HRQ] && !song.rec);
    bad += check("REC map labels UNDO and quantization state",
                 (ly_keyname(LAYER_REC, 0, 2, label), str_eq(label,"UNDO")));
    bad += check("REC map shows Q OFF", (ly_keyname(LAYER_REC, 0, 3, label), str_eq(label,"Q OFF")));
    rec_up();

    ui_power_on(); my_steps(&trk[0]); before=steps_sig(&trk[0]); press(B_REC);
    trk[0].step[0].note[0]=78; frames(400); press(B_REC);
    ly_white(B_REC,2);
    bad += check("disarming REC keeps the original pre-take undo snapshot", steps_sig(&trk[0])==before);
    bad += test_rec();
    bad += test_rec_layer();
    bad += test_rec_combos();
    return bad != 0;
}
