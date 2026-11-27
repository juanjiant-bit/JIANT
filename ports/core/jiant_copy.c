/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* One copy of the core (jiant_copies.h): jiant_core.c whole, every external symbol of it renamed by JIANT_COPY
 * (copy_symbols.h), and its API as a table, jcK_jiant_api_table. A port compiles it once per copy, each time with
 * JIANT_COPY defined to its number (a one-line file per copy that defines it and includes this one), with the flags
 * jiant_core.c takes. Nothing of the core changes for it. */
#ifndef JIANT_COPY
#error "JIANT_COPY: this copy's number"
#endif
#define JC_CAT_(a, b) a##b
#define JC_CAT(a, b) JC_CAT_(a, b)
#define JC_SYM(s) JC_CAT(JC_CAT(jc, JIANT_COPY), _##s)
#include "copy_symbols.h"
/* what the core exports from a shared library (felucca_web.c EXPORT, jiant_core.c JC_API: visibility "default") stays
 * inside the plugin for a copy: its callers are the table below */
#define visibility(v) visibility("hidden")
#include "jiant_core.c"
#include "jiant_copies.h"

#define JC_TABLE_ENTRY_(ret, name, args) , name
const jiant_api JC_SYM(jiant_api_table) = {
    (uint32_t)sizeof(jiant_api), JIANT_COPY JIANT_API_LIST(JC_TABLE_ENTRY_)
};
