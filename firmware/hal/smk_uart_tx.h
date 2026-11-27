/* SPDX-License-Identifier: GPL-3.0-only
 * SMK-37 TRS MIDI OUT: UART1 TX on PC7, after Rhythmer's hal/smk_uart_tx.h (Jazzy Rhythmer, 2026, GPL-3.0), whose
 * register setup follows the stock v016 analysis documented by genkigenki: github.com/genkigenki/fm1-to-smk37
 * docs/01-hardware.md section 9 (status there: analysis, not yet confirmed on a unit).
 * (JIANT: BOARD_SMK37 builds only, src/smk_midi_out.h drives it. The registers are hal/fm1_uart.h's, the FM-1's
 *  verified UART1: CON0 0x12100, BAUD 0x12108 (383: 31250 baud from the 48 MHz UART clock), BUF 0x1210C, the UART
 *  clock in 0x10010 [11:10], IOMAP_CON3 0x51028.) No RX, no DMA, no interrupt: TIMER5 sends one byte at a time. */
#pragma once
#include <stdint.h>
#include "fm1_gpio.h"
#include "fm1_uart.h"
#define SMK_UT1_BUF (*(volatile uint32_t *)0x1210Cu)
static void smk_uart_tx_restart(void)
{
    FM1_UT1_CON0 = 0x3400u;                            /* off, pendings cleared */
    FM1_UT1_CON1 = 0;
    FM1_UT1_BAUD = 48000000u / 31250u / 4u - 1u;       /* 383 */
    __asm__ volatile("csync" ::: "memory");
    FM1_UT1_CON0 = 0x3401u;                            /* UTEN: TX only */
}
static void smk_uart_tx_init(void)                     /* before timer5_start() */
{
    FM1_UM_CLK_CON1 = (FM1_UM_CLK_CON1 & ~(3u << 10)) | (1u << 10);   /* the UART clock: PLL48M */
    FM1_UM_IOMAP3 = (FM1_UM_IOMAP3 & ~(0x80u | (15u << 20))) | (1u << 20);   /* UT1 TX onto its output channel */
    FM1_PR(FM1_PC, FM1_PU) |= 1u << 7;                 /* (stock: PC7 pull-up and pull-down both set) */
    FM1_PR(FM1_PC, FM1_PD) |= 1u << 7;
    FM1_PR(FM1_PC, FM1_OUT) &= ~(1u << 7);
    FM1_PR(FM1_PC, FM1_DIR) &= ~(1u << 7);             /* an output */
    FM1_PR(FM1_PC, FM1_DIE) |= 1u << 7;
    smk_uart_tx_restart();
}
static int smk_uart_tx_done(void) { return (FM1_UT1_CON0 & 0x8000u) != 0u; }   /* TPND: the last byte has gone */
static void smk_uart_tx_byte(uint8_t b)
{
    FM1_UT1_CON0 |= 0x2000u;                           /* clear TPND */
    SMK_UT1_BUF = b;
    __asm__ volatile("csync" ::: "memory");
}
