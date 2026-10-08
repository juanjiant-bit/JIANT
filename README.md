# JIANT

[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

Firmware de vivo para el **M-VAVE FM-1**: un instrumento para componer y tocar canciones enteras sin
computadora, con control directo de todo lo que suena. Es un fork de
[Felucca](https://github.com/hugelton/Felucca) 1.1.5.1 de Hügelton Instruments (GPL-3.0), y toma de
[SLOOP](https://github.com/isod89/sloop-fm1) el sistema de canciones.

> **Estado: en desarrollo (v0.1).** Compila y pasa todos los tests de Felucca y los propios, pero
> **todavía no se probó en un FM-1 real**. No lo instales sin hacer antes un backup del flash.

## La idea

- **Muchas canciones dentro del aparato.** 8 canciones, cada una con 4 variaciones (A–D) y una cadena
  de partes al estilo SLOOP: quick chain, SONG REC y guardar / recuperar el estado de una parte.
- **Canciones que evolucionan.** Cada paso de la cadena puede cambiar mutes, macros y efectos (escenas):
  no quedar preso de un loop donde solo se mueve un cutoff.
- **Batería tipo Microtonic (DRUM-X).** Síntesis en tiempo real con morph A↔B por sonido, mutes por grupo
  (KICK, SNARE, HAT, PERCS).
- **Deformación en vivo.** Punch-in FX que actúan sobre las notas (octava, stutter, arp, decay, random)
  y se pueden secuenciar; 4 macros en los knobs; master con clipper y PUNCH.
- **Menos engines, pero más profundos.** Se recorta lo que ocupa memoria sin aportar, y se suma
  modulación a lo que queda.

La visión completa está en [FELUCCA-TONIC-VISION.md](FELUCCA-TONIC-VISION.md) y el orden de trabajo en
[FELUCCA-TONIC-SPEC.md](FELUCCA-TONIC-SPEC.md).

## Estado

| Objetivo | Estado |
| --- | --- |
| Auditoría de Felucca, medidas de flash / RAM ([docs/TONIC-AUDIT.md](docs/TONIC-AUDIT.md)) | Hecho |
| Limpieza para liberar recursos (PHYS, efectos de la capa FX, undo, lock de capas) | Hecho |
| Song mode estilo SLOOP, 8 canciones × 4 variaciones ([docs/TONIC-SONG-PLAN.md](docs/TONIC-SONG-PLAN.md)) | Hecho (falta backup de todas las canciones) |
| Escenas por paso de la cadena | Pendiente |
| DRUM-X y mutes por grupo ([docs/TONIC-DRUMX.md](docs/TONIC-DRUMX.md)) | En curso (Fase 1 hecha: KIT X) |
| Master: clipper y PUNCH | Pendiente |
| Macros M1–M4 | Pendiente |
| Punch-in FX MIDI y sus lanes | Pendiente |
| Más modulación y mejores efectos | Pendiente |
| Estética nueva de la interfaz | Más adelante |

### Qué cambió respecto de Felucca 1.1.5.1

- **PHYS** se retiró (liberó 50 KB de RAM). Un sonido PHYS de un proyecto o preset viejo suena como el
  primer preset de ANALOG.
- **Sin samples de usuario.** Se fueron los slots USR1–3 (SAMPLE, GRAIN, SLICE), la subida y grabación
  desde el editor, la página EDIT > SLICES y las slices manuales (MAN). Los samples de fábrica, BREAK y
  PIANO siguen. Un sonido viejo en USR suena con un set de fábrica. Libera 240 KB de flash para las
  canciones.
- **8 canciones.** Cada una con sus 4 secciones (A–D) y sus filas. La canción 1 son los 4 proyectos de
  siempre, así que lo guardado con Felucca aparece ahí.
- **Capa FX:** quedan REPEAT 1/8, 1/16, 1/32 y los filtros LPF / HPF. Se fueron REVERSE, TAPE STOP,
  FREEZE y el armonizador.
- **Sin undo.** SAVE sostenido abre la capa de canción; OCT− ya no "devuelve" en las capas. En su lugar,
  dentro de la capa de canción, OCT+ guarda la sección que suena y OCT− la recupera (STORE / RECALL).
- **Canción por compases, estilo SLOOP.** Una fila es {sección A–D, compases 1–64} y cada sección es el
  proyecto completo (sonidos, patrones, automatización). Live sections, quick chain y SONG REC en SAVE
  sostenido. SEQ sostenido ya no abre la canción.
- **Sin lock de capas** por doble toque: una capa está abierta mientras se mantiene su botón.
- **Capa REC:** CLEAR y CLICK. COUNT-IN y CLICK LEVEL siguen en MENU > AUDIO.
- El splash y ABOUT dicen JIANT. Los nombres USB siguen siendo "Felucca" para que el editor web conecte.

## Lo que tiene hoy

- **4 tracks**, cada uno con su engine y su sonido, 8 voces compartidas.
- **12 engines:** ANALOG, FM6 (Dexed, importa .syx), PHASE, LOFI, SAMPLE, VOICE, TRIO, WHEEL, GRAIN,
  NOISE, SLICE y DRUM (kit de 8 lanes sintetizado, sin samples).
- **Secuenciador:** 64 pasos por track con acordes, ties, accent, slide, chance y ratchets; piano roll;
  grilla de batería; parameter locks; automatización de perillas; grabación en vivo con overdub;
  metrónomo y count-in.
- **SEQ TOOLS:** CLEAR, REVERSE, SHIFT, RANDOM y COOK sobre el patrón (BEAT y herramientas por lane en
  DRUM).
- **Teclas de acordes**, arpegiador, 16 escalas con modo de teclas blancas, glide, MONO / LEGATO / UNISON.
- **Modulación:** 4 slots por track, con controladores MIDI como fuentes.
- **Efectos:** distorsión y SLICER por track; envíos a chorus, delay y reverb (ROOM o SPRING); limiter
  en el master.
- **USB:** MIDI class-compliant y entrada de audio "Felucca" (el master en la computadora). MIDI por TRS,
  clock interno, USB o TRS.
- **Proyectos y presets:** 4 proyectos y 32 presets de usuario, con nombre; autoguardado al apagar.
- **Editor web** de todos los parámetros (editor de patches FM6, grilla, mezcla, backup completo).

## Controles

- Tocar un botón de página abre su página; otra vez, la siguiente. HOME vuelve a la pantalla principal.
- **Mantener** un botón abre su capa rápida: las teclas y las perillas cambian de función mientras está
  apretado.

| Mantener | Teclas | Perillas |
| --- | --- | --- |
| **FX** | F3 G3 A3 REPEAT 1/8, 1/16, 1/32; B3 LPF; C4 HPF; teclas negras 1–4: mute de T1–T4 | FILTER, CRUSH, THROW, DEPTH |
| **GLO** | Teclas negras 1–4 mute (fijo), F3–B3 solo mientras se mantiene, C4 unmute, F4 tap tempo | Nivel de T1–T4 |
| **SCL** | Cualquier tecla elige la raíz | ROOT, SCL, CHRD, VOIC |
| **EDIT** | Las teclas blancas eligen el engine; la siguiente, INIT | Engine, sonido, favorito |
| **SEQ** | En las páginas SEQ: SEQ TOOLS | LEN, DIV, SWING, GATE |
| **REC** | F3 CLEAR del track, G3 CLICK | CLICK |
| **HOME** | Menú | — |
| **SAVE** | Capa de canción: F3–B3 tocan A–D en el próximo compás (varias en un mismo hold: quick chain), C4–F4 guardan en A–D, D5 LOOP / SONG, E5 SONG REC, G5 página SONG; OCT+ guarda la sección que suena, OCT− la recupera | KNOB 1: canción 1–8 (entra al soltar SAVE, parado) |

Otros: **PLAY** arranca y para; **REC** arma el track elegido; **SELECT** cambia el tempo; **ALGORITHM**
elige el track en todas las páginas; **PRESETS** cambia el sonido; **OCT− / OCT+** la octava (en páginas
de acción y diálogos: volver / confirmar); GLO + PLAY reinicia desde el principio.

## Compilar y probar

Ver [BUILDING.md](BUILDING.md). En resumen:

```
./build.sh                 # firmware: build/felucca.fwsc
web/emu/build.sh           # emulador en el navegador: build/emu
tests/run_tests.sh         # tests de host, editor web y emulador
```

Todo cambio se prueba primero en el emulador.

## Instalar (bajo tu riesgo)

1. Hacé un **backup del flash** antes de la primera instalación.
2. Instalá `build/felucca.fwsc` con `python3 tools/fm1_install.py build/felucca.fwsc` o con una copia local
   del instalador web (ver [BUILDING.md](BUILDING.md)).
3. Para volver al firmware oficial, usá el actualizador de M-VAVE o **Return to official V15** del
   instalador.

Si el FM-1 queda en negro después de una actualización interrumpida, revisá si la computadora lo ve como
**WL80UBOOT** (o un dispositivo USB 4C4A:8057): es el modo de arranque del chip y se puede recuperar
volviendo a correr el instalador con otro cable. Si no alcanza,
[FM-1 Transporter](https://github.com/kurogedelic/FM-1-transporter) lee y escribe el flash con una
Seeed XIAO RP2040.

## Documentos

| Documento | Qué tiene |
| --- | --- |
| [FELUCCA-TONIC-VISION.md](FELUCCA-TONIC-VISION.md) | La visión y la lista completa de deseos |
| [FELUCCA-TONIC-SPEC.md](FELUCCA-TONIC-SPEC.md) | Las fases y las reglas de trabajo |
| [docs/TONIC-AUDIT.md](docs/TONIC-AUDIT.md) | Auditoría: memoria, engines, efectos, song mode de SLOOP |
| [docs/TONIC-SONG-PLAN.md](docs/TONIC-SONG-PLAN.md) | Plan del song mode |
| [BUILDING.md](BUILDING.md) | Toolchain, build, emulador y tests |
| [web/EDITOR_PROTOCOL.md](web/EDITOR_PROTOCOL.md) | Protocolo SysEx del editor |
| [LICENSING.md](LICENSING.md) | Licencias de cada parte |

## Créditos

- **[Felucca](https://github.com/hugelton/Felucca)** de [Hügelton Instruments](https://hugelton.com)
  (Leo Kuroshita, [@kurogedelic](https://github.com/kurogedelic)): la base completa de este firmware;
  las formas de onda de PHASE (port de [CrispyZebra](https://github.com/hugelton/CrispyZebra), GPL-3.0);
  las voces y kits de DRUM; el Hügelton Sample Pack (GPL-3.0-only, no CC0); la fuente de íconos
  [Fukiai](https://github.com/hugelton/Fukiai) ([MIT](LICENSES/MIT-Fukiai.txt)). Y quienes contribuyeron
  a Felucca: keremimo, ChanceTheMaker, andreahaku, spinkham, zednaked, jasonpersinger.
- **[SLOOP](https://github.com/isod89/sloop-fm1)** de isod89 (GPL-3.0): el sistema de secciones,
  quick chain y SONG REC en el que se basa el song mode.
- Fuentes: [Inter Tight](https://github.com/rsms/inter-tight) ([SIL OFL 1.1](LICENSES/OFL-InterTight.txt));
  en el emulador, [DotGothic16](https://github.com/fontworks-fonts/DotGothic16) ([SIL OFL 1.1](LICENSES/OFL-DotGothic16.txt)).
- Samples: [Versilian Studios](https://versilian-studios.com/) VSCO-2 CE y VCSL, CC0 1.0
  ([atribución](assets/samples-cc0/ATTRIBUTION.txt)).
- VOICE: según [klattsch](https://github.com/tgies/klattsch) de Tony Gies (MIT); datos de formantes de
  Klatt (1980) y Hillenbrand et al. (1995).
- FM6: msfa de [Dexed](https://github.com/asb2m10/dexed), Google Inc. y Pascal Gauthier
  ([Apache-2.0](LICENSES/Apache-2.0-msfa.txt)).
- Emulador: según [X0X](https://github.com/charlesvestal/fm1-x0x) de charlesvestal (GPL-3.0).
- Formato de paquete: [JieLi AC79 SDK](https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK)
  ([Apache-2.0](LICENSES/Apache-2.0.txt); tres de sus archivos van en cada paquete, ninguno en este árbol).

## Desarrollo asistido por IA

JIANT se desarrolla con ayuda de agentes de IA para el código, los tests y la documentación. El diseño
del instrumento y las decisiones las toma su autor.

## Licencia

Software libre: [GPL-3.0-only](LICENSE), igual que Felucca. Las fuentes y el DSP portado conservan sus
licencias ([LICENSES/](LICENSES/)); detalles en [LICENSING.md](LICENSING.md).

"Felucca" y "Hügelton Instruments" son nombres de Hügelton Instruments; JIANT no está afiliado a ellos.
M-VAVE y FM-1 son marcas de sus dueños; JIANT no está afiliado ni avalado por ellos.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments — Felucca.
Las modificaciones de JIANT se distribuyen bajo la misma licencia.
