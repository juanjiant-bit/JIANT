/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT on the M-Vave SMK-37 (BOARD_SMK37): Israel Peña (pedeplei1281) */
/* SMK-37 TRS MIDI OUT (hal/smk_uart_tx.h) on the host, the UART's registers stood in for: each USB-MIDI packet gives its
 * MIDI bytes by its CIN (channel messages, realtime, system common), SysEx and a status that does not match its CIN are
 * dropped, a full ring drops a whole message (never half), the setting OFF sends nothing, one byte goes per tick and only
 * once the last has gone, and a UART that never finishes leaves the jack quiet (4 bytes in a row, 3 ms each) until the
 * setting is set again. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t con0, con1, baud, clk, iomap;
static uint32_t buf_reg;
#define SMK_UT1_CON0 con0
#define SMK_UT1_CON1 con1
#define SMK_UT1_BAUD baud
#define SMK_UT1_BUF buf_reg
#define SMK_CLK_CON1 clk
#define SMK_IOMAP_CON3 iomap
#define SMK_CSYNC() ((void)0)
#include "smk_uart_tx.h"

static int bad;
static void check(const char *what, int ok)
{
    printf("%-72s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint8_t out[1024];
static uint32_t nout;
/* n ticks of a UART that finishes each byte at once (TPND set when written) */
static void run(uint32_t n, int works)
{
    while (n--) {
        uint32_t before = smk_trs_r;
        smk_trs_tick();
        if ((uint8_t)(smk_trs_r - before) == 1u && smk_trs_busy && !smk_trs_wait) {   /* a byte written (not a flush) */
            out[nout++ % sizeof out] = (uint8_t)buf_reg;
            if (works)
                con0 |= SMK_UT_TPND;
            else
                con0 &= ~SMK_UT_TPND;
        }
    }
}
static uint32_t pk(uint32_t cin, uint32_t st, uint32_t d1, uint32_t d2) { return cin | st << 8 | d1 << 16 | d2 << 24; }

int main(void)
{
    uint32_t i;
    (void)smk_trs_init;
    smk_trs_up = 1;
    smk_trs_set(1);
    smk_trs_put(pk(0x9, 0x90, 60, 100));                /* note on */
    smk_trs_put(pk(0xC, 0xC1, 5, 0));                   /* program change: 2 bytes */
    smk_trs_put(pk(0xF, 0xF8, 0, 0));                   /* clock: 1 byte */
    smk_trs_put(pk(0x4, 0xF0, 0x22, 0x24));             /* SysEx: dropped */
    smk_trs_put(pk(0x9, 0xFF, 60, 100));                /* status not its CIN (MIDI OFF's 0xFF): dropped */
    smk_trs_put(pk(0xB, 0xB0, 1, 64));                  /* CC1 */
    run(100, 1);
    {
        static const uint8_t want[] = {0x90, 60, 100, 0xC1, 5, 0xF8, 0xB0, 1, 64};
        check("bytes by CIN: note on, program change (2), clock (1), CC; SysEx and a bad status dropped",
              nout == sizeof want && !memcmp(out, want, sizeof want));
    }
    nout = 0;
    smk_trs_put(pk(0x8, 0x80, 60, 0));
    smk_trs_tick();
    check("one byte a tick, the next only once TPND says the last has gone", nout == 0 && smk_trs_busy);
    con0 |= SMK_UT_TPND;
    run(10, 1);
    nout = 0;
    smk_trs_set(0);
    smk_trs_put(pk(0x9, 0x90, 61, 1));
    run(20, 1);
    check("TRS MIDI OUT OFF: nothing queued, nothing sent", nout == 0 && smk_trs_r == smk_trs_w);
    smk_trs_set(1);
    for (i = 0; i < 100u; i++)                           /* 300 bytes into a 256-byte ring, none sent */
        smk_trs_put(pk(0x9, 0x90, 60, 100));
    check("a full ring drops whole messages: 85 of 100 notes kept (255 bytes)", (uint8_t)(smk_trs_w - smk_trs_r) == 255u);
    run(2000, 1);
    {
        int whole = nout == 255u;
        for (i = 0; whole && i < 255u; i += 3u)
            whole = out[i] == 0x90 && out[i + 1u] == 60 && out[i + 2u] == 100;
        check("... and every message sent whole", whole);
    }
    run(5, 1);                                           /* (the last byte's TPND seen: not busy) */
    nout = 0;
    for (i = 0; i < 10u; i++)
        smk_trs_put(pk(0x9, 0x90, 62, 90));
    con0 &= ~SMK_UT_TPND;
    run(400, 0);
    check("a UART that never finishes: given up after 4 bytes, the jack quiet", smk_trs_dead && nout == 4u &&
          smk_trs_r == smk_trs_w);
    smk_trs_put(pk(0x9, 0x90, 63, 90));
    check("... nothing queued while quiet", smk_trs_r == smk_trs_w);
    smk_trs_set(0);
    smk_trs_set(1);
    nout = 0;
    smk_trs_put(pk(0x9, 0x90, 64, 90));
    run(50, 1);
    check("... the setting set again: it sends again", !smk_trs_dead && nout == 3u && out[1] == 64);
    printf("%s\n", bad ? "SMK TRS TEST FAILED" : "smk trs test passed");
    return bad != 0;
}
