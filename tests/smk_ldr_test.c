/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* Host test of the SMK-37 build of the update loader (firmware/loader/ldr_core.c, -DBOARD_SMK37=1 -Ifirmware/hal):
 * a fake host serves a package over the SysEx protocol to a simulated SMK-37 whose every byte holds a pattern (the
 * stock firmware's head and region table, BTIF, USRTRIM, USRFLASH with its marker, USR, key_mac, JIANT's store with
 * a record look-alike in it, the stock update record at 0xBFF00). The flash hooks are the RAM driver's: FL_GUARD
 * over the loader's FL_RANGE_OK (loader.c: FL_LDR_RANGE_OK), so a refused erase / program is counted, not done.
 *   smk_ldr_test OLD_SMK.fwsc NEW_SMK.fwsc FM1.fwsc
 * Checks: the install writes the app area [0x4000, 0x9D000) only, clears the record in the OTA area, keeps every
 * other byte; the FM-1's package (flash.bin 0x93000) and a package whose region table is not the device's are
 * refused before anything is erased. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1 -Ifirmware/hal"
#endif
#define FL_RANGE_OK(off, n) FL_LDR_RANGE_OK(off, n)       /* as loader.c, before the map */
#include "fm1_flash_map.h"

#define FELUCCA_ID "ota-" SMK_MODEL "_900"
#define FELUCCA_OTA_DRYRUN 0

static uint8_t nor[0x100000], ref[0x100000], *logical;
static size_t logical_len;
static uint8_t rx[1024];
static uint32_t rx_len, rx_full, now_ms, erases, refused, record_cleared;

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
static int ota_wire_send(const uint8_t *p, uint32_t n)
{
    uint8_t u[64] = {0}, m[600];
    uint32_t addr, len, i, s = 0;
    unpack7h(p + 1, n - 2, u);
    if (u[2] == 0x11) return 0;
    addr = u[7] | u[8] << 8 | u[9] << 16 | (uint32_t)u[10] << 24;
    len = u[11] | u[12] << 8 | u[13] << 16;
    memcpy(m, "\x00\x59\x30", 3);
    m[3] = (uint8_t)(len + 8); m[4] = (uint8_t)((len + 8) >> 8); m[5] = 0; m[6] = 0;
    memcpy(m + 7, &addr, 4);
    m[11] = (uint8_t)len; m[12] = (uint8_t)(len >> 8); m[13] = 0;
    if (addr >= 0xE0000000u) {
        memset(m + 14, 0, len);
        memcpy(m + 14, "success", 8);
    } else {
        if (addr + len > logical_len) { printf("read past the package %#x\n", addr); exit(1); }
        memcpy(m + 14, logical + addr, len);
    }
    for (i = 6; i < 14 + len; i++) s += m[i];
    m[14 + len] = (uint8_t)~s;
    rx_len = pack7(m, 15 + len, rx);
    rx_full = 1;
    return 0;
}
static int ota_frame_get(const uint8_t **p, uint32_t *n) { if (!rx_full) return 0; *p = rx; *n = rx_len; return 1; }
static void ota_frame_done(void) { rx_full = 0; }
static uint32_t ota_now_ms(void) { return now_ms; }
static void ota_idle(void) { now_ms++; }
static int ota_erase(uint32_t off) { (void)off; return -1; }
static int ota_prog(uint32_t off, const void *p, uint32_t n) { (void)off; (void)p; (void)n; return -1; }
static int ota_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static void ota_show(uint32_t step, int32_t code) { (void)step; (void)code; }
static void ota_commit(const uint8_t *parm) { (void)parm; }
#include "../firmware/src/ota.c"

static int ldr_fread(uint32_t off, void *p, uint32_t n) { memcpy(p, nor + off, n); return 0; }
static int ldr_erase(uint32_t off)                  /* fm1_flash.h fl_erase4k_ram */
{
    if (!FL_GUARD(off, 0x1000u) || (off & 0xFFFu)) { refused++; return -1; }
    erases++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int ldr_prog(uint32_t off, const void *p, uint32_t n)   /* fm1_flash.h fl_prog_ram */
{
    const uint8_t *s = p;
    uint32_t i;
    if (!FL_GUARD(off, n) || n == 0 || n > 256u || ((off & 0xFFu) + n) > 256u) { refused++; return -1; }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
static void ldr_record_clear(void) { record_cleared++; }
static void ldr_progress(uint32_t done, uint32_t total) { (void)done; (void)total; }
#include "../firmware/loader/ldr_core.c"

static uint8_t *load_logical(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    uint8_t *raw = malloc(0x200000), *lg;
    size_t n, i;
    if (!f) { perror(path); exit(2); }
    n = fread(raw, 1, 0x200000, f);
    fclose(f);
    if (n < 20 * 48 + 0x400) { printf("%s: too short for a package\n", path); exit(2); }
    lg = malloc(n);
    for (i = 0; i < 20; i++) memcpy(lg + i * 47, raw + i * 48, 47);
    memcpy(lg + 20 * 47, raw + 20 * 48, n - 20 * 48);
    *len = n - 20;
    free(raw);
    return lg;
}
static uint32_t flash_off(const uint8_t *lg)
{
    uint8_t h[0x400];
    uint32_t i, n;
    memcpy(h, lg, sizeof h);
    ota_jl_enc(h, 0x40);
    n = ota_rd16(h + 8);
    for (i = 0; i < n; i++) {
        uint8_t *e = h + 0x40 + i * 0x50u;
        ota_jl_enc(e, 0x50);
        if (ota_rd16(e) == 0) return ota_rd32(e + 8);
    }
    return 0;
}
static int bad;
static void check(const char *what, int ok) { printf("smk ldr: %-90s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static int same(uint32_t lo, uint32_t hi) { return !memcmp(nor + lo, ref + lo, hi - lo); }

int main(int argc, char **argv)
{
    size_t olen, flen, own_len;
    uint8_t *old, *fm1, *own;
    uint32_t ofo, nfo, i;
    int rc;
    if (argc < 4) { printf("usage: smk_ldr_test OLD_SMK.fwsc NEW_SMK.fwsc FM1.fwsc\n"); return 2; }
    old = load_logical(argv[1], &olen);
    own = logical = load_logical(argv[2], &own_len);
    logical_len = own_len;
    fm1 = load_logical(argv[3], &flen);
    ofo = flash_off(old);
    nfo = flash_off(logical);
    /* the device: OLD installed over an SMK whose other bytes are all patterns */
    for (i = 0; i < sizeof nor; i++) nor[i] = (uint8_t)(i * 13u ^ i >> 9 ^ 0xA5u);
    memcpy(nor, old + ofo, 0x9D000);
    memset(nor + 0xC0000, 0x42, 0x1000);                       /* BTIF */
    for (i = 0; i < 0x22Cu; i++) nor[0xC1000 + i] = (uint8_t)(i * 3u + 1u);   /* USRTRIM */
    memcpy(nor + 0xEAFF8, "usrdata", 8);
    {
        uint8_t r[112] = {0};
        r[2] = 0x0D; r[3] = 0x5A; r[4] = 0x01; r[5] = 0x5A; r[6] = 0x41; r[7] = 0x54;
        ota_wr16(r, ota_crc16(r + 2, 78, 0));
        memcpy(nor + 0xBFF00, r, sizeof r);                    /* the update record (ota.c OTA_RES) */
        memcpy(nor + 0xA0F00, r, sizeof r);                    /* user data in store A that looks like one */
        memcpy(nor + 0xC1F00, r, sizeof r);                    /* ... and in USRTRIM, BTIF, USRFLASH, USR */
        memcpy(nor + 0xC0F00, r, sizeof r);
        memcpy(nor + 0xD5F00, r, sizeof r);
        memcpy(nor + 0xF8F00, r, sizeof r);
    }
    memcpy(ref, nor, sizeof nor);
    /* the FM-1's package: refused before anything is erased */
    logical = fm1; logical_len = flen;
    erases = refused = 0;
    rc = ldr_session();
    check("the FM-1's package (flash.bin 0x93000) refused, nothing erased", rc == -3 && erases == 0 && same(0, 0x100000u));
    logical = own; logical_len = own_len;
    /* a package whose region table is not the device's (an SMK package on an FM-1, another layout) */
    nor[FL_RT_OFF + 7] ^= 0x01u;
    erases = 0;
    rc = ldr_session();
    check("a region table not the device's refused (-14), nothing erased", rc == -14 && erases == 0);
    nor[FL_RT_OFF + 7] ^= 0x01u;
    /* the install */
    erases = refused = record_cleared = 0;
    rc = ldr_session();
    printf("  rc %d, %u sector erases\n", rc, erases);
    check("install completes, no erase / program refused by the guards", rc == 0 && refused == 0);
    check("app area 0x4000-0x9CFFF == the new package's flash.bin", !memcmp(nor + 0x4000, logical + nfo + 0x4000, 0x9D000 - 0x4000));
    check("the head [0, 0x4000) untouched", same(0, 0x4000u));
    check("the update record at 0xBFF00 cleared (RAM and flash)", record_cleared && nor[0xBFF06] == 0xFF);
    check("store A (0x9D000-0xBAFFF), its record look-alike included, untouched", same(0x9D000u, 0xBB000u));
    check("BTIF, USRTRIM (their record look-alikes included), USRFLASH, the marker, store C, USR, key_mac untouched",
          same(0xC0000u, 0x100000u));
    check("the OTA area: only the record's sector erased", same(0xBB000u, 0xBF000u));
    erases = 0;
    rc = ldr_session();
    check("re-run with the same package erases nothing", rc == 0 && erases == 0);
    printf("%s\n", bad ? "SMK LOADER TEST FAILED" : "smk loader test passed");
    return bad != 0;
}
