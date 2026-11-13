#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# codex/filters2: the before / after renders of renders/filtros/ (LEEME.txt), through tests/hostsim.c.
#   renders/filtros/render.sh BASE    BASE: a checkout of the firmware before (e7297140), for the *_antes renders
# Parameter ids (core.h): P_FTYPE 112, P_FCUT 113, P_FRES 114, ANALOG's CUT RES DRIVE (P_E4..E6) 119 120 121,
# FILTER 2: KEY 85, NOISE 86, RATE 87, MODE 88 (SOFT 115, STEP 89, METAL 64, BREAK 38, DUST 12), DRIVE 89; all stored
# 127 - the amount (127: 0 %); ANALOG's KEY: 100 = 0 %, 127 = 21 %, 99 = 100 %.
set -e
HERE=$(cd "$(dirname "$0")" && pwd); NEW=$(cd "$HERE/../.." && pwd); BASE=${1:?"usage: $0 BASE"}
T=$(mktemp -d)
cc -O2 -w -I"$NEW/build/gen" -I"$NEW/firmware/src" -o "$T/new" "$NEW/tests/hostsim.c" -lm
cc -O2 -w -I"$BASE/build/gen" -I"$BASE/firmware/src" -o "$T/old" "$BASE/tests/hostsim.c" -lm
RIFF=36,36,48,36,39,36,43,46,36,36,48,36,41,36,43,48
ARP=36,48,60,72,84,72,60,48
LEAD=60,63,67,72,70,67,63,58,60,63,67,75,74,70,67,63
r() {   # r BIN NAME ENGINE PRESET [VAR=..]: dry (sends 0), mono
    bin=$1; name=$2; e=$3; pr=$4; shift 4
    env "$@" "$T/$bin" "$e" "$pr" 1 "$T/$name.wav" 0 > /dev/null
}
# LP24: CUT and RES swept over a held C4 (hostsim SWEEP), DRIVE 0 / 70 % / 100 %; a part's (PHASE) with DRIVE 100 %
r new 01_lp24_barrido_limpio 0 9 SECS=6 SWEEP=1 PSET=112:6,121:0
r new 02_lp24_barrido_drive70 0 9 SECS=6 SWEEP=1 PSET=112:6,121:89
r new 03_lp24_barrido_drive100 0 9 SECS=6 SWEEP=1 PSET=112:6,121:127
r new 04_lp24_phase_drive100_cut 2 0 SECS=6 STEPS=$RIFF PSET=112:6,114:110,89:0 PSWEEP=113:20:110
# MS20 before (its own filter) and now (LP24 hot, its alias): ANALOG FAT BASS on a riff; PHASE's part filter; and GEN's
# EBM bass as it asks now (LP24, DRIVE 55 %)
for b in old new; do s=antes; [ $b = new ] && s=ahora
    r $b 05_ms20_analog_$s 0 14 SECS=6 STEPS=$RIFF PSET=112:5,119:50,120:75,5:40
    r $b 06_ms20_phase_$s 2 0 SECS=6 STEPS=$RIFF PSET=112:5,113:50,114:100
done
r new 07_ebm_gen_lp24_drive55_ahora 0 14 SECS=6 STEPS=$RIFF PSET=112:6,119:50,120:75,121:70,5:40
# NOISE: SOFT and STEP on CUT, the depth 0 -> 100 % over the render (RATE 30 % / 60 %), then the RATE 0 -> 100 %
r new 08_noise_soft_profundidad_0a100 0 9 SECS=8 NOTE=7 PSET=119:60,120:70,88:115,87:89 PSWEEP=86:127:0
r new 09_noise_soft_rate_0a100 0 9 SECS=8 NOTE=7 PSET=119:60,120:70,88:115,86:25 PSWEEP=87:127:0
r new 10_noise_step_profundidad_0a100 0 9 SECS=8 NOTE=7 PSET=119:60,120:70,88:89,87:51 PSWEEP=86:127:0
r new 11_noise_step_rate_0a100 0 9 SECS=8 NOTE=7 PSET=119:60,120:70,88:89,86:0 PSWEEP=87:127:0
r new 12_noise_soft_phase_lp24 2 0 SECS=6 STEPS=$LEAD PSET=112:6,113:70,114:90,88:115,86:20,87:70
# KEY: an arpeggio C2 .. C6 through a low CUT, KEY 0 % and 100 %; ANALOG 0 / 21 % (as before) / 100 %
r new 13_key_phase_0 2 0 SECS=6 STEPS=$ARP PSET=113:45,114:60,85:127
r new 13_key_phase_100 2 0 SECS=6 STEPS=$ARP PSET=113:45,114:60,85:0
r new 14_key_analog_0 0 9 SECS=6 STEPS=$ARP PSET=119:45,120:60,85:100
r new 14_key_analog_21_como_antes 0 9 SECS=6 STEPS=$ARP PSET=119:45,120:60,85:127
r new 14_key_analog_100 0 9 SECS=6 STEPS=$ARP PSET=119:45,120:60,85:99
# the textures, NOISE 0 -> 100 % over the render: METAL (RATE 50 %), BREAK (RATE 40 %), DUST (RATE 30 %); DUST's RATE
# 0 -> 100 % (crackle to hiss); METAL at a fast RATE
r new 15_metal_profundidad_0a100 0 0 SECS=8 STEPS=$LEAD PSET=119:90,120:40,88:64,87:64 PSWEEP=86:127:0
r new 16_metal_rate_rapido 0 0 SECS=6 STEPS=$LEAD PSET=119:90,120:40,88:64,87:10,86:40
r new 17_break_profundidad_0a100 0 0 SECS=8 STEPS=$LEAD PSET=119:90,120:40,88:38,87:76 PSWEEP=86:127:0
r new 18_dust_profundidad_0a100 0 0 SECS=8 STEPS=$LEAD PSET=119:90,120:60,88:12,87:89 PSWEEP=86:127:0
r new 19_dust_rate_0a100 0 0 SECS=8 STEPS=$LEAD PSET=119:90,120:60,88:12,86:30 PSWEEP=87:127:0
for f in "$T"/*.wav; do   # stereo -> mono (the renders are dry: the two sides the same but for IMAGE)
    python3 - "$f" "$HERE/$(basename "$f")" <<'PY'
import sys, wave, array
i = wave.open(sys.argv[1]); n = i.getnframes(); d = array.array('h', i.readframes(n)); i.close()
m = array.array('h', ((d[k] + d[k + 1]) // 2 for k in range(0, len(d), 2)))
o = wave.open(sys.argv[2], 'wb'); o.setnchannels(1); o.setsampwidth(2); o.setframerate(44100); o.writeframes(m.tobytes()); o.close()
PY
done
rm -rf "$T"
