/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * Adapted from Melodee's firmware/hal/fm1_flash_map.h:
 *   Copyright (C) 2026 Kerem Kilic (Ellic Studio)
 *   Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 *   SMK-37 port: Copyright (C) 2026 M. Geisler (genkigenki/makkuro)
 * Hardware notes: https://github.com/genkigenki/fm1-to-smk37 */
/* The SMK-37's flash map (BOARD_SMK37, fm1_cc.h): which ranges the RAM flash driver (fm1_flash.h) may erase /
 * program, as macros and compile-time checks only, so the host tests include it (tests/smk_flash_test.c). The FM-1
 * keeps its own map in fm1_flash.h, unchanged.
 *
 *   0x00000-0x03FFF  head (SPL, isd_config)                         NEVER
 *   0x04000-0x9CFFF  app area (the stock PRCT)                      the update loader only (FL_APP)
 *   0x9D000-0xBAFFF  VM: JIANT's store A (storage.c; the owner tag at 0xAF000)           FL_DATA
 *   0xBB000-0xBFFFF  VM: M-UPGRADE loader staging + the update record at 0xBFF00 (ota.c) FL_OTA
 *   0xC0000-0xC0FFF  BTIF (BT pairing)                              NEVER
 *   0xC1000-0xC1FFF  USRTRIM (the keys' / pads' factory calibration "trimdat")   NEVER
 *   0xC2000-0xE9FFF  USRFLASH: JIANT's store B (the stock mapping until JIANT first writes there)   FL_UFL
 *   0xEA000-0xEAFFF  USRFLASH's tail: the stock end marker "usrdata\0" at 0xEAFF8; never data, only erased whole,
 *                    once, before the first write into FL_UFL (FL_STOCKTAG; storage.c st_stock_release)
 *   0xEB000-0xF3FFF  unlisted: JIANT's store C                                            FL_GAP
 *   0xF4000-0xFDFFF  USR (the stock resources; kept for a return to stock)   NEVER
 *   0xFE000-0xFEFFF  free, unused                                    NEVER (not in any window)
 *   0xFF000-0xFFFFF  key_mac                                         NEVER
 * Everything is an explicit allow-list; the asserts keep each window out of the protected ranges at compile time,
 * and FL_PROTECTED is a second, independent net inside the RAM driver (FL_GUARD), also under the update loader's
 * own FL_RANGE_OK. */
#pragma once
#include <stdint.h>
#include "fm1_cc.h"

#if BOARD_SMK37
/* [off, off + n) inside [lo, hi), without wrapping (as fm1_flash.h's): off + n can overflow, and the 1 MiB part
 * ignores the high address bits, so a wrapped range lands low. */
#define FL_IN(off, n, lo, hi) ((uint32_t)(off) >= (lo) && (uint32_t)(off) <= (hi) && \
                               (uint32_t)(n) <= (hi) - (uint32_t)(off))
/* The flash chip the SMK build stores on: the SMK-37's own 1 MiB part (0xEB6014) and nothing else; the driver uses
 * only generic SPI NOR commands (9F, 05/35, 06, 20, 02, 0B). Any other id: RAM-only (project.c persist_boot). */
#define FL_JEDEC_OK(id) ((uint32_t)(id) == 0xEB6014u)

#define FL_HEAD_HI      0x00004000u                /* protected: [0, FL_HEAD_HI) */
#define FL_CAL_LO       0x000C0000u                /* protected: BTIF + USRTRIM */
#define FL_CAL_HI       0x000C2000u
#define FL_USRRES_LO    0x000F4000u                /* protected: the stock USR resources ... */
#define FL_USRRES_HI    0x000FF000u                /* ... and the free sector 0xFE000 above them */
#define FL_KEYMAC_LO    0x000FF000u                /* protected: key_mac and above */
#define FL_APP_LO       0x00004000u                /* the update loader: the app area */
#define FL_APP_HI       0x0009D000u
#define FL_PLAIN_LO     0x0009D000u                /* the plain XIP window from the VM's start */
#define FL_DATA_LO      0x0009D000u                /* JIANT's store A */
#define FL_DATA_HI      0x000BB000u
#define FL_OTA_LO       0x000BB000u                /* M-UPGRADE loader staging, ota.c */
#define FL_OTA_HI       0x000C0000u
#define FL_UFL_LO       0x000C2000u                /* JIANT's store B: USRFLASH up to the marker's sector */
#define FL_UFL_HI       0x000EA000u
#define FL_GAP_LO       0x000EB000u                /* JIANT's store C: the unlisted sectors below USR */
#define FL_GAP_HI       0x000F4000u
/* USRFLASH holds the stock firmware's key / knob mapping (global settings 0xC2000, 8 presets from 0xC3000) with its
 * end marker "usrdata\0" at 0xEAFF8: while the marker is there the stock firmware trusts the area and never rebuilds
 * it. So before the first write into USRFLASH the marker's sector is erased (storage.c st_stock_release), and a
 * return to stock rebuilds the factory mapping. Exactly that one sector, whole, and never as data. */
#define FL_STOCKTAG_LO  0x000EA000u
#define FL_STOCKTAG_HI  0x000EB000u
#define FL_STOCKTAG_OFF 0x000EAFF8u
#define FL_STOCKTAG_OK(off, n) ((uint32_t)(off) == FL_STOCKTAG_LO && (uint32_t)(n) == FL_STOCKTAG_HI - FL_STOCKTAG_LO)
/* a window [lo, hi) is allowed: sector aligned, non-empty, and clear of every protected range */
#define FL_WIN_SAFE(lo, hi) ((lo) % 0x1000u == 0u && (hi) % 0x1000u == 0u && (lo) < (hi) && \
                             (lo) >= FL_HEAD_HI && (hi) <= FL_KEYMAC_LO && \
                             ((hi) <= FL_CAL_LO || (lo) >= FL_CAL_HI) && \
                             ((hi) <= FL_USRRES_LO || (lo) >= FL_USRRES_HI))
_Static_assert(FL_WIN_SAFE(FL_DATA_LO, FL_DATA_HI), "SMK: FL_DATA overlaps a protected range");
_Static_assert(FL_WIN_SAFE(FL_OTA_LO, FL_OTA_HI), "SMK: FL_OTA overlaps a protected range");
_Static_assert(FL_WIN_SAFE(FL_UFL_LO, FL_UFL_HI), "SMK: FL_UFL overlaps a protected range");
_Static_assert(FL_WIN_SAFE(FL_GAP_LO, FL_GAP_HI), "SMK: FL_GAP overlaps a protected range");
_Static_assert(FL_WIN_SAFE(FL_APP_LO, FL_APP_HI), "SMK: FL_APP overlaps a protected range");
_Static_assert(FL_WIN_SAFE(FL_STOCKTAG_LO, FL_STOCKTAG_HI) && FL_STOCKTAG_LO == FL_UFL_HI &&
               FL_STOCKTAG_HI == FL_GAP_LO && FL_STOCKTAG_OFF + 8u == FL_STOCKTAG_HI,
               "SMK: the stock marker's sector lies between FL_UFL and FL_GAP, in neither");
_Static_assert(FL_UFL_LO == FL_CAL_HI && FL_GAP_HI == FL_USRRES_LO, "SMK: FL_UFL / FL_GAP bounds");
_Static_assert(FL_OTA_HI == FL_CAL_LO && FL_DATA_HI == FL_OTA_LO && FL_APP_HI == FL_DATA_LO,
               "SMK: the windows A, OTA and the app area meet, in that order, below BTIF");
/* the update loader (loader.c): the app area, and the update record's sectors (the OTA area) only */
#define FL_LDR_RANGE_OK(off, n) (FL_IN(off, n, FL_APP_LO, FL_APP_HI) || FL_IN(off, n, FL_OTA_LO, FL_OTA_HI))
/* JIANT's own store: FL_DATA, FL_UFL and FL_GAP (never the marker's sector) */
#define FL_STORE_OK(off, n) (FL_IN(off, n, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, n, FL_UFL_LO, FL_UFL_HI) || \
                             FL_IN(off, n, FL_GAP_LO, FL_GAP_HI))
#ifndef FL_RANGE_OK
#define FL_RANGE_OK(off, n) (FL_STORE_OK(off, n) || FL_IN(off, n, FL_OTA_LO, FL_OTA_HI) || FL_STOCKTAG_OK(off, n))
#endif
/* the second, independent net in the RAM driver: [off, off + n) touches a protected range (also when a caller
 * supplies its own FL_RANGE_OK, as the update loader does) */
#define FL_HITS(off, n, lo, hi) ((uint32_t)(off) < (hi) && \
                                 ((uint32_t)(off) >= (lo) || (uint32_t)(n) > (lo) - (uint32_t)(off)))
#define FL_PROTECTED(off, n) (FL_HITS(off, n, 0u, FL_HEAD_HI) || FL_HITS(off, n, FL_CAL_LO, FL_CAL_HI) || \
                              FL_HITS(off, n, FL_USRRES_LO, FL_USRRES_HI) || (uint32_t)(off) >= FL_KEYMAC_LO || \
                              (uint32_t)(n) > FL_KEYMAC_LO - (uint32_t)(off))
#define FL_GUARD(off, n) (FL_RANGE_OK(off, n) && !FL_PROTECTED(off, n))
/* the stock region table in flash.bin and in the device (app area offset 0x60: VM, PRCT, BTIF, USRTRIM, USRFLASH,
 * USR, SFC-enciphered with the chip key): ota.c and ldr_core.c refuse a package whose table is not the device's */
#define FL_RT_OFF       0x00004060u
#define FL_RT_LEN       0x000000C0u
#endif /* BOARD_SMK37 */
