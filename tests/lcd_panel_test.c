// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant). All rights reserved.
/* (V10.8.63) MENU > DISPLAY > PANEL (lcd.c): A, today's panel, sends to the ST7789 exactly what V10.8.62 sent (its
 * sequence, byte for byte, and lcd_init's stream: commands, their data, the clear); B adds the datasheet's power, VCOM
 * and gamma registers and INVOFF after A, once; going back to A runs A's reset and sequence again. The hardware calls
 * logged instead of sent. Built and run by CI (voice-user-check.yml) */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define FELUCCA_SCREEN_MIRROR 0
static uint32_t lg[4096], ln;
static void put(uint32_t v) { if (ln < 4096u) lg[ln++] = v; }
static void fm1_lcd_hw_init(void) { put(0x1000u); }
static void fm1_lcd_send_cmd(uint8_t c) { put(0x100u | c); }
static void fm1_lcd_send_data(const void *p, uint32_t n)
{
    uint32_t i;
    put(0x2000u | (n & 0xFFFu));
    for (i = 0; i < n && i < 16u; i++) put(((const uint8_t *)p)[i]);
}
static void fm1_lcd_wait(void) {}
static void fm1_lcd_deselect(void) {}
static void fm1_lcd_baud(uint32_t b) { put(0x3000u | b); }
static void fm1_lcd_backlight(uint32_t on) { put(0x4000u | on); }
#define lcd_spin lcd_spin_real
#include "../firmware/src/lcd.c"
#undef lcd_spin

/* V10.8.62's sequence and its loop, as they were */
static const uint8_t SEQ62[] = {0x01, 0, 0x00, 1, 150, 0x11, 0, 0x00, 1, 120, 0x3A, 1, 0x55, 0x36, 1, 0x00, 0x21, 0, 0x13, 0};
static void init62(void)
{
    uint32_t r, x;
    fm1_lcd_hw_init();
    for (r = 0; r < sizeof SEQ62; r += 2u + SEQ62[r + 1u]) {
        if (SEQ62[r] == 0x00u)
            continue;
        lcd_cmd(SEQ62[r]);
        for (x = 0; x < SEQ62[r + 1u]; x++)
            lcd_small[x] = SEQ62[r + 2u + x];
        if (SEQ62[r + 1u])
            lcd_data(lcd_small, SEQ62[r + 1u]);
    }
    fm1_lcd_baud(LCD_BAUD);
    lcd_fill(0, 0, 240, 240, 0);
    lcd_cmd(0x29);
}
static int check(const char *what, int ok) { printf("lcd: %-96s %s\n", what, ok ? "ok" : "FAIL"); return !ok; }
int main(void)
{
    static uint32_t a[4096], an;
    uint32_t i, k, found;
    int bad = 0;
    bad += check("A's sequence: V10.8.62's bytes", sizeof LCD_SEQ == sizeof SEQ62 && !memcmp(LCD_SEQ, SEQ62, sizeof SEQ62));
    ln = 0; init62(); memcpy(a, lg, sizeof lg); an = ln;
    ln = 0; lcd_init();
    bad += check("A's lcd_init: V10.8.62's stream (commands, data, the clear, DISPON)", ln == an && !memcmp(a, lg, ln * 4u));
    ln = 0; lcd_panel_apply(0, 1);
    bad += check("A at boot: nothing more sent", ln == 0u);
    ln = 0; lcd_panel_apply(1, 1);
    for (i = 0, k = 0; i < ln; i++) k += lg[i] >= 0x100u && lg[i] < 0x200u;
    found = 0;
    for (i = 0; i < ln; i++) found |= (lg[i] == (0x100u | 0xE0u)) << 0 | (lg[i] == (0x100u | 0xE1u)) << 1 | (lg[i] == (0x100u | 0x20u)) << 2 |
                                       (lg[i] == (0x100u | 0xBBu)) << 3 | (lg[i] == (0x100u | 0x01u)) << 4;
    bad += check("B at boot: its twelve registers after A (gammas, VCOMS, INVOFF), no reset", k == 12u && found == 15u);
    ln = 0; lcd_panel_apply(1, 0);
    bad += check("B again: nothing (already sent)", ln == 0u);
    ln = 0; lcd_panel_apply(0, 0);
    bad += check("back to A: A's reset and sequence again (V10.8.62's stream)", ln == an && !memcmp(a, lg, ln * 4u));
    printf(bad ? "lcd: %d FAILED\n" : "lcd: all ok\n", bad);
    return bad != 0;
}
