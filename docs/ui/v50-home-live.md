# V10.7.50: selected-engine HOME and GLOBAL LIVE

HOME opens ENGINE. K1 is the selected channel's filter. K2–K4 are timbre controls from the engine's quick-control mapping. ANALOG uses FILTER, RES, WAVE and DETUNE; the other engines use their part filter plus their first three quick controls. ALGORITHM selects the channel. The controls share parameter IDs with step locks, MIDI learn/output and recorded automation. EDIT-held momentary changes return to their original values.

The picture now belongs to the selected engine, including in LARGE mode. MACROS remain on the existing LFO hold/latch layer: hold LFO for M1–M4, release to latch, press LFO again to leave. Existing macro values, routes and stored scenes are preserved. HOME + SELECT consistently opens GEN; the macro layer retains its own dice controls.

GLOBAL keeps MIXER as its initial page. Its LIVE group now starts with LIVE, followed by KEY FX:

- LIVE: TARGET, SPEED, LOOP and DENSITY; selected tracks are marked beside their timelines. ORIGINAL, MODIFIED and BYPASS distinguish performance state.
- OCT− resets the target on the next beat; holding OCT− temporarily hears the original. OCT+ opens the existing write confirmation. Reset remains reversible performance work; writing is explicit.
- KEY FX retains the live keyboard performance map, rates, loops, reverse, ping-pong, ratchets, thinning and octave changes. SELECT moves between LIVE and KEY FX. Keys play notes normally outside KEY FX.

The existing ghost playhead preserves the original phase and recording position. LIVE settings stay temporary unless explicitly written. V49 adaptive sample rate, voice admission and independent kits are retained.

## Resource and layout considerations

The six-being HOME scene is no longer rendered. With LIFE compiled out, TRACKS updates its peak meters without computing swimming, faces or colors. The old UI storage is retained explicitly to avoid letting removal of the scene radically change RAM addressing. Chorus admission (one check per block) moves from hot RAM to reachable flash; the chorus sample loop stays in RAM. No voice/FX admission threshold is changed.

Removing unused UI code and changing the compiler's UI-global representation changes compiled addressing and code sizes. `audio_layout.txt` is regenerated with `FELUCCA_REPIN=keep`, preserving function order, then checked again by a normal build. Slot limits, RAM headroom, call reach, ISR checks and other build guards remain unchanged.

## Validation

- Full host UI regression, including navigation, macro latch, recording and persistence.
- Dedicated HOME test across every available engine and all six selected channels: distinct controls, isolation, correct lock IDs, recorded cutoff, momentary return and LFO access.
- LIVE regression: beat/step quantization, original phase, return/bypass, write/undo, recording at the ghost, track targets and keyboard ownership.
- Animation tests cover the new selected-engine HOME under load, UI freeze/recovery and unchanged-frame drawing. Frame-stepped engine pictures use their existing timing expectations; the retired swimming-scene assertions are replaced.
- Screen lint compared with unmodified V49: no new findings; existing unrelated GEN and contrast findings remain. Alignment checks have no deviations of 1 pixel or more.

Physical FM-1 listening remains necessary to assess the candidate's six-track load and performance combinations.
