# V50: complete kit with five melodic tracks

Juan reproduced missing important DRUM-X hits on the V10.7.49 beta. The previous
six-track fixture checked chord and melody survival but not every kit onset.
The new fixture explicitly checks a complete eight-lane kit, a four-note chord,
and four mono lines (arpeggio, bass and two arrangements): sixteen slots total.

## Reproduced causes and correction

- AUTO incorrectly applied the four-voice synth default to DRUM-X too. Kick,
  snare and clap left just one slot for five other kit sounds. The old code
  missed four kit notes in the initial fixture despite spare simulated CPU.
  Synth AUTO stays at four; kit AUTO now allows eight. Explicit kit caps remain.
- A sample-timed retrigger keeps the old lane in `s[0]` until its render position.
  Allocation protected the old sound rather than the pending sound. A burst on
  a four-slot kit reproducibly lost its pending snare. Core-hit protection now
  reads the pending note when appropriate. Lane reuse also recognizes pending
  alternate GM notes and rejects stale ownership of another intended lane.
- With an explicitly tiny kit cap, an auxiliary hit could fall back to slot zero
  even when it contained a protected core hit. That fallback now refuses the
  auxiliary hit; it does not exceed an explicit user limit.

Six-track admission now requests half-rate synthesis of the most costly eligible
channel as soon as the predicted burst passes the existing early threshold (63%).
The first complete render block can apply it, instead of waiting for the periodic
control passes. No speculative saving is subtracted from the admission estimate.
The device must actually measure cheaper work before that saving permits growth.
Existing interpolation, rate recovery hysteresis and engine time bases remain.

DRUM-X already synthesizes at half rate internally (roughly 22 kHz). Applying the
generic half-rate wrapper again would change its envelope/filter/pitch behavior;
this change does not do that. ANALOG, FM6, WHEEL and ADD retain their existing
engine-aware adaptive rate paths. Other engines have no newly invented rate mode.

## Evidence

`tests/kit_continuity_test.c` reproduces five failures against the pre-fix code:
four missing kit lanes and one pending snare. The corrected implementation passes
with ten synth engines (ANALOG, PHASE, MATH, VOICE, WHEEL, STRING, WEST, FM6, ADD,
ANIMAL). Per engine, 64 phrases retrigger all eight kit lanes (512 onsets), change
the four-note chord and four mono lines, and check held notes through 1,024 audio
blocks. It includes off-grid starts, post-render core hits, alternate GM hats,
explicit small caps and refusal above the CPU ceiling. Peak is sixteen voices;
no missing checked notes or guard fades. ASan and UBSan pass with the repository's
existing shift/signed-overflow exclusions. This is simulated 68% load, not a
measurement of the hardware deadline. Deliberate hat choke and same-lane
retrigger remain part of the instrument.

Audio ISR, six-track budget, adaptive-rate, independent-kit persistence, GEN and
402-preset overload/recovery checks pass locally. Target packaging keeps all
existing guards: image 583,404/584,336 bytes; RAM data 97,904/98,304; RAM code
32,736/34,816 with 2,080 bytes headroom; pool spare 8,704 bytes. The source workflow
also builds and compares the emulator against native audio.

## Measured host work

Raw counts are in `v50-kit-profile.json`. GCC 14 `-O2`, x86-64, Callgrind 3.24.0.
These numbers rank work in these fixtures; they are **not FM-1 CPU percentages**.
DSP instructions, cache, memory contention, nested interrupts and the two-core
schedule differ on the FM-1. Rendering and UI counts must not be added as if they
were components of a single device percentage.

Dry channel work, default preset, four held notes (kit: eight lanes retriggered
every 16 blocks), 16,384 rendered samples. Actual voice counts are checked in the
profile output rather than assuming requested notes were admitted:

| Channel | Host instructions/output sample |
|---|---:|
| ANIMAL | 130 |
| STRING | 502 |
| WEST | 569 |
| ANALOG | 629 |
| PHASE | 642 |
| ADD | 644 |
| MATH | 702 |
| WHEEL | 771 |
| VOICE | 803 |
| FM6 | 863 |
| DRUM-X, **eight** lanes | 1,011 |

These are preset-specific, include channel envelopes/modulation/mixing, and do
not establish a worst-case ordering of engines. ANIMAL's quiet/held source in
particular is not representative of every mode. The existing heavier reference
profiles are retained separately in the JSON, with one important caveat: their
historic eight-note synth requests currently resolve to four AUTO voices.

The full sixteen-slot musical fixture rendered 65,536 samples with every checked
note preserved. Comparing adaptive rendering to full-rate rendering of this same
candidate (not to V49) reduces total host instruction work by:

| Five melodic tracks use | Reduction |
|---|---:|
| ANALOG | 14.29% |
| WHEEL | 18.77% |
| FM6 | 13.01% |
| ADD | 13.24% |

The reference whole-mixer profiler also measures delay rates/drives, chorus,
all five reverbs, master EQ/filter, plain/heavy sequences, LIVE, idle and CPU TEST.
Before this correction, sequencer-only work was 81 instructions/sample (plain)
and 91 with LIVE modifiers; idle mixer work was 168; CPU TEST with six tracks
was 2,898. These fixtures differ from the isolated channel fixture. Reverb/delay
sections include a pad and the mixer, so they must not be presented as effect-only
costs. Cheaper delay sample rates are not automatically cheaper in the host:
resampling and transitions also cost work.

The UI profiler covers HOME engine, six-track meters, timeline, piano roll,
FM6 operators, kit animation and GEN at 15/50/100 ms intervals. A representative
native-host run measured about 70 us/frame for HOME, 17 for six-track meters,
21 for timeline and 157 for the kit animation at 15 ms. The slower drawing modes
reduce calls per second; per-frame work does not always fall, because larger time
steps can invalidate more screen regions. UI timings include host drawing hooks
and a simulated LCD, not real SPI traffic. The piano-roll scene's unchanged
playhead sends no new pixels and is not evidence of scrolling cost.

## Reproduce

With generated headers and Valgrind installed:

```sh
cc -O2 -w -Wno-implicit-function-declaration -Itests -Ibuild/gen -Ifirmware/src tests/system_cpu_profile.c -lm -o build/host/system-cpu
valgrind --tool=callgrind --instr-atstart=no --callgrind-out-file=build/channels.out build/host/system-cpu
valgrind --tool=callgrind --instr-atstart=no --callgrind-out-file=build/reference.out build/host/system-cpu reference
cc -O2 -w -Wno-implicit-function-declaration -Itests -Ibuild/gen -Ifirmware/src tests/ui_cpu_profile.c -lm -o build/host/ui-cpu
valgrind --tool=callgrind --instr-atstart=no --callgrind-out-file=build/ui.out build/host/ui-cpu
python3 tools/read_cpu_profile.py 'build/channels.out*' --units 16384
python3 tools/read_cpu_profile.py 'build/ui.out*' --units 60
```

For the musical comparison, compile `tests/kit_continuity_test.c` with
`-DKIT_CALLGRIND` and run engine IDs 0, 7, 12, 13 with pressure argument 40 versus
68. Parse with 65,536 samples. Keep identical compilers and fixture versions.

## Remaining physical validation

Sixteen is still a shared slot ceiling, not unlimited overlapping release tails.
Four-note synth AUTO means a new four-note chord reuses released slots on that
channel; truly independent old/new tetrads require more slots and more work.
Admission may still refuse growth above its measured 78% ceiling, or without
healthy two-core headroom. The overload fallback remains. This patch does not
claim every possible patch/FX/USB combination can preserve every note.

The target measures channel work (`mc_tk`/`mc_nv`), FX work (`mc_fx_tk`), nested
input work and deadline overruns already. The serial diagnostics expose late
halves, guard trips, shedding and second-core status. Exact per-component device
percentages, USB/Bluetooth bursts, actual display bus contention and the user's
saved project still require an FM-1 run. Confirm the full arrangement with normal
playing and punch-ins before calling the candidate stable publicly.
