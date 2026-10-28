# SHIFT and CHORD: development branch

Experimental SAMPLE beta, available only in the beta installer as SAMPLE + CONTROLS + STEP COMPONENTS / par14-beta2. Target image capacity, RAM and software checks pass; physical FM-1 testing is still pending.

## SHIFT

- Values -63..+63 track steps; 0 leaves the event in place. The playhead does not jump.
- Replaces the destination event at playback time, removes the original event there, preserves the saved pattern. Wraps inside LEN. If several moved sources collide, the later source in score order wins.
- Both slots add their shifts. LENGTH also moves the following events in its range.
- Pass conditions: ALWAYS, FIRST, !FIRST, N:M and !N:M. Future-dependent PRE/NEI/FILL and random conditions are intentionally not offered for this action, since anticipation has to be decided before its source is reached.
- The mapping is prepared once per pass, with a separate lookahead map for microtiming at the next loop boundary. Edits while playing take effect on the next pass. Automation/locks retain destination grid positions.
- SHIFT is a local component. The global lane retains its existing MONO/POLY behavior for other components.

## CHORD

- Uses the existing chord builder on the event's first note, selecting TRIAD, 7TH, ADD9, SUS2, SUS4, MAJ, MIN, 7, MAJ7, MIN7 or POWER.
- Uses the track's scale and voicing; does not change the track's CHORD setting or saved notes.
- Both local component slots and the global/master component lane support CHORD. Global first, local over it in POLY; MONO suppresses local actions. A later CHORD selects its shape over the previous shape rather than adding another chord.
- Existing LENGTH, conditions, ratchet, transpose and arpeggiation remain in the component pipeline.
- Keeps the generator's mono/kit guards and four-note maximum. This does not increase hardware polyphony.

## Validation

Host regressions cover positive and negative displacement on the first pass, replacement, wrap, N:M, C2, addition of shifts, all chord shapes, C2 ELSE, global POLY/MONO, mono voices and project save/load. These pass locally and in CI. The target image, RAM, DSP budgets and timing comparison against the exact base also pass. Physical FM-1 tests are still pending. The beta uses 584192 of 584336 image bytes (144 bytes free), with 90508 bytes of .data + .bss; it does not add engines or increase polyphony.

New panel controls: REC does not start PLAY; SELECT pages through SEQ while armed; PRESET moves steps in SEQ and both global COMPS pages. Public firmware retains yesterday's behavior.
