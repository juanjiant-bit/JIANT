**JIANT FM beyond the FM-1: preview.** The same firmware as the FM-1, now on more machines. Free and GPL-3.0.
**Nothing here has been tried on real hardware yet: we are looking for testers.**

| File | What it is |
| --- | --- |
| `JIANT-FM-MPC-Sample-*.zip` | JIANT FM as the whole instrument of the **Akai MPC Sample**, started from the microSD card (README inside). |
| `JIANT-FM-MPC-plugin-*.zip` | JIANT FM as a plugin for the **MPC standalones** (MPC OS 3.10.1+, Enhancements > External Plugin Support) and desktop VST2 hosts. |
| `JIANT-FM-Linux-*.zip` | **VST3, LV2 and a standalone app** for Linux. |
| `JIANT-FM-Windows-*.zip` | **VST3 and a standalone app** for Windows. |

**New on the big CPUs** (MPC Sample, plugins), not on the FM-1:

- **Every track has its own 8 voices**, and the CPU limits are lifted.
- **SAMPLE**: a one-shot sampler with experimental modes (GATE, LOOP, PING, REV, SLICE, SCRUB, TAPE), trigger modes
  (CHOKE, RATCHET, RANDOM, ROUND, VEL, LEGATO, CHANCE), a feedback comb, DESTROY, and resampling.
- **GRAIN**: granular synthesis with travel modes (FWD, REV, PING, RANDOM, STRETCH, FREEZE, KEY), time-stretch,
  feedback with shimmer, LIVE mode on the input or the master, and resampling.
- **DRUM-X**: a sample layer on every lane, with a MORPH from synth to sample.
- **16 sample slots** with recording, and a sample browser on the MPC Sample's SAMPLE button.

**Try it in the browser first:** https://juanjiant-bit.github.io/JIANT/emu/devices.html. It has the FM-1, SMK-37 and
MPC Sample panels; the MPC panel has the sample engines, and you can drop your own WAVs on the page.

**Not in this release:**

- The **M-VAVE SMK-37** firmware is not in this release: it is built from source (`docs/SMK37.md`) until someone
  tries it on a unit.
- **macOS and iOS** (AU, AUv3) builds come later.

Problems and test reports: [Issues](https://github.com/juanjiant-bit/JIANT/issues). Sources: [`ports/`](https://github.com/juanjiant-bit/JIANT/tree/main/ports).

---

**JIANT FM más allá de la FM-1: preview.** El mismo firmware de la FM-1, ahora en más aparatos. Gratis, GPL-3.0.
**Todavía no se probó en hardware real: buscamos testers.**

- **MPC Sample:** el zip va a la microSD; adentro está el README.
- **MPC standalone:** el plugin.
- **Computadora:** VST3 y app standalone para Linux y Windows.
- **Motores nuevos en estos aparatos:** SAMPLE (one-shot experimental, feedback, resample, triggers complejos),
  GRAIN (granular con recorridos, stretch, feedback y modo LIVE) y DRUM-X con morph entre síntesis y samples.
- **Probalo antes en el navegador:** https://juanjiant-bit.github.io/JIANT/emu/devices.html
- **SMK-37:** todavía sin paquete, se compila desde el código.
- **Mac e iOS:** más adelante.
