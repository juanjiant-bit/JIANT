/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Host test of the SMK-37's control mapping (docs/SMK37.md): firmware/src/panel.c's SMK table and hal/smk_input.h's
 * keybed / pad / knob maps, built with -DBOARD_SMK37=1:
 *   - every FM-1 function's default control is the printed SMK control the port maps it to (FX->FX, SCALE->SCL,
 *     CHORD->ENV, NOTE REPEAT->LFO, PARA->EDIT, GLOBE->GLO, PATCH->HOME, BT->SAVE, ARP, SEQ, SEQ PLAY->PLAY,
 *     SEQ REC->REC, OCTAVE-/+ -> OCT-/+), and HARDWARE CALIBRATION asks for it by that printed name;
 *   - K1..K4 = KNOB 1..4, K6 SELECT, K7 ALGORITHM, K8 PRESETS; K5 is no panel role: the MASTER (a virtual pot;
 *     its arithmetic needs the HAL's interrupt lock, pi32v2 asm: not run here);
 *   - KNOB BANK, FADER BANK, < and > carry no FM-1 function; panel_init takes the SMK's ids (0..17, 8 knobs);
 *   - the 27 middle keys (keybed keys 5..31) are the FM-1's F3..G5 (note bits 0..26), the outer 10 go to their queue;
 *   - the 16 pads are the FM-1's 16 white keys, the lowest notes on the bottom row. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#pragma GCC diagnostic ignored "-Wint-to-pointer-cast"
#define __attribute__(x)
#include "../firmware/hal/smk_input.h"
#undef __attribute__
#define __attribute__(x)
/* what panel.c needs from the rest of the firmware */
#define NPALETTES 8u
#define UI_JIANT_INDEX 0u
static uint8_t fx_lowcut;
static uint32_t palette_from_stored(uint32_t v) { return v; }
static void palette_set(uint32_t p) { (void)p; }
#include "../firmware/src/panel.c"
#undef __attribute__

static int bad;
static void check(const char *what, int ok) { printf("smk map: %-92s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

/* smk_input.h: button id -> printed name (confirmed on a unit by Melodee) */
static const char *const PRINTED[FM1_NBTN] = {"KNOB BANK", "FADER BANK", "<", ">", "OCTAVE-", "OCTAVE+", "ARP",
                                              "NOTE REPEAT", "SCALE", "CHORD", "GLOBE", "BT", "PATCH", "PARA", "FX",
                                              "SEQ", "SEQ PLAY", "SEQ REC"};

int main(void)
{
    static const struct { uint32_t label; const char *smk; } WANT[] = {
        {B_FX, "FX"}, {B_SCL, "SCALE"}, {B_ENV, "CHORD"}, {B_LFO, "NOTE REPEAT"}, {B_EDIT, "PARA"}, {B_GLO, "GLOBE"},
        {B_HOME, "PATCH"}, {B_SAVE, "BT"}, {B_ARP, "ARP"}, {B_SEQ, "SEQ"}, {B_PLAY, "SEQ PLAY"}, {B_REC, "SEQ REC"},
        {B_OCTDN, "OCTAVE-"}, {B_OCTUP, "OCTAVE+"}};
    uint32_t i, ok = 1, used = 0, k;
    memset(&panel, 0, sizeof panel);
    panel_init();
    check("panel_init: a blank table becomes the SMK default (\"PNS5\")", panel.magic == PANEL_MAGIC && PANEL_MAGIC == 0x504E5335u);
    for (i = 0; i < sizeof WANT / sizeof WANT[0]; i++) {
        uint32_t id = panel.btn[WANT[i].label];
        ok &= id < FM1_NBTN && !strcmp(PRINTED[id], WANT[i].smk) && !strcmp(B_NAME[WANT[i].label], WANT[i].smk);
        used |= 1u << id;
    }
    check("the 14 FM-1 functions on their printed SMK buttons; calibration asks by that name", ok && sizeof WANT / sizeof WANT[0] == NB);
    check("KNOB BANK, FADER BANK, < and > carry no FM-1 function (ids 0..3)", (used & 0xFu) == 0 &&
          panel_btn_of(SMK_BTN_KNOB_BANK) == NB && panel_btn_of(SMK_BTN_LEFT) == NB && panel_btn_of(SMK_BTN_RIGHT) == NB &&
          panel_btn_of(SMK_BTN_FADER_BANK) == NB && panel.btn[B_OCTDN] == FM1_BTN_OCT_DOWN && panel.btn[B_OCTUP] == FM1_BTN_OCT_UP);
    check("K1..K4 = KNOB 1..4 (encoders 0..3), K6 SELECT, K7 ALGORITHM, K8 PRESETS, all clockwise +",
          panel.enc[EN_K1] == 0 && panel.enc[EN_K2] == 1 && panel.enc[EN_K3] == 2 && panel.enc[EN_K4] == 3 &&
          panel.enc[EN_SELECT] == 5 && panel.enc[EN_ALGO] == 6 && panel.enc[EN_PRESET] == 7 &&
          !strcmp(E_NAME[EN_SELECT], "K6") && !strcmp(E_NAME[EN_K1], "K1") &&
          panel.dir[0] == 1 && panel.dir[6] == 1);
    for (i = 0, k = 0; i < NE; i++)
        k |= panel.enc[i] == SMK_ENC_MASTER;
    check("K5 (encoder 4) is no panel role: it is the MASTER", !k && SMK_ENC_MASTER == 4u);
    check("MASTER: K5's virtual pot starts at 724 (master_q12 2048; fm1_adc_read: 24 a detent, 0..1023)",
          smk_master == 724 && FM1_ADC_MASTER == 4);
    panel.btn[B_FX] = 17u; panel.btn[B_REC] = 14u;     /* a calibrated table with the high ids */
    panel_init();
    check("panel_init keeps a calibrated table with button ids up to 17 and knob 7", panel.btn[B_FX] == 17u && panel.enc[EN_PRESET] == 7u);
    panel.btn[B_FX] = 18u;
    panel_init();
    check("... and refuses id 18 (back to the default)", panel.btn[B_FX] == 14u);
    panel.enc[EN_K1] = 8u;
    panel_init();
    check("... and knob 8", panel.enc[EN_K1] == 0u);
    /* the keybed */
    for (k = 0, ok = 1; k < FM1_NKEY; k++) {
        uint32_t want = k >= 5u && k < 32u ? 1u << (k - 5u) : 0u, q0 = smk_extq_w;
        smk__key_event(k, 90);
        ok &= fm1_in.notes == want && (want ? smk_extq_w == q0 : smk_extq_w != q0);
        smk__key_event(k, 0);
        ok &= fm1_in.notes == 0u;
        while (smk_extkey_take() >= 0)
            ;
    }
    check("keybed keys 5..31 (F3..G5) are the FM-1's 27 keys (bits 0..26); keys 0..4 and 32..36 go to their queue", (int)ok);
    /* the pads */
    {
        static const uint8_t WHITE[16] = {0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26};
        uint32_t seen = 0, row = 1;
        for (i = 0; i < SMK_NPAD; i++) {
            int32_t n = SMK_PK_KEYS[i];
            uint32_t w;
            for (w = 0; w < 16u && WHITE[w] != n; w++) {}
            if (w < 16u) seen |= 1u << w;
            if (i < 8u) row &= SMK_PK_KEYS[i + 8u] == WHITE[i] && n == WHITE[i + 8u];
        }
        check("the 16 pads: the FM-1's 16 white keys (F3..G5), each once", seen == 0xFFFFu && smk_pad_key == SMK_PK_KEYS);
        check("... the lowest on the bottom row (pads 8..15: F3..F4), the top row (pads 0..7) G4..G5", (int)row);
        fm1_in.notes = fm1_in.knotes = fm1_in.pnotes = 0;
        smk__padq(8, 100);
        smk__padq(15, 100);
        ok = fm1_in.notes == (1u << 0 | 1u << 12) && fm1_in.key_vel[SMK_KEY_F3] == 100u;
        smk__padq(8, 0);
        smk__padq(15, 0);
        check("a pad hit presses its key (bottom-left = F3, bottom-right = F4), its release lets it go",
              ok && fm1_in.notes == 0u && smk_pad_take() < 0);
    }
    printf("%s\n", bad ? "SMK MAP TEST FAILED" : "smk map test passed");
    return bad != 0;
}
