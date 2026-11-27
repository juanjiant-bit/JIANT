# JIANT FM on the M-VAVE SMK-37 Pro / Elite

**Status: experimental.** The previous SMK-37 build (main, v0.9.18 line) has run on one owner's unit. This one is
the port of that support to the public firmware **V10.8.48**, with a new control layout the owner approved. Everything
new here was built and checked on the host only: every item marked *untested on hardware* must be checked on a unit.

The SMK-37 has the same JieLi AC791N chip as the FM-1. It also has the same SPL and the same chip key (0x980F)
in the stock v016 packages. Its board differs: the keybed, buttons, knobs, faders, wheels, pedal, LEDs, LCD pins,
flash layout and factory data. The port is a build-time board option. The FM-1 build gets no new flag and is byte for
byte what it was (`cmp build/felucca.bin`, `.fwsc`, `loader/ota.bin`, `bt.bin` against the unmodified V10.8.48 build).

The port follows Melodee's `BOARD_SMK37` (GPL-3.0-only; its SMK-37 port is by M. Geisler (genkigenki/makkuro),
with hardware notes at https://github.com/genkigenki/fm1-to-smk37). `hal/smk_input.h` and
`tests/smk_keys_test.c` are Melodee's files; JIANT's changes in them are marked "(JIANT)". The flash map, the store
owner tag, the LCD wiring, the pad LEDs and the panel table are adapted from Melodee and keep its attribution lines.
M-VAVE's open SMK firmware was used for hardware facts only. No M-VAVE SDK binary is linked or copied.

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
| package identity | `FM-1_909` | `SMK-37 Pro_909` / `SMK-37 Elite_909` |
| update loader's identity | `ota-FM-1_900` | `ota-SMK-37 Pro_900` / `ota-SMK-37 Elite_900` |
| update record's identity | `ota-FM-1_015` | `ota-SMK-37 Pro_016` / `ota-SMK-37 Elite_016` |
| USB product | `FM-1` / `FM-1 Update` | `JIANT SMK-37` / `JIANT SMK Update` |
| flash.bin (PRCT) | 0x93000 | 0x9D000, with the stock v016 region table |
| package entries | flash.bin, ota.bin, bt.bin | flash.bin, ota.bin (no Bluetooth segment) |
| Bluetooth (`FELUCCA_BT`) | on: the segment at 0xEA000-0xFBFFF | **off** (`#error` if on): those sectors are the SMK's USRFLASH marker, store C and USR. GEN, the lyrics dictionary and the other code the FM-1 keeps in the segment are in the app image instead |
| USB HOST (`FELUCCA_USB_HOST`) | on | off (the keybed is the keyboard; its flash is needed) |
| TRS MIDI IN (UART1, PH8) | on | off: PH8 is a key-matrix row on the SMK (`#error` if turned on) |
| key velocity | `src/vel.c`, `vel_diag.c` (the FM-1's carbon pills) | the HAL's two-contact time and pad peaks (`hal/smk_input.h`); MENU > CONTROL > VELOCITY sets its curve (OFF = 100) |
| LCD | SPI1 group B, PC7 CS, backlight PA2 | SPI1 group A: PB6 CLK, PB7 DO, PC8 CS, PB5 D/C, no backlight control |
| input HAL | `hal/fm1_input.h`, `hal/fm1_adc.h` | `hal/smk_input.h` (it stands in for `fm1_adc.h`: PB6 is the LCD clock) |
| the sound's pinned flash layout | `tools/audio_layout.txt` | `tools/audio_layout_smk37.txt` (its own, both models; `FELUCCA_REPIN=1` records it anew) |

The app slot (0x8EA90 bytes) is the FM-1's. The encrypted region still ends at 0x92FFE inside the SMK's
0x9D000 app area. The SMK image (V10.8.48 + SMK) is **581072 B of the 584336 B slot: 3264 B free**; RAM .data+.bss
92740 B of 98304; the pool's spare 11776 B; RAM code 32660 B of 34816. Everything SMK-only is compiled for the SMK
board only (`firmware/src/smk.c`, `smk_kb.c`, `smk_map.h`, `#if BOARD_SMK37`).

## The control mapping (V10.8 layout)

The owner's layout: the keybed always plays, the pads are steps / tracks / shortcuts, the knobs and faders have banks.
The code: `firmware/src/smk.c` (UI side), `firmware/src/smk_kb.c` (audio side), `firmware/src/smk_map.h` (the rules,
host-tested), `firmware/src/panel.c` (the buttons and knobs).

### Keys, wheels, pedal

| SMK-37 | JIANT |
|---|---|
| the 37 keys (C3..C6, all of them, both ends) | **always play the selected track**, at the velocity they are struck with. OCT- / OCT+ (-3..+3, both: 0) move the whole keyboard: key 1 is C3 + 12 x OCT, key 37 C6 + 12 x OCT; the window is moved, never folded, so every key stays a note of its own in 0..127. They go in as MIDI in on the selected track's channel (the firmware's own MIDI path: ARP, recording, DRUM-X's MIDI map, MIDI LEARN, the matrix's VEL), and out to MIDI OUT as the FM-1's keys do. A key held while the track or the octave changes ends the note it started. They play chromatically (the FM-1 keys' SCL key maps, QNT WHITE / SNAP and CHORD, are the FM-1 keys'). |
| PITCH wheel | pitch bend on the selected track (its rest at power-on is the centre, a small dead zone; the track's bend range, RPN 0) |
| MOD wheel | CC1 on the selected track: the matrix's MOD WHEEL source (0 at its rest) |
| SUSTAIN pedal | CC64 on the selected track: the notes held while it is down (either polarity: its reading at power-on is "up") |
| another track picked | the old track's bend, CC1 and pedal let go; the new one gets them |

### Pads (16, velocity)

Pad n is hardware pad n - 1: **1..8 the top row, 9..16 the bottom row**, left to right. What they do depends on the
page (`smk_pads_rule`):

| where | the pads |
|---|---|
| a layer (a button held: GLO, SEQ, EDIT, FX, SCL, REC, SAVE ...), NAME, SEQ > STEP on a DRUM track (the grid), STEP FX, the song pages | **the FM-1's 16 white keys** F3..G5, pad n the n-th: the layers' shortcuts (GLO + pad 9 = G4 UNDO, SEQ + pad 16 = G5 CLR STEP, SAVE + pad 12 = C5 NAME ...), NAME's letters, the DRUM grid's steps 1..16 (hold + KNOB 1..4: a lock), STEP FX's components. Black-key functions are not on the pads. |
| the other SEQ pages (STEP, NOTE, PATTERN, EUCLID ... on a melodic track; the non-grid pages on DRUM) | **the 16 steps of the bar shown**: a pad on an empty step writes it (melodic: the keybed's last note at the pad's velocity; DRUM: the lane picked); on a full step it clears it when let go. **Hold a pad + K1..K4**: a parameter lock on that step (as the FM-1's held step; then it is not cleared). The cursor follows the pad. |
| everywhere else | **1..6** select track T1..T6 (an asleep T5 / T6 says so); **PATCH held + 1..6**: that track's **mute** (no HOME tap, no MENU); **7** UNDO (as GLO + G4); **8** TAP tempo (as GLO's TAP); **9..16** page shortcuts: 9 GLO > MIXER, 10 EDIT > FILTER, 11 ENV > ENV, 12 LFO > MOD (the matrix), 13 FX > DLY, 14 FX > REVERB, 15 HOME > SONG > ARRANGE, 16 HOME > GENERATOR (GEN) |
| the MENU, a dialog | nothing (dark) |

The pads' LEDs show it: white keys as the FM-1's key LEDs (lit, blinking, the grid's glow); steps in the track's
colour (the cursor white, the playhead bright, past LEN dark, the first of each four dim); tracks in their colours
(the selected one bright, a muted one red, an asleep one dark), 7 / 8 dim white, the shortcuts dim blue (the page
shown bright).

### Knobs, faders, buttons

| SMK-37 (printed) | JIANT |
|---|---|
| K1..K4 | KNOB 1..4 (the page's four cards) |
| K5 | SELECT (the page chain) |
| K6 | PRESETS |
| K7 | ALGORITHM (the track) |
| K8 | MASTER (a virtual pot: 24 steps a detent, 724 at power-on) |
| **KNOB BANK** (its LED lit = MACROS) | K1..K8 become 8 fixed macros: **K1..K6 the cutoff of T1..T6** (the engine's own brightness as CC74 finds it, else the FILTER page's CUT), **K7 the delay's MIX**, **K8 the reverb's SIZE**. The screen says the bank, then each value (`T3 CUT 64`). Tap again: the page's knobs (and MASTER) back. |
| F1..F4, **FADER BANK** A (its LED dark) | the **levels of T1..T4** |
| F1..F4, FADER BANK B (its LED lit) | **T5 level, T6 level, the selected track's DLY send, its REV send** |
| faders: pick-up | a fader moves its value only once it meets it (within 2 / 127, or crossing it); until then the screen says `F2 PICK UP: UP` / `DOWN`. A value changed elsewhere (a knob, a load, the other bank) is picked up again. |
| < / > | the page before / after (one SELECT step) |
| FX | FX |
| SCALE | SCL |
| CHORD | ENV |
| NOTE REPEAT | LFO |
| PARA | EDIT |
| GLOBE | GLO |
| PATCH | HOME (and the mute modifier for pads 1..6) |
| BT | SAVE |
| ARP | ARP |
| SEQ | SEQ |
| SEQ PLAY | PLAY |
| SEQ REC | REC |
| OCTAVE- / OCTAVE+ | OCT- / OCT+ (the keybed's octave; back / YES on action pages and dialogs, as on the FM-1) |

V10.8.48's twelve FM-1 buttons are the same as before (HOME ENV LFO FX SCL EDIT GLO SAVE ARP SEQ PLAY REC), so the
printed-button table is the one the owner already uses; only K5..K8 changed (they were K5 MASTER, K6 SELECT,
K7 ALGORITHM, K8 PRESETS). The panel table's magic is now `PNS6`, so the old calibration is replaced by these defaults
once.

Other behaviour:
- OCTAVE- + OCTAVE+ held at power-on opens HARDWARE CALIBRATION. It asks for each FM-1 function by its printed SMK
  name, and for the knobs by K5 (SELECT), K7 (ALGORITHM), K6 (PRESETS) and K1..K4. The table's magic is `PNS6`, so an
  FM-1 table (or the previous SMK layout's) is never taken.
- OCTAVE- + OCTAVE+ held for 5 s enters UBOOT, as on the FM-1.
- A key struck again before a full lift sounds again (a release, then the new note at its new velocity).
- Button LEDs are on or off. What breathes on the FM-1 blinks at about 1 Hz here. The FM-1's dim idle glow has no
  level on the SMK, so idle buttons are dark, as with MENU > LEDS OFF. MENU > LEDS INV lights the idle buttons, as
  on the FM-1. SEQ PLAY shows its green LED while playing. KNOB BANK and FADER BANK light for MACROS and B.
- The screen texts still name the FM-1's controls (for example HOME, SCL, KNOB 1).
- MENU > CONTROL's AFTERTOUCH and VEL LEARN do nothing on the SMK (they are the FM-1's carbon pills).

## Flash: RAM-only by default

The SMK's flash holds factory data that cannot be recreated: the per-unit key and pad calibration (USRTRIM) and the
Bluetooth pairing (BTIF). So the default SMK build is **RAM-only**. It never erases or writes the flash: besides the
JEDEC check, `storage_hw.c` and `ota_hw.c` refuse every erase and write at compile time unless the build has
`FELUCCA_SMK_FLASH=1` (`SMK_MAY_WRITE`), whatever any caller asks. Settings, projects and presets are lost at power-off.
An update from JIANT's own M-UPGRADE entry is off too, because it needs the flash. As on the FM-1, any unknown flash
chip also gives RAM-only.

`FELUCCA_SMK_FLASH=1` turns the store on. It does so only on the SMK's own flash part (JEDEC 0xEB6014) and only in
the SMK's map. *Untested on hardware.*

The SMK's flash map (`firmware/hal/fm1_flash_map.h`, the store table in `firmware/src/storage.c`):

| range | what | JIANT |
|---|---|---|
| 0x00000-0x03FFF | head (SPL, isd_config) | never written |
| 0x04000-0x9CFFF | app area (stock PRCT) | the update loader only |
| 0x9D000-0xBAFFF | VM | store A: settings, 4 projects (song 1), FM6 patches, autosave, voice bank, song index, presets, song 2; the owner tag `SMK37DATAJIANT` at 0xAF000 |
| 0xBB000-0xBFFFF | VM | OTA staging; the update record at 0xBFF00 |
| 0xC0000-0xC0FFF | BTIF (Bluetooth pairing) | **never** |
| 0xC1000-0xC1FFF | USRTRIM (factory calibration) | **never** |
| 0xC2000-0xE9FFF | USRFLASH (the stock key mapping) | store B: songs 3..6 (V10.8 has six songs) at 0xC2000-0xE1FFF, only after the marker release; 0xE2000-0xE9FFF spare |
| 0xEA000-0xEAFFF | USRFLASH's end marker `usrdata` at 0xEAFF8 | erased once, as a whole sector, before the first write into store B, so that stock rebuilds its mapping |
| 0xEB000-0xF3FFF | unlisted | store C: V10.8's JAM takes (OBJ_JAM) at 0xEB000 / 0xEC000; 0xED000-0xF3FFF spare |
| 0xF4000-0xFDFFF | USR (stock resources, for a return to stock) | **never** |
| 0xFE000-0xFEFFF | free | **never** |
| 0xFF000-0xFFFFF | key_mac | **never** |

The guards:
1. The RAM flash driver (`fl_erase4k_ram`, `fl_prog_ram`) refuses any range that `FL_GUARD` refuses.
   `FL_GUARD = FL_RANGE_OK && !FL_PROTECTED`. `FL_PROTECTED` is an independent net over the head, BTIF, USRTRIM, USR
   with 0xFE000, and key_mac. It holds under the update loader's own allow-list too. Wrapping and straddling ranges
   are refused.
2. `storage_hw.c` allows erases and writes only in stores A, B and C, and (RAM-only) none at all. The marker sector is
   allowed only as one whole-sector erase.
3. On the first boot over another firmware's store, JIANT clears stores A, B and C and tags the store. It keeps an
   intact stock mapping in store B until JIANT first writes there.
4. The app's M-UPGRADE entry stages only in 0xBB000-0xBFFFF. Before anything is erased, it refuses:
   - an FM-1 package (-15);
   - a package whose region table differs from the device's (-16);
   - an official package with USR anywhere other than 0xF4000 (-17).
5. JIANT's update loader writes only the app area 0x4000-0x9CFFF. It erases update records only in the OTA area. It
   refuses an FM-1 package (-3), a package whose region table differs from the device's (-14) and (V10.8) any package
   with a Bluetooth segment entry (`bt.bin`: it would land at 0xEA000.., -14).
6. The package carries the stock v016 region table byte for byte, and no `bt.bin`. This was checked against M-VAVE's
   stock Pro and Elite v016 packages (for the table).

Do **not** install the FM-1 package on an SMK. The FM-1's loader cannot be changed (its build must stay
byte-identical), and it does not know the SMK's map.

## Host tests

`tests/run_tests.sh` runs these. The package checks run once the SMK packages are built. Set `SMK_STOCK=dir`
(holding `SMK37-Pro-V16.fwsc` and `SMK37-Elite-V16.fwsc`) to compare the region table with the stock packages too.
(V10.8.48's tree: some of run_tests.sh's FM-1 tests are stale against its firmware and stop the script before the SMK
steps, as they do on the unmodified V10.8.48 tree; the SMK steps pass when run alone.)

- `tests/smk_flash_test.c` checks the map and its guards:
  - every protected sector, page and byte against every allow-list and the driver's net;
  - straddling and wrapping ranges;
  - the store table (V10.8: 31 objects, the JAM in store C);
  - a simulated SMK with patterns in every protected range: first boot, every object saved and loaded, the marker
    released exactly once, erases and writes aimed at each forbidden range refused, then every protected byte
    compared.
- A check that the RAM driver uses `FL_GUARD`; one that the default build is RAM-only and has no Bluetooth segment.
- `tests/smk_ldr_test.c` checks the SMK update loader on a simulated SMK (and refuses an SMK package with a bt.bin).
- `tests/ota_test.c`, SMK build: staging at 0xBB000, the model's record, the board check.
- `tests/smk_pkg_test.py` checks the package: identity, size, region table, no bt.bin, and app.bin.
- `tests/smk_map_test.c` checks the control mapping: the buttons and knobs; **all 37 key ids through the HAL's queue and
  `smk_kb.c`, at every octave and on every track: a distinct note each, on the selected track, at its velocity, its
  release ending it** (also across a track / octave change, a re-strike, the splash); the wheels and the pedal; the
  pads' rule, numbers and white keys; the banks' targets; the faders' pick-up.
- A check that every page shortcut of pads 9..16 is a page of its section's chain.
- `tests/smk_keys_test.c` is Melodee's keybed test, run on `hal/smk_input.h`.

## Untested on hardware

Everything that touches the SMK. In particular, new in this V10.8.48 port:
- **The outer keys.** The previous build played only F3..G5; the 10 outer keys (C3..E3 = key ids 0..4, G#5..C6 = ids
  32..36) were drained, which is why the owner heard nothing from the first and last keys. They now play. Their matrix
  codes are Melodee's stock table (`SMK_KEYC`, from M-VAVE's v016 firmware) and were never heard in JIANT: please
  play C3, C#3, D3, D#3, E3 and G#5, A5, A#5, B5, C6 and report any key that is silent or plays the wrong note.
- Which mux C address is the PITCH wheel and which the MOD wheel (the HAL reads addresses 4 and 5; JIANT takes 4 as
  PITCH, 5 as MOD), the wheels' ranges and the pitch wheel's centre, the pedal's address (6) and its reading pressed /
  released (JIANT takes "far from the power-on reading" as pressed, either polarity; with no pedal plugged it must
  read steady).
- The faders' addresses (0..3 -> F2 F3 F4 F1, Melodee's `SMK_FADMUX`) and direction (0 at the bottom assumed).
- The pads' numbering on the unit (JIANT: 1..8 the top row, 9..16 the bottom, as the HAL's mux A / B) and their
  colours (WS2812 GRB order, the 250-byte latch).
- KNOB BANK's and FADER BANK's LEDs (LED ids 0 and 1 assumed to be under those buttons, as Melodee's table says).
- The encoders' K1..K8 order (Melodee: "K1..K8 order to confirm"); if K5..K8 come out swapped, HARDWARE CALIBRATION
  re-teaches SELECT, ALGORITHM and PRESETS (not MASTER, which is fixed at encoder 7).
- The image without the Bluetooth segment: GEN, the lyrics dictionary and the rest of the segment's code now run from
  the app image (never on an FM-1 that way).
- From before: the SMK SPL booting this app (as the previous build did on the owner's unit), the stock updater
  accepting the package, chip key 0x980F on every unit, UBOOT, LCD orientation / SPI speed, the ADC timing
  (`adc_miss`), the pads' thresholds, the audio codec, the battery reading (mux C address 7), the Pro vs the Elite.
- With `FELUCCA_SMK_FLASH=1`: flash ID 0xEB6014, the marker release and stock rebuilding its mapping, BTIF and USRTRIM
  byte-identical before and after (dump 0xC0000-0xC1FFF), the stock VM cleared on JIANT's first boot.

## Not done yet

- TRS MIDI out on PC7.
- The factory velocity curves from USRTRIM (read-only).
- A boot-stub rescue (OCTAVE- at power-on).
- SMK models in `tools/fm1_install.py`, including refusing cross-model installs.
- An SMK mode for the browser emulator (this tree has no `web/emu/devices.html`).
- The keybed through the FM-1 keys' own path (SCL's key maps, CHORD, KEY SPLIT, step entry while stopped on STEP):
  the keybed is MIDI in to the firmware, so those stay the FM-1 keys'.
