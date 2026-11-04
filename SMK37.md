# JIANT on the M-Vave SMK-37

JIANT also builds for the M-Vave SMK-37, a 37-key controller with the same chip as the FM-1. The FM-1 build is
unchanged; everything for the SMK-37 is behind `BOARD_SMK37`. The port follows the
[fm1-to-smk37](https://github.com/genkigenki/fm1-to-smk37) guide; its decisions are in
[smk37-port.yaml](smk37-port.yaml).

Status: builds and host tests pass; not yet tested on a unit.

## Supported models

- **SMK-37 Pro**: the default build (package identity `SMK-37 Pro_900`).
- **SMK-37 Elite**: `MODEL=elite` (identity `SMK-37 Elite_900`).
- **MKE-P37**: `MODEL=mke` (identity `MKE-P37_900`).
- **Starrykey 37 Play**: uses the SMK-37 Pro build.

They are the same hardware. Each build carries its model's name, and the installers refuse a package made for an
FM-1 on an SMK, or the other way round.

## Controls

The screen is JIANT's, as on the FM-1; read JIANT's own documentation for the pages and the layers.

| SMK-37 | Does |
| --- | --- |
| Keys F3..G5 (the middle 27) | the FM-1's 27 keys |
| Keys C3..E3 and G#5..C6 (the outer 10) | more notes, through the same key path (chords, strum, latch, recording, MIDI OUT) |
| K1 | MASTER (volume) |
| K2 / K3 / K4 | SELECT / ALGORITHM (the track) / PRESETS |
| K5..K8 | KNOB 1..4, the screen's four cards (the large cards show them as K5..K8) |
| PATCH / CHORD / NOTE REPEAT / FX / SCALE / PARA / GLOBE / BT | HOME / ENV / LFO / FX / SCL / EDIT / GLO / SAVE |
| ARP / SEQ / SEQ PLAY / SEQ REC | ARP / SEQ / PLAY / REC |
| OCTAVE- / OCTAVE+ | OCT- / OCT+ |
| PATCH held | MENU |
| < / > | on a SEQ page: the pads' bar (steps 1-16, 17-32, ...); elsewhere the page before / after (as SELECT) |
| KNOB BANK held on a SEQ page | the pads are drum pads while it is held |
| FADER BANK | the faders' bank: A, B, C (the top bar names it) |
| Pitch wheel | pitch bend of the selected track, MENU > MIDI > KEYBOARD > BEND RANGE (2 semitones by default) |
| Mod wheel | the MODW modulation source (as MIDI CC1); route it on a track's LFO > MOD |

**Pads.** On every SEQ page the 16 pads are the steps of the bar shown, 1-8 on the top row and 9-16 on the bottom
row. On a DRUM track a tap toggles the selected lane's hit there (its accent while ACC is held) and plays the lane.
On another track a tap on an empty step writes the last note played there, and a tap on a filled step clears it
when you let go; the step plays either way. Hold a step pad and turn K5..K8 to lock a parameter on that step, as
with a held step on the FM-1. A lit pad has a hit or a note; the playhead is bright.

Everywhere else, and while KNOB BANK is held on a SEQ page, the pads play T1's eight drum sounds, BD SD CP CH OH TM RS
CB from left to right: the top row their patch A, the bottom row their patch B. They follow your touch, and with T1
armed (REC) and playing they are recorded. In a quick layer, NAME and LIVE MANO the pads are the white keys F3..G5,
as before.

**Faders.** A fader acts only once it moves, also after a bank change, so nothing jumps.

| Bank | Fader 1 | Fader 2 | Fader 3 | Fader 4 |
| --- | --- | --- | --- | --- |
| A | T1 level | T2 level | T3 level | T4 level |
| B | T5 level | T6 level | reverb return (full at power-on) | delay return (DLY MIX) |
| C | selected track's cutoff | its DIST | its REV send | its DLY send |

**MENU > MIDI > KEYBOARD:** KEY VELOCITY (DYNAMIC, SOFT, HARD, or FIXED 64 / 100 / 127) for the keys and the pads,
BEND RANGE, and TRS MIDI OUT (ON by default). **MENU > MIDI > CHANNELS** (and CHANNEL2): the MIDI channel each track
sends on, 1-16 (T1 = 1 .. T6 = 6 by default; MIDI IN is not affected).

## MIDI

USB MIDI works as on the FM-1, each track on its own channel (MENU > MIDI > CHANNELS). The SMK-37's 3.5 mm TRS jack is a MIDI **output** (the FM-1's is an input): it sends the
same stream as USB MIDI OUT, after MENU > MIDI OUT (KEYS, CLOCK, ALL or OFF), even with no computer connected. The
jack is listed as type A; if a synth stays silent, try a type B adapter. There is no TRS MIDI IN on the SMK-37.

## Build

See [BUILDING.md](BUILDING.md) for the toolchain.

```sh
BOARD=smk37 ./build.sh                 # build/jiant-smk37.fwsc (SMK-37 Pro, Starrykey 37 Play)
BOARD=smk37 MODEL=elite ./build.sh     # build/jiant-smk37-elite.fwsc
BOARD=smk37 MODEL=mke ./build.sh       # build/jiant-smk37-mke.fwsc
./build.sh                             # the FM-1 build again before tests/run_tests.sh (the tests use its files)
```

## Install

Back up first, and keep M-Vave's stock package for your model and its updater, M-UPGRADE, at hand. Then connect the
keyboard by USB and run, with the package for your model:

```sh
python3 tools/fm1_install.py build/jiant-smk37.fwsc
```

`fm1_install.py` needs `mido` and `python-rtmidi` (`pip3 install mido python-rtmidi`). It checks that the package
is for the connected model, also when the keyboard is already in update mode.

## Going back to the stock firmware

Install M-Vave's firmware package for your model with M-UPGRADE. It works directly, because the installed package
keeps the keyboard's model name. `tools/fm1_install.py` can also install the stock SMK-37 Pro V016 package.

## Safety

- Keep the USB cable connected until the install has finished and the keyboard has restarted.
- The first start clears the stock firmware's settings, and any other firmware's data, in the shared data area. It
  keeps 7 songs (the FM-1 build keeps 8).
- The firmware never writes the keyboard's calibration, Bluetooth pairing or key areas; the flash map refuses
  them at compile time and again in the flash driver.
- Installing other firmware is at your own risk. This port is not made or endorsed by M-Vave.
