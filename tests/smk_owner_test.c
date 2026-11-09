/* SPDX-License-Identifier: GPL-3.0-only
 * JIANT on the M-Vave SMK-37 Pro (BOARD_SMK37): Israel Peña (pedeplei1281) */
/* SMK-37 Pro store owner tag (storage.c st_owner_check, build with -DBOARD_SMK37=1): a simulated flash with
 * another firmware's store, Felucca's, an untagged one and JIANT's own. Another's: FL_DATA's objects, the voice bank,
 * the song index and the songs there cleared, and in USRFLASH only the sectors holding one of JIANT's own song records
 * (another firmware's data there, a user sample or the stock firmware's, is kept); ours is kept whole. Nothing is ever
 * written outside the store's windows, and nothing but the tag is programmed. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "fm1_cc.h"
#if !BOARD_SMK37
#error "build with -DBOARD_SMK37=1"
#endif
#include "fm1_flash_map.h"

static uint8_t nor[0x100000];
static int bad_write;
static int st_read(uint32_t off, void *dst, uint32_t n) { memcpy(dst, nor + off, n); return 0; }
static int st_erase(uint32_t off)
{
    if ((off & 0xFFFu) || !(FL_IN(off, 0x1000u, FL_DATA_LO, FL_DATA_HI) || FL_IN(off, 0x1000u, FL_SMP_LO, FL_SMP_HI)))
        bad_write++;
    memset(nor + off, 0xFF, 0x1000);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *s = src;
    uint32_t i;
    if (off != 0xAF000u)
        bad_write++;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    return 0;
}
#include "../firmware/src/storage.c"

#define SONG_REC (OBJ_SONG1 + 10u)                  /* a song object in USRFLASH ... */
#define SONG_REC_COPY 1u                            /* ... its copy B holds one of JIANT's records */

static int bad;
static void check(const char *what, int ok) { printf("smk owner: %-82s %s\n", what, ok ? "ok" : "FAIL"); bad += !ok; }

static void fill(void)
{
    static const uint8_t song[64] = {1, 2, 3, 4, 5};
    memset(nor, 0xFF, sizeof nor);
    memset(nor + 0x9D000, 0x11, 0x12000);          /* the layer's objects */
    memset(nor + 0xB0000, 0x22, 0x1000);           /* voice bank */
    memset(nor + 0xB1000, 0x55, 0xA000);           /* song index, songs in FL_DATA */
    memset(nor + 0xC1000, 0x33, 0x1000);           /* USRTRIM: never touched */
    memset(nor + 0xC2000, 0x44, 0x1000);           /* USRFLASH: another firmware's data (a user sample) */
    memset(nor + 0xF4000, 0x66, 0x1000);           /* USR: never touched */
    st_save_to(SONG_REC, song, sizeof song, (int)SONG_REC_COPY);   /* a JIANT song record in USRFLASH */
    bad_write = 0;                                  /* (planting it is no fault of the check) */
}
static int store_erased(void)
{
    uint32_t i;
    for (i = 0x9D000; i < 0xAF000; i++) if (nor[i] != 0xFF) return 0;
    for (i = 0xB0000; i < 0xBB000; i++) if (nor[i] != 0xFF) return 0;
    return 1;
}
static int song_rec_gone(void)
{
    uint32_t i, at = st_sector(SONG_REC, SONG_REC_COPY);
    for (i = 0; i < 0x1000u; i++) if (nor[at + i] != 0xFF) return 0;
    return at >= FL_SMP_LO && at < FL_SMP_HI;
}
static int others_kept(void)
{
    return nor[0xC1000] == 0x33 && nor[0xC1FFF] == 0x33 && nor[0xC2000] == 0x44 && nor[0xC2FFF] == 0x44 &&
           nor[0xF4000] == 0x66;
}

int main(void)
{
    st_hdr_t h;
    fill();
    check("(setup) a JIANT song record in USRFLASH reads back", !st_head(SONG_REC, SONG_REC_COPY, &h));
    memcpy(nor + 0xAF000, "SMK37DATASLOOP\0", 16);
    st_owner_check();
    check("another family's store (SLOOP): cleared, tagged JIANT", store_erased() && st_owner_cleared &&
          !memcmp(nor + 0xAF000, "SMK37DATAJIANT\0", 16));
    check("... JIANT's song record in USRFLASH cleared, another firmware's data there kept", song_rec_gone() &&
          nor[0xC2000] == 0x44);
    check("... calibration (USRTRIM) and USR untouched", others_kept());
    fill();
    memcpy(nor + 0xAF000, "SMK37DATAFELUCCA", 16);  /* Felucca's own SMK port: same layer, other formats */
    st_owner_cleared = 0;
    st_owner_check();
    check("Felucca's store (FELUCCA): cleared, tagged JIANT", store_erased() && st_owner_cleared &&
          !memcmp(nor + 0xAF000, "SMK37DATAJIANT\0", 16) && song_rec_gone() && others_kept());
    fill();
    memcpy(nor + 0xAF000, "SMK37DATAJIANT\0", 16);
    st_owner_cleared = 0;
    st_owner_check();
    check("our own store: kept as it is (objects, voice bank, songs, the song record)",
          nor[0x9D000] == 0x11 && nor[0xB0000] == 0x22 && nor[0xBAFFF] == 0x55 && !st_owner_cleared &&
          !st_head(SONG_REC, SONG_REC_COPY, &h));
    fill();
    st_owner_check();
    check("an untagged store (the stock settings; FELUCCA_DATA_UNTAGGED_OK 0): cleared, tagged", store_erased() &&
          !memcmp(nor + 0xAF000, "SMK37DATAJIANT\0", 16) && song_rec_gone() && others_kept());
    check("nothing erased outside FL_DATA / FL_SMP, nothing programmed but the tag", bad_write == 0);
    printf("%s\n", bad ? "SMK OWNER TEST FAILED" : "smk owner test passed");
    return bad != 0;
}
