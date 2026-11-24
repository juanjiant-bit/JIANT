#pragma once

// FM-1 Rescue payload: the firmware to restore and JieLi's loader, stored in
// the RP2040's own flash at PL_FLASH_OFFSET by the rescue builder
// (rescue/fm1-rescue.html), in the same UF2 as this firmware.
//
// Layout (offsets from the start of the payload):
//   0x0000  pl_header_t (256 bytes, little-endian)
//   0x1000  wl82loader.bin (24064 bytes)
//   0x8000  the FM-1 flash region 0x4000..0x92FFF (0x8F000 bytes)
//
// With no valid payload the board is a plain FM-1 Transporter (fm1t.py and
// the web app work as before). With one, it restores the FM-1 by itself.

#include <stdbool.h>
#include <stdint.h>

#define PL_FLASH_OFFSET 0x100000u          // 1 MiB into the RP2040 flash
#define PL_MAGIC "FM1-RESCUE-PL-1"         // 15 chars + NUL = 16 bytes
#define PL_VERSION 1u
#define PL_LOADER_OFF 0x1000u
#define PL_IMAGE_OFF 0x8000u
#define PL_IMAGE_ADDR 0x4000u
#define PL_IMAGE_LEN 0x8F000u

typedef struct {
    char magic[16];
    uint32_t version;
    uint32_t hdr_size;          // 256
    uint32_t loader_off;
    uint32_t loader_len;
    uint32_t loader_crc32;
    uint32_t image_off;
    uint32_t image_addr;        // FM-1 flash address of image[0]: 0x4000
    uint32_t image_len;         // 0x8F000
    uint32_t image_crc32;
    char label[64];             // e.g. "FM-1_015 official V15", NUL-terminated
    uint8_t image_sha256[32];   // checked by the builder; logged here
    uint8_t reserved[104];
    uint32_t hdr_crc32;         // CRC-32 of bytes 0..251
} pl_header_t;

_Static_assert(sizeof(pl_header_t) == 256, "pl_header_t must be 256 bytes");

typedef enum {
    PL_NONE,        // no magic: plain Transporter
    PL_VALID,
    PL_BAD,         // magic present but something does not check out: never write
} pl_status_t;

// Checks the payload (CRCs over all of it, ~50 ms). Call once.
pl_status_t payload_check(void);

const pl_header_t *payload_header(void);
const uint8_t *payload_loader(void);
const uint8_t *payload_image(void);

uint32_t pl_crc32(const uint8_t *data, uint32_t len, uint32_t crc);
