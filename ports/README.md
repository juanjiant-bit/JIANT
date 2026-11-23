# JIANT beyond the FM-1

JIANT FM is meant to run whole, on its own, on more than one machine: the M-VAVE FM-1, the M-VAVE SMK-37 Pro / Elite
and the Akai MPC Sample, plus a plugin for the MPC standalones and desktop hosts. Every one of them plays the same
firmware sources (`firmware/src`), so a song sounds the same everywhere.

```
                        firmware/src  (engines, sequencer, songs, generator, FX, UI, 240 x 240 screen)
                                  |
          +-----------------------+------------------------+
          |                                                |
   firmware/hal  (bare metal, JieLi AC79)           ports/core  (one C library, any CPU)
          |                                                |
   +------+-------+                     +------------------+-------------------+
   |              |                     |                  |                   |
 FM-1     SMK-37 Pro / Elite     web/emu (browser)   ports/mpc-plugin    MPC Sample app
 (today)  (same chip: next)      (today)             (this folder)       (next: its own OS)
```

| Port | What it is | State |
| --- | --- | --- |
| [`core/`](core/) | The whole firmware as one C library: boot, audio, MIDI, the panel, the screen, the music saved and restored. The browser emulator's host layer behind a stable API ([`jiant_core.h`](core/jiant_core.h)). | Works, tested |
| [`mpc-plugin/`](mpc-plugin/) | JIANT FM as an instrument plugin: MPC standalones (MPC OS 3.10.1+), and desktop VST2 hosts. | Builds for ARM32, ARM64 and desktop; passes the SDK's host simulator; not yet tried on an MPC |
| [`juce-plugin/`](juce-plugin/) | JIANT FM as VST3, AU, AUv3 (AUM on the iPad), LV2 and a standalone app, an editor made for a DAW (the screen, the tracks, each sound's presets and EDIT parameters, a sample browser with drag and drop, the samples saved in the session); up to 8 in one session, each its own JIANT. | Builds and passes its host check on Linux; macOS, iOS and Windows built by CI |
| [`mpc-sample/`](mpc-sample/) | JIANT as the MPC Sample's whole instrument, its own screen, the MPC's pads, buttons and knobs as the FM-1's panel, started from the microSD card (on keremimo's sample-base). | Builds for ARM64; runs and passes its tests on Linux; not yet tried on an MPC Sample |
| SMK-37 | JIANT on the SMK-37 Pro / Elite, bare metal like the FM-1 (same AC791N chip, same screen and audio format). | Next |

## Full power on a bigger CPU

The FM-1's chip is small, and JIANT lives within it: when the load runs high it gives way (STRING, WEST and VOICE at
half the rate, FM6 at a quarter, the heavy reverbs as lighter ones, voices faded), all parts share 8 voices, and
EXPLORE keeps its pieces light. The ports run on far bigger CPUs, so the core builds with `FELUCCA_HOST_POWER`
(`firmware/src/core.h`; the FM-1, the browser emulator and the host tests are untouched by it):

- **Every track its own 8 voices** (48 in all), not 8 between all of them.
- **EXPLORE at full size**: no lighter pieces for the FM-1's CPU.
- **The load measured in real time on that CPU** (`web/emu/felucca_web.c` `JIANT_REAL_LOAD`): the firmware's own
  governor and overload guard stay, so the sound gives way only if that CPU itself runs short; on one with room it
  never does. `jiant_cpu_percent()` and friends report it; the MPC Sample app shows it on the screen (CPU, red when
  anything gave way), in `status`, and `jiant --bench` measures the bench song on the MPC's own CPU.

- **Samples** (`firmware/src/smp_slots.c`, the API in [`jiant_core.h`](core/jiant_core.h)): 16 slots of up to 60 s,
  mono or stereo, loaded from the host's PCM at any rate (made and resampled on the host's own thread, handed to the
  audio thread by pointer, freed by `jiant_service` on the host's service thread), recorded from the device's input
  (`jiant_input`), JIANT's master or a track, and played by two engines in the FM-1's retired slots, **SAMPLE** (4:
  one shot with experimental modes and triggers, a feedback comb, DESTROY, REC) and **GRAIN** (8: granular, on a slot
  or LIVE on the input or the master, time-stretch, feedback and shimmer), and by a sample layer on every **DRUM-X**
  sound (SMORPH and its blends). The controls: [`mpc-sample/README.md`](mpc-sample/README.md#samples-sample-grain-and-drum-xs-sample-layer).
  On the FM-1 none of it exists: a project made with them plays SAMPLE and GRAIN as ANALOG there (the retired engines'
  way) and DRUM-X as its synthesized kit (the layer's values sit in slots a DRUM track there never reads).

On this computer (x86) the bench song renders at ~37x real time; on the MPC Sample's Cortex-A55 a share of one of
its four cores, to be measured with `--bench` on the hardware.

## The core

```sh
ports/core/build.sh test      # build/core/libjiant_core.a, then its test
```

One device per library (the firmware keeps its state in globals), not thread safe; see the header. For several in
one process (JIANT FM on several tracks of a DAW), [`jiant_copies.h`](core/jiant_copies.h): the core compiled N times
(`jiant_copy.c`, 8 in the JUCE plugin), each copy one translation unit with every external symbol renamed (the list,
`copy_symbols.h`, is made from the core's object by `ports/core/build.sh symbols` and checked by the test), its API a
table of functions; an instance claims a free copy and gives it back. The copies share nothing, so each may run on its
own thread. Plain C and the preprocessor: no object tools, the same on Linux, Windows (clang-cl) and macOS. The test boots it,
plays a MIDI note, draws the screen, plays a generated song, saves and restores the music, follows an external MIDI
clock, loads and records sample slots. `sample_test` drives the sample engines with the firmware's internals at hand:
every SAMPLE mode and trigger, GRAIN's travel modes, time-stretch, feedback and LIVE input, DRUM-X's blends (and with
SMORPH 0 bit for bit the FM-1's DRUM-X), REC from the master, the bounds at the engines' tops and their cost; then
`fm1_load_test`, the core built as the FM-1 is, loads the project it saved; `copies_test` links 8 copies beside the
single library: copy 0 plays what the single core plays, sample for sample; three copies on three threads at once play
what three others play one after another, bit for bit; one copy's music and samples touch no other's.

## The MPC plugin

```sh
pip install ziglang           # the cross compiler, as inMusic's SDK uses
python3 ports/mpc-plugin/build.py
```

It fetches inMusic's [MPC Plugin SDK](https://github.com/inmusic-software/mpc-plugin-builder-sdk) (MIT) at a pinned
commit into `build/mpc-plugin-sdk`, builds the core and the plugin for the desktop, ARM32 and ARM64, runs the SDK's
host simulator (the MPC's load sequence and its quirks, at 44.1 kHz and at 48 kHz), and assembles the folder the MPC
installs, checked with the SDK's `validate.py`:

```
build/mpc-plugin/Juan Jiant - VST - JIANT FM/      (and a zip of it)
```

On an MPC: Preferences > Enhancements (Beta) > Enable Enhancements and External Plugin Support; copy the folder to
`VST2 Plugins` on a USB drive or SD card, restart, and load JIANT FM on a plugin track.

What the plugin does:

- **MIDI**: channels 1 to 4 play tracks 1 to 4 (GLO > MIDI IN ROUTE CH1-4).
- **Sync Host** (default): the sequencer follows the MPC's transport and tempo (a 24 PPQN clock made from the host's
  beats). **Internal**: its own tempo; **Play** starts and stops it.
- **GENE**: genre, seed, energy and form; **Generate** makes a song.
- **The music is saved in the MPC project**: the live section, the songs and the user presets.
- **Any sample rate**: 44.1 kHz as is (the MPC's), other rates resampled.

Limits, today:

- **No JIANT screen inside an MPC**: MPC plugins show only the MPC's parameter page. The full JIANT, its screen and
  its UI, is the MPC Sample app's job ([`mpc-sample/`](mpc-sample/)), not the plugin's.
- **One JIANT per project**: a second instance stays silent (the firmware is one device).
- Tried in the SDK's host simulator, not yet on MPC hardware.

## Licences

JIANT is GPL-3.0-only. The SDK's framework is MIT and links into the plugin under the GPL; the plugin folder carries
JIANT's `LICENSE` and where its source is. The SMK-37's own SDK library is proprietary: the SMK-37 port is to run bare
metal on JIANT's own HAL, as on the FM-1, not linked with it.
