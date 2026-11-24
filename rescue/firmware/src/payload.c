#include "payload.h"

#include <string.h>

#include "hardware/flash.h"
#include "pico/stdlib.h"

#include "jieli_uboot.h"
#include "log.h"

#define PL_BASE ((const uint8_t *)(XIP_BASE + PL_FLASH_OFFSET))

static uint32_t crc_table[256];

uint32_t pl_crc32(const uint8_t *data, uint32_t len, uint32_t crc) {
    if (!crc_table[1]) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_table[i] = c;
        }
    }
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++) crc = crc_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

const pl_header_t *payload_header(void) {
    return (const pl_header_t *)PL_BASE;
}

const uint8_t *payload_loader(void) {
    return PL_BASE + payload_header()->loader_off;
}

const uint8_t *payload_image(void) {
    return PL_BASE + payload_header()->image_off;
}

pl_status_t payload_check(void) {
    const pl_header_t *h = payload_header();
    if (memcmp(h->magic, PL_MAGIC, sizeof(h->magic)) != 0) {
        dlog("RESCUE: no payload at +0x%06X - plain Transporter mode", PL_FLASH_OFFSET);
        return PL_NONE;
    }
    if (pl_crc32((const uint8_t *)h, 252, 0) != h->hdr_crc32) {
        dlog("RESCUE: payload header CRC mismatch");
        return PL_BAD;
    }
    if (h->version != PL_VERSION || h->hdr_size != sizeof(pl_header_t) ||
        h->loader_off != PL_LOADER_OFF || h->loader_len != JL_LOADER_SIZE ||
        h->image_off != PL_IMAGE_OFF || h->image_addr != PL_IMAGE_ADDR ||
        h->image_len != PL_IMAGE_LEN || h->label[sizeof(h->label) - 1] != 0) {
        dlog("RESCUE: payload layout not supported (v%lu)", (unsigned long)h->version);
        return PL_BAD;
    }
    if (pl_crc32(payload_loader(), h->loader_len, 0) != h->loader_crc32) {
        dlog("RESCUE: loader CRC mismatch");
        return PL_BAD;
    }
    if (pl_crc32(payload_image(), h->image_len, 0) != h->image_crc32) {
        dlog("RESCUE: firmware image CRC mismatch");
        return PL_BAD;
    }
    char sha[17];
    for (int i = 0; i < 8; i++) {
        static const char hx[] = "0123456789abcdef";
        sha[2 * i] = hx[h->image_sha256[i] >> 4];
        sha[2 * i + 1] = hx[h->image_sha256[i] & 15];
    }
    sha[16] = 0;
    dlog("RESCUE: payload OK: %s, 0x%lX bytes for 0x%05lX, sha256 %s..., crc32 %08lX",
         h->label, (unsigned long)h->image_len, (unsigned long)h->image_addr, sha,
         (unsigned long)h->image_crc32);
    return PL_VALID;
}
