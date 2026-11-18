# V51: DRUM-X plus one ANALOG track

The reported case is only two audible channels: DRUM-X loses claps, snares and
hats beside ANALOG; chords were also reported unavailable. The device's CPU
reading and the synth's POLY/MONO/LEG setting were not available. This regression
reproduces a software cause without assuming those missing observations.

## Reproduction

`tests/two_track_admission_test.c` uses the production `audio.c` admission path
and simulated deadline measurements. It earns headroom, sounds a complete kit
plus a mono line or four-note chord, applies temporary overload, then returns
to 55% measured load. Only two channels sound, with TRACKS configured as either
four or six. Unused tracks remain unmuted, matching a normal empty project.

V50 clears `voice_headroom` under pressure but can restore it only below 50%.
The overload guard can recover completely while the shared ceiling stays at
eight indefinitely. Six-track admission also reserves slots for unused channels,
so the kit and chord can effectively be restricted to four sounding slots.
With the old implementation, this regression fails 2,584 of 5,820 checks.

## Correction and boundaries

Six-track FULL mode retains its earned ceiling while both cores are healthy.
The existing overload guard still restricts growth to the sounding work and
restores capacity gradually; its thresholds and the strict 78% predictive
admission ceiling are unchanged. There is no speculative downsampling credit.

Opening headroom requires sixteen consecutive whole deadlines below 60%, both
instantaneously and on average, with no active guard. This permits recovery at
moderate load and avoids a cold average hiding an already busy deadline.
Four-track legacy allocation retains its previous 65% closing threshold and
overload closure because that allocator still has hard-note admission bypasses;
only its reopening threshold changes. LITE, ECO and single-core restrictions
remain. Core failure clears earned headroom.

The corrected two-track regression passes all 5,820 checks, including ANALOG in
every available channel, MONO and POLY, sixteen repeated phrases per case,
both note-on orders, off-grid starts, post-render kit hits and sustained chord
notes. No recovery steals/fades occur.
`tests/chord_entry_test.c` also selects POLY through the actual VOICES knob in
each of the six channels and verifies four keyboard voices and four recorded
step notes beside all eight kit lanes, after engine switching has completed. The same test passes ASan/UBSan with the
repository's existing shift/signed-overflow exclusions. Overload still blocks
growth in six-track mode. Audio ISR and 402-preset overload/FX tests also pass.

These are host simulations, not a measurement of the physical FM-1's CPU or
proof that every possible project fits its deadline. Explicit small voice caps,
MONO/LEG presets, natural drum decay and intentional open-hat choke still apply.
No synth preset's intentional voice mode is changed by this patch.

Target build: image 583,416/584,336 bytes; RAM data 97,904/98,304;
RAM code 32,736/34,816; pool spare 8,704 bytes. All existing build guards pass.
