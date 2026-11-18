# V52: recover kit hits and chords after CPU pressure

Juan reported claps, snares and hats disappearing with two synth tracks, and
chords disappearing when two mono tracks were added. A six-track video labeled
v0.9.9 motivated a comparison against published v0.9.9-par12
(`527c7e462a1542cbbde2c2cbf6b4bf151954127d`). The screenshot does not identify
its suffix, project, engines, polyphony or CPU reading; this is a reproducible
comparison against that historical source, not a reconstruction of the video.

## Same music, same simulated load

The production allocator and renderer run a kit (kick, snare, clap, hat), an
ANALOG tetrad and two mono lines, leaving the remaining channels empty and
unmuted. Each active voice is priced at 700 simulated device ticks per block.
After a 95% transient, the supplied deadline measurements settle at 65% for
about 5.8 seconds. These percentages are injected policy inputs, not physical
FM-1 benchmarks.

| Revision | Before transient: kit/chord/mono/mono | After transient | Shared cap after transient |
| --- | --- | --- | --- |
| v0.9.9-par12 | 4 / 4 / 1 / 1 | 4 / 4 / 1 / 1 | 12 |
| V10.7.51 | 4 / 1 / 1 / 1 | 1 / 1 / 1 / 1 | 6 |
| V10.7.52 | 4 / 4 / 1 / 1 | 4 / 4 / 1 / 1 | 16 |

The same V52 regression passes with TRACKS set to four and six. A separate
case admits eight kit lanes, a tetrad and four mono parts: sixteen voices.

## Causes and correction

V51 retained earned headroom in six-track mode, but `guard_cap` still ratcheted
down to the low point in sounding voices. At 65% it could never recover because
guard release required less than 60%. CPU and voice reservations for empty
channels further prevented kits and chords from refilling.

Healthy two-core FULL now retains a sixteen-slot ceiling in either TRACKS
setting and prices only sounding work plus requested notes. Four-track FULL
uses the same strict predictive admission as six-track mode: new slots must
fit below 78%, fading voices count, and pressure cannot steal another channel
or a held chord. The numerical ceiling does not bypass the CPU check. Existing
adaptive rate reduction starts before that limit and only measured savings
can fund new voices. Conservative startup, LITE, ECO and core-fault restrictions
remain; opening FULL headroom still needs sixteen calm halves.

A second defect divided a smoothed whole-track time by the latest instantaneous
voice count. When eight voices became one, it priced the survivor like most of
the former group. The production timing regression measures this directly:
V51 cost jumps from 446 to 3,186 Q8 units; V52 stays at 446 to 462 (small fixed
track overhead is still charged). V52 normalizes each matching time/count
sample before smoothing, retains valid estimates through silence, and replaces
them on engine changes. Mixed-engine fades do not supply per-voice estimates.
The scheduler still uses its original aggregate track-time estimate.

## Validation and limitations

`policy_recovery_test.c` detects the historical regression; the measured test
exercises actual mixer timing updates with nonzero synthetic per-core ticks,
eight-to-one changes, repeated drums, held chord gates and mono lines. Its
four-channel scene spans 40–77% simulated load and activates reduced-rate
synthesis; its six-channel scene spans 35–64%. Both reject zero requested notes
and retain all checked chord notes. This is evidence about allocation and
recovery, not a measurement of device utilization or a guarantee that every
combination of engines and effects fits the hardware.

The 78% predictive limit remains: a genuinely overloaded new note can still be
refused. Explicit voice limits, intentional MONO/LEG operation, normal drum
decay, and open/closed-hat choke are unchanged. No presets are forcibly changed
to POLY. Physical validation should repeat the reported project, including its
punch-ins, and check the on-device CPU and late-half counters.

Local validation also passes the 5,820-check recovery suite, kit continuity and
six-phrase suites across ten melodic engines, 402-preset overload/FX stress,
audio ISR, channel budgets, keyboard/step chords, emulator boot polyphony,
adaptive-rate phase/seam/hysteresis tests, mailbox ownership and layout packing.
Target guards pass without relaxing limits: image 583,660/584,336 bytes;
RAM data 97,920/98,304; RAM code 32,696/34,816; pool spare 8,704 bytes.
Per-block count/measurement helpers stay in flash within RAM caller reach.
