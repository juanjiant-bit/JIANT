/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (devcpu2) valgrind lackey's trace of tests/cpu_lines_test.c -> per block, per core and per part: the first address
 * touched of each 32-byte line of code (.text) and of constants (.rodata), and the instructions run (tests/cpu_lines_test.py)
 *   cpu_lines_lk MARK C1 PART EX RO_LO RO_HI TEXT_LO TEXT_HI OUT_PREFIX < trace
 * OUT_PREFIX.iK / .dK, K: 0 core 0, 1 core 1, 2..7 parts 0..5 (a "C n" line: the core it ran on), 8 core 1 outside the
 * parts (the buses), 9 core 0 outside them; a block ends with "N instructions" and "E" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define H (1 << 17)
#define NS 10
static unsigned long ik[NS][H], dk[NS][H];
static int add(unsigned long *t, unsigned long k)
{
    unsigned long h = (k * 0x9E3779B97F4A7C15ul) >> 47;
    while (t[h]) {
        if (t[h] == k) return 0;
        h = (h + 1) & (H - 1);
    }
    t[h] = k;
    return 1;
}
int main(int argc, char **argv)
{
    unsigned long mark, c1, pm, ex, lo, hi, tlo, thi, ni[NS] = {0};
    char fn[512], ln[256];
    FILE *fi[NS], *fd[NS];
    int on = 0, core = 0, part = -1, exo = 0, c;
    if (argc < 10) return 2;
    mark = strtoul(argv[1], 0, 16); c1 = strtoul(argv[2], 0, 16); pm = strtoul(argv[3], 0, 16); ex = strtoul(argv[4], 0, 16);
    lo = strtoul(argv[5], 0, 16); hi = strtoul(argv[6], 0, 16); tlo = strtoul(argv[7], 0, 16); thi = strtoul(argv[8], 0, 16);
    for (c = 0; c < NS; c++) {
        snprintf(fn, sizeof fn, "%s.i%d", argv[9], c); fi[c] = fopen(fn, "w");
        snprintf(fn, sizeof fn, "%s.d%d", argv[9], c); fd[c] = fopen(fn, "w");
        if (!fi[c] || !fd[c]) return 2;
    }
    while (fgets(ln, sizeof ln, stdin)) {
        char k = ln[0] == ' ' ? ln[1] : ln[0], *p = ln + 2;
        unsigned long a;
        int ps;
        while (*p == ' ') p++;
        a = strtoul(p, 0, 16);
        if (k == 'S' && a == mark) {
            if (on) {
                for (c = 0; c < NS; c++) { fprintf(fi[c], "N %lu\nE\n", ni[c]); fprintf(fd[c], "E\n"); ni[c] = 0; }
                memset(ik, 0, sizeof ik); memset(dk, 0, sizeof dk);
            }
            on = !on; core = 0; part = -1;
            continue;
        }
        if (k == 'S' && a == c1) { core = !core; continue; }
        if (k == 'S' && a >= pm && a < pm + 32) {
            int q = (int)((a - pm) / 4);
            part = part == q ? -1 : q;
            if (part >= 0 && part < 6 && on) fprintf(fi[2 + part], "C %d\n", core);
            continue;
        }
        if (k == 'S' && a == ex) { exo = !exo; continue; }
        if (!on || exo) continue;
        ps = part >= 0 && part < 6 ? 2 + part : 9 - core;
        if (k == 'I') {
            ni[core]++; ni[ps]++;
            if (a >= tlo && a < thi) {
                unsigned long l = (a >> 5) + 1;
                if (add(ik[core], l)) fprintf(fi[core], "%lx\n", a);
                if (add(ik[ps], l)) fprintf(fi[ps], "%lx\n", a);
            }
        } else if (a >= lo && a < hi) {
            unsigned long l = (a >> 5) + 1;
            if (add(dk[core], l)) fprintf(fd[core], "%lx\n", a);
            if (add(dk[ps], l)) fprintf(fd[ps], "%lx\n", a);
        }
    }
    for (c = 0; c < NS; c++) { fclose(fi[c]); fclose(fd[c]); }
    return 0;
}
