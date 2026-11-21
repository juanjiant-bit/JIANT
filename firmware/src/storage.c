/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Persistent storage on the SPI NOR.
 *
 * Every object has an A/B sector pair. A save goes to the copy that is not
 * the current one: erase the sector, program the payload pages (from offset
 * 256), then the 32-byte header at offset 0 LAST. The header is the commit
 * record (magic, seq, length, payload CRC, header CRC); on load the valid
 * copy with the newest seq wins (including wrap), so a write torn at any point leaves the
 * previous copy in charge.
 *
 * Flash access goes through three hooks (also used by the host test):
 *   st_read(off, dst, n)   st_erase(off)   st_prog(off, src, n)
 */
#define ST_MAGIC 0x554C4546u                   /* "FELU" */
#define ST_SECTOR 4096u
#define ST_PAYLOAD_OFF 256u
#define ST_PAYLOAD_MAX (ST_SECTOR - ST_PAYLOAD_OFF)

/* flash map (FL_DATA 0x97000..0xDFFFF, FL_GLOB 0xFC000.., FL_AUTO 0xE5000..0xE6FFF, FL_VBANK 0xE7000): settings 0xFC000, projects
 * 0x97000..0x9EFFF, 0xA0000..0xDBFFF free (Felucca's user sample slots: JIANT's songs, docs/TONIC-SONG-PLAN.md 4d),
 * user preset banks 0xDC000..0xDFFFF (upreset.c),
 * the user presets' FM6 patches (up_fm6.c, since 1.0.3): copy A 0x9F000, copy B 0xFE000; the autosave (project.c,
 * 1.2): A 0xE5000, B 0xE6000, above the OTA staging (0xE0000..0xE4FFF), in the margin the stock firmware's own update
 * stages into (only ever while the stock firmware runs: a later Felucca finds no valid copy and starts as without one).
 * Those two sectors held the FM6 patch bank of 1.0..1.0.2 (OBJ_FM6BANK, retired): both objects use the same pair,
 * told apart by the commit record's type. The bank is only read, once, to move its patches into the user presets
 * (up_fm6.c upf_boot); the first write of the new object goes to the sector that does not hold the bank's newest
 * copy (st_save_to), so a power cut never loses both.
 * The FM6 voice bank (JIANT 0.6.11, from Felucca 1.4.1, fm6_vbank.c): 0xE7000, the sector above the autosave, with a
 * commit record of the same kind (type OBJ_FM6VB, not an st_load object); a new bank is first staged in the
 * autosave's older copy (fm6_vbank.c). */
enum { OBJ_SETTINGS, OBJ_PROJECT0, OBJ_UPRESET0 = OBJ_PROJECT0 + 4, OBJ_FM6BANK = OBJ_UPRESET0 + 2, OBJ_UPFM6,
       OBJ_AUTOSAVE, OBJ_SONGIDX, OBJ_SONG1, OBJ_COUNT = OBJ_SONG1 + 7 * 4 };
/* JIANT's songs (song_chain.c): song 1's four sections are the four project slots (OBJ_PROJECT0..3, 0x97000..), songs
 * 2..8 four objects each (OBJ_SONG1 + (song - 1) * 4 + section) at 0xA0000..0xD7FFF, the song index (the songs' rows,
 * the current song) at 0xD8000 / 0xD9000; 0xDA000..0xDBFFF free */
#define OBJ_FM6VB 64u                          /* the voice bank's commit records (fm6_vbank.c; >= OBJ_COUNT, 39 with
                                                * the songs: st_load never takes one, no object takes its sector.
                                                * Felucca's 32 is a song object here) */
_Static_assert(OBJ_FM6VB >= OBJ_COUNT, "the voice bank's record type is no object's");

typedef struct {
    uint32_t magic;
    uint16_t type, slot;
    uint32_t seq, len, crc, rsv[2];
    uint32_t hcrc;
} st_hdr_t;
_Static_assert(sizeof(st_hdr_t) == 32u, "storage commit record layout");

static int st_read(uint32_t off, void *dst, uint32_t n);
static int st_erase(uint32_t off);
static int st_prog(uint32_t off, const void *src, uint32_t n);

static uint32_t st_crc32_add(uint32_t c, const void *p, uint32_t n)   /* zlib CRC-32, 4 bits per step; running:
                                                                         * from 0xFFFFFFFF, the CRC is ~c */
{
    static const uint32_t T[16] = {
        0x00000000u, 0x1DB71064u, 0x3B6E20C8u, 0x26D930ACu, 0x76DC4190u, 0x6B6B51F4u, 0x4DB26158u, 0x5005713Cu,
        0xEDB88320u, 0xF00F9344u, 0xD6D6A3E8u, 0xCB61B38Cu, 0x9B64C2B0u, 0x86D3D2D4u, 0xA00AE278u, 0xBDBDF21Cu};
    const uint8_t *b = p;
    while (n--) {
        c ^= *b++;
        c = (c >> 4) ^ T[c & 15u];
        c = (c >> 4) ^ T[c & 15u];
    }
    return c;
}
static uint32_t st_crc32(const void *p, uint32_t n) { return ~st_crc32_add(0xFFFFFFFFu, p, n); }

#if BOARD_SMK37
/* (SMK-37, hal/fm1_flash_map.h) the stock region table keeps the head, BTIF / USRTRIM 0xC0000..0xC1FFF (the keys'
 * factory calibration), USR 0xF4000..0xFDFFF and key_mac, and its app area runs to 0x9D000; JIANT's objects are the
 * FM-1's, only their places differ, in three windows (after Melodee's store; 78 of its 79 sectors):
 *   store A, FL_DATA 0x9D000..0xBAFFF (the stock VM): 0x9D000 the settings pair; 0x9F000..0xA6FFF the four projects
 *     (song 1); 0xA7000 / 0xA8000 the user presets' FM6 patches (OBJ_UPFM6); 0xA9000 / 0xAA000 the autosave;
 *     0xAB000 the FM6 voice bank (fm6_vbank.c FVB_HOME); 0xAC000 / 0xAD000 the song index; 0xAE000 (A) + 0xBA000 (B)
 *     song 2's section 4; 0xAF000 the store owner tag (st_owner_check); 0xB0000..0xB3FFF the user preset banks;
 *     0xB4000..0xB9FFF song 2's sections 1..3
 *   store B, FL_UFL 0xC2000..0xE9FFF (USRFLASH below its end marker): songs 3..7 (20 pairs)
 *   0xEA000 the stock end marker's sector: never data (FL_STOCKTAG, only erased whole as the release below)
 *   store C, FL_GAP 0xEB000..0xF3FFF (unlisted, below USR): song 8 (4 pairs); 0xF3000 spare
 * tests/smk_flash_test.c checks every object's pair against the windows, the tag and each other. */
#define ST_SMK_TAG 0xAF000u
#define ST_SMK_VBANK 0xAB000u                      /* (fm6_vbank.c FVB_HOME) */
static uint32_t st_sector(uint32_t obj, uint32_t copy)  /* flash offset of copy A (0) / B (1) */
{
    if (obj == OBJ_SETTINGS)
        return 0x9D000u + copy * ST_SECTOR;
    if (obj == OBJ_FM6BANK || obj == OBJ_UPFM6)          /* (the same pair, as the FM-1's) */
        return 0xA7000u + copy * ST_SECTOR;
    if (obj == OBJ_AUTOSAVE)
        return 0xA9000u + copy * ST_SECTOR;
    if (obj == OBJ_SONGIDX)
        return 0xAC000u + copy * ST_SECTOR;
    if (obj >= OBJ_SONG1) {
        uint32_t j = obj - OBJ_SONG1;                     /* (song - 2) * 4 + section, 0..27 */
        if (j < 3u)
            return 0xB4000u + j * 2u * ST_SECTOR + copy * ST_SECTOR;
        if (j == 3u)
            return copy ? 0xBA000u : 0xAE000u;
        if (j < 24u)
            return FL_UFL_LO + (j - 4u) * 2u * ST_SECTOR + copy * ST_SECTOR;
        return FL_GAP_LO + (j - 24u) * 2u * ST_SECTOR + copy * ST_SECTOR;
    }
    if (obj >= OBJ_UPRESET0)
        return 0xB0000u + (obj - OBJ_UPRESET0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    return 0x9F000u + (obj - OBJ_PROJECT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
}
_Static_assert(OBJ_COUNT - OBJ_SONG1 == 28u && OBJ_UPRESET0 - OBJ_PROJECT0 == 4u && OBJ_FM6BANK - OBJ_UPRESET0 == 2u,
               "SMK store: st_sector's table is laid out for 4 projects, 2 preset banks and 28 song objects");
_Static_assert(FL_UFL_LO + 20u * 2u * 0x1000u == FL_UFL_HI && FL_GAP_LO + 4u * 2u * 0x1000u < FL_GAP_HI &&
               ST_SMK_TAG >= FL_DATA_LO && ST_SMK_TAG < FL_DATA_HI, "SMK store: the windows and the table disagree");

/* The owner of the store (after Melodee's storage.c st_owner_check). Several FM-1 firmwares are ported to the SMK-37
 * with the same store windows, each with its own formats: a firmware must never parse another's data. The sector
 * 0xAF000 (the same in every port) holds "SMK37DATA" + the firmware's family (JIANT). Ours: load as ever. Erased or
 * another family's: clear JIANT's whole store (A, B unless the stock mapping is intact there, and C) and take it.
 * BTIF, USRTRIM, USR, key_mac and the head lie outside the three windows: never touched. */
#define ST_DATA_FAMILY "JIANT"
#ifndef ST_IDLE
#define ST_IDLE() ((void)0)                        /* (the firmware: the watchdog, storage_hw.c) */
#endif
/* The stock firmware's mapping in USRFLASH (global settings at 0xC2000, magic A3 3A; 8 presets from 0xC3000 every
 * 0x5000, the rest erased; the end marker "usrdata\0" at 0xEAFF8, FL_STOCKTAG): the stock firmware trusts the area
 * while the marker is there and never rebuilds it, so after a return to stock JIANT's store B would be read as its
 * key mapping. So an intact stock mapping is kept (its sectors read as empty objects: no JIANT commit record), and
 * the marker is released (its sector erased: the stock firmware rebuilds its factory mapping at its next start)
 * before the first erase or program in USRFLASH (storage_hw.c) and at boot when it stands over anything else. */
static int st_stock_tagged(void)
{
    static const uint8_t TAG[8] = {'u', 's', 'r', 'd', 'a', 't', 'a', 0};
    uint8_t t[8];
    uint32_t i;
    if (st_read(FL_STOCKTAG_OFF, t, 8))
        return 0;
    for (i = 0; i < 8u; i++)
        if (t[i] != TAG[i])
            return 0;
    return 1;
}
static int st_stock_intact(void)                /* the marker over the stock layout */
{
    uint8_t b[8];
    uint32_t n, k, i, ff;
    if (!st_stock_tagged() || st_read(FL_UFL_LO, b, 2) || b[0] != 0xA3u || b[1] != 0x3Au)
        return 0;
    for (n = 0; n < 8u; n++)                    /* each preset: its head written, its last two sectors erased */
        for (k = 0; k < 3u; k++) {
            if (st_read(FL_UFL_LO + 0x1000u + n * 0x5000u + (k ? 0x2000u + k * 0x1000u : 0u), b, 8))
                return 0;
            for (ff = 1, i = 0; i < 8u; i++)
                ff &= b[i] == 0xFFu;
            if (ff != (k != 0u))
                return 0;
        }
    return 1;
}
#ifndef ST_ERASE_MARKER
#define ST_ERASE_MARKER() st_erase(FL_STOCKTAG_LO)   /* (host tests; the firmware: storage_hw.c, the raw erase) */
#endif
static void st_stock_release(void)              /* erase the marker's sector, once: later calls find no marker */
{
    if (st_stock_tagged())
        (void)ST_ERASE_MARKER();
}
static int st_in_usrflash(uint32_t off) { return off >= FL_UFL_LO && off < FL_STOCKTAG_LO; }
static uint8_t st_foreign __attribute__((unused));                      /* the store could not be cleared: RAM-only this boot (persist_boot) */
static int st_owner_check(void)                 /* 0 ours; 1 another firmware's (or untagged) store was cleared; -1 an
                                                 * erase failed: not tagged as ours, so the next boot clears it again */
{
    static const char FAM[] = ST_DATA_FAMILY;
    uint8_t tag[16], want[16];
    uint32_t i, ours = 1, s, keep, bad = 0;
    for (i = 0; i < 16u; i++)
        want[i] = i < 9u ? (uint8_t)"SMK37DATA"[i] : i - 9u < sizeof FAM - 1u ? (uint8_t)FAM[i - 9u] : 0u;
    if (!st_stock_intact())
        st_stock_release();                     /* the marker over anything but the stock mapping: let it rebuild */
    if (st_read(ST_SMK_TAG, tag, 16))
        return -1;
    for (i = 0; i < 16u; i++)
        ours &= tag[i] == want[i];
    if (ours)
        return 0;
    for (s = FL_DATA_LO; s < FL_DATA_HI; s += ST_SECTOR)
        if (s != ST_SMK_TAG) {
            bad |= st_erase(s) != 0;
            ST_IDLE();
        }
    keep = st_stock_intact();                   /* the stock mapping: kept until JIANT writes there */
    for (s = FL_UFL_LO; !keep && s < FL_UFL_HI; s += ST_SECTOR) {
        bad |= st_erase(s) != 0;
        ST_IDLE();
    }
    for (s = FL_GAP_LO; s < FL_GAP_HI; s += ST_SECTOR) {
        bad |= st_erase(s) != 0;
        ST_IDLE();
    }
    if (bad || st_erase(ST_SMK_TAG) || st_prog(ST_SMK_TAG, want, 16))
        return -1;                              /* the tag only over a store that is all ours */
    return 1;
}
#else
static uint32_t st_sector(uint32_t obj, uint32_t copy)  /* flash offset of copy A (0) / B (1) */
{
    if (obj == OBJ_SETTINGS)
        return 0xFC000u + copy * ST_SECTOR;
    if (obj == OBJ_FM6BANK || obj == OBJ_UPFM6)          /* (the same pair: see the flash map) */
        return copy ? 0xFE000u : 0x9F000u;
    if (obj == OBJ_AUTOSAVE)
        return 0xE5000u + copy * ST_SECTOR;
    if (obj == OBJ_SONGIDX)
        return 0xD8000u + copy * ST_SECTOR;
    if (obj >= OBJ_SONG1)
        return 0xA0000u + (obj - OBJ_SONG1) * 2u * ST_SECTOR + copy * ST_SECTOR;
    if (obj >= OBJ_UPRESET0)
        return 0xDC000u + (obj - OBJ_UPRESET0) * 2u * ST_SECTOR + copy * ST_SECTOR;
    return 0x97000u + (obj - OBJ_PROJECT0) * 2u * ST_SECTOR + copy * ST_SECTOR;
}
#endif /* BOARD_SMK37 */

static uint8_t st_buf[ST_PAYLOAD_MAX] __attribute__((aligned(4), section(".lowbuf")));   /* (0.9.0: app.ld LOWBUF, was
                                                   * .bss: main loop only, zeroed at boot as .bss) */

static int st_head(uint32_t obj, uint32_t copy, st_hdr_t *h)   /* commit record valid: 0 */
{
    if (obj >= OBJ_COUNT || copy > 1u)
        return -1;
    if (st_read(st_sector(obj, copy), h, sizeof *h))
        return -1;
    if (h->magic != ST_MAGIC || h->type != obj || h->slot != copy || h->len > ST_PAYLOAD_MAX ||
        h->hcrc != st_crc32(h, sizeof *h - 4u))
        return -1;
    return 0;
}

static int st_body(uint32_t obj, uint32_t copy, const st_hdr_t *h)   /* payload -> st_buf, CRC ok: 0 */
{
    if (st_read(st_sector(obj, copy) + ST_PAYLOAD_OFF, st_buf, h->len) || st_crc32(st_buf, h->len) != h->crc)
        return -1;
    return 0;
}

/* the current copy: the newest valid sequence (A on a tie), -1 when
 * neither is valid. Headers first, so only the winner's payload is read (it
 * is left in st_buf); *h gets its header. */
static int st_current(uint32_t obj, st_hdr_t *h)
{
    st_hdr_t a, b;
    int va = st_head(obj, 0, &a) == 0, vb = st_head(obj, 1, &b) == 0;
    if (vb && (!va || (b.seq != a.seq && b.seq - a.seq < 0x80000000u))) {
        if (st_body(obj, 1, &b) == 0) {
            *h = b;
            return 1;
        }
        vb = 0;
    }
    if (va && st_body(obj, 0, &a) == 0) {
        *h = a;
        return 0;
    }
    if (vb && st_body(obj, 1, &b) == 0) {
        *h = b;
        return 1;
    }
    return -1;
}

/* load the whole object into dst; returns its length, or -1 if it does not fit */
static int st_load(uint32_t obj, void *dst, uint32_t max)
{
    uint32_t i;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || st_current(obj, &h) < 0 || h.len > max)
        return -1;
    for (i = 0; i < h.len; i++)
        ((uint8_t *)dst)[i] = st_buf[i];
    return (int)h.len;
}

/* save into copy `to` (0 A, 1 B; -1: the one that is not the current copy, as st_save) */
static int st_save_to(uint32_t obj, const void *src, uint32_t len, int to)
{
    uint32_t seq, base, off;
    int cur, rc;
    st_hdr_t h;
    if (obj >= OBJ_COUNT || len > ST_PAYLOAD_MAX || to > 1)
        return -1;
    cur = st_current(obj, &h);
    seq = cur < 0 ? 0u : h.seq;
    if (to < 0)
        to = cur == 0 ? 1 : 0;                        /* write the other copy */
    base = st_sector(obj, (uint32_t)to);
    for (off = 0; off < len; off++)
        st_buf[off] = ((const uint8_t *)src)[off];    /* the driver wants RAM sources */
    if ((rc = st_erase(base)) != 0)
        return rc;
    for (off = 0; off < len; off += 256u) {
        uint32_t n = len - off > 256u ? 256u : len - off;
        if ((rc = st_prog(base + ST_PAYLOAD_OFF + off, st_buf + off, n)) != 0)
            return rc;
    }
    h.magic = ST_MAGIC;
    h.type = (uint16_t)obj;
    h.slot = (uint16_t)to;
    h.seq = seq + 1u;
    h.len = len;
    h.crc = st_crc32(st_buf, len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    if ((rc = st_prog(base, &h, sizeof h)) != 0)       /* the commit record, last */
        return rc;
    {   /* read back: a write-protected or failing part must not report SAVED */
        st_hdr_t chk;
        uint32_t c = (uint32_t)to;
        if (st_head(obj, c, &chk) || memcmp(&chk, &h, sizeof h) || st_body(obj, c, &chk))
            return -7;
    }
    return 0;
}

static int st_save(uint32_t obj, const void *src, uint32_t len) { return st_save_to(obj, src, len, -1); }
