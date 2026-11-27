# JIANT FM on the M-VAVE SMK-37 Pro / Elite

**Status: experimental.** The previous SMK-37 build (main, v0.9.18 line) has run on one owner's unit. This one is
the port of that support to the public firmware **V10.8.48**, with **Rhythmer's control layout** (below). Everything
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

Credits for this layout and its hardware details (all GPL-3.0; adapted, their notices kept in the files' headers):
- **Rhythmer** by **Jazzy Rhythmer** (https://github.com/JazzyRhythmer/Rhythmer), a fork of JIANT for the FM-1 with an
  SMK-37 Pro build (0.1.2-smk3 guide, 0.1.2 source): the control layout (K1..K8, the buttons, KNOB BANK's PLAY / STEPS
  pads, FADER BANK's four banks with the T5 / T6 window and pick-up), all 37 keys through the original key path with
  the outer ten as extra notes, and the TRS MIDI OUT driver (`smk_map.h`, `smk.c`, `smk_kb.c`, `smk_uart_tx.h`,
  `smk_midi_out.h`).
- **genkigenki** (M. Geisler): the SMK-37 board layer (https://github.com/genkigenki/Felucca, branch `smk37`), which
  `hal/smk_input.h` is, and the porting reference https://github.com/genkigenki/fm1-to-smk37 (`docs/01-hardware.md`:
  the key matrix, the encoders' order, the pads' rows, the mux addresses, the wheels' measured ranges, the TRS MIDI
  OUT pin).

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
| TRS MIDI OUT (`FELUCCA_SMK_TRS`) | none | on: UART1 TX on PC7, following MENU > CONTROL > MIDI OUT |
| key velocity | `src/vel.c`, `vel_diag.c` (the FM-1's carbon pills) | the HAL's two-contact time and pad peaks (`hal/smk_input.h`); MENU > CONTROL > VELOCITY sets its curve (OFF = 100) |
| LCD | SPI1 group B, PC7 CS, backlight PA2 | SPI1 group A: PB6 CLK, PB7 DO, PC8 CS, PB5 D/C, no backlight control |
| input HAL | `hal/fm1_input.h`, `hal/fm1_adc.h` | `hal/smk_input.h` (it stands in for `fm1_adc.h`: PB6 is the LCD clock) |
| the sound's pinned flash layout | `tools/audio_layout.txt` | `tools/audio_layout_smk37.txt` (its own, both models; `FELUCCA_REPIN=1` records it anew) |

The app slot (0x8EA90 bytes) is the FM-1's. The encrypted region still ends at 0x92FFE inside the SMK's
0x9D000 app area. The SMK image (V10.8.48 + SMK, Rhythmer's layout, TRS MIDI OUT) is **580664 B of the 584336 B
slot: 3672 B free** (Elite: 580668 B, 3668 B free; the TRS jack costs ~1.2 KB); RAM .data+.bss 92828 B of 98304; the
pool's spare 11552 B; RAM code 32184 B of 34816. Everything SMK-only is compiled for the SMK board only
(`firmware/src/smk.c`, `smk_kb.c`, `smk_map.h`, `smk_midi_out.h`, `hal/smk_uart_tx.h`, `#if BOARD_SMK37`).

## The control mapping (Rhythmer's layout)

The layout is **Rhythmer's** SMK-37 Pro layout (Rhythmer OS 0.1.2-smk3, a GPL-3.0 fork of JIANT for the FM-1 by
Jazzy Rhythmer), adapted to V10.8.48. The keybed goes through the FM-1's own key path, the pads are the FM-1's white
keys or the page's steps, the faders have four banks. The code: `firmware/src/smk.c` (UI side), `firmware/src/smk_kb.c`
(the keys' block's SMK step: the outer keys, the pedal, the wheels), `firmware/src/smk_map.h` (the rules, host-tested),
`firmware/src/seq.c` (the FM-1's key path, holding 37 keys on the SMK), `firmware/src/panel.c` (the buttons and knobs).

| SMK-37 (printed) | JIANT V10.8.48 |
|---|---|
| K1 | **MASTER** (a virtual pot: 24 steps a detent, 724 at power-on) |
| K2 | **SELECT** (the section's pages) |
| K3 | **ALGORITHM** (the track, as on the FM-1) |
| K4 | **PRESETS** |
| K5..K8 | **KNOB 1..4** (the screen's four page knobs) |
| PATCH | HOME (held ~0.7 s: the MENU, as the FM-1's HOME) |
| PARA | EDIT |
| CHORD | ENV |
| NOTE REPEAT | LFO |
| SCALE / FX / GLOBE / ARP / SEQ | SCL / FX / GLO / ARP / SEQ |
| BT | SAVE (not Bluetooth) |
| SEQ PLAY | PLAY (again: STOP) |
| SEQ REC | REC |
| OCTAVE- / OCTAVE+ | OCT- / OCT+: the octave, and back / YES on action pages and dialogs, as on the FM-1 |
| < / > | the page before / after (one SELECT detent; not in a dialog) |
| KNOB BANK | the pads **PLAY <-> STEPS** (its LED lit in STEPS; blinking while a change waits for the pads) |
| FADER BANK | the faders **LEVEL -> PAN -> REVERB -> DELAY** (its LED lit for PAN, REVERB, DELAY) |
| the 37 keys | all through the FM-1's key path (below) |
| 16 pads | PLAY: the FM-1's 16 white keys; STEPS: the page's 16 steps (below) |
| F1..F4 | the bank's value of T1..T4; with T5 or T6 selected F1 / F2 are T5 / T6 (F3, F4 nothing) |
| PITCH / MOD wheels | pitch bend / modulation (CC1, the matrix's MOD WHEEL) of the selected track |
| SUSTAIN pedal | sustain (JIANT's; Rhythmer has none) |
| TRS MIDI OUT (3.5 mm) | what MENU > CONTROL > MIDI OUT sends over USB, also out of the jack, track n on channel n |

### Keys

All 37 keys go through the FM-1's **original key path** (`seq.c` keyboard_block, kb_map, key_on / key_off), with
velocity: SCL's key maps (QUANT WHITE / SNAP), CHORD, STRUM, ARP, LATCH, KEY SPLIT, recording, DRONE CLAY, MIDI OUT.

- **The central 27 (the 6th to the 32nd key, F3..G5)**, are the FM-1's 27 keys (`fm1_in.notes`): everything the FM-1's keys do
  they do, on every page: the layers' shortcuts (GLO + G4 UNDO ...), NAME's letters, the DRUM grid, step entry while
  stopped, SCENE.
- **The outer ten (C3..E3 and G#5..C6)** play extra notes: MIDI 48..52 and 80..84 at OCT 0 (the keybed is MIDI
  48..84), moved by OCT, through the same key path (a DRUM track: a lane each). Where the FM-1's keys are the UI's
  (a layer's button held, NAME, the DRUM grid) and under the splash they are silent.
- A key struck again before a full lift sounds again (a release, then the new note at its new velocity); a key held
  while the track or the octave changes ends the note it started.
- Key ids: the HAL's key k (0 = C3 .. 36 = C6, genkigenki's matrix table, verified in use on an SMK-37 Pro); ids
  5..31 are the FM-1's keys 0..26, ids 0..4 the key path's keys 27..31 and ids 32..36 keys 32..36 (Rhythmer's
  order; `smk_map.h` smk_kb_key).

**SUSTAIN pedal**: a key let go while it is down keeps sounding until the pedal is up, or the key is struck again; it
is also CC64 on the selected track (for notes from MIDI) and to MIDI OUT. Either polarity: its reading at power-on is
"up". **Wheels**: as MIDI in on the selected track's channel (its bend range, RPN 0), and to MIDI OUT with the keys;
another track picked: the old one's bend, CC1 and pedal let go.

### Pads (16, velocity)

Pad n is hardware pad n - 1: **1..8 the top row, 9..16 the bottom row**, left to right (genkigenki: verified with
`padtest`). KNOB BANK picks what they are:

| KNOB BANK | the pads |
|---|---|
| **PLAY** (dark) | **the FM-1's 16 white keys** F3..G5, pad n the n-th: on each page they do what the FM-1's white keys do there (notes, the steps of SEQ's pages, the layers' shortcuts, NAME's letters, the DRUM grid, STEP FX ...), at the velocity they are hit with. |
| **STEPS** (lit) | **the 16 steps of the SEQ page shown** (the cursor's bar; the DRUM grid: the lane picked): a pad on an empty step writes it (melodic: the keys' last note at the pad's velocity; DRUM: the lane); on a full step it clears it when let go. **Hold a pad + KNOB 1..4 (K5..K8)**: a parameter lock on that step (then it is not cleared). In the MENU, a dialog, a layer or NAME the pads do nothing. |

Switching while a pad is held waits until every pad is let go (no stray note, no half a step). The pads' LEDs: PLAY
the FM-1 keys' picture (lit white, breathing blinking, the DRUM grid's glow); STEPS the steps in the track's colour,
the cursor white, **the playhead bright**, past LEN dark.

### Faders

FADER BANK cycles **LEVEL -> PAN -> REVERB -> DELAY** (the reverb and delay sends) and the screen says the bank
(`FADERS PAN T1-T4`). F1..F4 are T1..T4; with T5 or T6 selected, F1 and F2 are T5 and T6 and F3, F4 do nothing.
**Pick-up**: a fader moves its value only once it meets it (within 2 / 127, or crossing it); until then the screen
says `F2 PICK UP: UP` / `DOWN`. A bank change, a change of track window, or the value changed elsewhere (a knob, a
load) arms the pick-up again, so loading a project never jumps to the faders' positions.

Other behaviour:
- OCTAVE- + OCTAVE+ held at power-on opens HARDWARE CALIBRATION. It asks for each FM-1 function by its printed SMK
  name, and for the knobs by K2 (SELECT), K3 (ALGORITHM), K4 (PRESETS) and K5..K8. The table's magic is now `PNS7`,
  so an older table (V10.8.48's first SMK layout `PNS6`, the FM-1's) is replaced by these defaults once.
- OCTAVE- + OCTAVE+ held for 5 s while running enters UBOOT, as on the FM-1 (a different path from the calibration).
- Button LEDs are on or off. What breathes on the FM-1 blinks at about 1 Hz here. The FM-1's dim idle glow has no
  level on the SMK, so idle buttons are dark, as with MENU > LEDS OFF. MENU > LEDS INV lights the idle buttons, as
  on the FM-1. SEQ PLAY shows its green LED while playing.
- The screen texts still name the FM-1's controls (for example HOME, SCL, KNOB 1).
- MENU > CONTROL's AFTERTOUCH and VEL LEARN do nothing on the SMK (they are the FM-1's carbon pills); VELOCITY sets
  the keys' and pads' curve.

### TRS MIDI OUT

The 3.5 mm jack is a MIDI OUT (UART1 TX on PC7, 31250 baud; `firmware/hal/smk_uart_tx.h`, `firmware/src/smk_midi_out.h`,
after Rhythmer's driver; the pin and the register setup are the stock v016's as genkigenki documented them). It follows
**MENU > CONTROL > MIDI OUT** as USB does: KEYS (the keys, chords, the wheels and the pedal), CLOCK (and the clock with
START / STOP on the internal tempo), ALL (and the sequencer's and the arp's notes), OFF (the jack silent). Track n is
channel n. USB MIDI keeps working beside it. SysEx (the editor's, the update's) never goes to the jack. A full queue or
a UART that stops answering sends CC64 0 / CC120 / CC121 / CC123 on every channel rather than leave a note hanging;
after four timeouts in a row the jack stops until MIDI OUT is set again. A change of MIDI OUT lets the jack's held
notes go. Build without it: `FELUCCA_SMK_TRS=0`. *Untested on hardware* (the jack's signal, the adapter's wiring, the
timing).

## Hardware details from the references

JIANT's SMK board layer (`firmware/hal/smk_input.h`) is genkigenki's (via Melodee); it was compared with genkigenki's
Felucca `smk37` branch, its porting reference (`docs/01-hardware.md`) and Rhythmer's 0.1.2 source:

| item | genkigenki's status | JIANT |
|---|---|---|
| key matrix (37 x 2 contacts, k 0 = C3 .. 36 = C6) | verified in use | identical table; the outer keys' codes were right: the old build's silent ends were its routing (it drained them) |
| buttons (18), their printed names | verified | identical |
| encoders K1..K8 order | **verified** | identical (the old note "order to confirm" is gone) |
| pads: top row = hardware pads 0..7 (mux A) | verified (padtest) | identical |
| faders: mux C 0..3 -> F2 F3 F4 F1, full scale 1023 | verified | identical |
| wheels: PITCH mux C 4, rest ~535, travel ~445..640; MOD 5, ~474..629, up = lower | verified | **corrected**: JIANT scaled them on 0..1023 (a full bend needed 1023, so the real travel gave about a sixth of the bend and a quarter of MOD); now a self-calibrating range as genkigenki's and Rhythmer's (`smk_map.h`) |
| pedal: mux C 6, ~985 with nothing plugged | analysis | kept (either polarity from its power-on reading) |
| button LEDs, SEQ REC's at 0x1b | verified for the LEDs driven | identical (the table's 17 / 18 order as confirmed on a unit) |
| TRS MIDI OUT: UART1 TX, PC7, BAUD 383 | analysis | new, as Rhythmer's |

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
- `tests/smk_map_test.c` checks the control mapping: the printed buttons and K1..K8 ("PNS7"); **all 37 key ids
  through the HAL and the key path** (`smk_kb.c` and keyboard_block's loop as seq.c has it): ids 5..31 as the FM-1's
  keys, the outer ten through the queue, **at OCT -3..+3 on T1..T6 a key_on each of a note of its own (C3 + id + 12
  OCT, both ends), at its velocity, its release a key_off on the track it started on**; a re-strike (central and
  outer), a layer / NAME / the DRUM grid / the splash (the outer keys silent), a DRUM track; the pedal's sustain; the
  wheels on genkigenki's measured ranges; the pads PLAY (white keys) / STEPS (the UI's) and KNOB BANK's change waiting
  for the pads; FADER BANK's cycle, the T5 / T6 window, the pick-up's context and the pick-up.
- A check that seq.c's key path holds 37 keys on the SMK (`KB_NK`, the outer ten mapped, the 27 from smk_kb_block).
- `tests/smk_keys_test.c` is Melodee's keybed test, run on `hal/smk_input.h`.

## Untested on hardware

Everything that touches the SMK. In particular, new in this layout:
- **The keys through the key path**, the outer ten above all: please play C3, C#3, D3, D#3, E3 and G#5, A5, A#5, B5,
  C6 (they are MIDI 48..52 and 80..84 at OCT 0) and report any that is silent or plays the wrong note. Their matrix
  codes are genkigenki's verified table; the path from the queue to key_on is host-tested only.
- The pads in PLAY (the FM-1's white keys on every page) and STEPS (steps, locks, the playhead on the LEDs); KNOB
  BANK's LED (LED id 0 under it, as genkigenki's table says) and FADER BANK's.
- The fader banks PAN / REVERB / DELAY and the T5 / T6 window; the faders' direction (0 at the bottom assumed).
- The wheels' self-calibration on other units (the ranges are one unit's), the pedal (address 6, either polarity;
  with no pedal plugged it must read steady) and its sustain.
- **TRS MIDI OUT**: the jack's signal on PC7, the adapter's wiring (type A in a spec sheet, not measured), the timing,
  USB MIDI beside it.
- The image without the Bluetooth segment: GEN, the lyrics dictionary and the rest of the segment's code now run from
  the app image (never on an FM-1 that way).
- From before: the SMK SPL booting this app (as the previous build did on the owner's unit), the stock updater
  accepting the package, chip key 0x980F on every unit, UBOOT, LCD orientation / SPI speed, the ADC timing
  (`adc_miss`), the pads' thresholds, the audio codec, the battery reading (mux C address 7), the Pro vs the Elite.
- With `FELUCCA_SMK_FLASH=1`: flash ID 0xEB6014, the marker release and stock rebuilding its mapping, BTIF and USRTRIM
  byte-identical before and after (dump 0xC0000-0xC1FFF), the stock VM cleared on JIANT's first boot.

## Not done yet

- The factory velocity curves from USRTRIM (read-only).
- A boot-stub rescue (OCTAVE- at power-on).
- SMK models in `tools/fm1_install.py`, including refusing cross-model installs.
- An SMK mode for the browser emulator (this tree has no `web/emu/devices.html`).
- The outer ten keys in SCENE and in step entry while stopped (the central 27 have both, as the FM-1's keys).
