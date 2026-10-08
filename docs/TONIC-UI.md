# FELUCCA TONIC — Interfaz JIANT FM

El concepto ([jiant-fm-ui-concept.jpg](jiant-fm-ui-concept.jpg)) tiene 20 paneles. Cada uno corresponde a una
página o capa del firmware. Las ilustraciones son el fondo; los valores (BPM, sección, MORPH, pasos, niveles)
los dibuja el firmware encima, y algunas partes reaccionan a los knobs: arcos, pétalos, celdas, fila activa.

## Mapa: panel → firmware

| # | Panel | En el firmware | Estado de la función |
| --- | --- | --- | --- |
| 01 | HOME | HOME: los 4 tracks con su engine, tempo, canción y sección | existe |
| 02 | TRACK | EDIT del track (los parámetros de su engine) | existe |
| 03 | FM ALGORITHM | FM6: algoritmo y operadores | existe |
| 04 | TRACK MAP | los tracks y las 8 voces compartidas (activas / libres / robadas) | existe (falta la vista) |
| 05 | SEQUENCER | SEQ > STEP, grilla de batería de 8 lanes, 64 pasos | existe |
| 06 | SEQ PERFORMANCE | capa SEQ: SEQ TOOLS, grabación y mutes en vivo | existe |
| 07 | DRUM-X | el kit DRUM-X: un pétalo por sonido | Fase 1 hecha (KIT X) |
| 08 | DRUM MORPH | edición de un sonido: lado A / B, MORPH, PITCH DECAY NOISE DRIVE PAN | Fase 2 |
| 09 | MACRO | macros M1–M4 y sus destinos | pendiente (Fase 6) |
| 10 | MACRO MAP | qué mueve cada macro | pendiente (Fase 6) |
| 11 | PUNCH-IN FX | capa FX: hoy REPEAT 1/8, 1/16, 1/32, LPF, HPF; después los punch-in MIDI | parcial (Fase 7) |
| 12 | FX RACK | efectos por track: distorsión, envíos a chorus, delay, reverb | existe |
| 13 | MIXER | MIXER (GLO): nivel, pan, envíos, mute | existe |
| 14 | SONG | página SONG y capa de canción (SAVE sostenido) | hecho |
| 15 | CHORD | acordes y voicings (capa SCL, página CHORD) | existe |
| 16 | ARP | arpegiador | existe |
| 17 | AUTOMATION | automatización por paso y de knobs (AUTO LIST) | existe |
| 18 | VISUALIZER | osciloscopio | existe (sencillo) |
| 19 | SAVE / PROJECT | 8 canciones × secciones A–D: guardar, cargar, renombrar | hecho (falta renombrar) |
| 20 | PERFORMANCE | tempo, sección y macros en vivo, el punch-in activo | pendiente (macros) |

Equivalencias de nombres: en el concepto **PRJ** es la canción (1–8) y **VAR** la sección (A–D). FM1, FM2,
FM3, DRUM y VOICE son ejemplos de engines: el firmware tiene 4 tracks y cualquier engine en cada uno.

## Formato y medidas

El paquete (`assets/ui-art/`) convierte cada pantalla a 16 colores, 4 bits por píxel y deflate crudo. Se
descomprime una vez al cambiar de página, en un buffer de 28,8 KB en `.pool`.

Medido en este árbol (compilando las 20 con el firmware actual): **no entran**. La app desborda la región XIP
por **113 768 bytes**. Hoy la app ocupa 431 KB de 568 KB. Las 20 pantallas comprimidas suman ≈ 262 KB, y
todavía faltan DRUM-X completo, macros, punch-in y master.

Caminos (para decidir):

1. **Solo las pantallas que usa el firmware**, con arte sin textos (los valores van encima). Unas 10 pantallas
   de fondo ocupan ≈ 130 KB; puede entrar si se recorta otra cosa (por ejemplo los samples CC0, ~120 KB).
2. **8 colores y arte más simple** (líneas sobre negro): comprime mucho mejor (a medir con el arte nuevo).
3. **Dibujado por código:** las formas orgánicas como vectores (pétalos, tallos, arcos) con `cv_line` /
   `cv_rrect` y una paleta JIANT. Casi no ocupa flash y todo puede reaccionar a los knobs, pero se parece menos
   a la ilustración.

Lo razonable parece una mezcla: una paleta y tipografía JIANT para todo el firmware (sin costo de flash),
arte de fondo en 8 colores para las páginas principales (HOME, DRUM-X, SONG, PERFORMANCE) y elementos vivos
dibujados por código.

## Pasos

1. Paleta JIANT FM (fondo negro, línea crema, coral, teal, mostaza, menta) como tema del firmware.
2. Arte sin texto de las pantallas elegidas, en el formato que se decida; tabla de zonas de texto por pantalla.
3. Integración en `ui_draw.c` por página, con capturas en `tests/ui_render.c`.
