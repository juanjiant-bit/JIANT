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

## Decisión: todo dibujado por código

Medido: las 20 pantallas como imágenes (16 colores, deflate, ≈ 262 KB) desbordaban la región de la app por
113 768 bytes. Decisión del usuario: **todo por código**, con animaciones que reaccionen a los parámetros, sin
gastar memoria en lo visual. Las pantallas del concepto quedan como referencia en `assets/ui-art/art/`.

- **`firmware/src/ui_organic.c`**: líneas antialiasadas en subpíxel (Q4), curvas Bézier, pétalos con nervaduras,
  anillos y nodos. Todo con enteros y la tabla de senos de `dsp.c`. Los colores (crema, coral, teal, mostaza,
  menta) salen de la paleta: en GREY y MONO todo queda en grises.
- **Paleta JIANT** (por defecto desde JIANT 0.1): fondo casi negro, línea crema, teal para los valores, coral
  para lo activo. Las otras paletas siguen en MENU > COLOR.
- **Animación**: el gráfico se redibuja cuando cambia su firma; las páginas vivas suman `ui.frame` a la firma.
  Respiración y vaivén salen de senos de `ui.frame`; los golpes, de las envolventes de las voces.

## Estilo: lámina anatómica

Referencia del usuario: las láminas murales de anatomía (Deyrolle / Auzoux, años 60). Línea crema fina y cerrada
sobre negro; color solo en los detalles (puntas rojas, vasos amarillos, cyan y verdes en paralelo); letras de
referencia cyan en círculos; ejes punteados; punteado para sombrear. Nada de flores genéricas: cada pantalla es un
**espécimen** cuyos órganos son partes del instrumento, y cuya anatomía cambia con los parámetros.

## Pantallas hechas

| Panel | Página | Qué se mueve |
| --- | --- | --- |
| 07 DRUM-X | EDIT de DRUM con KIT X | Una orquídea anatómica. Sus órganos son los grupos del kit: sépalo dorsal = hats (cyan), pétalos laterales = snare y clap (amarillo), sépalos inferiores = kick (rojo), labelo y columna = percusión (verde). A y B son dos anatomías y el MORPH mueve cada contorno punto por punto. Cada golpe enciende los vasos interiores de su órgano, que se apagan con la caída. El ruido de los hats punteado en su sépalo. Escala A–B con el MORPH encima, eje punteado; respira y los zarcillos se mecen |

Las formas son tablas de puntos de control (int8): un órgano ocupa 20 bytes por lado (A y B). Costo total de la
pantalla y la librería: ≈ 3 KB de flash, 0 de RAM.

## Pasos

1. HOME: los 4 tracks como un organismo (un nodo por track, latido con sus voces), tempo, canción y sección.
2. SONG y capa de canción: tallo con las filas como hojas, la que suena iluminada.
3. DRUM MORPH (Fase 2 de DRUM-X): la edición de un sonido con sus lados A y B.
4. PERFORMANCE, MACRO y PUNCH-IN cuando existan esas funciones.
