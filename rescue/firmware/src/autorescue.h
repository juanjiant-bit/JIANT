#pragma once

// Stand-alone rescue: with a valid payload (payload.h) in the RP2040's flash,
// the board restores the FM-1 on its own, with no computer program:
//
//   1. USB_KEY at power-on (recovery.c), or the V15 soft key if the FM-1 is
//      already running stock V15, puts the FM-1 in its mask-ROM UBOOT.
//   2. The embedded wl82loader is uploaded, the chip key (0x980F) and flash
//      ID (0x856014 / 0xB36014) are checked.
//   3. Every 4 KiB sector of 0x4000..0x92FFF that differs from the payload is
//      erased, written and read back (jl_flash_write_sector). Nothing below
//      0x4000 or from 0x93000 up is ever written (settings, sounds, songs and
//      the boot head stay as they are).
//   4. A final read of the whole region is compared with the payload.
//
// LED (one colour or pattern per state; core 1 only):
//   yellow, blinking      waiting for the FM-1: switch it on
//   yellow, steady        the FM-1 answered, entering boot mode
//   blue, flickering      reading / writing
//   green, steady         done: switch the FM-1 off and on again
//   red, fast blinking    error (see the console log); nothing below 0x4000
//                         was touched; repeat the procedure
//   purple, steady        the FM-1 runs a firmware that has no soft key:
//                         switch it off, re-plug the board, switch it on
//   red/blue alternating  the payload is damaged: rebuild the .uf2

#include <stdbool.h>

typedef enum {
    AR_OFF,         // no payload: plain Transporter
    AR_WAIT,        // waiting for UBOOT
    AR_RUN,
    AR_DONE,
    AR_ERROR,
    AR_BAD_PAYLOAD,
} ar_state_t;

// core 1, before recovery: checks the payload.
void ar_init(void);

// core 1, from the host loop.
void ar_task(void);

bool ar_enabled(void);
ar_state_t ar_state(void);
const char *ar_describe(void);     // core 1

// One status line (no newline) for the data port's `rescue` request; safe on
// core 0 while core 1 works:
//   state=<off|wait|run|done|error|bad-payload>[ foreign=1] phase=<idle|check|
//   write|verify> sector=<n>/143 written=<n> same=<n> err=<none|link|chip|
//   write|verify> addr=<hex>
void ar_status_line(char *s, unsigned n);
