/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (studio) The emulator answers the whole web editor (web/EDITOR_PROTOCOL.md) with the firmware's own handler,
 * firmware/src/editor.c, as the FM-1 runs it over USB and as ports/core/jiant_core.c does on a computer: a request goes in
 * whole (web_sysex), the replies and the live pushes (WATCH) come out whole (web_sysex_out). JIANT Studio puts the editor
 * and this emulator side by side and joins them (studio.js emuMidi, worklet.js "sysex"), so the editor edits the FM-1 in
 * the browser as it edits a real one. It takes the place of transfer-api.h (the backup commands alone: editor.c has
 * them too, editor_backup.c) and keeps its entry points (web_ed, web_ed_buf, web_ed_reply, web_version: transfer.js).
 * Included at the end of felucca_web.c (after everything editor.c uses). Nothing of the firmware changes for this file.
 *   web_sysex_buf()      the request: F0 7D 46 4C cmd args.. F7
 *   web_sysex(n)         runs it (0 ok, 1 not an editor frame)
 *   web_editor_poll()    the live pushes, once a frame
 *   web_sysex_out()      the next reply or push's length (0: none), the message at web_sysex_msg() */
#ifndef JIANT_TRANSFER_API
#define JIANT_TRANSFER_API 1
#define JIANT_EDITOR_API 1
static uint8_t web_sx_in[640], web_sx_req[640];
static uint32_t web_sx_in_n, web_sx_ready, web_sx_on;
static uint8_t web_sx_out[32768], web_sx_msg[1024];          /* F0..F7 messages, back to back */
static uint32_t web_sx_w, web_sx_r;                          /* (free-running) */
#if !FELUCCA_OTA
#define SXQ 64u                                              /* (usb.c's outgoing ring, which editor.c ed_room asks: never full here) */
static uint32_t so_w, so_r;
#endif
static uint32_t ota_now_ms(void) { return fm1_ms; }
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint32_t i;
    if (n > sizeof web_sx_out - (web_sx_w - web_sx_r))
        return -1;                                            /* (nobody reads: dropped whole) */
    for (i = 0; i < n; i++)
        web_sx_out[(web_sx_w + i) % sizeof web_sx_out] = p[i];
    web_sx_w += n;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!web_sx_ready)
        return 0;
    *p = web_sx_in;
    *n = web_sx_in_n;
    return 1;
}
static void ota_frame_done(void) { web_sx_ready = 0; }
/* editor.c waits for the transport to stop (100 ms at most) feeding the watchdog; here the device's clock runs only with
 * the audio asked for, so each wait runs the device itself, a millisecond at a time (as transfer-api.h's did) */
static void web_ms(void);
#define fm1_wdt_feed web_ms
#include "../../firmware/src/editor.c"
#undef fm1_wdt_feed

EXPORT uint8_t *web_sysex_buf(void) { return web_sx_req; }
EXPORT uint32_t web_sysex(uint32_t n)
{
    if (!web_booted || n < 6u || n - 2u > sizeof web_sx_in || web_sx_req[0] != 0xF0u || web_sx_req[n - 1u] != 0xF7u ||
        web_sx_req[1] != ED_HDR0 || web_sx_req[2] != ED_HDR1 || web_sx_req[3] != ED_HDR2)
        return 1;
    if (!web_sx_on) {                                         /* the editor's link is up (editor.c ed_sync: WATCH) */
        web_sx_on = 1;
        usb.config = 1;
    }
    memcpy(web_sx_in, web_sx_req + 1, n - 2u);
    web_sx_in_n = n - 2u;
    web_sx_ready = 1;
    ed_service();
    web_sx_ready = 0;
    return 0;
}
EXPORT void web_editor_poll(void)
{
    if (web_sx_on && web_booted)
        ed_service();                                         /* the live pushes (WATCH) */
}
EXPORT uint8_t *web_sysex_msg(void) { return web_sx_msg; }
EXPORT uint32_t web_sysex_out(void)
{
    uint32_t n = 0, i;
    while (web_sx_r != web_sx_w) {
        while (web_sx_r + n != web_sx_w && web_sx_out[(web_sx_r + n) % sizeof web_sx_out] != 0xF7u)
            n++;
        n++;                                                  /* (its F7) */
        if (n <= sizeof web_sx_msg) {
            for (i = 0; i < n; i++)
                web_sx_msg[i] = web_sx_out[(web_sx_r + i) % sizeof web_sx_out];
            web_sx_r += n;
            return n;
        }
        web_sx_r += n;                                        /* (longer than any reply: dropped) */
        n = 0;
    }
    return 0;
}

/* transfer.js's entry points (transfer-api.h's): cmd and its arguments in, the reply's arguments out */
static uint8_t web_ed_req[400], web_ed_rep[1024];
EXPORT uint8_t *web_ed_buf(void) { return web_ed_req; }
EXPORT uint8_t *web_ed_reply(void) { return web_ed_rep; }
EXPORT uint32_t web_ed(uint32_t n)
{
    uint32_t i, m;
    if (!n || n + 5u > sizeof web_sx_req)
        return 0;
    for (i = 0; i < n; i++)
        if (web_ed_req[i] > 127u)
            return 0;
    while (web_sysex_out())                                   /* (what was waiting is not this reply) */
        ;
    web_sx_req[0] = 0xF0; web_sx_req[1] = ED_HDR0; web_sx_req[2] = ED_HDR1; web_sx_req[3] = ED_HDR2;
    memcpy(web_sx_req + 4, web_ed_req, n);
    web_sx_req[4u + n] = 0xF7;
    if (web_sysex(n + 5u))
        return 0;
    while ((m = web_sysex_out()) != 0)
        if (m >= 6u && web_sx_msg[4] == web_ed_req[0]) {      /* the reply: its bytes after the command */
            memcpy(web_ed_rep, web_sx_msg + 5, m - 6u);
            return m - 6u;
        }
    return 0;
}
EXPORT const char *web_version(void) { return FELUCCA_VERSION; }
#endif
