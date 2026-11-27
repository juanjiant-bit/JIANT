/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (after Melodee's tests/smk_flash_test.c, Copyright (C) 2026 M. Geisler (genkigenki/makkuro),
 *  Copyright (C) 2026 Kerem Kilic (Ellic Studio)) */
/* The SMK-37's flash map and JIANT's store on it (build with -DBOARD_SMK37=1 -Ifirmware/hal):
 *   - every 4 KiB sector of the 1 MiB part, and pages / bytes at every protected boundary, through the app's
 *     allow-list (FL_STORE_OK, FL_RANGE_OK), the RAM driver's net (FL_PROTECTED, FL_GUARD) and the update loader's
 *     (FL_LDR_RANGE_OK): none may reach the head 0x0-0x3FFF, BTIF / USRTRIM 0xC0000-0xC1FFF, USR 0xF4000-0xFDFFF,
 *     the free 0xFE000 or key_mac 0xFF000-; straddling and wrapping ranges refused;
 *   - storage.c's SMK table (st_sector): every object's pair inside the store windows, no two sharing a sector,
 *     never the owner tag, the voice bank, the marker's sector or the OTA area;
 *   - the store (storage.c + the storage_hw.c hooks, mirrored) on a simulated NOR whose protected ranges and stock
 *     data hold patterns: a first boot over another firmware's store, every object saved and loaded back, the voice
 *     bank's sector, the stock mapping in USRFLASH kept until the first write there, its marker released exactly
 *     once; erases and programs aimed at each forbidden range refused; afterwards every protected byte, the app area
 *     and the OTA area byte-identical;
 *   - FL_JEDEC_OK: the SMK's 0xEB6014 only. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1 -Ifirmware/hal"
#endif
#include "fm1_flash_map.h"

static uint8_t nor[0x100000], ref[0x100000];
static uint32_t refused, erases, tag_erases, fail_erase, net_refused;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off > sizeof nor || n > sizeof nor - off) return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
/* the RAM driver (fm1_flash.h fl_erase4k_ram / fl_prog_ram): FL_GUARD, then the part */
static int drv_erase(uint32_t off)
{
    if (!FL_GUARD(off, 0x1000u) || (off & 0xFFFu)) { net_refused++; return -1; }
    if (fail_erase && off == fail_erase) return -3;
    if (off == FL_STOCKTAG_LO) tag_erases++;
    memset(nor + off, 0xFF, 0x1000u);
    erases++;
    return 0;
}
static int drv_prog(uint32_t off, const uint8_t *s, uint32_t n)
{
    uint32_t i;
    if (!FL_GUARD(off, n) || n == 0 || n > 256u || ((off & 0xFFu) + n) > 256u) { net_refused++; return -1; }
    for (i = 0; i < n; i++) nor[off + i] &= s[i];
    return 0;
}
/* storage_hw.c's st_erase / st_erase_raw / st_prog, line for line, on the simulated driver */
static void st_stock_release(void);
static int st_in_usrflash(uint32_t off);
static int st_erase_raw(uint32_t off);
#define ST_ERASE_MARKER() st_erase_raw(FL_STOCKTAG_LO)
static int st_erase(uint32_t off)
{
    if (!FL_STORE_OK(off, 0x1000u)) { refused++; return -8; }
    if (st_in_usrflash(off)) st_stock_release();
    return st_erase_raw(off);
}
static int st_erase_raw(uint32_t off)
{
    if (!FL_STORE_OK(off, 0x1000u) && !FL_STOCKTAG_OK(off, 0x1000u)) { refused++; return -8; }
    return drv_erase(off);
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    if (!FL_STORE_OK(off, n)) { refused++; return -8; }
    if (st_in_usrflash(off)) st_stock_release();
    while (n) {                                     /* fl_write: page by page */
        uint32_t k = 256u - (off & 0xFFu);
        int rc;
        if (k > n) k = n;
        if ((rc = drv_prog(off, s, k)) != 0) return rc;
        off += k; s += k; n -= k;
    }
    return 0;
}
#include "../firmware/src/storage.c"

static int bad;
static void check(const char *what, int ok) { printf("smk flash: %-92s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

static int protected_byte(uint32_t a)
{
    return a < 0x4000u || (a >= 0xC0000u && a < 0xC2000u) || (a >= 0xF4000u && a < 0xFF000u) || a >= 0xFF000u;
}
static int same(uint32_t lo, uint32_t hi) { return !memcmp(nor + lo, ref + lo, hi - lo); }
static int all_protected_same(void)
{
    return same(0, 0x4000u) && same(0xC0000u, 0xC2000u) && same(0xF4000u, 0x100000u);
}
static void stock(void)                            /* the stock mapping in USRFLASH, as V016 writes it */
{
    uint32_t n;
    memset(nor + 0xC2000, 0xFF, 0x29000);
    memset(nor + 0xC2000, 0x07, 0x86);
    nor[0xC2000] = 0xA3; nor[0xC2001] = 0x3A;
    for (n = 0; n < 8u; n++) {
        memset(nor + 0xC3000 + n * 0x5000, 0x01, 0x29E3);
        nor[0xC3000 + n * 0x5000 + 4] = 0x7F;
    }
    memcpy(nor + 0xEAFF8, "usrdata", 8);
}
static void device(void)                           /* an SMK as M-VAVE ships it: every byte a pattern */
{
    uint32_t i;
    for (i = 0; i < sizeof nor; i++) nor[i] = (uint8_t)(i * 29u ^ i >> 11 ^ 0x5Au);
    memset(nor + 0xC0000, 0x42, 0x1000);           /* BTIF */
    for (i = 0; i < 0x22Cu; i++) nor[0xC1000 + i] = (uint8_t)(i * 3u + 1u);   /* USRTRIM "trimdat" */
    stock();
    memcpy(nor + ST_SMK_TAG, "SMK37DATASLOOP", 15);   /* another port's store */
}

static uint8_t payload[ST_PAYLOAD_MAX], back[ST_PAYLOAD_MAX];

int main(void)
{
    uint32_t s, o, c, k, leak = 0, leak_net = 0, leak_ldr = 0, allowed = 0;
    /* ---- the map ---- */
    for (s = 0; s < 0x100000u; s += 0x1000u) {
        int p = protected_byte(s);
        allowed += FL_GUARD(s, 0x1000u);
        if (!p) continue;
        for (o = 0; o < 0x1000u; o += 0x100u) {
            leak += FL_STORE_OK(s + o, 256u) || FL_RANGE_OK(s + o, 256u) || FL_GUARD(s + o, 256u) || FL_GUARD(s + o, 1u);
            leak_net += !FL_PROTECTED(s + o, 256u) || !FL_PROTECTED(s + o, 1u);
            leak_ldr += FL_LDR_RANGE_OK(s + o, 256u) && !FL_PROTECTED(s + o, 256u);
        }
        leak += FL_STORE_OK(s, 0x1000u) || FL_RANGE_OK(s, 0x1000u) || FL_GUARD(s, 0x1000u);
        leak_net += !FL_PROTECTED(s, 0x1000u);
        leak_ldr += FL_LDR_RANGE_OK(s, 0x1000u);
    }
    check("no protected sector, page or byte passes the app's allow-list (FL_STORE_OK / FL_RANGE_OK / FL_GUARD)", leak == 0);
    check("the RAM driver's own net (FL_PROTECTED) covers every protected sector, page and byte", leak_net == 0);
    check("no protected sector passes the update loader's allow-list (FL_LDR_RANGE_OK)", leak_ldr == 0);
    {   /* the boundaries, one byte either side */
        static const uint32_t B[] = {0x4000u, 0xC0000u, 0xC2000u, 0xF4000u};   /* (0xF4000.. protected to the end) */
        uint32_t ok = 1;
        for (k = 0; k < sizeof B / sizeof B[0]; k++) {
            ok &= FL_PROTECTED(B[k] - 1u, 1u) != FL_PROTECTED(B[k], 1u);
            ok &= FL_PROTECTED(B[k] - 1u, 2u);          /* any range touching a protected byte */
        }
        check("each protected range starts / ends exactly at its boundary; a range touching it is refused", (int)ok);
    }
    check("straddling ranges refused: 0xBF000+0x2000, 0xE9000+0x2000, 0xEA000+0x2000, 0xF3000+0x2000, 0x3000+0x2000",
          !FL_GUARD(0xBF000u, 0x2000u) && !FL_GUARD(0xE9000u, 0x2000u) && !FL_GUARD(0xEA000u, 0x2000u) &&
          !FL_GUARD(0xF3000u, 0x2000u) && !FL_LDR_RANGE_OK(0xBF000u, 0x2000u) && !FL_LDR_RANGE_OK(0x3000u, 0x2000u) &&
          FL_PROTECTED(0xBF000u, 0x2000u) && FL_PROTECTED(0x3F00u, 0x200u) && FL_PROTECTED(0xF3F00u, 0x200u));
    check("wrapping ranges refused (off + n past 4 GiB lands low on the 1 MiB part)",
          !FL_GUARD(0xBB000u, 0xFFFFF000u) && !FL_LDR_RANGE_OK(0x9D000u, 0xFFFFFF00u) && FL_PROTECTED(0xBB000u, 0xFFFFF000u) &&
          !FL_GUARD(0xFFFFFF00u, 0x200u) && FL_PROTECTED(0xFFFFFF00u, 0x200u) && FL_PROTECTED(0x100000u, 0x100u));
    check("app windows: FL_DATA 0x9D000-0xBAFFF, FL_OTA 0xBB000-0xBFFFF, FL_UFL 0xC2000-0xE9FFF, FL_GAP 0xEB000-0xF3FFF",
          FL_GUARD(0x9D000u, 0x1000u) && FL_GUARD(0xBA000u, 0x1000u) && FL_GUARD(0xBF000u, 0x1000u) &&
          FL_GUARD(0xC2000u, 0x1000u) && FL_GUARD(0xE9000u, 0x1000u) && FL_GUARD(0xEB000u, 0x1000u) &&
          FL_GUARD(0xF3000u, 0x1000u) && !FL_GUARD(0xF4000u, 0x1000u) && !FL_GUARD(0xFE000u, 0x1000u) &&
          !FL_GUARD(0x9C000u, 0x1000u) &&
          allowed == (0xC0000u - 0x9D000u + 0xEA000u - 0xC2000u + 0x1000u + 0xF4000u - 0xEB000u) / 0x1000u);
    check("the stock marker's sector 0xEA000: its whole-sector erase only (the release), never a store window",
          FL_GUARD(0xEA000u, 0x1000u) && !FL_STORE_OK(0xEA000u, 0x1000u) && !FL_GUARD(0xEA000u, 256u) &&
          !FL_GUARD(0xEAF00u, 256u) && !FL_STORE_OK(0xEAFF8u, 8u) && FL_STOCKTAG_OFF == 0xEAFF8u);
    check("loader: the app area 0x4000-0x9CFFF and the OTA area 0xBB000-0xBFFFF only",
          FL_LDR_RANGE_OK(0x4000u, 0x1000u) && FL_LDR_RANGE_OK(0x9C000u, 0x1000u) && FL_LDR_RANGE_OK(0xBF000u, 0x1000u) &&
          FL_LDR_RANGE_OK(0xBB000u, 0x1000u) && !FL_LDR_RANGE_OK(0x9D000u, 0x1000u) && !FL_LDR_RANGE_OK(0xBA000u, 0x1000u) &&
          !FL_LDR_RANGE_OK(0xC0000u, 0x1000u) && !FL_LDR_RANGE_OK(0xC2000u, 0x1000u) && !FL_LDR_RANGE_OK(0x3000u, 0x1000u));
    check("FL_JEDEC_OK: the SMK's 0xEB6014 only (not the FM-1's 0x856014 / 0xB36014, not 0, not 0xFFFFFF)",
          FL_JEDEC_OK(0xEB6014u) && !FL_JEDEC_OK(0x856014u) && !FL_JEDEC_OK(0xB36014u) && !FL_JEDEC_OK(0u) &&
          !FL_JEDEC_OK(0xFFFFFFu) && !FL_JEDEC_OK(0x1EB6014u));
    check("FL_PLAIN_LO = the VM start 0x9D000; the region table at 0x4060, 0xC0 bytes",
          FL_PLAIN_LO == 0x9D000u && FL_RT_OFF == 0x4060u && FL_RT_LEN == 0xC0u);

    /* ---- storage.c's table ---- */
    {
        static uint8_t owner[256];
        uint32_t outside = 0, overlap = 0, n = 0;
        memset(owner, 0xFF, sizeof owner);
        owner[ST_SMK_TAG >> 12] = 0xF0;
        owner[ST_SMK_VBANK >> 12] = 0xF1;
        for (o = 0; o <= OBJ_COUNT; o++) {             /* (OBJ_COUNT: here OBJ_JAM, V10.8's JAM takes) */
            if (o == OBJ_FM6BANK) continue;            /* (OBJ_UPFM6's pair: one object retired into the other) */
            for (c = 0; c < 2u; c++) {
                uint32_t at = st_sector(o == OBJ_COUNT ? OBJ_JAM : o, c);
                n++;
                outside += !FL_STORE_OK(at, 0x1000u) || (at & 0xFFFu) || FL_PROTECTED(at, 0x1000u) ||
                           (at >= FL_OTA_LO && at < FL_OTA_HI) || at == FL_STOCKTAG_LO;
                if (owner[at >> 12] != 0xFF) overlap++;
                owner[at >> 12] = (uint8_t)o;
            }
        }
        check("storage.c: 62 copies (31 objects x A/B: V10.8's 6 songs and the JAM) in FL_DATA / FL_UFL / FL_GAP, off OTA",
              n == 62u && outside == 0 && OBJ_COUNT - OBJ_SONG1 == 20u);
        check("storage.c: no two copies share a sector, nor the owner tag 0xAF000 or the voice bank 0xAB000",
              overlap == 0 && owner[0xAF] == 0xF0 && owner[0xAB] == 0xF1);
        check("storage.c: OBJ_FM6BANK and OBJ_UPFM6 one pair (as the FM-1's); the settings at 0x9D000 / 0x9E000",
              st_sector(OBJ_FM6BANK, 0) == st_sector(OBJ_UPFM6, 0) && st_sector(OBJ_FM6BANK, 1) == st_sector(OBJ_UPFM6, 1) &&
              st_sector(OBJ_SETTINGS, 0) == 0x9D000u && st_sector(OBJ_SETTINGS, 1) == 0x9E000u);
        for (s = 0, k = 0; s < 0x100u; s++) k += owner[s] == 0xFF && FL_STORE_OK(s << 12, 0x1000u);
        check("storage.c: 15 store sectors spare (0xE2000..0xE9FFF, 0xED000..0xF3FFF); the JAM at 0xEB000 / 0xEC000",
              k == 15u && owner[0xF3] == 0xFF && owner[0xE2] == 0xFF && owner[0xE1] != 0xFF &&
              st_sector(OBJ_JAM, 0) == 0xEB000u && st_sector(OBJ_JAM, 1) == 0xEC000u);
    }

    /* ---- the store on a simulated SMK ---- */
    device();
    memcpy(ref, nor, sizeof nor);
    refused = net_refused = erases = tag_erases = 0;
    k = (uint32_t)st_owner_check();
    check("first boot over another port's store: cleared (A, C), the intact stock mapping in USRFLASH kept, tagged JIANT",
          k == 1u && nor[0x9D000u] == 0xFF && nor[0xEB000u] == 0xFF && nor[0xF3FFFu] == 0xFF &&
          same(0xC2000u, 0xEB000u) && st_stock_intact() && tag_erases == 0 &&
          !memcmp(nor + ST_SMK_TAG, "SMK37DATAJIANT\0", 16) && refused == 0 && net_refused == 0);
    check("... nothing outside the store windows touched: head, app area, OTA area, BTIF, USRTRIM, USR, key_mac",
          all_protected_same() && same(0x4000u, 0x9D000u) && same(0xBB000u, 0xC0000u));
    check("... and at the next boot (ours now): nothing erased", (erases = 0, st_owner_check() == 0) && erases == 0);
    for (o = 0; o < OBJ_SONG1 + 4u; o++) {         /* everything outside USRFLASH first: the stock mapping stays */
        if (o == OBJ_FM6BANK) continue;
        for (k = 0; k < sizeof payload; k++) payload[k] = (uint8_t)(k * 7u + o);
        if (st_save(o, payload, sizeof payload) || st_save(o, payload, sizeof payload)) break;
    }
    check("every object outside USRFLASH saved twice (both copies): the stock mapping and its marker kept",
          o == OBJ_SONG1 + 4u && st_stock_intact() && tag_erases == 0 && refused == 0 && net_refused == 0);
    o = OBJ_JAM;                                    /* (V10.8) the JAM takes (store C) */
    for (k = 0; k < sizeof payload; k++) payload[k] = (uint8_t)(k * 5u + o);
    k = st_save(o, payload, sizeof payload) == 0 && st_save(o, payload, sizeof payload) == 0;
    check("the JAM takes (store C) saved: the stock mapping still kept", k && st_stock_intact() && tag_erases == 0);
    for (o = OBJ_SONG1 + 4u; o < OBJ_COUNT; o++) {   /* songs 3..6: USRFLASH */
        for (k = 0; k < sizeof payload; k++) payload[k] = (uint8_t)(k * 3u + o);
        if (st_save(o, payload, sizeof payload) || st_save(o, payload, sizeof payload)) break;
    }
    check("songs 3..6 into USRFLASH: the marker's sector erased first, exactly once",
          o == OBJ_COUNT && tag_erases == 1u && !st_stock_tagged() && nor[0xEAFF8u] == 0xFF &&
          refused == 0 && net_refused == 0);
    {
        uint32_t good = 1;
        for (o = 0; o <= OBJ_COUNT; o++) {
            uint32_t ob = o == OBJ_COUNT ? OBJ_JAM : o, mul = o == OBJ_COUNT ? 5u : o < OBJ_SONG1 + 4u ? 7u : 3u;
            if (o == OBJ_FM6BANK) continue;
            for (k = 0; k < sizeof payload; k++) payload[k] = (uint8_t)(k * mul + ob);
            good &= st_load(ob, back, sizeof back) == (int)sizeof back && !memcmp(back, payload, sizeof back);
        }
        check("every object loads back", (int)good);
    }
    check("the voice bank's sector (fm6_vbank.c FVB_HOME 0xAB000): erase + program pass the guards",
          st_erase(ST_SMK_VBANK) == 0 && st_prog(ST_SMK_VBANK + 224u, payload, 600u) == 0 && net_refused == 0);
    {   /* aimed at every forbidden range: refused by st_erase / st_prog, and by the driver's own net */
        static const uint32_t BAD[] = {0x0u, 0x1000u, 0x3000u, 0x3F00u, 0x4000u, 0x50000u, 0x9C000u, 0x9CF00u,
                                       0xBB000u, 0xBFF00u, 0xC0000u, 0xC0F00u, 0xC1000u, 0xC1F00u, 0xEA000u, 0xEAF00u,
                                       0xF4000u, 0xF8000u, 0xFD000u, 0xFDF00u, 0xFE000u, 0xFF000u, 0xFFF00u,
                                       0x100000u, 0x1C0000u, 0xFFFFF000u};
        uint32_t ok = 1, before = refused, nb = net_refused;
        for (k = 0; k < sizeof BAD / sizeof BAD[0]; k++) {
            uint32_t a = BAD[k];
            ok &= st_prog(a, payload, 256u) != 0;
            if (!(a & 0xFFFu)) ok &= st_erase(a) != 0 && (a == FL_STOCKTAG_LO || st_erase_raw(a) != 0);
            if (protected_byte(a & 0xFFFFFu) || a >= 0x100000u)
                ok &= drv_erase(a & ~0xFFFu) != 0 && drv_prog(a, payload, 16u) != 0;
        }
        check("erase / program of the head, the app area, OTA, BTIF, USRTRIM, the marker, USR, 0xFE000, key_mac, wraps: refused",
              ok && refused > before && net_refused > nb);
    }
    check("after it all: head, BTIF, USRTRIM, USR, 0xFE000, key_mac byte-identical", all_protected_same());
    check("after it all: the app area 0x4000-0x9CFFF and the OTA area 0xBB000-0xBFFFF byte-identical",
          same(0x4000u, 0x9D000u) && same(0xBB000u, 0xC0000u));
    {   /* the owner check's other cases */
        device();
        memset(nor + 0xC2000u, 0x33, 0x2000u);      /* USRFLASH holds something else under the marker */
        tag_erases = 0;
        (void)st_owner_check();
        check("the marker over anything but the stock mapping: released at boot, USRFLASH cleared",
              !st_stock_tagged() && tag_erases == 1u && nor[0xC2000u] == 0xFF);
        device();
        memcpy(ref, nor, sizeof nor);
        fail_erase = 0xB3000u;                      /* one sector of another port's store will not erase */
        check("an erase fails: -1, the other port's tag stays (cleared again at the next boot)",
              st_owner_check() == -1 && !memcmp(nor + ST_SMK_TAG, "SMK37DATASLOOP", 15));
        fail_erase = 0;
        check("... the next boot clears it and tags it",
              st_owner_check() == 1 && !memcmp(nor + ST_SMK_TAG, "SMK37DATAJIANT", 14) && all_protected_same());
    }
    printf("%s\n", bad ? "SMK FLASH TEST FAILED" : "smk flash test passed");
    return bad != 0;
}
