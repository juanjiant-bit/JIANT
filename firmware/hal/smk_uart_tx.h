/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 JIANT port: Copyright (C) 2026 Israel Peña (pedeplei1281) */
/* SMK-37: the 3.5 mm TRS jack as MIDI OUT, UART1 transmit only on PC7, 31250 baud. The stock v016 setup (hardware
 * notes: https://github.com/genkigenki/fm1-to-smk37, docs/01-hardware.md "MIDI"; status there: analysis, not yet
 * measured on a unit): IOMAP_CON3 bit 7 clear and [23:20] = 1 (UT1 TX onto an output channel), PC7 pull-up and pull-down
 * set (the output channel), output low, input enabled; UART1 CON0 0x3400 (pendings cleared), BAUD 383 from PLL48M, then
 * UTEN. No receive: the FM-1's TRS MIDI IN pin, PH8, is a key-matrix row here and is never touched.
 *
 *   smk_trs_init()      once at power-on (main.c), before TIMER5 starts
 *   smk_trs_put(pkt)    a USB-MIDI packet (usb.c midi_out_event: the same stream as USB MIDI OUT, after MENU > MIDI OUT),
 *                       whole messages into a 256-byte ring (a full ring drops the message, never half of it)
 *   smk_trs_tick()      from the 10 kHz TIMER5 tick (smk_input.h fm1_input_tick): one byte when the last has gone;
 *                       never a wait loop. A byte not gone in 30 ticks (3 ms) is given up; after 4 in a row the jack
 *                       stays quiet until MENU > KEYBOARD > TRS MIDI OUT is set again (smk_trs_dead)
 * smk_trs_on: the setting (the UI, ui_input.c smk_cfg_poll). */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"

#ifndef SMK_UT1_CON0                                     /* (tests/smk_trs_test.c stands in for the registers) */
#define SMK_UT1_CON0 (*(volatile uint32_t *)0x12100u)
#define SMK_UT1_CON1 (*(volatile uint32_t *)0x12104u)
#define SMK_UT1_BAUD (*(volatile uint32_t *)0x12108u)
#define SMK_UT1_BUF (*(volatile uint32_t *)0x1210Cu)
#define SMK_CLK_CON1 (*(volatile uint32_t *)0x10010u)    /* [11:10] the UART clock: 1 = PLL48M */
#define SMK_IOMAP_CON3 (*(volatile uint32_t *)0x51028u)  /* UT1: bit 7 fixed IO, [23:20] TX output channel */
#endif
#ifndef SMK_CSYNC
#define SMK_CSYNC() __asm__ volatile("csync" ::: "memory")
#endif
#define SMK_UT_TPND (1u << 15)
#define SMK_UT_CLR_TPND (1u << 13)

#define SMK_TRS_RING 256u                                /* (uint8_t indexes wrap with it) */
static volatile uint8_t smk_trs_ring[SMK_TRS_RING];
static volatile uint8_t smk_trs_w, smk_trs_r;
static volatile uint8_t smk_trs_on;                      /* MENU: TRS MIDI OUT ON (and the driver set up) */
static volatile uint8_t smk_trs_dead;                    /* 4 bytes in a row never went: quiet */
static volatile uint8_t smk_trs_up;                      /* smk_trs_init done */
static uint8_t smk_trs_busy, smk_trs_wait, smk_trs_stuck;

static void smk_trs_init(void)
{
    SMK_UT1_CON0 = 0x3400u;                              /* off, pendings cleared */
    SMK_UT1_CON1 = 0;
    SMK_CLK_CON1 = (SMK_CLK_CON1 & ~(3u << 10)) | (1u << 10);
    SMK_IOMAP_CON3 &= ~0x80u;
    SMK_IOMAP_CON3 = (SMK_IOMAP_CON3 & ~(0xFu << 20)) | (1u << 20);
    FM1_PR(FM1_PC, FM1_PU) |= 1u << 7;
    FM1_PR(FM1_PC, FM1_PD) |= 1u << 7;
    FM1_PR(FM1_PC, FM1_OUT) &= ~(1u << 7);
    FM1_PR(FM1_PC, FM1_DIE) |= 1u << 7;
    FM1_PR(FM1_PC, FM1_DIR) &= ~(1u << 7);
    SMK_UT1_BAUD = 48000000u / 31250u / 4u - 1u;        /* 383 */
    SMK_CSYNC();
    SMK_UT1_CON0 = 0x3400u;
    SMK_CSYNC();
    SMK_UT1_CON0 |= 1u;                                  /* UTEN last */
    smk_trs_up = 1;
}

/* a USB-MIDI packet (CIN in bits 0..3, then the status and the data bytes): its MIDI bytes, by its CIN. Channel
 * messages (8..E: C and D two bytes), realtime (F), system common (2, 3, 5); SysEx and a status that does not match its
 * CIN (a track with MIDI OFF can give 0xFF) are dropped */
static void smk_trs_put(uint32_t pkt)
{
    uint32_t cin = pkt & 0xFu, st = (pkt >> 8) & 0xFFu, n, i;
    uint8_t w = smk_trs_w;
    if (!smk_trs_on || !smk_trs_up || smk_trs_dead)
        return;
    if (cin >= 0x8u && cin <= 0xEu) {
        if ((st >> 4) != cin)
            return;
        n = cin == 0xCu || cin == 0xDu ? 2u : 3u;
    } else if (cin == 0xFu && st >= 0xF8u) {
        n = 1;
    } else if (cin == 0x5u && st == 0xF6u) {
        n = 1;
    } else if (cin == 0x2u && (st == 0xF1u || st == 0xF3u)) {
        n = 2;
    } else if (cin == 0x3u && st == 0xF2u) {
        n = 3;
    } else {
        return;
    }
    if ((uint8_t)(smk_trs_r - w - 1u) < n)               /* (room: r - w - 1 bytes) */
        return;
    for (i = 0; i < n; i++)
        smk_trs_ring[(uint8_t)(w + i)] = (uint8_t)(pkt >> (8u + 8u * i));
    __asm__ volatile("" ::: "memory");
    smk_trs_w = (uint8_t)(w + n);
}

static void smk_trs_tick(void)
{
    if (!smk_trs_up)
        return;
    if (smk_trs_busy) {
        if (SMK_UT1_CON0 & SMK_UT_TPND) {
            smk_trs_busy = 0;
            smk_trs_stuck = 0;
        } else if (++smk_trs_wait >= 30u) {              /* 3 ms: given up */
            smk_trs_busy = 0;
            if (++smk_trs_stuck >= 4u)
                smk_trs_dead = 1;
        } else {
            return;
        }
    }
    if (smk_trs_dead || !smk_trs_on) {
        smk_trs_r = smk_trs_w;                           /* (what was queued goes) */
        return;
    }
    if (smk_trs_r == smk_trs_w)
        return;
    SMK_UT1_CON0 |= SMK_UT_CLR_TPND;
    SMK_UT1_BUF = smk_trs_ring[smk_trs_r];
    smk_trs_r = (uint8_t)(smk_trs_r + 1u);
    smk_trs_busy = 1;
    smk_trs_wait = 0;
}

/* the setting, from the UI: ON again also wakes a jack gone quiet */
static void smk_trs_set(int on)
{
    if (on && !smk_trs_on) {
        smk_trs_stuck = 0;
        smk_trs_dead = 0;
    }
    smk_trs_on = (uint8_t)(on != 0);
}
