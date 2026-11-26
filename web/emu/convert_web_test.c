/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (studio's converter, /emu-convert/: V10.8.50 + web_proj_load / web_proj_save) a project of before V10.7, which
 * V10.8.51 refuses (OLD PROJECT: OPEN IN STUDIO), loaded here and saved in this build's FUNC: the saved bytes have
 * the SLICER block and no old GM drum part, so V10.8.51's proj_import takes them. argv[1]: the saved bytes written
 * there (for a check against the later code). */
#include "felucca_web.c"
static int bad;
static void ck(const char *what, int ok) { printf("%-66s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }
static void run(uint32_t blocks) { while (blocks--) web_render(512); }
static uint32_t old_image(int kind, project_store_t *out)   /* kind 0: the old GM drum part; 1: no SLICER block */
{
    static project_t o1, o2;
    static project_store_t a2;
    uint32_t at = 0, n2, hz;
    project_capture(&o1);
    if (kind == 0) { o1.t[3].preset = PROJ_DEF_KEEP; o1.sum = proj_sum(&o1); return proj_pack(out, &o1) ? PROJ_STORE_SIZE : 0u; }
    o1.g[G_SLMODE] = 0; o1.sum = proj_sum(&o1);
    o2 = o1; o2.g[G_SLMODE] = 1; o2.sum = proj_sum(&o2);
    if (!proj_pack(out, &o1) || !proj_pack(&a2, &o2)) return 0;
    while (at < PROJ_STORE_SIZE && out->raw[at] == a2.raw[at]) at++;
    at -= 2u;
    if (out->raw[at] != 'S' || out->raw[at + 1u] != 'L') return 0;
    n2 = 2u + G_SL_N;                                     /* (before JIANT 0.6.3) no such block */
    memmove(out->raw + at, out->raw + at + n2, PROJC_PFX_OFF - at - n2);
    memset(out->raw + PROJC_PFX_OFF - n2, 0, n2);
    hz = proj_hash(out->raw, PROJ_STORE_SIZE - 4u); memcpy(out->raw + PROJ_STORE_SIZE - 4u, &hz, 4);
    return PROJ_STORE_SIZE;
}
int main(int argc, char **argv)
{
    static project_store_t old;
    static project_t chk;
    uint32_t n, len, kind;
    web_nor_erase();
    web_boot();
    run(40);
    web_gen(1u + 2u, 1791, 60, 2);                         /* some music to make the old project of */
    run(8);
    for (kind = 0; kind < 2u; kind++) {
        n = old_image((int)kind, &old);
        ck(kind ? "a project of before 0.6.3 (no SLICER block) built" : "a project with the old GM drum part built", n != 0);
        memcpy(web_bk_buf(), &old, n);
        ck("  web_proj_load(1, len): 0 (converted, loaded)", web_proj_load(1, n) == 0);
        run(20);
        len = web_proj_save();
        ck("  web_proj_save: PROJ_STORE_SIZE bytes, FUNC", len == PROJ_STORE_SIZE && ((uint32_t *)web_bk_buf())[0] == PROJ_MAGIC);
        ck("  the saved bytes load, with their SLICER block (V10.8.51 reads them)",
           proj_import(&chk, web_bk_buf(), (int)len) && proj_sl_found == 1 && chk.t[3].preset != PROJ_DEF_KEEP);
        if (kind == 1u && argc > 1) { FILE *f = fopen(argv[1], "wb"); if (f) { fwrite(web_bk_buf(), 1, len, f); fclose(f); } }
    }
    memset(web_bk_buf(), 0x5A, 64);
    ck("bytes that are not a project: 2", web_proj_load(0, 64) == 2);
    ck("slot 4: 2", web_proj_load(4, 64) == 2);
    printf("convert-web: %s\n", bad ? "FAIL" : "all ok");
    return bad != 0;
}
