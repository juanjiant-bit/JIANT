/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (studio's RADIO page) web_gen_info / web_gen_genre / web_gen_cmd on the native build of the emulator's glue:
 * a throw reported, NEXT counts a new piece with another seed, GENRE and MOOD set and read back, the names end */
#include "felucca_web.c"
static int bad;
static void ck(const char *what, int ok) { printf("%-60s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void run(uint32_t blocks) { while (blocks--) web_render(512); }
int main(void)
{
    int32_t *o;
    uint32_t i, n0, s0;
    web_nor_erase();
    web_boot();
    run(40);
    for (i = 0; web_gen_genre(i); i++) {}
    ck("web_gen_genre: the names, then NULL", i == web_gen_count() && i > 10u);
    web_gen_cmd(2, 3); web_gen_cmd(3, -20); run(2);
    o = web_gen_info();
    ck("GENRE and MOOD set and read back (asked 3, mood -20)", o[8] == 3 && o[3] == -20);
    web_gen_cmd(1, 0); run(8);
    o = web_gen_info(); n0 = (uint32_t)o[6]; s0 = (uint32_t)o[2];
    printf("  after a throw: radio %d genre %d seed %d mood %d energy %d form %d n %d playing %d asked %d\n",
           o[0], o[1], o[2], o[3], o[4], o[5], o[6], o[7], o[8]);
    ck("a throw: counted, the genre sounding is the one asked", n0 >= 1u && o[1] == 3);
    web_gen_cmd(4, 1); run(20);
    ck("PLAY", web_gen_info()[7] == 1);
    web_gen_cmd(0, 1); web_gen_cmd(1, 0); run(30);
    o = web_gen_info();
    ck("NEXT: one more piece, another seed, still playing, RADIO on", (uint32_t)o[6] == n0 + 1u && (uint32_t)o[2] != s0 && o[7] == 1 && o[0] == 1);
    web_gen_cmd(2, -1); web_gen_cmd(1, 0); run(30);
    o = web_gen_info();
    ck("EXPLORE asked: -1 asked, -1 sounding", o[8] == -1 && o[1] == -1);
    web_gen_cmd(2, -2); web_gen_cmd(1, 0); run(30);
    o = web_gen_info();
    ck("RANDOM asked: -2 asked, a real genre sounding", o[8] == -2 && o[1] >= 0 && o[1] < (int32_t)web_gen_count());
    web_gen_cmd(4, 0); run(30);
    ck("STOP", web_gen_info()[7] == 0);
    printf(bad ? "gen-web: %d FAILED\n" : "gen-web: all ok\n", bad);
    return bad != 0;
}
