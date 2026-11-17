# V10.7.46 audio overload validation

Base: `6ebc1b73521b831a8cbfdfcbbe272f2c88d8d63b` (published V10.7.45).

The audio IRQ already measured both render time and total time including nested input/USB work. The voice guard, admission predictor, half-rate recovery and UI load throttling used render time, so sustained total load could fail to engage protection or reopen it too soon. They now use total time; render-only diagnostics remain available.

The core-1 mailbox has one slot. A pending asynchronous USB resampler could be overwritten by a subsequent mixer submission. Submission now refuses a busy mailbox, using the existing serial mixer fallback.

## Checks

- `tests/audio_test.c`: simulated 60% rendering plus 30% nested work engages the guard, keeps it engaged, and prevents the admission predictor from spending occupied CPU time. Existing spike, recovery and DRUM-X checks remain.
- `tests/realtime_regression_test.py`: executes the production submission function; verifies pending USB ownership, subsequent completion, fault handling and job-counter wrap.
- `tests/overload_stress_test.c`: four tracks across all 201 presets, sustained overload/recovery under ASan and UBSan (legacy signed shifts/overflow excluded).
- `tests/anim_test.c`: nested work contributes to animation FREEZE; existing timing and animation regressions retained.
- Serial/deferred/threaded/chaotic core scheduling renders match in all 11 multicore scenes.
- The existing golden suite has the same two mismatches on V45 and V46: `song/4track_mix` -> `5e7b49a9cd8ec3da`, `slicer/song_gate_stut` -> `382e15e98db1154e`. No expected hashes were rewritten. All 219 renders have no new audio, health, routing or crash regressions.
- The existing `micro_test` legacy save-byte assertion and `voice_fairness_test load` final steals assertion also fail identically on the unmodified V45 baseline. These unrelated expectations were not changed.

An experimental bit-exact CLOUD reciprocal passed 19,223,100 arithmetic comparisons but did not demonstrate a lower target cost, so it is excluded from this release. No generator recipe or voice cap was reduced to make a host benchmark pass.

No physical FM-1 was available. Host timings are relative benchmarks, not proof of an on-device CPU percentage or absence of all glitches. Recheck the reported heavy projects, with and without USB, on an FM-1 after installation.
