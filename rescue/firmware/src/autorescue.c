// FM-1 Rescue: stand-alone restore of the FM-1 firmware region from the
// payload in the RP2040 flash. See autorescue.h.

#include "autorescue.h"

#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"

#include "jieli_uboot.h"
#include "log.h"
#include "payload.h"
#include "pio_host.h"
#include "recovery.h"

#define FLASH_ID_ALT 0xB36014       // 1 MiB part of the 2026 units with the logo on the front
#define WRITE_TRIES 3
#define SOFTKEY_DELAY_MS 1500       // after a V15 mount, let the probe finish first
#define FOREIGN_AFTER_MS 15000      // mounted, neither UBOOT nor V15, for this long
#define DONE_SCRATCH 2              // watchdog scratch: survives the host's re-attach reboots
#define DONE_MAGIC 0x444F4E45       // "DONE"

typedef enum {
    E_NONE,
    E_LINK,         // lost the FM-1 before anything was written
    E_CHIP,         // not an FM-1 (chip key / flash ID)
    E_WRITE,        // a sector could not be written: the region is incomplete
    E_VERIFY,       // the final read does not match
} ar_err_t;

static ar_state_t state = AR_OFF;
static ar_err_t err;
static uint32_t err_addr;
static uint32_t n_written, n_same;
static bool any_written;
static bool foreign;

static uint8_t __attribute__((aligned(4))) sec[JL_SECTOR_SIZE];

bool ar_enabled(void) {
    return state != AR_OFF;
}

ar_state_t ar_state(void) {
    return state;
}

const char *ar_describe(void) {
    static char s[96];
    static const char *const names[] = {"off", "wait", "run", "done", "error", "bad-payload"};
    static const char *const errs[] = {"none", "link", "chip", "write", "verify"};
    snprintf(s, sizeof(s), "state=%s err=%s addr=%06lX written=%lu same=%lu", names[state],
             errs[err], (unsigned long)err_addr, (unsigned long)n_written,
             (unsigned long)n_same);
    return s;
}

void ar_init(void) {
    switch (payload_check()) {
    case PL_NONE:
        state = AR_OFF;
        return;
    case PL_BAD:
        state = AR_BAD_PAYLOAD;
        dlog("RESCUE: the payload is damaged; nothing will be written. Rebuild the .uf2.");
        return;
    case PL_VALID:
        break;
    }
    if (watchdog_hw->scratch[DONE_SCRATCH] == DONE_MAGIC) {
        // The host rebooted the board after the FM-1 left UBOOT: stay done.
        state = AR_DONE;
        dlog("RESCUE: already done in this power cycle");
        return;
    }
    state = AR_WAIT;
    dlog("RESCUE: stand-alone mode. Switch the FM-1 ON now (it must be OFF before).");
}

static void fail(ar_err_t e, uint32_t addr) {
    err = e;
    err_addr = addr;
    state = AR_ERROR;
    rgb(true, false, false);
    static const char *const why[] = {
        "", "lost the FM-1 before writing anything",
        "this is not an FM-1 (chip key / flash ID)",
        "a sector could not be written - the firmware region is INCOMPLETE; repeat the rescue",
        "the final read-back does not match - repeat the rescue",
    };
    dlog("RESCUE: ERROR at %06lX: %s", (unsigned long)addr, why[e]);
    dlog("RESCUE: %s", ar_describe());
}

static bool read_sector(uint32_t addr) {
    uint16_t chunk = jl_read_chunk();
    for (uint32_t off = 0; off < JL_SECTOR_SIZE; off += chunk) {
        uint16_t n = (JL_SECTOR_SIZE - off) < chunk ? (uint16_t)(JL_SECTOR_SIZE - off) : chunk;
        if (!jl_flash_read(addr + off, n, &sec[off])) return false;
    }
    return true;
}

// One pass over the region: write every sector that differs. Returns false
// (after fail()) on an error.
static bool write_pass(const uint8_t *image, uint32_t base, uint32_t len) {
    n_written = n_same = 0;
    for (uint32_t off = 0; off < len; off += JL_SECTOR_SIZE) {
        uint32_t addr = base + off;
        const uint8_t *want = image + off;
        rgb(false, false, (off / JL_SECTOR_SIZE) & 1);
        if (!read_sector(addr)) {
            fail(any_written ? E_WRITE : E_LINK, addr);
            return false;
        }
        if (memcmp(sec, want, JL_SECTOR_SIZE) == 0) {
            n_same++;
            continue;
        }
        bool ok = false;
        for (int t = 0; t < WRITE_TRIES && !ok; t++) {
            any_written = true;
            ok = jl_flash_write_sector(addr, want);
            if (!ok) {
                dlog("RESCUE: sector %06lX attempt %d failed", (unsigned long)addr, t + 1);
                if (!fm1_host_uboot_ready()) break;
            }
        }
        if (!ok) {
            fail(E_WRITE, addr);
            return false;
        }
        n_written++;
        if (n_written % 16 == 1) {
            dlog("RESCUE: wrote %06lX (%lu written, %lu already right)", (unsigned long)addr,
                 (unsigned long)n_written, (unsigned long)n_same);
        }
    }
    return true;
}

static bool verify_pass(const uint8_t *image, uint32_t base, uint32_t len) {
    for (uint32_t off = 0; off < len; off += JL_SECTOR_SIZE) {
        rgb(false, (off / JL_SECTOR_SIZE) & 1, true);
        if (!read_sector(base + off) || memcmp(sec, image + off, JL_SECTOR_SIZE) != 0) {
            dlog("RESCUE: verify mismatch or read error at %06lX", (unsigned long)(base + off));
            return false;
        }
    }
    return true;
}

static void run(void) {
    const pl_header_t *h = payload_header();
    const uint8_t *image = payload_image();
    uint64_t t0 = time_us_64();
    state = AR_RUN;
    rgb(false, false, true);
    dlog("RESCUE: FM-1 in boot mode - restoring %s", h->label);

    if (!jl_have_loader() && !jl_loader_load(0, payload_loader(), JL_LOADER_SIZE)) {
        fail(E_LINK, 0);
        return;
    }
    jl_info_t info;
    if (!jl_info(&info)) {
        fail(E_LINK, 0);
        return;
    }
    dlog("RESCUE: chip key %04X, type %u, flash id %06lX", info.chip_key, info.dev_type,
         (unsigned long)info.flash_id);
    if (info.chip_key != JL_CHIP_KEY ||
        (info.flash_id != JL_FLASH_ID && info.flash_id != FLASH_ID_ALT)) {
        fail(E_CHIP, 0);
        return;
    }

    // The boot head is never written; log what it is, for the record.
    uint32_t head_crc = 0;
    for (uint32_t a = 0; a < JL_WRITE_MIN; a += JL_SECTOR_SIZE) {
        if (!read_sector(a)) {
            fail(E_LINK, a);
            return;
        }
        head_crc = pl_crc32(sec, JL_SECTOR_SIZE, head_crc);
    }
    dlog("RESCUE: boot head 0x0000..0x3FFF crc32 %08lX (not written)", (unsigned long)head_crc);

    for (int pass = 0; pass < 2; pass++) {
        if (!write_pass(image, h->image_addr, h->image_len)) return;
        dlog("RESCUE: pass %d: %lu sectors written, %lu already right", pass + 1,
             (unsigned long)n_written, (unsigned long)n_same);
        if (n_written == 0 || verify_pass(image, h->image_addr, h->image_len)) {
            state = AR_DONE;
            watchdog_hw->scratch[DONE_SCRATCH] = DONE_MAGIC;
            rgb(false, true, false);
            dlog("RESCUE: DONE in %.1f s. Switch the FM-1 OFF, remove the wires, switch it ON.",
                 (time_us_64() - t0) / 1e6);
            return;
        }
        if (!fm1_host_uboot_ready()) break;
    }
    fail(E_VERIFY, 0);
}

void ar_task(void) {
    static uint64_t mount_seen_us;
    static bool was_mounted, softkey_sent;
    uint64_t now = time_us_64();
    bool blink_slow = (now / 400000) & 1;
    bool blink_fast = (now / 120000) & 1;

    switch (state) {
    case AR_OFF:
        return;
    case AR_BAD_PAYLOAD:
        rgb(blink_slow, false, !blink_slow);
        return;
    case AR_DONE:
        rgb(false, true, false);
        return;
    case AR_ERROR:
        rgb(blink_fast, false, false);
        return;
    case AR_RUN:
        return;
    case AR_WAIT:
        break;
    }

    bool mounted = fm1_pio_host_mounted();
    if (mounted && !was_mounted) {
        mount_seen_us = now;
        softkey_sent = false;
        foreign = false;
    }
    was_mounted = mounted;

    if (fm1_host_uboot_ready()) {
        run();
        return;
    }
    if (mounted && fm1_host_v15_mounted() && !softkey_sent &&
        now - mount_seen_us > SOFTKEY_DELAY_MS * 1000ull) {
        softkey_sent = true;
        dlog("RESCUE: stock V15 is running - sending the soft key to enter boot mode");
        fm1_host_softkey();
    }
    if (mounted && !fm1_host_v15_mounted() && !foreign &&
        now - mount_seen_us > FOREIGN_AFTER_MS * 1000ull) {
        foreign = true;
        dlog("RESCUE: a device is attached but it is neither UBOOT nor stock V15. Switch the "
             "FM-1 OFF, unplug and re-plug this board, then switch the FM-1 ON.");
    }
    if (mounted && foreign) {
        rgb(true, false, true);
    } else {
        rgb(blink_slow, blink_slow, false);
    }
}
