// Host test of the stand-alone rescue (payload.c + autorescue.c, unmodified)
// against a mock FM-1: a 1 MiB flash behind mock jl_* / fm1_host_* calls.
//
//   test/run.sh RESCUE.uf2 FM-1_V15_REGION.bin     (rescue/build.sh runs it)
//
// Checks: the payload from the real .uf2 is accepted, a damaged one is
// refused, only differing sectors of 0x4000..0x92FFF are written, the head
// and the data area are never touched, a wrong chip is refused, a write that
// keeps failing ends in an error, an already-right unit is not written.

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "autorescue.h"
#include "jieli_uboot.h"
#include "log.h"
#include "payload.h"
#include "pio_host.h"
#include "recovery.h"
#include "hardware/watchdog.h"

uint8_t test_rp_flash[2 * 1024 * 1024];
test_watchdog_t test_watchdog;
static uint64_t now_us;
uint64_t time_us_64(void) { return now_us += 50; }

static int quiet = 1;
void dlog(const char *fmt, ...) {
    if (quiet) return;
    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    putchar('\n');
}
void rgb(bool r, bool g, bool b) { (void)r; (void)g; (void)b; }

// ---- mock FM-1 ----
static uint8_t fm1[JL_FLASH_SIZE];
static int sector_writes[JL_FLASH_SIZE / JL_SECTOR_SIZE];
static bool uboot = true, v15 = false, have_loader;
static uint16_t chip_key = JL_CHIP_KEY;
static uint32_t flash_id = JL_FLASH_ID;
static int fail_writes_at = -1;     // sector address whose writes always fail
static int softkeys;

bool fm1_pio_host_mounted(void) { return uboot || v15; }
bool fm1_host_uboot_ready(void) { return uboot; }
bool fm1_host_v15_mounted(void) { return v15; }
bool fm1_host_softkey(void) { softkeys++; v15 = false; uboot = true; return true; }
bool jl_have_loader(void) { return have_loader; }
bool jl_loader_load(uint32_t off, const uint8_t *d, uint32_t len) {
    const pl_header_t *h = payload_header();
    if (off || len != JL_LOADER_SIZE || pl_crc32(d, len, 0) != h->loader_crc32) return false;
    have_loader = true;
    return true;
}
bool jl_info(jl_info_t *i) {
    if (!have_loader || !uboot) return false;
    i->chip_key = chip_key; i->dev_type = 3; i->flash_id = flash_id;
    return true;
}
uint16_t jl_read_chunk(void) { return 4096; }
bool jl_flash_read(uint32_t a, uint16_t n, uint8_t *b) {
    if (!uboot || a + n > JL_FLASH_SIZE) return false;
    memcpy(b, fm1 + a, n);
    return true;
}
bool jl_flash_write_sector(uint32_t a, const uint8_t *d) {   // same guard as jieli_uboot.c
    if (a < JL_WRITE_MIN || a >= JL_WRITE_END || a % JL_SECTOR_SIZE) return false;
    if (!uboot) return false;
    if ((int)a == fail_writes_at) { memset(fm1 + a, 0x00, JL_SECTOR_SIZE); return false; }
    memcpy(fm1 + a, d, JL_SECTOR_SIZE);
    sector_writes[a / JL_SECTOR_SIZE]++;
    return true;
}

// ---- helpers ----
static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("  FAIL: " __VA_ARGS__); printf("\n"); } } while (0)

static uint8_t *region;   // 0x8F000 bytes of V15

static void load_uf2(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); exit(2); }
    memset(test_rp_flash, 0xFF, sizeof(test_rp_flash));
    uint8_t b[512];
    int n = 0;
    while (fread(b, 1, 512, f) == 512) {
        uint32_t addr, size;
        memcpy(&addr, b + 12, 4);
        memcpy(&size, b + 16, 4);
        if (addr < 0x10000000 || addr + size > 0x10000000 + sizeof(test_rp_flash)) { printf("bad block\n"); exit(2); }
        memcpy(test_rp_flash + (addr - 0x10000000), b + 32, size);
        n++;
    }
    fclose(f);
    printf("loaded %d UF2 blocks\n", n);
}

static void reset_unit(uint8_t fill) {
    for (uint32_t a = 0; a < JL_FLASH_SIZE; a++) fm1[a] = (uint8_t)(fill ^ (a * 7) ^ (a >> 9));
    memset(sector_writes, 0, sizeof(sector_writes));
    uboot = true; v15 = false; have_loader = false;
    chip_key = JL_CHIP_KEY; flash_id = JL_FLASH_ID; fail_writes_at = -1; softkeys = 0;
    memset(&test_watchdog, 0, sizeof(test_watchdog));
}

static ar_state_t drive(int steps) {
    ar_init();
    for (int i = 0; i < steps; i++) {
        ar_task();
        now_us += 100000;
        ar_state_t s = ar_state();
        if (s == AR_DONE || s == AR_ERROR || s == AR_BAD_PAYLOAD || s == AR_OFF) break;
    }
    return ar_state();
}

static void check_untouched(const uint8_t *before) {
    CHECK(memcmp(fm1, before, JL_WRITE_MIN) == 0, "boot head changed");
    CHECK(memcmp(fm1 + JL_WRITE_END, before + JL_WRITE_END, JL_FLASH_SIZE - JL_WRITE_END) == 0,
          "data area 0x93000.. changed");
}

int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: host_test RESCUE.uf2 REGION.bin [-v]\n"); return 2; }
    quiet = !(argc > 3 && !strcmp(argv[3], "-v"));
    load_uf2(argv[1]);
    region = malloc(PL_IMAGE_LEN);
    FILE *f = fopen(argv[2], "rb");
    if (!f || fread(region, 1, PL_IMAGE_LEN, f) != PL_IMAGE_LEN) { perror(argv[2]); return 2; }
    fclose(f);
    static uint8_t before[JL_FLASH_SIZE];

    printf("1. bricked unit (random app region) -> restored, head and data untouched\n");
    reset_unit(0x5A);
    memcpy(before, fm1, sizeof(fm1));
    CHECK(drive(100) == AR_DONE, "state %s", ar_describe());
    CHECK(memcmp(fm1 + JL_WRITE_MIN, region, PL_IMAGE_LEN) == 0, "region != V15");
    check_untouched(before);
    int total = 0;
    for (int s = 0; s < (int)(JL_FLASH_SIZE / JL_SECTOR_SIZE); s++) {
        total += sector_writes[s];
        CHECK(sector_writes[s] <= 1, "sector %05X written %d times", s * 4096, sector_writes[s]);
    }
    CHECK(total == 143, "%d sectors written, expected 143", total);
    CHECK(test_watchdog.scratch[2] == 0x444F4E45, "done flag not set");

    printf("2. half-written unit -> only the differing sectors are written\n");
    reset_unit(0x11);
    memcpy(fm1 + JL_WRITE_MIN, region, 40 * JL_SECTOR_SIZE);
    memcpy(before, fm1, sizeof(fm1));
    CHECK(drive(100) == AR_DONE, "state %s", ar_describe());
    total = 0;
    for (int s = 0; s < 4 + 40; s++) total += sector_writes[s];
    CHECK(total == 0, "%d already-right sectors were rewritten", total);
    CHECK(memcmp(fm1 + JL_WRITE_MIN, region, PL_IMAGE_LEN) == 0, "region != V15");
    check_untouched(before);

    printf("3. unit already on V15 -> nothing written\n");
    reset_unit(0x22);
    memcpy(fm1 + JL_WRITE_MIN, region, PL_IMAGE_LEN);
    CHECK(drive(100) == AR_DONE, "state %s", ar_describe());
    total = 0;
    for (int s = 0; s < (int)(JL_FLASH_SIZE / JL_SECTOR_SIZE); s++) total += sector_writes[s];
    CHECK(total == 0, "%d sectors written", total);

    printf("4. running stock V15 -> soft key, then restore\n");
    reset_unit(0x33);
    uboot = false; v15 = true;
    CHECK(drive(200) == AR_DONE, "state %s", ar_describe());
    CHECK(softkeys == 1, "%d soft keys", softkeys);

    printf("5. wrong chip / flash -> error, nothing written\n");
    reset_unit(0x44);
    flash_id = 0xEF4014;
    memcpy(before, fm1, sizeof(fm1));
    CHECK(drive(100) == AR_ERROR, "state %s", ar_describe());
    CHECK(memcmp(fm1, before, sizeof(fm1)) == 0, "flash changed");
    reset_unit(0x44);
    flash_id = 0xB36014;                 // the 2026 part is accepted
    CHECK(drive(100) == AR_DONE, "B36014: state %s", ar_describe());
    reset_unit(0x44);
    chip_key = 0x1234;
    CHECK(drive(100) == AR_ERROR, "bad key: state %s", ar_describe());

    printf("6. a sector that never takes the write -> error\n");
    reset_unit(0x55);
    fail_writes_at = 0x20000;
    CHECK(drive(100) == AR_ERROR, "state %s", ar_describe());
    CHECK(strstr(ar_describe(), "err=write addr=020000") != NULL, "%s", ar_describe());

    printf("7. already done in this power cycle -> stays done, no writes\n");
    reset_unit(0x66);
    test_watchdog.scratch[2] = 0x444F4E45;
    memcpy(before, fm1, sizeof(fm1));
    CHECK(drive(10) == AR_DONE, "state %s", ar_describe());
    CHECK(memcmp(fm1, before, sizeof(fm1)) == 0, "flash changed");

    printf("8. damaged payload (one image byte flipped) -> refused, nothing written\n");
    reset_unit(0x77);
    memcpy(before, fm1, sizeof(fm1));
    test_rp_flash[PL_FLASH_OFFSET + PL_IMAGE_OFF + 12345] ^= 1;
    CHECK(drive(10) == AR_BAD_PAYLOAD, "state %s", ar_describe());
    CHECK(memcmp(fm1, before, sizeof(fm1)) == 0, "flash changed");
    test_rp_flash[PL_FLASH_OFFSET + PL_IMAGE_OFF + 12345] ^= 1;

    printf("9. no payload -> plain Transporter, rescue off\n");
    reset_unit(0x88);
    memset(test_rp_flash + PL_FLASH_OFFSET, 0xFF, 16);
    CHECK(drive(10) == AR_OFF, "state %s", ar_describe());

    printf(failures ? "\n%d FAILURE(S)\n" : "\nall rescue host tests passed\n", failures);
    return failures ? 1 : 0;
}
