# 0.9.9 cpu3: performance investigation and four-track allocation

## Reproduced cross-track stealing without chords

Four FM6/VOICE tracks each play one note at a time, with overlapping releases. At capacity, the old POLY allocator could fade another track's oldest released voice even though the requesting track had a released slot of its own. The new note then rendered alongside the fading victim, briefly increasing actual render work beyond the admission count. `voice_fairness_test` reproduces the old result (one cross-track fade) and checks zero unnecessary cross-track fades after the fix, with budgets of both 8 and 12. Each other track retains its held lead and tail. Normal global fallback remains available when the requesting track has no released slot. This is a concrete allocator issue; it does not establish the cause of every hardware dropout.

At capacity, reuse the requesting track's oldest released slot in place before looking elsewhere. The engine's existing retrigger path retains its phases and envelope continuity. Held notes and the original global overload protection retain their protection. Two mixed-song reference hashes intentionally change because tails are kept differently: `song/4track_mix` and `slicer/song_gate_stut`. Only those two entries were revised; 155 existing references are retained.

## Lossless DSP changes

* Chorus: after a full 2048-sample ring of zero input (about 46 ms), all three interpolated taps are provably zero. Skip their calculations while preserving buffer writes, LFO/write positions, parameter glides and all other FX. Resume on any nonzero input; never discard a chorus tail. Four bytes of state. The reference test forces the old taps, moves parameters and LOAD, drains/restarts repeatedly, and compares every output sample and the complete FX state over 12000 blocks.
* VOICE vowels: when TALK's interpolation weight is exactly zero, the second vowel does not contribute. Avoid its formant preparation and interpolation. No reduced sample rate, omitted resonator or change to WORDS synthesis. The WORDS path remains a priority for further work.

## Host measurements

These are GCC 14.2 `-O2` x86-64 diagnostic timings, using thread CPU time and medians of nine interleaved before/after runs. They are NOT FM1 CPU percentages. Binary layout, host scheduling/frequency and scene details affect timings, including unchanged engines. Small differences should not be treated as proven gains. The comparison isolates the DSP edits; fairness is established by separate allocation tests. Every benchmark scene has the same before/after audio checksum. Full audio/state checks are separate tests.

The FX figures are for the whole bus chain with the named input/model, not individual reverb algorithms in isolation. The current chain still processes delay/reverb with silent sends; more aggressive sleeping needs state/tail/freeze/self-oscillation tests and is not included here.

| Scene | Before ns/sample | After ns/sample | Host delta |
|---|---:|---:|---:|
| FX no input | 34.9 | 24.5 | -29.9% |
| FX chorus input | 32.1 | 31.6 | -1.5% |
| FX delay input | 31.5 | 20.5 | -34.8% |
| FX ROOM | 31.6 | 21.4 | -32.2% |
| FX SPRING | 46.1 | 34.0 | -26.2% |
| FX SHIMMER | 41.1 | 30.0 | -26.9% |
| FX RESO | 37.5 | 25.3 | -32.4% |
| FX CLOUD | 47.8 | 35.1 | -26.6% |
| mixer idle | 60.9 | 48.5 | -20.4% |
| six FM6 tracks dry | 339.2 | 313.5 | -7.6% |
| six VOICE tracks dry | 334.7 | 303.7 | -9.3% |

The no-chorus FX shortcut shows a clear reduction of about one third in its isolated chain scenario. Active chorus remains fully processed. A 139-preset survey (three runs each, four requested notes, dry track rendering with periodic retriggers) gives the following most expensive preset observed per engine. This is a controlled pattern, not a worst-case guarantee; drums may have fewer live voices due to decay/choke.

| Engine index | Most expensive preset observed | ns/sample | Live at end |
|---|---|---:|---:|
| 0 | CELLO | 57.3 | 4 |
| 2 | RESO | 54.4 | 4 |
| 3 | FLOAT PAD | 65.2 | 4 |
| 5 | CANTOR ES | 87.3 | 4 |
| 7 | FULL ORGAN | 68.0 | 4 |
| 10 | 808 BOOM | 39.4 | 3 |
| 11 | TAPE AIR | 59.8 | 4 |
| 12 | TINE EP | 84.7 | 4 |
| 14 | ROAR | 49.6 | 4 |

FM6 and VOICE are the leading engine costs in these scenes; some WHEEL/LOFI presets are also significant. SPRING/CLOUD bus-chain scenes cost more than ROOM here. Idle event processing measured about 3.8 ns/sample; full idle mixing was about 61 ns/sample before these edits. This does not measure note-on bursts, UI/IRQ load, USB copies/resampling or flash stalls on the actual FM1. Do not remove MIDI, sequencer, safety or ownership work based on idle timings. Existing measured per-track/FX splitting and deadline/late/shed diagnostics remain essential for hardware diagnosis.

## Four FM operators

The controlled FM6 test uses algorithm 32, identical sustained envelopes, four held notes, and six versus four non-silent operators. Two operator sample loops stop running, but the six-operator envelopes/state/control code remain. It saves roughly a quarter of the measured track render time in this scene, not a quarter of the whole firmware. Outputs/timbres intentionally differ. This is an activity-count experiment, not a completed six-to-four patch converter or a native FM4 mode.

FM6 already avoids sample loops for operators below its amplitude threshold. The survey found presets with one, four and five active operators as well as six, so a blanket four-operator limit would not help every sound. Arbitrarily deleting two operators can remove carriers or whole modulation paths and make patches silent. The current six-operator state remains 6528 bytes even when only four operators sound.

The repository also retains DIGITAL, a separate four-operator engine behind FELUCCA_FM4=1. Its two extra envelope arrays alone require 960 bytes when it coexists with FM6; re-enabling it is not automatically a RAM saving. The audit compiles that flag for feasibility after uploading the default FM6 package. It is research only: the beta continues using FM6 and preserves existing patches. A practical next option is a separately selectable FM4 engine/preset bank with explicit project compatibility, or curated reduced patches, rather than silently replacing every FM6 voice.

## Reproduction

Compile `tests/performance_probe.c` against each source revision using `cc -O2 -w -Ibuild/gen -Ifirmware/src ... -lm`. Copy this new probe to the cpu2 reference checkout first so both builds use the same harness. `tools/profile_performance.py BEFORE AFTER 9` prints the scene comparison. Raw results are in `docs/performance/cpu3-host.jsonl` and `cpu3-presets.jsonl`. No timing thresholds or automatic FM1 utilization conversions are imposed.
