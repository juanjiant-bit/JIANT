# FELUCCA TONIC — Auditoría (Fase 0, en curso)

Base: Felucca 1.1.5.1 (`README.md:8`). Este documento junta lo comprobado en el código para
FELUCCA-TONIC-SPEC.md. Cada dato cita archivo y línea. Lo marcado **[ESTIMADO]** sale de una compilación
de host (x86-64, `-Os`, `tests/hostsim.c`), no del chip: sirve para comparar módulos entre sí, no como
valor absoluto. Los números exactos se completan cuando el toolchain de JieLi esté disponible.

## 1. Decisiones tomadas

| Decisión | Fuente |
|---|---|
| DRUM se reemplaza por DRUM-X, construido sobre `drum_voice.c` / `eng_drum.c` (mismo número de engine, 10) | Juan |
| Se sacan los slots de samples de usuario (libera flash de datos para canciones) | Juan |
| Song mode: se adopta el modelo de SLOOP (sección/variación = proyecto completo, arranger por compases, live sections, quick chain, SONG REC) | Juan |
| SLICER por track (GATE/STUT) candidato a salir, reemplazado por punch-in FX MIDI secuenciados en lanes | Juan (propuesto) |

## 2. Supuestos [VERIFICAR]

| Supuesto | Resultado | Dónde |
|---|---|---|
| Versión de Felucca | 1.1.5.1 | `README.md:8` |
| Tracks / voces / pasos | 4 tracks, 8 voces compartidas, 64 pasos | `firmware/src/core.h:11-14` |
| FPU | El DSP es todo punto fijo (Q15); DRUM-X va en punto fijo | `firmware/src/dsp.c:3` |
| Proyectos en Felucca | 4 slots, 3648 B (FUN9) en un sector de 4 KB, copia A/B | `project.c:128`, `storage.c:29,69` |
| Patterns A–D | No existen como variaciones: la song encadena los 4 proyectos (16 filas, repeat 1–16) | `song_chain.c:3`, `core.h:256-261` |
| SLOOP tiene 4 proyectos | **Refutado**: sus secciones A–D son los 4 slots de proyecto; hay una sola canción | `sloop-fm1: arranger_scene.c:6-8`, `SLOOP.md` (Song mode) |
| Automatización | Un solo store de 64 eventos para los 4 tracks del proyecto, resolución de 1 paso | `motion.c:11`, `core.h:246-255` |
| Mutes | Por track (teclas negras en la capa FX), no se guardan; no hay mute por lane | `perform.c:7,295` |
| Matriz de modulación | 4 slots por track, 8 fuentes, 20 destinos | `web/EDITOR_PROTOCOL.md` (MOD) |
| Límite del app | 0x8DFBC B (~568 KB) | `tools/fm1pkg_make.py:30`, `firmware/app.ld` |
| RAM | 96 KB `.data/.bss` + pool 336 KB + noinit | `firmware/app.ld` |
| Tamaño real del binario y CPU en el chip | **Pendiente**: requiere toolchain | — |

## 3. Mapa de flash (datos)

| Rango | Uso hoy | Fuente |
|---|---|---|
| `0x97000–0x9EFFF` | 4 proyectos × A/B (8 sectores) | `storage.c:20-21,69` |
| `0x9F000` | FM6 de user presets, copia A | `storage.c:22,64` |
| `0xA0000–0xDBFFF` | 3 slots de samples de usuario (240 KB) | `eng_sample.c:46`, `storage.c:21` |
| `0xDC000–0xDFFFF` | Bancos de user presets | `storage.c:68` |
| `0xE0000–0xE4FFF` | Staging del loader OTA | `fm1_flash.h:42-43` |
| `0xE5000–0xE6FFF` | Autosave A/B | `fm1_flash.h:44-45` |
| `0xE7000–0xFBFFF` | Libre (~84 KB). **[VERIFICAR]** el firmware oficial hace su staging de update en esta zona; SLOOP usa parte para su banco FM6 | `storage.c:23-24` |
| `0xFC000–0xFEFFF` | Settings, FM6 de user presets copia B | `storage.c:62-64` |

## 4. Song mode de SLOOP (sloop-fm1 2.4.1, GPL-3.0-only)

| Pieza | Qué hace | Dónde (sloop-fm1) |
|---|---|---|
| Sección | Un proyecto completo (~3,8 KB, FUN5), 4 en RAM `.noinit` | `firmware/src/project.c:108` |
| Arranger | Módulo autocontenido: reloj de compases (muestras × BPM, conserva el resto), 16 entradas `{sección, compases 1–64}` + loop | `firmware/src/arranger.h` |
| Cambio de sección | En el borde de bloque: aplica el proyecto y reinicia los tracks al paso 0, en el compás | `arranger_scene.c:34`, `seq.c:1641` |
| Live sections | SAVE + teclas 1–4 saltan en el próximo compás; 5–8 guardan el loop en A–D | `seq.c:1586`, `ui_layers.c:360` |
| Quick chain | SAVE + varias secciones (hasta 8), en loop; cada una dura su patrón más largo | `seq.c:1592`, `arranger_scene.c:11` |
| SONG REC | Graba sección + compases tocados; más de 64 compases sigue en otra entrada | `seq.c:1599-1678` |
| Song backup | Tocar la canción aparta el loop y lo devuelve al parar | `arranger_scene.c:52` |
| Guardado | Secciones a RAM al instante; a flash con el transporte parado (borrar corta el audio ~50 ms); la canción va dentro de settings | `project.c:521,739-765` |

Qué se reutiliza: `arranger.h` tal cual (con crédito); live sections, quick chain y SONG REC se portan adaptados al
reloj de `seq.c` de Felucca; el formato de proyecto y la UI son los de Felucca. `song_chain.c` se reemplaza.

## 5. Diseño propuesto: canciones

- **Variación = proyecto completo (FUN9).** Los pasos ocupan ~3 KB de los 3,6 KB; separar sonidos por canción
  ahorraría poco y quitaría la posibilidad de cambiar sonidos por variación.
- **Flash:** 8 canciones × 4 variaciones × A/B = 64 sectores + índice de canciones A/B (nombre, chain de 16
  entradas, escenas) = 66 sectores. Disponibles sin samples de usuario: 68 (`0x97000–0x9EFFF`, `0xA0000–0xDBFFF`).
  **Margen: 2 sectores.**
- **Escena por entrada de chain:** sección + compases + mutes de grupo (1 B) + macros (4 B) + punch-in FX (1 B):
  ~8 B × 16 = ~130 B por canción.
- **RAM:** solo la canción activa (4 variaciones, ~14,6 KB), reemplaza `chain.source[4]` (~13 KB).
- **Cambio de canción:** solo con el transporte parado.

## 6. Recursos liberables

### RAM [ESTIMADO]

| Bloque | RAM | Nota |
|---|---|---|
| Delay | 128 KB | mono, 16 bit, 1,49 s (`fx.c:5`) |
| PHYS | ~52 KB | |
| Buffer SLICER (compartido con la capa FX: REPEAT, REVERSE, TAPE, FREEZE, OCT) | 32 KB | `slicer.c:20`, `perform.c:17` |
| GRAIN | ~29 KB | engine más caro en CPU (`tests/target_budget.txt`: grain_render 1731) |
| Chain actual | ~12,5 KB | se reemplaza |
| FM6 | ~9,6 KB | |
| SLICE | ~9 KB | |

### Flash de código

- Samples de fábrica embebidos: 126 244 B ADPCM (PIANO, FLUTE, SAX, BREAK de SLICE), de `tools/gen_samples.py`.
  Los usan SAMPLE, SLICE y GRAIN.
- Sacar engines libera código y RAM, **no** espacio para canciones (eso sale solo de la región de datos).

### Dependencias de los samples de usuario

SAMPLE USR1–3, SRC USR de GRAIN, USR de SLICE, `slice_store.c`, subida/grabación/trim del editor,
`tools/fm1_sample_upload.py`, comandos de samples del protocolo SysEx.

## 7. Punch-in FX y lanes

- SLICER por track: 4 parámetros, `slicer_track` 632 en `tests/target_budget.txt`. Candidato a salir.
- Capa FX: REPEAT queda duplicado por el STUTTER MIDI. REVERSE, TAPE STOP, FREEZE y OCT (shimmer) son de audio y
  no se imitan con MIDI.
- Costo estimado en lanes: punch-in FX 64 B por variación (8 bits por paso), macros 256 B por variación
  (4 × 64 pasos × 1 B). No consumen el store de 64 eventos de automatización.

## 8. Engines (propuesta preliminar, decide Juan)

| Engine | Propuesta |
|---|---|
| DRUM | Reemplazar por DRUM-X |
| ANALOG, FM6, VOICE | Mantener |
| TRIO | Fusionar con ANALOG |
| SAMPLE, SLICE | Recortar |
| PHYS | Recortar (RAM) |
| GRAIN | Decidir (depende de los samples de fábrica) |
| PHASE, LOFI, NOISE, WHEEL | Opcional (poca RAM) |

## 9. Riesgos y decisiones abiertas

1. **Tamaño de proyecto:** un sector admite 3840 B; FUN9 usa 3648 (quedan 192 B). DRUM-X A/B por lane
   necesita ~160 B. Si no entra: comprimir pasos vacíos. Medir en Fase 1.
2. **Botón de la capa de canción:** en SLOOP es SAVE sostenido; en Felucca SAVE sostenido es undo y SEQ
   sostenido abre la canción. Pendiente: mover el undo (SLOOP: EDIT + OCT−) o usar SEQ.
3. **Margen de flash:** 2 sectores. La zona `0xE7000–0xFBFFF` queda [VERIFICAR] por el staging del firmware oficial.
4. **Efectos de audio de la capa FX:** sacarlos todos (32 KB para mejor reverb) o conservar alguno.
5. **PHYS / GRAIN / SAMPLE / SLICE:** confirmar recortes.

## 10. Pendiente de la Fase 0

- `CLAUDE.md` con arquitectura, build y mapa de módulos.
- Tamaño real del binario, RAM y CPU en el chip (toolchain JieLi: `pkgman.jieliapp.com`, SDK: `gitee.com`).
- Informe de efectos y mapa completo de modulación y automatización.
