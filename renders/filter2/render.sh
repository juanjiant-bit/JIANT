#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# codex/filter2-mod: the before / after renders of renders/filter2/ (LEEME.txt), through tests/hostsim.c.
#   renders/filter2/render.sh BASE V29
#   BASE: a checkout of a4716cda (V10.7.30) for the *_antes renders; V29: one of e7297140 (V10.7.29) for its MS20.
# Parameter ids (core.h): P_FTYPE 112, P_FCUT 113, P_FRES 114, ANALOG's CUT RES DRIVE (P_E4..E6) 119 120 121; FILTER 2:
# KEY 85, NOISE 86, RATE 87, MODE 88 (SOFT 115, STEP 89, METAL 64, BREAK 38, DUST 12), the part's DRIVE 89; all stored
# 127 - the amount (127: 0 %). NOISE 30 / 70 / 100 %: 89 / 38 / 0. METAL's RATIO x0.5 .. x4: RATE 127 - 16 k - 8.
# BREAK's DIV 1/8 1/16 1/16T 1/32: RATE 111 79 47 15. MOD slot 1: SRC 49, DEST 50, AMT 51 (F.NOISE 26, PITCH 1).
set -e
HERE=$(cd "$(dirname "$0")" && pwd); NEW=$(cd "$HERE/../.." && pwd); BASE=${1:?"usage: $0 BASE V29"}; V29=${2:?"usage: $0 BASE V29"}
T=$(mktemp -d)
for d in "$BASE" "$V29"; do [ -d "$d/build/gen" ] || { mkdir -p "$d/build/gen"; cp -r "$NEW/build/gen/." "$d/build/gen/"; }; done
cc -O2 -w -I"$NEW/build/gen" -I"$NEW/firmware/src" -o "$T/new" "$NEW/tests/hostsim.c" -lm
cc -O2 -w -I"$BASE/build/gen" -I"$BASE/firmware/src" -o "$T/old" "$BASE/tests/hostsim.c" -lm
cc -O2 -w -I"$V29/build/gen" -I"$V29/firmware/src" -o "$T/v29" "$V29/tests/hostsim.c" -lm
RIFF=36,36,48,36,39,36,43,46,36,36,48,36,41,36,43,48
LEAD=60,63,67,72,70,67,63,58,60,63,67,75,74,70,67,63
r() {   # r BIN NAME ENGINE PRESET [VAR=..]: dry (sends 0), MONO
    bin=$1; name=$2; e=$3; pr=$4; shift 4
    env "$@" "$T/$bin" "$e" "$pr" 1 "$T/$name.wav" 0 > /dev/null
}
# joins T/NAME.1 .. NAME.k into HERE/NAME.wav, mono, 0.3 s of silence between
join() {
    out=$1; shift
    python3 - "$HERE/$out.wav" "$@" <<'PY'
import sys, wave, array
o = array.array('h')
for k, f in enumerate(sys.argv[2:]):
    i = wave.open(f); d = array.array('h', i.readframes(i.getnframes())); i.close()
    o.extend((d[n] + d[n + 1]) // 2 for n in range(0, len(d), 2))
    if k + 2 < len(sys.argv): o.extend([0] * 13230)
w = wave.open(sys.argv[1], 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(44100); w.writeframes(o.tobytes()); w.close()
PY
}
# 1. each MODE at NOISE 30 / 70 / 100 % (one after the other), a held note (ANALOG 9, CUT 60 RES 70) and a riff
#    (ANALOG 0, CUT 90 RES 40, the sequencer's LEAD), before (the textures) and now (CUT moved)
for m in soft:115:89 step:89:89 metal:64:71 break:38:79 dust:12:89; do
    n=${m%%:*}; v=${m#*:}; mv=${v%%:*}; rt=${v#*:}; i=0
    for b in old new; do s=antes; [ $b = new ] && s=ahora; nl=""; nr=""
        for z in 89 38 0; do i=$((i + 1))
            r $b h$i 0 9 SECS=2 NOTE=7 PSET=119:60,120:70,88:$mv,87:$rt,86:$z; nl="$nl $T/h$i.wav"
            r $b f$i 0 0 SECS=2 STEPS=$LEAD PSET=119:90,120:40,88:$mv,87:$rt,86:$z; nr="$nr $T/f$i.wav"
        done
        join "01_${n}_nota_30_70_100_$s" $nl
        join "02_${n}_riff_30_70_100_$s" $nr
    done
done
# 2. METAL across its ratios, x0.5 x1 x1.41 x1.5 x2 x2.76 x3 x4, NOISE 70 %: a held note, then a riff (now)
nl=""; nr=""
for k in 0 1 2 3 4 5 6 7; do rt=$((127 - 16 * k - 8))
    r new mh$k 0 9 SECS=2 NOTE=7 PSET=119:60,120:70,88:64,87:$rt,86:38; nl="$nl $T/mh$k.wav"
    r new mr$k 0 0 SECS=2 STEPS=$LEAD PSET=119:90,120:40,88:64,87:$rt,86:38; nr="$nr $T/mr$k.wav"
done
join 03_metal_ratios_nota $nl
join 03_metal_ratios_riff $nr
# 3. F.NOISE on PITCH (FILTER 2's NOISE 0: the filter still): STEP (RATE 40 %), SOFT (RATE 30 %), BREAK (1/16, in time),
#    each a slot F.NOISE -> PITCH, AMT 20 (~ +-3.7 semitones), on the riff (ANALOG 0)
r new p1 0 0 SECS=6 STEPS=$LEAD PSET=88:89,87:76,49:26,50:1,51:20
r new p2 0 0 SECS=6 STEPS=$LEAD PSET=88:115,87:89,49:26,50:1,51:20
r new p3 0 0 SECS=6 STEPS=$LEAD PSET=88:38,87:79,49:26,50:1,51:20
join 04_fnoise_pitch_step "$T/p1.wav"
join 04_fnoise_pitch_soft "$T/p2.wav"
join 04_fnoise_pitch_break "$T/p3.wav"
# 4. LP24's power against V10.7.29's MS20: CUT and RES swept over a held C4 (hostsim SWEEP, ANALOG 9) at DRIVE 50 / 100 %
#    (before, now) and MS20 (V10.7.29); CUT closed 127 -> 10 at DRIVE 100 % (the level kept?); RES 90 -> 127 at DRIVE
#    50 % (self-oscillation now); the FAT BASS riff (ANALOG 14) through each; a part's (PHASE) riff, DRIVE 100 %, RES 100
for b in old new; do s=antes; [ $b = new ] && s=ahora
    r $b x1 0 9 SECS=6 SWEEP=1 PSET=112:6,121:64; join 05_lp24_barrido_drive50_$s "$T/x1.wav"
    r $b x2 0 9 SECS=6 SWEEP=1 PSET=112:6,121:127; join 05_lp24_barrido_drive100_$s "$T/x2.wav"
    r $b x3 0 9 SECS=6 NOTE=7 PSET=112:6,120:40,121:127 PSWEEP=119:127:10; join 06_lp24_cerrando_drive100_$s "$T/x3.wav"
    r $b x4 0 9 SECS=6 NOTE=7 PSET=112:6,119:60,121:64 PSWEEP=120:90:127; join 07_lp24_res_90a127_drive50_$s "$T/x4.wav"
    r $b x5 0 14 SECS=6 STEPS=$RIFF PSET=112:6,119:50,120:75,121:127,5:40; join 08_lp24_fatbass_riff_drive100_$s "$T/x5.wav"
    r $b x6 2 0 SECS=6 STEPS=$RIFF PSET=112:6,113:60,114:100,89:0; join 09_lp24_phase_riff_drive100_$s "$T/x6.wav"
done
r v29 y1 0 9 SECS=6 SWEEP=1 PSET=112:5; join 05_ms20_barrido_v29 "$T/y1.wav"
r v29 y3 0 9 SECS=6 NOTE=7 PSET=112:5,120:40 PSWEEP=119:127:10; join 06_ms20_cerrando_v29 "$T/y3.wav"
r v29 y4 0 9 SECS=6 NOTE=7 PSET=112:5,119:60 PSWEEP=120:90:127; join 07_ms20_res_90a127_v29 "$T/y4.wav"
r v29 y5 0 14 SECS=6 STEPS=$RIFF PSET=112:5,119:50,120:75,5:40; join 08_ms20_fatbass_riff_v29 "$T/y5.wav"
r v29 y6 2 0 SECS=6 STEPS=$RIFF PSET=112:5,113:60,114:100; join 09_ms20_phase_riff_v29 "$T/y6.wav"
rm -rf "$T"
