# V10.7.47 candidate: independent DRUM-X kits

Base: V10.7.46, `f44f81ce0ebbf6a94f37abbbc99125163e69585f`.
Candidate branch: `codex/drum-kits`. This is not a public release or a claim of hardware validation.

Each of six tracks owns its kit (four also work with TRACKS 4). Presets, edits, sends, modulation, lane mutes,
section FROM, live sound holds and recall use that track's configuration. Existing shared-kit FUNC files load
by copying the old kit to every track. DK v1 stores differing active DRUM kits, within the existing 3840-byte
project slot; a full save is refused before writing. Global group mutes and master FX remain shared.
RAND/S&H streams are now per track: patches are retained, but random variations in older multi-drum projects
can differ. Switching an inactive kit's engine to a synth before saving does not serialize that unused kit.

DRUM parts no longer form one indivisible multicore scheduling group. Their RNG and modulation state are
independent; a zero SEND adds zero to a core-local bus instead of writing a shared scratch sink. The voice
budget is unchanged. This improves possible parallelism, not the total processing capacity without limit.

## Local validation

- Strict JieLi build, **without FELUCCA_REPIN**, passed all existing slot, RAM, pool, LOWBUF and call-reach guards.
  App 583556 / 584336 B; data+bss 97888 / 98304 B; pool spare 8704 B (minimum 8192);
  LOWBUF 32568 / 32768 B; RAM code 32672 / 34816 B; shortest call margin 32958 B.
- Independent-kit test under ASan/bounds: four different presets, isolated edits, UI-selection-independent audio,
  save/load, an actual V46 fixture, malformed/truncated extensions, per-track FROM, held sound/mute/recall,
  and full-project refusal without overwriting the output passed.
- Full UI suite: all passed. The unmodified V46 full UI suite also passed; comparison reported no new failures.
  Old singleton-kit fixtures now explicitly select a DRUM track when testing its persistence.
- GEN suite: all passed. Song-hold suite under ASan/bounds: all passed.
- Multicore suite: 12 scenes in serial/deferred/threaded/chaos modes matched exactly; timeout recovery passed.
  Six distinct kits (HANDS/GLUE/TONE/RAND/S&H and repeated hits) used both cores in all 1024 blocks;
  each mode's output hash was `9281d8c0eeaefa10`.
- 219 golden audio renders: no new differences versus V46, no health/routing failures or crashes.
  Two existing mismatches against the checked-in golden file remain unchanged:
  `song/4track_mix -> 5e7b49a9cd8ec3da`, `slicer/song_gate_stut -> 382e15e98db1154e`.
  Golden references were not rewritten.
- Deterministic Callgrind CPU checks: all 60 sections within the existing budget versus the V45/V46 reference.
  CPU TEST 2833 versus 2831 instructions/sample; six-track CPU TEST 2927 versus 2925 (about +0.1%).
  These are host instruction counts, not measured percentages on the FM-1.
- The emulator save test now navigates PROJECTS > PART SLOTS > USER before arming SAVE. Its old direct
  SAVE gesture fails on both unmodified V46 and this candidate; the corrected gesture saves on both.
- Emulator native build linked and rendered the heavy-song smoke scenario. CI separately builds WebAssembly,
  compares native/Wasm audio, and tests transfers. It uploads a preview artifact without deploying Pages.

## Lossless packaging needed to fit

The uncompressed addition exceeded the firmware slot. The existing boot LZ4 stream now has an outer Huffman
layer, reducing 28237 B to 26586 B. The decoder uses the pool temporarily before audio/DMA start, restores the
original LZ4 bytes, and clears the pool before normal startup. It adds no work to the audio callback and does
not change decoded DSP code or initialized data. No memory limits were relaxed and no feature was removed.

`tests/boot_huff_test.py` passed 210 exact round-trip, truncation, malformed-tree and bound cases under
ASan/UBSan. The real C startup decoders restored all four sections of this build byte-for-byte, with guards
around each output. The existing LZ4 suite's 400 generated cases passed. Layout packing regression passed.

Package SHA-256: `e6f454c5a7cd09cdffbe0e76c51e6f4199d481860c971d2fb161ee6712e7fb01`.
The package is a candidate: hardware boot, four busy kits with live edits/FX, and worst-case total CPU on the
FM-1 still need measurement before a public release. No promise that every heavy arrangement is glitch-free.
