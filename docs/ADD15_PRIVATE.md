# ADD15 beta: private test build

Base: public ADDITIVE par10 (`30f05b3`), through its controls/clipboard beta (`42947e9`). Engine 13 stays **ADD**. The SAMPLE engine, bank protocol and automatic sample provisioning are not part of this firmware.

Development source: [codex/add15-private](https://github.com/juanjiant-bit/JIANT/tree/codex/add15-private). The exact published commit and validation run are pinned in `web/PRIVATE_BUILD.json`.

The public firmware and site are frozen. The private deployment copies a previously verified Pages artifact, replaces only `preview/`, and preserves the public installer and firmware byte for byte. The emulator is built separately from the same validated ADD source. Firmware comes from an exact successful candidate-validation run, checked against its source SHA and package SHA-256. The older ADD controls beta remains in the private selector as a rollback option. The browser emulator now runs ADD15 without sampler controls. Its browser saves use a separate key; earlier SAMPLE saves are preserved and are not automatically reinterpreted as ADD.

## Included

- REC + A3: one-level UNDO/REDO; snapshot before arming a take. REC + B3 toggles REC Q; REC + knob 2 adjusts it. REC and PLAY are independent.
- SELECT navigates SEQ pages while armed. PRESET moves the sequence cursor, including global components.
- SEQ + E5 COPY SEQ / SEQ + F5 PASTE SEQ: notes/hits, LEN, timing and components, up to 64 steps, with compatible per-step locks. The destination sound remains; continuous automation is not copied. Pasting supports undo.
- SHIFT component: move notes -63..+63 steps during playback, wrap inside LEN and replace the destination. The stored pattern is unchanged; pass conditions include ALWAYS, FIRST, !FIRST, N:M and !N:M.
- CHORD component: the first note is the root, with the existing chord shapes/voicings, COMP 2 and global/master components; existing mono/kit guards and four-note maximum remain.
- STRING/FDBK from par12: MOVE, velocity-sensitive bloom, speaker motion from SHAPE modulation, amplifier/fuzz behavior, ADSR attack/release, SQUEAL and EBOW presets. ADD is retained; the donor's engine-removal changes were not ported.
- DRUM-X **EDIT > KIT 4 > ACOUST**: 0..100% display (0..127 internally). This is a first synthesized acoustic voicing, not sampled drums or a new physical-model engine. It changes lane-specific pitch-envelope/decay/noise/color targets, adds a small hit variation, and makes brightness depend on velocity. Pitch tuning stays the kit's. It reuses the existing oscillators, filters and voice budget; no new delay buffers or voices.

ACOUSTIC is shared by the section's kit, like the existing kit controls. It is stored in a final optional `DA` project extension and restored with sections/undo. Older projects default to zero. Zero follows the original sound path; all eight lane renders were compared sample-for-sample with the prior ADD baseline.

## Space and performance

The font generator retains the same font faces, sizes, characters, kerning and 4-bit antialiasing, using one horizontal raster phase instead of two. Font tables shrink from 17027 to 10582 bytes; no engine is removed and the firmware flash boundary is not enlarged.

The first validated target image leaves 5352 bytes of application space, with `.data`/`.bss` 91724 / 98304 bytes and 10624 bytes spare in the pool after core-1 stacks. The final normal build is checked again after committing the audio layout.

Restored STRING feedback has a static loop-cost estimate of 220 versus 195 for the older ADD public implementation, matching its par12 donor. This is **not a measurement of whole-device CPU percentage**. CI compares STRING to the donor with the same +10% tolerance; all other DSP functions keep the exact ADD baseline limits. No claim is made that this build increases global polyphony.

## Validation and device trial

Tests cover REC/undo, sequence clipboard, SHIFT/CHORD and project round trips, all STRING and ADD presets, MOVE, all eight acoustic drum lanes at different velocities, all 128 acoustic storage values, legacy project formats and malformed section handling. A fresh/stored-project WASM boot smoke test is included. These do not validate physical boot, flash/cache synchronization or timing on an FM-1.

Keep a backup of public-version projects before testing. Use a copy of a project for the trial. Projects using SHIFT/CHORD action codes may be rejected by the older public firmware; do not overwrite the only compatible copy. Par14 remains withdrawn and its tester incident has no confirmed root cause. Porting features to ADD does not establish that the prior incident is fixed.

On the FM-1, start with one track, confirm REC/PLAY independence and the shortcuts, then test COPY/PASTE and STRING MOVE. Compare ACOUST 0% with 100% on the same DRUM-X pattern and read AUDIO LOAD/PEAK/LATE/SHED before and after adding tracks. Report the exact build label and those readings with any failure.

