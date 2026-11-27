# JIANT beyond the FM-1

JIANT FM is meant to run whole, on its own, on more than one machine: the M-VAVE FM-1, the M-VAVE SMK-37 Pro / Elite
and the Akai MPC Sample. Every one of them plays the same firmware sources (`firmware/src`, here V10.8.48), so a song
sounds the same everywhere.

```
                        firmware/src  (engines, sequencer, songs, generator, FX, UI, 240 x 240 screen)
                                  |
          +-----------------------+------------------------+
          |                                                |
   firmware/hal  (bare metal, JieLi AC79)           ports/core  (one C library, any CPU)
          |                                                |
   +------+-------+                              +---------+---------+
   |              |                              |                   |
 FM-1     SMK-37 Pro / Elite              web/emu (browser)    MPC Sample app
```

| Port | What it is | State |
| --- | --- | --- |
| [`core/`](core/) | The whole firmware as one C library: boot, audio, MIDI, the panel, the screen, the music saved and restored, the sample slots. The browser emulator's host layer behind a stable API ([`jiant_core.h`](core/jiant_core.h)). | Works, tested |
| [`mpc-sample/`](mpc-sample/) | JIANT FM V10.8.48 as the MPC Sample's whole instrument, its own screen, the MPC's pads, buttons and knobs as the FM-1's panel, started from the microSD card (on keremimo's sample-base). | Builds for ARM64 (CI); runs and passes its tests on Linux; not yet tried on an MPC Sample |
| SMK-37 | JIANT on the SMK-37 Pro / Elite, bare metal like the FM-1 (`./build.sh --board smk37 --model pro\|elite`, [docs/SMK37.md](../docs/SMK37.md)). | Builds |

The plugins (an MPC plugin, VST3 / AU / LV2) are not part of this line: they are sold separately.

## Full power on a bigger CPU

The FM-1's chip is small, and JIANT lives within it: when the load runs high it gives way (STRING, WEST and VOICE at
half the rate, the heavy reverbs as lighter ones, voices faded), the parts share a budget of voices (the ones not
playing chords a line of two), and GEN keeps its pieces within the FM-1's budget. The ports run on far bigger CPUs, so the core builds with `FELUCCA_HOST_POWER`
(`firmware/src/core.h`; the FM-1, the SMK-37, the browser emulator and the host tests are untouched by it):

- **Every track its own 8 voices** (48 in all, `voice.c` `voice_cap0`), none of them a line of two (VPOL off).
- **GEN at full size**: no lighter pieces for the FM-1's CPU (`gen.c` `gcpu_fit` not run).
- **The load measured in real time on that CPU** (`web/emu/felucca_web.c` `JIANT_REAL_LOAD`; each part's own time,
  `fx.c` `MC_TICKS`, for the admission of new voices): the firmware's own overload guard and half rate stay, so the
  sound gives way only if that CPU itself runs short; on one with room it never does. `jiant_cpu_percent()` and friends report it; the MPC Sample app shows it on the screen (CPU, red when
  anything gave way), in `status`, and `jiant --bench` measures the bench song on the MPC's own CPU.

- **Samples** (`firmware/src/smp_slots.c`, the API in [`jiant_core.h`](core/jiant_core.h)): 16 slots of up to 60 s,
  mono or stereo, loaded from the host's PCM at any rate (made and resampled on the host's own thread, handed to the
  audio thread by pointer, freed by `jiant_service` on the host's service thread), recorded from the device's input
  (`jiant_input`), JIANT's master or a track, and played by two engines in the FM-1's retired numbers, **SAMPLE** (4:
  one shot with experimental modes and triggers, a feedback comb, DESTROY, REC) and **GRAIN** (9, PHYS's: GRAIN's own
  8 is STRING's on V10.8: granular, on a slot or LIVE on the input or the master, time-stretch, feedback and shimmer),
  and by a sample layer on every **DRUM-X** sound (SMORPH and its blends). Their values beyond EDIT's eight sit in slots
  a track of theirs never reads otherwise (SAMPLE and GRAIN: SWARM's, `eng_input.c` `SW_SLOT`; DRUM-X's layer: WHEEL's
  ROTOR, DRIFT and MORPH, which a kit never reads). The controls: [`mpc-sample/README.md`](mpc-sample/README.md#samples-sample-grain-and-drum-xs-sample-layer).
  On the FM-1 and the SMK-37 none of it exists (their binaries are byte for byte as without it): a project made with
  them plays SAMPLE and GRAIN as ANALOG there (the retired engines' way, their values kept) and DRUM-X as its
  synthesized kit.

On an x86 computer the bench song renders at ~50x real time (`jiant --bench`: 2 % of one core); 48 SAMPLE voices
(LOOP, the comb and DESTROY on a stereo slot) at ~5x, 48 of GRAIN's densest clouds at ~0.8x (`sample_test`). On the
MPC Sample's Cortex-A55 a share of one of its four cores, to be measured with `--bench` on the hardware (CI runs it on
ARM64).

## The core

```sh
ports/core/build.sh test      # build/core/libjiant_core.a, then its test
```

One device per library (the firmware keeps its state in globals), not thread safe; see the header. For several in
one process (JIANT FM on several tracks of a DAW), [`jiant_copies.h`](core/jiant_copies.h): the core compiled N times
(`jiant_copy.c`, 8 for a plugin), each copy one translation unit with every external symbol renamed (the list,
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

## Licences

JIANT is GPL-3.0-only. The MPC Sample app runs on sample-base and JUCE (AGPL-3.0): it is conveyed as a whole under the
AGPLv3 ([`mpc-sample/README.md`](mpc-sample/README.md#licences)). The SMK-37's own SDK library is proprietary: the SMK-37 port is to run bare
metal on JIANT's own HAL, as on the FM-1, not linked with it.
