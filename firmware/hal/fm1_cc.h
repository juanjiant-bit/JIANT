/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* FM1_INLINE: register helpers that must inline into their caller, so the
 * SFR access sequence (and its timing, in ISRs and IRQ-off sections) is the
 * one written at the call site; -Os would otherwise out-line a helper with
 * several callers. RAM-resident code (.ram_text) may only use these, never
 * a called helper (build.py checks .ram_text for calls). */
#pragma once

#define FM1_INLINE static inline __attribute__((always_inline))

/* (JIANT, after Melodee's BOARD_SMK37) the target board: 0 the M-VAVE FM-1 (the default; its build gets no new flag
 * and stays byte-identical), 1 the M-VAVE SMK-37 Pro / Elite (same AC791N; tools/build.py --board smk37 sets
 * -DBOARD_SMK37=1, docs/SMK37.md). Every SMK difference is behind #if BOARD_SMK37. */
#ifndef BOARD_SMK37
#define BOARD_SMK37 0
#endif
#if BOARD_SMK37
#ifndef SMK_MODEL
#define SMK_MODEL "SMK-37 Pro"                     /* build.py --model elite: -DSMK_MODEL="SMK-37 Elite" */
#endif
#endif
