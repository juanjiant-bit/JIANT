# V10.7.48: voice admission and punch-in headroom

The V47 tester reports were short audio interruptions under load, particularly with six tracks and overlapping punch-ins. V48 keeps the independent DRUM-X kits and reduces work before adding new audio.

## Behavior

- AUTO polyphony now means at most four voices per channel, bounded by the engine's own maximum. Explicit user VOICES settings remain available.
- With six live tracks, each synth has at most two voices and each kit at most three. Lower user/engine caps win. Once the guard engages, measured expensive channels can tighten to one voice. These are runtime limits; saved sound parameters are preserved.
- The existing shared eight-voice budget remains; twelve voices are available only after sustained low total CPU on two healthy cores. Six-track admission reserves one slot for each other unmuted, non-input channel that has not sounded yet. At capacity it reuses a released voice of the same channel or refuses the new note rather than stealing another channel's held note.
- Six-track note prediction closes at 78% of the deadline (ECO stays at 70%). The averaged guard engages at 75%, or three consecutive halves above 82%; recovery starts below 60%. Fading voices still count until their rendering ends.
- Audio punch-ins are limited to two, one at measured average CPU >=70%, and none at >=80%. Mutually exclusive buffer effects occupy one slot. Each newly admitted effect reserves ten percentage points in the predictor while actual measurements catch up. This reserve decays over about 93 ms and cannot be erased by note admission. Stale repeated-note debt is reconciled against sounding work before admitting a new effect, so recovered headroom is usable. A refused/shed press stays refused until released, preventing repeated automatic on/off transitions. Existing gain ramps and release tails still run.
- Mutes and note/sequencer effects do not use audio-punch slots; their voices remain subject to the voice budget. DEC+ only extends tails while the guard is clear and average CPU is below 70%. DEC- shortens synth decay/release and kit decay; FM6 can also shorten native patches with zero envelope offsets. Parameter edits are restored after every render block.

## Default punch layout

| Previous key assignment | V48 assignment |
| --- | --- |
| COMB | DEC+ (decay and release longer) |
| GLITCH | DEC- (decay and release shorter) |
| REV ALL | MUTE 5 |
| RING | MUTE 6 |
| LOOP 5 | REV ALL |

Legacy layouts and explicitly customized keys remain available. Existing UI labels derive from the effect map. A blocked audio effect appears held/waiting rather than active.

## Work removed

THROW's sample loop is skipped while its target and ramp are both zero. The master punch stage skips inactive filter/crush ramp calculations and state clearing on every sample, while preserving their transitions. Settled wet sends bypass ramps and gain multiplies. The reverse/tape/pitch renderer and shared filter helper run from RAM; the once-per-block setup runs from flash. No memory/headroom or call-reach check is relaxed.

## Validation and limits

`tests/six_budget_test.c` checks per-engine AUTO defaults, six-channel reservation, drum budget escapes, measured channel tightening, four-to-six transitions, fading voice accounting, rejected-effect rearming, pending FX cost, the default map, envelope restoration and load transitions. `tests/overload_stress_test.c` exercises every preset with four and six tracks, simultaneous D16/reverse/wobble requests, overload and recovery, under sanitizers. Existing audio ISR, perform, UI, kit persistence and concurrent mixer suites cover their original behavior with explicit voice settings where their fixtures require more than four voices.

`tests/punch_cpu.c` is a reproducible whole-mixer instruction profile and transition hash probe. Host instructions are comparative work counts, not FM-1 CPU percentages. The automatic limits intentionally change dense chords and reduce overlapping effects. Software tests cannot establish glitch-free operation for every combination of USB/Bluetooth traffic, project and hardware timing. The beta still needs an FM-1 audition of six-track projects with long tails, D16/reverse combinations, external MIDI and recovery after overload.

## Recording phase report

A separate tester described instruments starting on bars two or three after recording. `tests/record_phase_test.c` reproduces live overdubs armed on bars 1, 2 and 3 with six equal-length tracks and internal clock: all six clocks stay aligned, and STOP/PLAY returns every clock to step one. The notes remain at their recorded global positions (steps 1, 17, 33). This reproduces the apparent late musical entrances without transport drift. The original tester's recording mode, pattern lengths, FIT/division and clock source are unknown, so the reported case is not conclusively diagnosed and no speculative transport change is made.

## Comparative profile

The same CPU TEST song, 1,024 warmup blocks, 512 settled FX blocks and 256 measured blocks (64 samples each), GCC `-O2` and Callgrind, against V47 `ed52df2`. The host is given spare measured CPU to expose work without emulating the real device's feedback loop. The new voice caps intentionally change dense chords; a third audio effect request is refused.

| Tracks / request | V47 instructions/sample | V48 instructions/sample |
| --- | ---: | ---: |
| 4 / no punch | 2861.63 | 2828.89 |
| 4 / D16 + reverse | 3167.78 | 3028.04 |
| 6 / no punch | 3302.78 | 3060.65 |
| 6 / D16 + reverse | 3609.31 | 3260.73 |
| 6 / D16 + reverse + wobble | 3751.73 | 3360.61 |

Raw results are in `v48-punch-profile.json`. Before changing the default AUTO cap, 20 four-track activation/filter/crush/throw/release traces were sample-identical to V47, isolating the idle-work optimization from the intentional admission changes. Device timing still needs beta validation.

The full generator suite also caught three Drum & Bass mix regressions after reducing kit polyphony: the bass stood 3.4–4.5 dB above the drums. Its existing genre mix trim is reduced by 3 dB; this changes no render cost and keeps the existing mix-balance assertions unchanged.
