/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* ST7789-class 240x240 panel on SPI1:
 * PC9 CLK, PC10 DO, PC7 CS, PC8 D/C, backlight PA2 active low. Polled DMA
 * transfers; every source buffer must be in RAM.
 * A pixel transfer is left running (lcd_busy): the next LCD access, or a write
 * to its source buffer (lcd_sync), waits for it, so the main loop works
 * while the last strip of a frame goes out. */
/* pins and SPI1: hal/fm1_lcd_hw.h */
#ifndef LCD_BAUD
#define LCD_BAUD 4u                /* lsb/(BAUD+1): 4 = 12 MHz */
#endif

static uint8_t lcd_small[64];

/* the editor's screen mirror (editor_screen.c, EDITOR_PROTOCOL.md "Screen mirror"): while the editor asks for it, every
 * fill and strip sent to the panel is also sent to the editor, compressed (the firmware keeps no frame of its own) */
#ifndef FELUCCA_SCREEN_MIRROR
#define FELUCCA_SCREEN_MIRROR 1          /* 0: no mirror (928 B of flash and 432 B of the pool back; the editor hides its button) */
#endif
#if FELUCCA_OTA && FELUCCA_SCREEN_MIRROR
#define SCR_HAS_LCD 1
uint8_t scr_on __attribute__((section(".pool.mirror")));   /* (not static, after the pool: kept out of the compiler's merged
                                                         * globals, so no other variable moves; app.ld) */
static void scr_rows(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride, uint32_t fill);
#define SCR_TAP(x, y, w, h, px, stride, fill) do { if (scr_on) scr_rows(x, y, w, h, px, stride, fill); } while (0)
#else
#define SCR_TAP(x, y, w, h, px, stride, fill) ((void)0)
#endif
static uint8_t lcd_busy;           /* a lcd_data DMA may still run; CS is low */

static void lcd_spin(uint32_t n)
{
    for (volatile uint32_t i = 0; i < n; i++)
        ;
}

static void lcd_sync(void)          /* finish the running transfer (before touching its buffer) */
{
    if (!lcd_busy)
        return;
    fm1_lcd_wait();
    fm1_lcd_deselect();
    lcd_busy = 0;
}

static void lcd_cmd(uint8_t c)
{
    lcd_sync();
    fm1_lcd_send_cmd(c);
    fm1_lcd_wait();
    fm1_lcd_deselect();
}

static void lcd_data(const void *p, uint32_t n)
{
    if (!n)
        return;                    /* SPI_CNT = 0 never completes */
    lcd_sync();
    fm1_lcd_send_data(p, n);
    lcd_busy = 1;                  /* completed by lcd_sync */
}

static void lcd_window(uint32_t x0, uint32_t y0, uint32_t x1, uint32_t y1)
{
    lcd_sync();
    lcd_small[0] = (uint8_t)(x0 >> 8);
    lcd_small[1] = (uint8_t)x0;
    lcd_small[2] = (uint8_t)(x1 >> 8);
    lcd_small[3] = (uint8_t)x1;
    lcd_cmd(0x2A);
    lcd_data(lcd_small, 4);
    lcd_small[4] = (uint8_t)(y0 >> 8);       /* own bytes: the x transfer may still read [0..3] */
    lcd_small[5] = (uint8_t)y0;
    lcd_small[6] = (uint8_t)(y1 >> 8);
    lcd_small[7] = (uint8_t)y1;
    lcd_cmd(0x2B);
    lcd_data(lcd_small + 4, 4);
    lcd_cmd(0x2C);
}

static uint16_t lcd_fillbuf[240];

static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, n, k, sw = (uint16_t)((c >> 8) | (c << 8));
    if (!w || !h || x >= 240u || y >= 240u)
        return;
    if (x + w > 240u)
        w = 240u - x;
    if (y + h > 240u)
        h = 240u - y;
    SCR_TAP(x, y, w, h, 0, 0, c);
    lcd_sync();
    k = 240u / w * w;                       /* whole rows per transfer (narrow fills: one DMA) */
    for (i = 0; i < k; i++)
        lcd_fillbuf[i] = (uint16_t)sw;
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    for (n = w * h; n; n -= k) {
        if (k > n)
            k = n;
        lcd_data(lcd_fillbuf, k * 2u);
    }
}

static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px)
{
    if (!w || !h)
        return;
    SCR_TAP(x, y, w, h, px, w, 0);
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    lcd_data(px, w * h * 2u);
}
/* (0.9.0) a w x h part of a wider buffer (stride px a row): one window, a transfer a row (as lcd_fill: the panel keeps
 * writing on through CS going high between them); gfx.c cv_blit_cells */
#define LCD_HAS_STRIDE 1
static void lcd_blit_stride(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *px, uint32_t stride)
{
    uint32_t r;
    if (!w || !h)
        return;
    SCR_TAP(x, y, w, h, px, stride, 0);
    lcd_window(x, y, x + w - 1u, y + h - 1u);
    for (r = 0; r < h; r++)
        lcd_data(px + r * stride, w * 2u);
}

/* ST7789V init: reset, then 16-bit colour, scan direction and IPS inversion;
 * voltage and gamma stay at the defaults. Format: cmd, n, n data bytes;
 * cmd 0x00 = wait (data byte: ~ms). */
static const uint8_t LCD_SEQ[] = {
    0x01, 0,                     /* SWRESET */
    0x00, 1, 150,
    0x11, 0,                     /* SLPOUT */
    0x00, 1, 120,
    0x3A, 1, 0x55,               /* COLMOD: RGB565 */
    0x36, 1, 0x00,               /* MADCTL: top-left origin, RGB order */
    0x21, 0,                     /* INVON: IPS panel */
    0x13, 0,                     /* NORON */
};

/* (V10.8.63, the owner; a user's report: the units sold from late 2026, flash 0xB36014, carry another 1.3" 240 x 240 IPS
 * panel, ST7789-compatible, whose colours look washed out with A) MENU > DISPLAY > PANEL B: after A's sequence (above,
 * unchanged), the ST7789V datasheet's power, VCOM and gamma registers set instead of left at their reset values, and the
 * inversion the other way (INVOFF: A's INVON undone). MADCTL (the colour order) as A's. UNTESTED on that panel: a
 * hypothesis from the report; OCT- + SAVE held at power-on starts with A (lcd_panel_boot) */
static const uint8_t LCD_SEQ_B[] = {
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,     /* PORCTRL: back / front porch 12, porch control off, idle / partial 3 + 3 */
    0xB7, 1, 0x35,                             /* GCTRL: VGH 13.26 V, VGL -10.43 V */
    0xBB, 1, 0x19,                             /* VCOMS: 0.725 V (the datasheet's 0x19 .. 0x20 range, its low end) */
    0xC0, 1, 0x2C,                             /* LCMCTRL: XMY XBGR XMX XMH as set by MADCTL, XMV */
    0xC2, 2, 0x01, 0xFF,                       /* VDVVRHEN: VDV and VRH from the commands */
    0xC3, 1, 0x12,                             /* VRHS: VAP 4.45 V */
    0xC4, 1, 0x20,                             /* VDVS: 0 V */
    0xC6, 1, 0x0F,                             /* FRCTRL2: 60 Hz, no column inversion */
    0xD0, 2, 0xA4, 0xA1,                       /* PWCTRL1: AVDD 6.8 V, AVCL -4.8 V, VDS 2.3 V */
    0xE0, 14, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F, 0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23,   /* PVGAMCTRL */
    0xE1, 14, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F, 0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23,   /* NVGAMCTRL */
    0x20, 0,                                   /* INVOFF (A: INVON) */
};
static void lcd_seq(const uint8_t *q, uint32_t n)
{
    uint32_t r, x;
    for (r = 0; r < n; r += 2u + q[r + 1u]) {
        if (q[r] == 0x00u) {                             /* pseudo command: wait */
            lcd_spin(q[r + 2u] * 25000u);                /* ~1 ms per unit (lcd_spin(3000000) ~ 120 ms) */
            continue;
        }
        lcd_cmd(q[r]);
        for (x = 0; x < q[r + 1u]; x++)
            lcd_small[x] = q[r + 2u + x];
        if (q[r + 1u])
            lcd_data(lcd_small, q[r + 1u]);
    }
}
static void lcd_init(void)
{
    fm1_lcd_hw_init();
    lcd_spin(2000000u);
    lcd_seq(LCD_SEQ, sizeof LCD_SEQ);
    fm1_lcd_baud(LCD_BAUD);
    lcd_fill(0, 0, 240, 240, 0);
    lcd_cmd(0x29);
}

/* (V10.8.63) MENU > DISPLAY > PANEL (menu_items.c lcd_panel_is_b: AUTO, A, B) */
static uint32_t fl_jedec_id;                   /* (project.c persist_boot) the flash's JEDEC id at boot (0: none read) */
static uint8_t lcd_b_on;                       /* (B's registers sent since the last A) */
/* the panel as the setting wants it: B after A's set-up, or A again (its reset and sequence: B's registers back to their
 * defaults); boot: A already sent (lcd_init), only B's added */
static void lcd_panel_apply(uint32_t b, uint32_t boot)
{
    if (!boot && lcd_b_on && !b) {
        lcd_init();
        lcd_b_on = 0;
    }
    if (b && !lcd_b_on) {
        lcd_seq(LCD_SEQ_B, sizeof LCD_SEQ_B);
        lcd_b_on = 1;
    }
}
/* MENU > SCREEN OFF (ui.c scr_frame): the panel dark while the sound goes on. s 0: DISPOFF, SLPIN; 1: SLPOUT (the
 * panel wants 120 ms after it, and 120 ms after SLPIN before it: ui.c waits, a frame at a time, never here); 2: DISPON
 * (after the frame that redrew it all). The panel keeps its RAM asleep, but the UI redraws everything before it shows
 * (ui.force). The backlight stays on: PA2 is also the key matrix 595's /OE, so with it
 * high no button, key or knob reads and nothing could wake the screen (#157 follow-up, found on 1.1.5) */
static void lcd_power(uint32_t s)
{
    if (s == 1u)
        lcd_cmd(0x11);                             /* SLPOUT */
    else if (s == 2u)
        lcd_cmd(0x29);                             /* DISPON */
    else {
        lcd_cmd(0x28);                             /* DISPOFF */
        lcd_cmd(0x10);                             /* SLPIN */
    }
}
/* the crash screen, UPDATE, UBOOT: the panel on at once if SCREEN OFF had it asleep (blocking, ~240 ms; the IRQs
 * left as they are: the crash handler calls it) */
static void lcd_wake_now(void)
{
    lcd_spin(3000000u);                            /* (120 ms after a SLPIN) */
    lcd_cmd(0x11);
    lcd_spin(3000000u);
    lcd_cmd(0x29);
    fm1_lcd_backlight(1);
}
