/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test for ui_art.c: inflates every screen through the real draw path (canvas strips, blit)
 * into a 240x240 frame and compares it byte for byte with the frames tools/gen_ui_art.py --dump wrote.
 *
 *   python3 tools/gen_ui_art.py art firmware/src/ui_art_data.h --dump test/expect
 *   cc -O2 -I firmware/src -o test/host_test test/host_test.c && ./test/host_test test/expect test/out
 *
 * test/out/NN.bin: what the LCD would receive (byte-swapped RGB565), for previews.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CV_MAX (240u * 124u)                     /* as gfx.c */
static uint16_t cv_px[CV_MAX];
static uint32_t cv_w, cv_h;
static uint16_t frame[240 * 240];
static uint32_t blits;
static void cv_begin(uint32_t w, uint32_t h, uint16_t bg) { cv_w = w; cv_h = h; (void)bg; }
static void cv_blit(uint32_t x, uint32_t y)
{
    uint32_t r;
    blits++;
    for (r = 0; r < cv_h; r++)
        memcpy(&frame[(y + r) * 240 + x], &cv_px[r * cv_w], cv_w * 2);
}
#include "ui_art.c"

int main(int argc, char **argv)
{
    static uint16_t want[240 * 240];
    uint32_t id, fails = 0;
    char p[512];
    FILE *f;
    if (argc < 2) { fprintf(stderr, "usage: host_test expect_dir [out_dir]\n"); return 2; }
    for (id = 0; id < UI_ART_N; id++) {
        memset(frame, 0xAA, sizeof frame);
        blits = 0;
        ua_last = -1;
        ui_art_draw(id);
        snprintf(p, sizeof p, "%s/%02u.bin", argv[1], id);
        f = fopen(p, "rb");
        if (!f || fread(want, 2, 240 * 240, f) != 240 * 240) { printf("%-16s missing %s\n", ui_art[id].name, p); fails++; if (f) fclose(f); continue; }
        fclose(f);
        if (memcmp(want, frame, sizeof frame)) { printf("%-16s MISMATCH\n", ui_art[id].name); fails++; }
        else printf("%-16s ok  %6u bytes  %u blits\n", ui_art[id].name, ui_art[id].zlen, blits);
        if (argc > 2) {
            snprintf(p, sizeof p, "%s/%02u.bin", argv[2], id);
            f = fopen(p, "wb");
            if (f) { fwrite(frame, 2, 240 * 240, f); fclose(f); }
        }
    }
    /* partial redraw from the cached screen */
    ui_art_draw(0);
    ui_art_rect(0, 30, 20, 60, 16);
    printf("%s\n", fails ? "FAILED" : "all screens match");
    return fails ? 1 : 0;
}
