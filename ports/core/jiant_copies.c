/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* The copies' table and who holds each (jiant_copies.h): built once, beside the JIANT_COPIES jiant_copy.c's. */
#include "jiant_copies.h"

#if JIANT_COPIES < 1 || JIANT_COPIES > 16
#error "JIANT_COPIES: 1..16"
#endif
#define JC_DECL(k) extern const jiant_api jc##k##_jiant_api_table;
#define JC_REF(k) &jc##k##_jiant_api_table,
#define JC_EACH(M) M(0) JC_EACH1(M)
#if JIANT_COPIES > 1
#define JC_EACH1(M) M(1) JC_EACH2(M)
#else
#define JC_EACH1(M)
#endif
#if JIANT_COPIES > 2
#define JC_EACH2(M) M(2) JC_EACH3(M)
#else
#define JC_EACH2(M)
#endif
#if JIANT_COPIES > 3
#define JC_EACH3(M) M(3) JC_EACH4(M)
#else
#define JC_EACH3(M)
#endif
#if JIANT_COPIES > 4
#define JC_EACH4(M) M(4) JC_EACH5(M)
#else
#define JC_EACH4(M)
#endif
#if JIANT_COPIES > 5
#define JC_EACH5(M) M(5) JC_EACH6(M)
#else
#define JC_EACH5(M)
#endif
#if JIANT_COPIES > 6
#define JC_EACH6(M) M(6) JC_EACH7(M)
#else
#define JC_EACH6(M)
#endif
#if JIANT_COPIES > 7
#define JC_EACH7(M) M(7) JC_EACH8(M)
#else
#define JC_EACH7(M)
#endif
#if JIANT_COPIES > 8
#define JC_EACH8(M) M(8) JC_EACH9(M)
#else
#define JC_EACH8(M)
#endif
#if JIANT_COPIES > 9
#define JC_EACH9(M) M(9) JC_EACH10(M)
#else
#define JC_EACH9(M)
#endif
#if JIANT_COPIES > 10
#define JC_EACH10(M) M(10) JC_EACH11(M)
#else
#define JC_EACH10(M)
#endif
#if JIANT_COPIES > 11
#define JC_EACH11(M) M(11) JC_EACH12(M)
#else
#define JC_EACH11(M)
#endif
#if JIANT_COPIES > 12
#define JC_EACH12(M) M(12) JC_EACH13(M)
#else
#define JC_EACH12(M)
#endif
#if JIANT_COPIES > 13
#define JC_EACH13(M) M(13) JC_EACH14(M)
#else
#define JC_EACH13(M)
#endif
#if JIANT_COPIES > 14
#define JC_EACH14(M) M(14) JC_EACH15(M)
#else
#define JC_EACH14(M)
#endif
#if JIANT_COPIES > 15
#define JC_EACH15(M) M(15)
#else
#define JC_EACH15(M)
#endif

JC_EACH(JC_DECL)
static const jiant_api *const jc_tables[JIANT_COPIES] = {JC_EACH(JC_REF)};
static int jc_held[JIANT_COPIES];              /* 1: claimed (atomics) */

const jiant_api *jiant_copy_claim(uint32_t *index)
{
    uint32_t k;
    for (k = 0; k < JIANT_COPIES; k++) {
        int idle = 0;
        if (jc_tables[k]->jiant_api_size == sizeof(jiant_api) &&
            __atomic_compare_exchange_n(&jc_held[k], &idle, 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            if (index)
                *index = k;
            return jc_tables[k];
        }
    }
    return 0;
}
void jiant_copy_release(const jiant_api *c)
{
    if (c && c->jiant_api_index < JIANT_COPIES && jc_tables[c->jiant_api_index] == c)
        __atomic_store_n(&jc_held[c->jiant_api_index], 0, __ATOMIC_RELEASE);
}
const jiant_api *jiant_copy_get(uint32_t k) { return k < JIANT_COPIES ? jc_tables[k] : 0; }
uint32_t jiant_copies_free(void)
{
    uint32_t k, n = 0;
    for (k = 0; k < JIANT_COPIES; k++)
        n += !__atomic_load_n(&jc_held[k], __ATOMIC_ACQUIRE);
    return n;
}
