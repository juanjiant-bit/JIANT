/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Host test of firmware/src/ota.c: a fake M-UPGRADE serves a real .fwsc over
 * the SysEx protocol (pack7, cmd 0x30 reads, 0xE0000000 -> "success") and a
 * simulated NOR plays the flash.
 *   cc -o ota_test tests/ota_test.c && ./ota_test FILE.fwsc
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FELUCCA_OTA_DRYRUN 0
/* (SMK-37) -DBOARD_SMK37=1 -Ifirmware/hal: the SMK build of ota.c (staging 0xBB000, the board check);
 *   ota_test SMK.fwsc FM1.fwsc */
#if BOARD_SMK37
#include "fm1_flash_map.h"
#define FELUCCA_ID SMK_MODEL "_900"
#define T_OTA_LO 0xBB000u
#else
#define FELUCCA_ID "FM-1_900"
#define T_OTA_LO 0xE0000u
#endif
#define T_OTA_HI (T_OTA_LO + 0x5000u)

static uint8_t nor[0x100000];
static uint8_t *logical;
static size_t logical_len;
static uint8_t rx[1024];                 /* one pending host frame (7-bit bytes) */
static uint32_t rx_len, rx_full;
static uint32_t now_ms, requests, ident_replies, mute_after = 0xFFFFFFFFu, bad_erase;
static int committed;
static uint8_t committed_parm[112];
static int32_t last_code;
static uint8_t *corrupt_at;              /* flip a served byte (loader body) */

static uint32_t pack7(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 8;
        while (nb >= 7) { out[o++] = acc & 0x7F; acc >>= 7; nb -= 7; }
    }
    if (nb) out[o++] = acc & 0x7F;
    return o;
}

static uint32_t unpack7h(const uint8_t *in, uint32_t n, uint8_t *out)
{
    uint32_t acc = 0, nb = 0, o = 0;
    while (n--) {
        acc |= (uint32_t)*in++ << nb;
        nb += 7;
        if (nb >= 8) { out[o++] = (uint8_t)acc; acc >>= 8; nb -= 8; }
    }
    return o;
}

static void host_reply(const uint8_t *msg, uint32_t n)       /* queue msg as a device-bound frame */
{
    rx_len = pack7(msg, n, rx);
    rx_full = 1;
}

/* ---- hooks ---- */
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64], m[600];
    uint32_t d, addr, len, i, s = 0;
    if (p[0] != 0xF0 || p[n - 1] != 0xF7) { printf("bad wire framing\n"); exit(1); }
    d = unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) { ident_replies++; return 0; }
    if (d != 15 || u[2] != 0x30) { printf("unexpected device msg cmd %02x len %u\n", u[2], d); exit(1); }
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    if (++requests > mute_after)
        return 0;                                         /* host gone */
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0;
    m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr == 0xE0000000u) {
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
        if (corrupt_at && corrupt_at >= logical + addr && corrupt_at < logical + addr + len)
            m[14 + (corrupt_at - (logical + addr))] ^= 0x01;
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    host_reply(m, 15 + len);
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n)
{
    if (!rx_full) return 0;
    *p = rx; *n = rx_len;
    return 1;
}
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms += 1; }
static int ota_erase(uint32_t off)
{
    if (off < T_OTA_LO || off >= T_OTA_HI || (off & 0xFFFu)) { bad_erase++; return -8; }
    memset(nor + off, 0xFF, 4096);
    return 0;
}
static int ota_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off < T_OTA_LO || off + n > T_OTA_HI) { bad_erase++; return -8; }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static int ota_fread(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { if (step == 9) last_code = code; }
static void ota_commit(const uint8_t *parm) { committed = 1; memcpy(committed_parm, parm, 112); }

#include "../firmware/src/ota.c"

static uint32_t ota_body_off(void)                  /* ota.bin data + 0x20 in the logical image */
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, logical, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 100) return ota_rd32(e + 8) + 0x20u;
    }
    return 0;
}

static int check(const char *what, int ok)
{
    printf("%-52s %s\n", what, ok ? "ok" : "FAIL");
    return ok ? 0 : 1;
}

static int area_erased(void)
{
    uint32_t i;
    for (i = T_OTA_LO; i < T_OTA_HI; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}

static void reset_dev(void)
{
    memset(nor, 0xFF, sizeof nor);
    memcpy(nor, logical + 0x400, 0x4000);                 /* device flash head = package flash.bin head */
#if BOARD_SMK37
    memcpy(nor + FL_RT_OFF, logical + 0x400 + FL_RT_OFF, FL_RT_LEN);   /* the stock region table (smk_pkg_test.py:
                                                                      * the package's is the stock v016's) */
#endif
    rx_full = 0; requests = 0; ident_replies = 0; committed = 0; last_code = 0;
    mute_after = 0xFFFFFFFFu; corrupt_at = NULL; bad_erase = 0;
}

int main(int argc, char **argv)
{
    FILE *f;
    uint8_t *raw;
    size_t n, i;
    int bad = 0, rc;
    if (argc < 2) { printf("usage: ota_test FILE.fwsc\n"); return 2; }
    f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    raw = malloc(0x200000);
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    if (n < 20 * 48 + 0x4400) { printf("%s: too short for a package\n", argv[1]); return 2; }
    logical = malloc(n);
    for (i = 0; i < 20; i++) memcpy(logical + i * 47, raw + i * 48, 47);   /* drop the marker bytes */
    memcpy(logical + 20 * 47, raw + 20 * 48, n - 20 * 48);
    logical_len = n - 20;

    /* handshake outside a session */
    reset_dev();
    {
        uint8_t m[7] = {0, 0x59, 0x11, 0, 0, 0, 0xFF};
        host_reply(m, 7);
        ota_service();
        bad += check("handshake answered", ident_replies == 1);
        bad += check("identity frame is " FELUCCA_ID,
                     ota_msg[2] == 0x11 && !memcmp(ota_msg + 6, FELUCCA_ID, sizeof FELUCCA_ID - 1));
    }

    /* full session */
    reset_dev();
    rc = ota_session();
    printf("  %u requests\n", requests);
    bad += check("session commits", rc == 0 && committed);
    bad += check("headers + loader + confirm, no head reads", requests >= 10 && requests <= 30);
    {
        uint8_t *h = nor + T_OTA_LO, *p = committed_parm;
        bad += check("staged body == the package's loader", !memcmp(nor + T_OTA_LO + 0x20u, logical + ota_body_off(), 64));
        bad += check("LOADER.BIN head CRC",
                     (h[0] | h[1] << 8) == ota_crc16(h + 2, 30, 0) && !memcmp(h + 16, "LOADER.BIN", 10));
        bad += check("record at the area's end - 256 == RAM record", !memcmp(nor + T_OTA_HI - 0x100u, p, 112));
        bad += check("record CRC / type / magic / ota_addr",
                     (p[0] | p[1] << 8) == ota_crc16(p + 2, 78, 0) && (p[2] | p[3] << 8) == 0x5A0D &&
                     (p[6] | p[7] << 8) == 0x5441 && ota_rd32(p + 72) == T_OTA_LO);
        bad += check("record identity " OTA_PARM_ID ", zero after it",
                     !memcmp(p + 8, OTA_PARM_ID, sizeof OTA_PARM_ID - 1u) && p[8 + sizeof OTA_PARM_ID - 1u] == 0 &&
                     p[0x27] == 0);
        bad += check("nothing written outside the OTA area", bad_erase == 0);
    }
    /* boot cleanup erases it */
    bad += check("boot cleanup erases the area", ota_boot_cleanup() == 1 && area_erased());
    bad += check("second boot: nothing to do", ota_boot_cleanup() == 0);

    /* host disappears in the middle */
    reset_dev();
    mute_after = 4;
    rc = ota_session();
    bad += check("host gone -> no commit, area clean", rc < 0 && !committed && area_erased());

    /* host never confirms */
    reset_dev();
    mute_after = 0xFFFFFFFFu;
    {
        uint32_t total;
        rc = ota_session();
        total = requests;                                  /* requests of a full run incl. confirm */
        reset_dev();
        mute_after = total - 1;                            /* answer everything but the confirmation */
        rc = ota_session();
        bad += check("no 'success' -> -13, no commit, area clean", rc == -13 && !committed && area_erased());
    }
#if BOARD_SMK37
    {   /* the board check: refused before anything is erased or staged */
        uint8_t *own = logical;
        size_t own_len = logical_len;
        reset_dev();
        nor[FL_RT_OFF + 5] ^= 0x10u;                      /* the device's region table differs (an FM-1, ...) */
        memset(nor + T_OTA_LO, 0x5A, 16);
        rc = ota_session();
        bad += check("SMK: a region table not the device's -> -16, nothing erased",
                     rc == -16 && !committed && nor[T_OTA_LO] == 0x5A && bad_erase == 0);
        if (argc > 2 && (f = fopen(argv[2], "rb")) != NULL) {   /* the FM-1's package (flash.bin 0x93000) */
            n = fread(raw, 1, 0x200000, f);
            fclose(f);
            logical = malloc(n);
            for (i = 0; i < 20; i++) memcpy(logical + i * 47, raw + i * 48, 47);
            memcpy(logical + 20 * 47, raw + 20 * 48, n - 20 * 48);
            logical_len = n - 20;
            memset(nor, 0xFF, sizeof nor);
            memcpy(nor, own + 0x400, 0x4000);              /* the SMK's own head and table */
            memset(nor + T_OTA_LO, 0x5A, 16);
            rx_full = 0; requests = 0; committed = 0; bad_erase = 0;
            rc = ota_session();
            bad += check("SMK: the FM-1's package (flash.bin 0x93000) -> -15, nothing erased",
                         rc == -15 && !committed && nor[T_OTA_LO] == 0x5A && bad_erase == 0);
            logical = own;
            logical_len = own_len;
        } else
            bad += check("SMK: the FM-1's package given (argv 2)", 0);
    }
#endif

    /* (add15-beta6) an official package over a device whose head is another: refused (-5, nothing staged) unless the
     * package's head is M-VAVE's V15 or V16 (fingerprints in ota.c): JIANT over V15 -> official V16, and back */
    {
        uint32_t k, crc = ota_crc16(logical + 0x400, 0x4000, 0), fnv = 0x811C9DC5u;
        int known;
        for (k = 0; k < 0x4000u; k++)
            fnv = (fnv ^ logical[0x400 + k]) * 0x01000193u;
        known = (crc == 0x3FEEu && fnv == 0x7A2DDB89u) || (crc == 0x22B0u && fnv == 0xB6EE7DCEu);
        reset_dev();
        nor[0x200] ^= 0x5Au;                               /* the device's head differs from the package's */
        rc = ota_session();
#ifdef OWN_PKG
        (void)known;
        bad += check("own loader: another device head does not matter (not compared)", rc == 0 && committed);
#else
        if (known)
            bad += check("official V15 / V16 head over another head: taken", rc == 0 && committed);
        else
            bad += check("official package, unknown head, another device head: -5, nothing staged",
                         rc == -5 && !committed && area_erased());
#endif
    }

    printf("%s\n", bad ? "OTA TEST FAILED" : "ota test passed");
    return bad != 0;
}
