# V10.7.49 candidate: six musical lines before voice shedding

The FM-1 V48 field test reported clean audio but missing notes with two four-note
chord tracks, DRUM-X and three lead/arpeggio tracks. V48 imposed two synth voices
per channel in six-track mode, then shared only eight voices (twelve with measured
headroom). That arrangement cannot fit those limits even when render time is spare.

V49 keeps AUTO at four voices per engine/channel, or the engine's lower maximum.
An explicit user limit remains effective. In FULL mode, sixteen calm audio halves
below 50% on two healthy cores open sixteen shared slots for six tracks (twelve
for four tracks). Six-track extra admission stays open through 70% rather than
closing at 65%. It still closes at 75%, on the existing guard or a failed second
core. LITE, ECO and single-core fallback retain their previous ceilings.

The six-track prediction still refuses growth above 78%, including pending FX
work. Both a slot and predicted CPU are reserved for other unmuted silent channels.
CPU pressure can recycle this channel's released note but cannot replace a held
chord note. A timing guard no longer forcibly reduces the channel cap to one/two
voices. Explicit cap reductions still fade surplus voices, whose CPU stays counted.
The last measured channel cost uses its measured voice count: note-ons queued since
that render must not dilute the per-voice estimate.

## Adaptive rendering

ANALOG and FM6 already supported half-rate rendering. Six-track sustained or
predicted pressure now requests it sooner (two control passes, about 12 ms),
starting with the most expensive eligible channel. Existing release-tail measures
remain. New notes inherit their channel's selected rate on their first complete
render block. ADSR, note events, sequencer, external clock, output device and FX
buses keep their original time base.

WHEEL and ADD now implement native half-rate rendering too. Their partial phase
increments and gain ramps cover the same real time; WHEEL's foldback, rotor and
percussion keep their original physical frequencies/durations. ADD removes
partials above the reduced Nyquist margin. A dedicated modulation flag distinguishes
half-rate synthesis from an ordinary half-length, sample-timed event block.
Interpolation carries the previous voice sample across rate changes. Bandwidth and
timbre can change under pressure; this is a temporary sound-quality tradeoff.

Recovery estimates the additional work of returning the reduced held voices to
full rate. The low CPU reading produced by downsampling alone is insufficient.
Six-track recovery also requires about 0.75 s of calm, preventing rapid rate toggling.
Stored sounds, preset parameters and musical positions are not edited.

This is not unlimited polyphony. Other engines retain their original renderer;
sixteen slots are a ceiling, not a promise that sixteen expensive voices and all FX
will fit. Last-resort overload/FX protections remain. Physical FM-1 testing is still
needed, especially USB/Bluetooth bursts and patches unlike the host fixtures.

## Evidence

`tests/six_phrase_test.c` starts two tetrads, three kit hits and three melodic
lines, then changes phrases. Against the published V48 source it starts only ten
voices and misses 128 of 352 chord/melody note checks. The candidate starts fourteen
and misses zero, without cross-channel fades, with up to sixteen rendered voices.
Tests exercise ANALOG, PHASE, MATH, VOICE, WHEEL, STRING, WEST, FM6, ADD and ANIMAL.
The load supplied by these fixtures is simulated; it is not a device CPU reading.

`tests/adaptive_rate_test.c` checks WHEEL/ADD partial phase and percussion/decay
age, partial event blocks, rate-change seams on steady tones, early pressure
response, stable recovery, held-chord preservation and measured-cost reservations.
The existing six-budget, audio ISR, generator and 402-preset overload tests remain.
The GEN mix-balance fixture now explicitly supplies the healthy two-core admission
state it intends to measure: the host has no audio ISR clock to open it. Its balance
assertions are unchanged. The concurrent mixer suite includes adaptive-rate
transitions in serial, deferred, threaded and disturbed scheduling.

Host Callgrind comparisons use the same 512 rendered blocks of the complete six
track fixture, five synth tracks of the named engine plus DRUM-X. The adaptive run
feeds 68% simulated load to exercise quality changes; the full-rate run feeds 40%.
Both retain every checked note. See `v49-adaptive-profile.json` for instruction
counts. They compare host work, not FM-1 deadline utilization.

The audio layout is regenerated because DSP code changed. The target build keeps
the existing app, RAM, pool, call-reach and minimum RAM-code headroom checks. New
note-admission helpers use size optimization; sample render loops keep their
existing optimization policy.
