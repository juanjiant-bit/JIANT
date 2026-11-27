/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * After Rhythmer's src/smk_midi_out.h (bounded standalone TRS MIDI output, Jazzy Rhythmer, 2026, GPL-3.0). */
/* (SMK-37, BOARD_SMK37 && FELUCCA_SMK_TRS only; usb.c includes it) TRS MIDI OUT on the 3.5 mm jack: what MIDI OUT sends
 * over USB (usb.c midi_out_event_at: MENU > CONTROL > MIDI OUT KEYS / CLOCK / ALL, track n on channel n), also out of
 * the jack. One producer (the audio ISR, midi_out_event_at), one consumer (TIMER5, smk_midi_out_poll: a byte when the
 * last has gone, never waiting), so neither USB nor the sound can be held up by it. Whole messages only: channel
 * voice messages, song position / select, tune request and the realtime bytes (a realtime byte may go between a
 * message's bytes, as MIDI allows); never SysEx (the editor's, the update's). A full queue, or a UART that stops
 * answering, drops the queue and sends every channel's CC64 0, CC120, CC121, CC123 instead (no note left hanging);
 * after four timeouts in a row the jack stops until MIDI OUT is set again. MIDI OUT OFF: silent (the held notes let
 * go). The state is in the pool (zeroed at boot). Untested on hardware: docs/SMK37.md. */
#pragma once
#include <stdint.h>
#include "smk_uart_tx.h"
#define SMK_MQ 64u                       /* channel messages (USB-MIDI packets) waiting */
static struct {
    uint32_t q[SMK_MQ];
    uint8_t rt[16];                      /* realtime bytes waiting */
    volatile uint16_t w, r;
    volatile uint8_t rw, rr, panic, reset, muted, fault;
    uint8_t b[3], pos, len, busy, wait, fails, pstep, on;
    uint32_t sent, dropped;
} smk_mo __attribute__((section(".pool"), aligned(4)));
#define SMK_MO_PUBLISH() __asm__ volatile("" ::: "memory")
static uint32_t smk_mo_len(uint32_t p)  /* a USB-MIDI packet's bytes on the wire, 0: not sent */
{
    uint32_t c = p & 15u, s = (p >> 8) & 255u, a = (p >> 16) & 255u, d = p >> 24;
    if (c >= 8u && c <= 14u) {
        if ((s >> 4) != c || a > 127u)
            return 0;
        return c == 12u || c == 13u ? 2u : d <= 127u ? 3u : 0u;
    }
    if (c == 2u && (s == 0xF1u || s == 0xF3u) && a <= 127u)
        return 2;
    if (c == 3u && s == 0xF2u && a <= 127u && d <= 127u)
        return 3;
    if (c == 5u && s == 0xF6u)
        return 1;
    if (c == 15u && (s == 0xF8u || s == 0xFAu || s == 0xFBu || s == 0xFCu || s == 0xFEu || s == 0xFFu))
        return 1;
    return 0;
}
static void smk_mo_init(void)            /* main.c, before timer5_start() */
{
    smk_uart_tx_init();
    smk_mo.on = 1;
}
static void smk_mo_enable(uint32_t on)   /* the audio ISR: MIDI OUT's setting changed (smk_kb.c) */
{
    smk_mo.muted = (uint8_t)!on;
    smk_mo.panic = 1;                    /* (the notes held out there let go either way) */
    SMK_MO_PUBLISH();
    smk_mo.reset = 1;                    /* (TIMER5 restarts the UART and sends the panic) */
}
static void smk_mo_packet(uint32_t p)    /* the audio ISR (usb.c midi_out_event_at) */
{
    uint32_t s = (p >> 8) & 255u;
    if (!smk_mo.on || !smk_mo_len(p))
        return;
    if ((p & 15u) == 15u && s >= 0xF8u) {   /* realtime */
        uint8_t w = smk_mo.rw;
        if (smk_mo.muted || smk_mo.fault || (uint8_t)(w - smk_mo.rr) >= 16u) {
            smk_mo.dropped++;
            return;
        }
        smk_mo.rt[w & 15u] = (uint8_t)s;
        SMK_MO_PUBLISH();
        smk_mo.rw = (uint8_t)(w + 1u);
        return;
    }
    if (smk_mo.muted || smk_mo.panic || smk_mo.fault || smk_mo.reset) {
        smk_mo.dropped++;
        return;
    }
    {
        uint16_t w = smk_mo.w;
        if ((uint16_t)(w - smk_mo.r) >= SMK_MQ) {   /* full: no message cut, every note let go instead */
            smk_mo.dropped++;
            SMK_MO_PUBLISH();
            smk_mo.panic = 1;
            return;
        }
        smk_mo.q[w % SMK_MQ] = p;
        SMK_MO_PUBLISH();
        smk_mo.w = (uint16_t)(w + 1u);
    }
}
static void smk_mo_poll(void)            /* TIMER5 (main.c), outside the render */
{
    uint8_t byte;
    if (!smk_mo.on)
        return;
    if (smk_mo.reset) {
        smk_uart_tx_restart();
        smk_mo.r = smk_mo.w;
        smk_mo.rr = smk_mo.rw;
        smk_mo.busy = smk_mo.pos = smk_mo.len = smk_mo.wait = 0;
        smk_mo.fault = smk_mo.fails = smk_mo.pstep = 0;
        smk_mo.reset = 0;
    }
    if (smk_mo.fault)
        return;
    if (smk_mo.busy) {
        if (!smk_uart_tx_done()) {
            if (++smk_mo.wait < 30u)     /* (a byte takes 0.32 ms: 30 polls is far more) */
                return;
            smk_mo.wait = 0;
            if (++smk_mo.fails >= 4u) {
                smk_mo.fault = 1;
                return;
            }
            smk_uart_tx_restart();
            smk_mo.busy = smk_mo.pos = smk_mo.len = 0;
            smk_mo.panic = 1;
            smk_mo.pstep = 0;
        } else {
            smk_mo.busy = smk_mo.wait = smk_mo.fails = 0;
        }
    }
    if (smk_mo.rr != smk_mo.rw) {        /* realtime first */
        byte = smk_mo.rt[smk_mo.rr & 15u];
        smk_mo.rr++;
    } else {
        if (smk_mo.pos >= smk_mo.len) {
            smk_mo.pos = smk_mo.len = 0;
            if (smk_mo.panic) {          /* CC64 0, CC120, CC121, CC123 on the 16 channels */
                static const uint8_t CC[4] = {64, 120, 121, 123};
                uint32_t at = smk_mo.pstep;
                smk_mo.r = smk_mo.w;
                if (at >= 64u) {
                    smk_mo.panic = smk_mo.pstep = 0;
                    return;
                }
                smk_mo.pstep++;
                smk_mo.b[0] = (uint8_t)(0xB0u + at / 4u);
                smk_mo.b[1] = CC[at % 4u];
                smk_mo.b[2] = 0;
                smk_mo.len = 3;
            } else {
                uint32_t p;
                if (smk_mo.r == smk_mo.w)
                    return;
                p = smk_mo.q[smk_mo.r % SMK_MQ];
                smk_mo.r++;
                smk_mo.len = (uint8_t)smk_mo_len(p);
                if (!smk_mo.len)
                    return;
                smk_mo.b[0] = (uint8_t)(p >> 8), smk_mo.b[1] = (uint8_t)(p >> 16), smk_mo.b[2] = (uint8_t)(p >> 24);
            }
        }
        byte = smk_mo.b[smk_mo.pos++];
    }
    smk_uart_tx_byte(byte);
    smk_mo.busy = 1;
    smk_mo.sent++;
}
