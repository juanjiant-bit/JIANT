# JIANT FM on the M-VAVE SMK-37 Pro / Elite

**Status: experimental. Nothing in this port has run on an SMK-37.** Everything in it was built and checked
on the host only. Every item marked *untested on hardware* below must be checked on a unit before anyone
installs this build for real use.

The SMK-37 has the same JieLi AC791N chip as the FM-1. It also has the same SPL and the same chip key (0x980F)
in the stock v016 packages. Its board differs: the keybed, buttons, knobs, LEDs, LCD pins, flash layout and
factory data. The port is a build-time board option. The FM-1 build gets no new flag and is byte for byte what it
was.

The port follows Melodee's `BOARD_SMK37` (GPL-3.0-only; its SMK-37 port is by M. Geisler (genkigenki/makkuro),
with hardware notes at https://github.com/genkigenki/fm1-to-smk37). `hal/smk_input.h` and
`tests/smk_keys_test.c` are Melodee's files, unchanged. The flash map, the store owner tag, the LCD wiring, the pad
LEDs and the panel table are adapted from Melodee and keep its attribution lines. M-VAVE's open SMK firmware was
used for hardware facts only. No M-VAVE SDK binary is linked or copied.

## Build

```sh
./build.sh                                  # FM-1: build/felucca.fwsc (unchanged)
./build.sh --board smk37                    # SMK-37 Pro:   build/smk37-pro/felucca-smk37-pro.fwsc
./build.sh --board smk37 --model elite      # SMK-37 Elite: build/smk37-elite/felucca-smk37-elite.fwsc
FELUCCA_SMK_FLASH=1 ./build.sh --board smk37    # the same, with the flash store on (see below)
```

`BOARD=smk37` and `MODEL=elite` in the environment work too. `--release X.Y` gives the identity
`SMK-37 Pro_9XY` or `SMK-37 Elite_9XY`.

What the SMK build changes:

| | FM-1 | SMK-37 |
|---|---|---|
| define | none | `-DBOARD_SMK37=1 -DSMK_MODEL="SMK-37 Pro"` (or `"SMK-37 Elite"`) |
| package identity | `FM-1_900` | `SMK-37 Pro_900` / `SMK-37 Elite_900` |
| update loader's identity | `ota-FM-1_900` | `ota-SMK-37 Pro_900` / `ota-SMK-37 Elite_900` |
| update record's identity | `ota-FM-1_015` | `ota-SMK-37 Pro_016` / `ota-SMK-37 Elite_016` |
| USB product | `JIANT` / `JIANT Update` | `JIANT SMK-37` / `JIANT SMK Update` |
| flash.bin (PRCT) | 0x93000 | 0x9D000, with the stock v016 region table |
| TRS MIDI IN (UART1, PH8) | on | off: PH8 is a key-matrix row on the SMK (`#error` if turned on) |
| LCD | SPI1 group B, PC7 CS, backlight PA2 | SPI1 group A: PB6 CLK, PB7 DO, PC8 CS, PB5 D/C, no backlight control |
| input HAL | `hal/fm1_input.h`, `hal/fm1_adc.h` | `hal/smk_input.h` (it stands in for `fm1_adc.h`: PB6 is the LCD clock) |

The app slot (0x8EA90 bytes) is the FM-1's. The encrypted region still ends at 0x92FFE inside the SMK's
0x9D000 app area.

## The control mapping

| SMK-37 (printed) | JIANT (the FM-1's control) |
|---|---|
| FX | FX |
| SCALE | SCL |
| CHORD | ENV |
| NOTE REPEAT | LFO |
| PARA | EDIT |
| GLOBE | GLO |
| PATCH | HOME |
| BT | SAVE |
| ARP | ARP |
| SEQ | SEQ |
| SEQ PLAY | PLAY |
| SEQ REC | REC |
| OCTAVE- / OCTAVE+ | OCT- / OCT+ |
| K1..K4 | KNOB 1..4 |
| K5 | MASTER (a virtual pot: 24 steps a detent, 724 at power-on) |
| K6 / K7 / K8 | SELECT / ALGORITHM / PRESETS |
| `<` / `>` | one SELECT step each, as one detent of K6 |
| the 27 middle keys F3..G5 | the FM-1's 27 keys |
| the 16 pads | the FM-1's 16 white keys F3..G5. Bottom row, left to right: F3 G3 A3 B3 C4 D4 E4 F4. Top row: G4 A4 B4 C5 D5 E5 F5 G5 |
| the outer 10 keys (C3..E3, G#5..C6) | nothing yet (their events are drained) |
| KNOB BANK, FADER BANK | nothing yet |
| faders 1..4, pitch / mod wheel, sustain pedal | nothing yet (the HAL reads them; JIANT does not use them) |

Other behaviour:
- OCTAVE- + OCTAVE+ held at power-on opens HARDWARE CALIBRATION. It asks for each FM-1 function by its printed SMK
  name, and for the knobs by K6, K7, K8 and K1..K4. The table's magic is `PNS5`, so an FM-1 table is never taken.
- OCTAVE- + OCTAVE+ held for 5 s enters UBOOT, as on the FM-1.
- Keys and pads play at the velocity the HAL measures (the keybed's two-contact time, a pad's peak); notes sent to MIDI OUT carry it. The FM-1 still plays at 100. *Untested on hardware.*
- A key struck again before a full lift sounds again (`notes_retrig`).
- Button LEDs are on or off. What breathes on the FM-1 blinks at about 1 Hz here. The FM-1's dim idle glow has no
  level on the SMK, so idle buttons are dark, as with MENU > LEDS OFF. MENU > LEDS INV lights the idle buttons, as
  on the FM-1. SEQ PLAY shows its green LED while playing.
- The pads show their key's LED from the FM-1 picture:
  - white when lit;
  - blinking when it breathes;
  - dim for the DRUM grid's glow and beats and for SCALE LEDS.
- The screen texts still name the FM-1's controls (for example HOME, SCL, KNOB 1).

## Flash: RAM-only by default

The SMK's flash holds factory data that cannot be recreated: the per-unit key and pad calibration (USRTRIM) and the
Bluetooth pairing (BTIF). So the default SMK build is **RAM-only**. It never erases or writes the flash. Settings,
projects and presets are lost at power-off. An update from JIANT's own M-UPGRADE entry is off too, because it needs
the flash. As on the FM-1, any unknown flash chip also gives RAM-only.

`FELUCCA_SMK_FLASH=1` turns the store on. It does so only on the SMK's own flash part (JEDEC 0xEB6014) and only in
the SMK's map. *Untested on hardware.*

The SMK's flash map (`firmware/hal/fm1_flash_map.h`):

| range | what | JIANT |
|---|---|---|
| 0x00000-0x03FFF | head (SPL, isd_config) | never written |
| 0x04000-0x9CFFF | app area (stock PRCT) | the update loader only |
| 0x9D000-0xBAFFF | VM | store A: settings, 4 projects (song 1), FM6 patches, autosave, voice bank, song index, presets, song 2; the owner tag `SMK37DATAJIANT` at 0xAF000 |
| 0xBB000-0xBFFFF | VM | OTA staging; the update record at 0xBFF00 |
| 0xC0000-0xC0FFF | BTIF (Bluetooth pairing) | **never** |
| 0xC1000-0xC1FFF | USRTRIM (factory calibration) | **never** |
| 0xC2000-0xE9FFF | USRFLASH (the stock key mapping) | store B: songs 3..7, only after the marker release |
| 0xEA000-0xEAFFF | USRFLASH's end marker `usrdata` at 0xEAFF8 | erased once, as a whole sector, before the first write into store B, so that stock rebuilds its mapping |
| 0xEB000-0xF3FFF | unlisted | store C: song 8; 0xF3000 spare |
| 0xF4000-0xFDFFF | USR (stock resources, for a return to stock) | **never** |
| 0xFE000-0xFEFFF | free | **never** |
| 0xFF000-0xFFFFF | key_mac | **never** |

The guards:
1. The RAM flash driver (`fl_erase4k_ram`, `fl_prog_ram`) refuses any range that `FL_GUARD` refuses.
   `FL_GUARD = FL_RANGE_OK && !FL_PROTECTED`. `FL_PROTECTED` is an independent net over the head, BTIF, USRTRIM, USR
   with 0xFE000, and key_mac. It holds under the update loader's own allow-list too. Wrapping and straddling ranges
   are refused.
2. `storage_hw.c` allows erases and writes only in stores A, B and C. The marker sector is allowed only as one
   whole-sector erase.
3. On the first boot over another firmware's store, JIANT clears stores A, B and C and tags the store. It keeps an
   intact stock mapping in store B until JIANT first writes there.
4. The app's M-UPGRADE entry stages only in 0xBB000-0xBFFFF. Before anything is erased, it refuses:
   - an FM-1 package (-15);
   - a package whose region table differs from the device's (-16);
   - an official package with USR anywhere other than 0xF4000 (-17).
5. JIANT's update loader writes only the app area 0x4000-0x9CFFF. It erases update records only in the OTA area. It
   refuses an FM-1 package (-3) and a package whose region table differs from the device's (-14).
6. The package carries the stock v016 region table byte for byte. This was checked against M-VAVE's stock Pro and
   Elite v016 packages.

Do **not** install the FM-1 package on an SMK. The FM-1's loader cannot be changed (its build must stay
byte-identical), and it does not know the SMK's map.

## Host tests

`tests/run_tests.sh` runs these. The package checks run once the SMK packages are built. Set `SMK_STOCK=dir`
(holding `SMK37-Pro-V16.fwsc` and `SMK37-Elite-V16.fwsc`) to compare the region table with the stock packages too.

- `tests/smk_flash_test.c` checks the map and its guards:
  - every protected sector, page and byte against every allow-list and the driver's net;
  - straddling and wrapping ranges;
  - the store table;
  - a simulated SMK with patterns in every protected range: first boot, every object saved and loaded, the marker
    released exactly once, erases and writes aimed at each forbidden range refused, then every protected byte
    compared.
- A check that the RAM driver uses `FL_GUARD`.
- `tests/smk_ldr_test.c` checks the SMK update loader on a simulated SMK.
- `tests/ota_test.c`, SMK build: staging at 0xBB000, the model's record, the board check.
- `tests/smk_pkg_test.py` checks the package: identity, size, region table and app.bin.
- `tests/smk_map_test.c` checks the control mapping.
- `tests/smk_keys_test.c` is Melodee's keybed test, run on `hal/smk_input.h`.

## Untested on hardware

Everything that touches the SMK. In particular:
- The SMK SPL boots this app (app_area_head ends at 0x92FFE inside a 0x9D000 PRCT).
- The stock SMK firmware's updater accepts this package. M-VAVE's Pro v016 file interleaves 36 identity blocks;
  JIANT's packages use 20, as on the FM-1.
- Chip key 0x980F on every SMK unit.
- `fm1_enter_uboot` (UBOOT) and the SPL honouring the update record at 0xBFF00 with `ota-SMK-37 Pro_016`.
- LCD orientation, colours and SPI speed. No glitches when SPI1 switches to the pad LEDs.
- The pad LED chain order, colours and timing (the 250-byte latch).
- Button ids, LED positions and encoder directions (Melodee confirmed these on an Elite only).
- The ADC timing (console: `adc_miss` near 0), the pads' thresholds, and the K5 MASTER pot's feel.
- The audio codec and output path (PC0/1/2/6, the same lines as the FM-1's).
- The battery reading (mux C address 7).
- With `FELUCCA_SMK_FLASH=1`:
  - flash ID 0xEB6014;
  - the marker release, and stock rebuilding its mapping after a return to stock;
  - BTIF and USRTRIM byte-identical before and after an install and a first boot (dump 0xC0000-0xC1FFF);
  - the stock VM's contents being cleared on JIANT's first boot.
- The Pro, and every Elite other than the one Melodee ran on.

## Not done yet

- TRS MIDI out on PC7, the faders, the wheels and the sustain pedal.
- KNOB BANK and FADER BANK, and the outer 10 keys.
- The factory velocity curves from USRTRIM (read-only).
- A boot-stub rescue (OCTAVE- at power-on).
- SMK models in `tools/fm1_install.py`, including refusing cross-model installs.
- An SMK mode for the browser emulator.
