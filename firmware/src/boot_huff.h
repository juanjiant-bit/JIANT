/* SPDX-License-Identifier: GPL-3.0-only */
/* Boot only: unpack the outer Huffman stream into the not-yet-initialised pool.
 * The existing LZ4 decoder then restores each RAM section byte for byte. No
 * allocation, tables in RAM, or reads/writes outside the supplied bounds. */
#pragma once
#include <stdint.h>

static uint32_t boot_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static int boot_huff(const uint8_t *s, const uint8_t *end, uint8_t *out, uint32_t capacity)
{
    const uint8_t *tree, *bits;
    uint32_t count, size, packed, i, bit = 0;
    if (end < s || (uint32_t)(end - s) < 12u || s[0] != 'H' || s[1] != 'F' || s[2] != 1u)
        return 0;
    count = s[3]; size = boot_u32(s + 4); packed = boot_u32(s + 8);
    if (size > capacity || size > 65536u || packed > 65536u || (size && !count) ||
        12u + 4u * count + packed > (uint32_t)(end - s))
        return 0;
    tree = s + 12; bits = tree + 4u * count;
    for (i = 0; i < size; i++) {
        uint32_t node = 0, depth = 0;
        while (node < 256u) {
            uint32_t edge;
            if (node >= count || bit >= packed * 8u || depth++ >= 32u)
                return 0;
            edge = 4u * node + 2u * ((bits[bit >> 3] >> (7u - (bit & 7u))) & 1u);
            node = (uint32_t)tree[edge] | (uint32_t)tree[edge + 1u] << 8;
            bit++;
        }
        if (node > 511u) return 0;
        out[i] = (uint8_t)(node - 256u);
    }
    return 1;
}
