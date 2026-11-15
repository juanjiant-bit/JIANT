/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (V10.7.36 emu-transfer) The emulator answers the editor's full-backup commands (web/EDITOR_PROTOCOL.md "v7: full
 * backup": 65 BACKUP_LIST, 66 BACKUP_GET, 67 BACKUP_PUT) with the firmware's own code: editor_backup.c, built here as
 * tests/backup_test.c builds it. So the page moves projects, user presets and whole backups between this emulator and a
 * real FM-1 (web/emu/transfer.js) with one protocol and one format on both sides, the bytes the FM-1 itself writes.
 * Included at the end of felucca_web.c (after everything editor_backup.c uses: storage, projects, user presets, songs,
 * settings, the panel). Nothing of the firmware changes for this file.
 *   web_ed_buf()   the request: cmd, then its arguments (7-bit bytes, as between F0 7D 46 4C and F7)
 *   web_ed(n)      runs it; the reply's length (0: no reply, as the device sends none), the reply at web_ed_reply()
 *   web_version()  FELUCCA_VERSION, as the device's INFO names it */
#ifndef JIANT_TRANSFER_API                 /* (appended by a site build as additive-api.h is, and #included here: once) */
#define JIANT_TRANSFER_API 1
enum { ED_BACKUP_LIST = 65, ED_BACKUP_GET, ED_BACKUP_PUT };
static uint8_t web_ed_req[400], web_ed_rep[400];
static uint32_t web_ed_n;
static void ed_b(uint32_t v) { if (web_ed_n < sizeof web_ed_rep) web_ed_rep[web_ed_n++] = (uint8_t)(v & 127u); }
static uint32_t ed_unpack7(const uint8_t *a, uint32_t na, uint8_t *out, uint32_t max)   /* as editor.c's */
{
    uint32_t n = 0;
    while (na && n < max) {
        uint32_t m = *a++, j, k;
        na--;
        k = na > 7u ? 7u : na;
        if (!k || n + k > max || (m >> k))
            return 0;
        for (j = 0; j < k; j++, na--)
            out[n++] = (uint8_t)(*a++ | ((m >> j) & 1u) << 7);
    }
    return na ? 0u : n;
}
/* editor.c's: the transport stopped before a flash operation, within 100 ms. Here the device's clock runs only with
 * the audio the worklet asks for, so the wait runs the device itself (web_ms: the audio ISR stops it at its next block) */
static void web_ms(void);
static int ed_flash_stop(void)
{
    uint32_t t0 = fm1_ms;
    if (!transport_busy())
        return 0;
    transport_req = 2;
    while ((song.playing || chain_busy()) && fm1_ms - t0 < 100u)
        web_ms();
    return song.playing || chain_busy();
}
#include "../../firmware/src/editor_backup.c"

EXPORT uint8_t *web_ed_buf(void) { return web_ed_req; }
EXPORT uint8_t *web_ed_reply(void) { return web_ed_rep; }
EXPORT uint32_t web_ed(uint32_t n)
{
    uint32_t i;
    web_ed_n = 0;
    if (!n || n > sizeof web_ed_req)
        return 0;
    for (i = 0; i < n; i++)
        if (web_ed_req[i] > 127u)                      /* (every data byte of a SysEx is 7 bit) */
            return 0;
    if (!web_booted || !ed_backup_handle(web_ed_req[0], web_ed_req + 1, n - 1u))
        return 0;
    return web_ed_n;
}
EXPORT const char *web_version(void) { return FELUCCA_VERSION; }
#endif
