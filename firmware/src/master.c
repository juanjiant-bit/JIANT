/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* MASTER (#137): the pot read before the audio starts, so the first block plays at its level (it was 2048, about
 * 70 % of the knob, until the main loop's first read after the splash, then a 1/8-per-frame glide from mid-scale:
 * the keys, live from the audio ISR at once, sounded loud under the splash with the knob turned down).
 * master_knob: the pot x16, smoothed per UI frame (master_poll) */
static int32_t master_knob = 512 * 16;
/* (V10.8.19, the power-on pop) mf_on (main.c, power-on): the output's enable (PC6, fm1_audio_unmute) once the DMA has
 * played ~100 ms of zeros (the halves the audio ISR counts; polled here, by the splash's loop: the boot waits for
 * nothing), the level 0 until then and from it (mf_t0, its time + 1) 0 .. the knob's over MF_MS; the emulator never
 * sets mf_on (its level as before) */
#define MF_HALVES ((FS / 10u + HALF_FRAMES - 1u) / HALF_FRAMES)
#define MF_MS 128u                                      /* (the ramp: f = ms x 2, 0 .. 256) */
static uint8_t mf_on;
static uint32_t mf_t0;
static __attribute__((noinline)) uint32_t master_lvl(uint32_t k10)
{
    uint32_t q = master_of_pot(k10), f;
    if (!mf_on)
        return q;
    if (!mf_t0 && audio_halves >= MF_HALVES) {         /* (the zeros played: the output on) */
        fm1_audio_unmute();
        mf_t0 = fm1_ms + 1u;
    }
    f = mf_t0 ? (fm1_ms + 1u - mf_t0) * (256u / MF_MS) : 0u;
    if (f >= 256u) { mf_on = 0; return q; }
    return q * f >> 8;
}
static void master_poll(void)
{
    int32_t a = fm1_adc_read(FM1_ADC_MASTER);
    if (a >= 0) {
        master_knob += (a * 16 - master_knob) / 8;
        song.master_q12 = master_lvl((uint32_t)(master_knob / 16));
    }
}
/* power-on (main.c), before audio_init: the knob as it stands, no glide */
static void master_boot(void)
{
    uint32_t i;
    (void)fm1_adc_read(FM1_ADC_MASTER);                 /* (the first conversion after the pin setup: dropped) */
    for (i = 0; i < 3u; i++) {
        int32_t a = fm1_adc_read(FM1_ADC_MASTER);
        if (a >= 0) {
            master_knob = a * 16;
            song.master_q12 = master_lvl((uint32_t)a);
            return;
        }
    }                                                   /* (no reading: felucca_init's level, the loop glides on) */
}
