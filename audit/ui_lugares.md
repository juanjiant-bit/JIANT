# Auditoría de UI: cada cosa en su lugar

Base: `codex/v30` (V10.7.29). Fuentes: `firmware/src/params.c` (PAGES), `ui_lens.c` (CE_*/CG_*), `ui.c` (page_visible),
`ui_layer.c` (LAYERS, ly_keyname, layer_keys), `ui_input.c` (combos), `menu_items.c`, `docs/MANUAL.md`.
No se toca código. Lo ya decidido (canciones a HOME, KEYS/RAILS/KEY SPLIT/STAGE/ENV TRACKS afuera, MOD→MATRIX,
FOCUS→AUTO KNOBS, AUTOMATION+AUTO LIST→LOCKS, PUNCH IN al FX sostenido, renombres SCALE/COMP/DELAY/BUS SENDS, HOME
TRACKS) se da por hecho y no se vuelve a proponer.

La promesa de cada botón: HOME = ver y armar la canción · EDIT = el sonido del track · ENV = envolventes · LFO = lo que
mueve el sonido · SEQ = pasos · SCL = tonalidad, acordes, escenas · ARP = arpegiador · FX = efectos · GLO = mezcla y
setup · SAVE = guardar y cargar · MENU = preferencias.

---

## 1. Cosas fuera de lugar o confusas (por impacto)

| # | Qué | Dónde está hoy | Para qué sirve | Dónde debería estar | Por qué |
|---|---|---|---|---|---|
| 1 | **PRESETS en las páginas de SEQ** | Perilla PRESETS: en todo el synth carga sonidos; en cualquier página de SEQ mueve el cursor de paso ("STEP n"); en G.COMP mueve el cursor de la lane global (`ui_input.c presets_turn`) | Cursor de paso | PRESETS = sonidos siempre (regla 3). Cursor de paso: KNOB 1 de STEP (regla 4) | La misma perilla cambia de significado según la sección. El MANUAL además dice que en SEQ carga patrones (PHRASES), y no es lo que hace el código. Es la contradicción más grande que encontré |
| 2 | **Knobs 3-4 de la capa SCL** | SCL sostenido: KNOB 1..4 = ROOT, SCALE, **ROTATE**, **PITCH** (`LY_SCL`: P_SOFS, P_POFS) | ROTATE desplaza la secuencia; PITCH transpone el track | SCL sostenido: ROOT SCALE **CHORD VOICING** (como dice el comentario de `ui_layer.c`, que quedó viejo) | ROTATE es de SEQ > STRETCH: girar SCL y que se corra el ritmo no tiene nada que ver con "tonalidad". El comentario del código y el MANUAL no coinciden |
| 3 | **Envolventes de operador en EDIT** | EDIT > OP ENV (FM6), EDIT > OP1 ENV..OP4 ENV y OP LEVEL (DIGITAL) | ADSR de cada operador | ENV, como páginas que solo se ven en tracks FM6/DIGITAL (cadena de ENV por tipo de track, igual que EDIT ya tiene CE_FM6/CE_DIG) | Quien busca "envolvente" toca ENV y en un FM no encuentra la de los operadores. OP LEVEL se queda en EDIT |
| 4 | **Capa de SAVE = capa de canción** | SAVE sostenido "[SAVE] SONG": PLAY A-D, quick chain, STORE A-D, LOOP/SONG (D5), SONG REC (E5), ir a ARRANGE (G5), NAME (C5), KNOB 1 cambia de proyecto al soltar | Tocar y armar la canción en vivo | Cuando las páginas de canción pasen a HOME, PLAY A-D / chain / LOOP-SONG / SONG REC / G5 son de HOME. En SAVE dejar solo STORE A-D, OCT+ STORE, OCT- RECALL y NAME | Hoy "SAVE" sostenido toca partes y cambia de proyecto: la mitad no es "guardar". Si HOME sostenido sigue abriendo el MENU, conviene pasar el MENU a HOME + OCT- (o similar) y dejar HOME sostenido para la capa de canción |
| 5 | **"MACROS" son tres cosas distintas** | HOME > MACROS (M1..M4 ruteadas por la MOD matrix); capa LFO "[LFO] MACRO" (las mismas M1..M4); FX sostenido: KNOB 1..4 con pie "MACROS" = FILTER CRUSH THROW DEPTH (`perform.c`, otra cosa) | Las dos primeras: macros M1-M4. La tercera: perillas de performance del FX | FX sostenido: pie "FX KNOBS" (o "PERFORM"), nunca "MACROS" | La misma palabra en dos capas para cosas que no tienen relación |
| 6 | **Pie de la capa LFO miente** | `LAYERS[LAYER_MACRO]` y `MAC_LATCH_FOOT` muestran "SELECT DICE" | El código: LFO + SELECT = **dado del LFO** (CF_DICE_LFO). El dado de macros es F3 | Pie: "SELECT LFO DICE" y "F3 MACRO DICE", o que SELECT tire las macros dentro de la capa | El usuario lee "DICE" sobre las macros y le cambian el LFO del track |
| 7 | **LATCH de LFO y de FX secuestran las perillas** | LFO sostenido y soltado = LATCH: KNOB 1..4 manejan M1-M4 **en todas las páginas** hasta tocar LFO de nuevo. Con FX LATCH (MENU) pasa lo mismo con FX | Tocar macros/efectos con las manos libres | Mantenerlo, pero que el latch se corte al cambiar de sección, o un indicador grande en cada página ("KNOBS = M1-M4") | Vas a EDIT, girás CUT y se mueve M2. Es la confusión típica de "no anda la perilla" |
| 8 | **SAVE > TOOLS** | Primera página de SAVE: CLEAR PROJECT, CLEAR SEQ, INIT SOUND, DELETE ROW, CLEAR SONG, CPU TEST (`ui_lzsave.c WHAT[]`) | Borrar cosas | CLEAR PROJECT: queda (es del proyecto). CLEAR SEQ → SEQ TOOLS (ya está ahí, F3). INIT SOUND → capa EDIT (ya está, F3). DELETE ROW / CLEAR SONG → HOME con las páginas de canción. CPU TEST → MENU > SYSTEM | "Guardar" abre en una página para borrar. Cuatro de las seis acciones ya existen en otro lado o no son de SAVE |
| 9 | **PITCH, CHANCE, HUMAN, REC Q en SEQ > TRACK** | SEQ grupo PATTERN, página TRACK | PITCH = transponer el track; CHANCE/HUMAN = feel; REC Q = cuantizar la grabación | PITCH → SCL > SCALE (al lado de ALL TR). REC Q → capa REC (ya está en B3). Lo que queda: CHANCE HUMAN = página "FEEL" | "TRACK" no dice nada y mezcla tonalidad, feel y grabación |
| 10 | **LOOP de la envolvente en ENV DEST** | ENV > ENV DEST, KNOB 4 = LOOP (P_ELOOP) | La envolvente cicla | ENV > ENV (o una ENV 2/MODE), lejos de los destinos | Una página llamada "DEST" esconde un modo de la envolvente |
| 11 | **G.COMP (lane global de acciones)** | GLO grupo G.COMP (G.COMP, G.COMP 2) | Lane de pasos global: ACTION LENGTH CONDITION VALUE, por paso | SEQ, después de COMP 2 (es una lane de pasos), o HOME si se piensa como "canción" | GLO es mezcla y setup; esto se edita paso por paso con las teclas. Ver además el choque de nombres en §4 |
| 12 | **MIDI desparramado** | GLO > GLOBAL CLOCK; GLO > SYSTEM ROUTE; GLO > SYSTEM 2 (estado USB/TRS/BT); MENU > CONTROL MIDI OUT, MIDI MAP, LEARN; MENU > SYSTEM USB, BLUETOOTH | Conexiones y MIDI | Un solo lugar: pestaña MIDI en MENU (MIDI OUT, MAP, LEARN, ROUTE, CLOCK, estado de puertos, BLUETOOTH, USB) | Hoy hay que saber que el clock está en GLO y el MIDI OUT en el MENU. Además CLOCK y ROUTE son `song.g`, o sea que se guardan con el proyecto: cargar un proyecto te cambia el ruteo MIDI del aparato |
| 13 | **TRACKS y LOAD duplicados en GLO > SYSTEM** | GLO > SYSTEM KNOB 1 TRACKS, KNOB 2 LOAD; también MENU > AUDIO > TRACKS / LOAD | Cantidad de tracks (4/6), calidad CPU | Solo MENU > SYSTEM (son del aparato, no del proyecto: `G_TRKS` "the device's setting") | Dos lugares para lo mismo; y en el MENU están en la pestaña AUDIO, que no es su lugar |
| 14 | **PHRASES y CHANCE, páginas huérfanas** | `page_visible` las deja ver solo "si el código puso la pantalla ahí", y no encontré ningún camino que las abra (PRESETS en SEQ ya no abre PHRASES). SONG (FAM_SEQ) está siempre oculta | Cargar patrones; CHANCE/RATCH de un paso | PHRASES → SAVE (cargar un patrón es cargar) o PRESETS de SEQ si se decide que PRESETS = patrones ahí. CHANCE → borrar (CHANCE vive en COMP / locks) | Código y manual muertos: el MANUAL las sigue explicando |
| 15 | **LYRIC STEP en el grupo AUTO** | SEQ, grupo 6 (AUTO), detrás de AUTOMATION y AUTO LIST | La letra paso por paso (VOICE) | Grupo STEP de SEQ (es edición por paso), o EDIT > WORDS 2 | En LOCKS no se busca letra |
| 16 | **TRACK GEN** | SEQ, grupo propio "GEN" | Genera la secuencia del track | Junto a EUCLID / MUTATE (grupo "GENERA") | Hay cuatro generadores de secuencia (TRACK GEN, EUCLID, MUTATE, SEQ+SELECT). Que estén en el mismo grupo ayuda a entender que son variantes |
| 17 | **Página PLAY / PLAY 2 en EDIT** | EDIT grupo PLAY: POLY GLIDE GL MODE PRIO / ALLOC DETUNE VEL IMAGE | Voces y portamento | Lugar correcto; problema de nombre (§4) | "PLAY" choca con el botón PLAY y con "PLAY A" de la capa SAVE |
| 18 | **X-MOD (DRUM)** | EDIT, grupo TONE de un track DRUM | LFO del kit sobre MORPH, RAND | LFO (cadena de LFO para tracks DRUM) | "Lo que mueve el sonido" es LFO; en un track DRUM el LFO no muestra esto |
| 19 | **TEMPO en GLO > MIXER KNOB 4** | GLO > MIXER: LEVEL PAN MUTE **BPM** (`ui_input.c tracks_edit`) | Tempo | GLO > GLOBAL (ya está) y GLO + SELECT; en MIXER, KNOB 4 = un envío o el BALANCE | En una página de mezcla por track, la cuarta perilla cambia el tempo de toda la canción |
| 20 | **BALANCE** | FX > MASTER (cuarta página, solo FELUCCA_XFX) | Balance L/R del master | GLO > MIXER | Es mezcla, no efecto |
| 21 | **ENV 2 sin destino propio** | ENV > ENV 2 (ADSR) | Fuente de MOD (SRC ENV2) | Se queda, pero con un aviso "ROUTE IN LFO > MATRIX" si no tiene ruta | Girás ENV 2 y no pasa nada |
| 22 | **Páginas de una sola perilla** | LFO 2 (solo PHASE), MSEQ 2 (solo DIV), RETRIG (solo RTRG) | — | PHASE dentro de LFO (KNOB 3 se reparte con MODE: FREE atenúa PHASE); DIV en MSEQ 2 → MSEQ; RETRIG → EDIT 2 del motor LOFI | Tres SELECT de más para una perilla |
| 23 | **BPM LOCK** | MENU > CONTROL | Que el dado de GEN no toque el tempo | GEN (página de HOME), como opción del generador | Solo afecta al dado de GEN |
| 24 | **OCT BEND** | MENU > AUDIO ("CONTROL's eight are full") | Rango del bend de OCT+ | MENU > CONTROL (al lado de VELOCITY/AFTERTOUCH) | Está en AUDIO solo porque no entraba. CONTROL tiene lugar para 12 filas (MTAB_ROWS) |

---

## 2. Todas las combinaciones de botones

Notación de teclas: blancas F3..G5 (16), negras F#3..F#5 (11). ★ = oculta o arbitraria (nadie la adivina).

### Botones solos

| Combo | Qué hace | ¿Tiene sentido? | Sugerencia |
|---|---|---|---|
| HOME tap | HOME; adentro, el grupo siguiente MACROS → SCOPE → GENE | Sí | — |
| HOME sostenido | Abre/cierra el MENU | ★ Más o menos: no tiene nada que ver con "canción" | Ver §1 #4 |
| (sección) tap | La página donde la dejaste; adentro, el grupo siguiente | Sí (regla 2) | — |
| PLAY sostenido | PANIC | Sí | — |
| REC tap | Arma/desarma la grabación | Sí | — |
| REC sostenido y soltado sin tocar nada | **UNDO** (otra vez: REDO) | ★ Arbitrario y peligroso: el mismo gesto que abre la capa REC, si soltás un poco antes de tocar algo, deshace | Sacarlo; UNDO ya está en GLO+G4 y REC+A3 |
| OCT- + OCT+ al encender | HARDWARE CALIBRATION | Sí (técnico) | — |
| OCT- + OCT+ 5 s | Modo update | ★ Pero aceptable por seguridad | — |
| Nota sostenida + OCT-/OCT+ | Vibrato / bend | ★ No se adivina, pero es expresivo | Mostrar "BEND" en pantalla la primera vez |

### Botón sostenido + SELECT (regla 5)

| Combo | Qué hace | ¿Tiene sentido? | Sugerencia |
|---|---|---|---|
| HOME + SELECT | GEN (generador de canción) | Sí, HOME = canción | — |
| EDIT + SELECT | Dado del sonido | Sí | — |
| SEQ + SELECT | Dado de la secuencia | Sí | — |
| LFO + SELECT | Dado del LFO del track | Sí, pero el pie de la capa dice que es el dado de macros | Arreglar el pie (§1 #6) |
| FX + SELECT | Dado de FX | Sí | — |
| GLO + SELECT | Tempo | ★ Arbitrario (el resto de las secciones tira dados o navega) | Aceptable porque GLO = setup, pero mostrarlo en el pie de la capa GLO ("SELECT BPM") |
| ENV / SCL / ARP / SAVE + SELECT | Entrar a esa sección y recorrer su cadena | ★ Redundante: el tap del botón + SELECT hace lo mismo | Eliminar, o darles un dado (ENV, ARP) para que la regla sea pareja: "botón + SELECT = dado" |

### Botón sostenido + PRESETS / ALGORITHM

| Combo | Qué hace | ¿Tiene sentido? | Sugerencia |
|---|---|---|---|
| EDIT + PRESETS | Salta al primer sonido del motor siguiente | Sí | Mostrarlo en el pie de EDIT |
| (otra sección) + PRESETS | Nada | Sí | — |
| capa + ALGORITHM | MAP ↔ KNOBS ↔ STATE | ★ Nadie lo adivina | Indicarlo en el pie ("ALG VIEW") |

### Capas (botón sostenido + teclas/perillas)

| Combo | Qué hace | ¿Tiene sentido? | Sugerencia |
|---|---|---|---|
| FX + F3..F5 | Efectos de performance | Sí | — |
| FX + G5 | CLR AUTO (lane de punch-in) | Pasa a ser del FX sostenido (decidido) | — |
| FX + negras 1-6 | MUTE T1-T6 mientras se sostiene | ★ Los mutes viven en GLO; acá son momentáneos | Dejar, pero nombrar "KILL T1" para distinguir del MUTE latcheado de GLO |
| FX + G#4 / C#5 / D#5 / F#5 | LOOP 3 / LOOP 5 / LOOP 7 / GLITCH | ★ No están en el MANUAL | Documentar |
| FX + A#4 | Tracks que tocan los efectos MIDI: ALL/SYN/DRM | ★ | — |
| FX + KNOB 1-4 | FILTER CRUSH THROW DEPTH (pie "MACROS") | Sí, mal nombre | "FX KNOBS" |
| FX sostenido durante PLAY | **FILL** de pasos (`seq_fill`) | ★ Oculto; no aparece en el pie | Mostrarlo en el pie ("HOLD = FILL") |
| REC+PLAY, FX + tecla | Graba la lane de punch-in | ★ Solo lo dice un hint | — |
| GLO + negras 1-6 | MUTE T1-T6 (latch) | Sí | — |
| GLO + G#4 A#4 C#5 D#5 | Mute de grupos DRUM-X (KICK SNARE HAT PERC) | Sí | — |
| GLO + F3..D4 | SOLO T1-T6 mientras se sostiene | Sí | — |
| GLO + E4 / F4 / G4 | UNMUTE todo / TAP / **UNDO** | UNDO en GLO es ★ | UNDO en un solo lugar (ver §3) |
| GLO + PLAY | Reinicia desde el principio | Sí | — |
| GLO + KNOB 1-4 | LEVEL T1-T4 | Sí | — |
| SCL + tecla | ROOT = esa nota | Sí | — |
| SCL + KNOB 3-4 | ROTATE, PITCH | ★ No son de SCL | CHORD, VOICING (§1 #2) |
| EDIT + F3 / G3 | INIT / RECALL (con diálogo) | Sí | — |
| EDIT + negras 1-8 (DRUM) | Mute de cada sonido del kit | ★ Hay otro mute de batería en GLO (por grupo) | Unificar en GLO |
| EDIT + KNOB 1-4 | MOMENTARY sobre la página de abajo | ★ Potente pero invisible | — |
| SEQ + F3..B4 | Herramientas (CLEAR REVERSE SHIFT… CLR AUTO, lane DRUM) | Sí | — |
| SEQ + E5 / F5 | COPY SEQ / PASTE SEQ | ★ No están en el MANUAL | Documentar |
| SEQ + G5 | CLR STEP (en AUTO LIST: DEL REC) | Sí | — |
| SEQ + REC | CLEAR ALL SEQUENCES? | ★ | Mover a SEQ + tecla (por ejemplo D5 "CLR ALL"), con el resto de las herramientas |
| SEQ + negras 1-8 (DRUM) | Elige lane | Sí | — |
| SEQ + KNOB 1-4 | LEN DIV SWING GATE | Sí | — |
| REC + F3 / G3 | CLEAR (el track, sin diálogo) / CLICK | Sí | — |
| REC + A3 | **UNDO** | ★ No está en el MANUAL | Ver §3 |
| REC + B3 | Q ON/OFF (REC Q) | ★ No está en el MANUAL | Bien que esté acá; sacarlo de SEQ > TRACK |
| REC + FX/EDIT/ENV/LFO/SCL/ARP/GLO | Borra la automatización de esa parte | ★ Nadie lo adivina, y no pide confirmación | Que pida confirmación, o pasarlo a LOCKS |
| SAVE + F3..B3 | PLAY parte A-D (varias = chain) | Es canción, no "guardar" | HOME (§1 #4) |
| SAVE + C4..F4 | STORE A-D | Sí | — |
| SAVE + C5 | NAME | Sí | — |
| SAVE + D5 / E5 | LOOP/SONG, SONG REC | No es "guardar" | HOME |
| SAVE + G5 | Va a ARRANGE | ★ | HOME tap |
| SAVE + OCT+ / OCT- | STORE / RECALL de la parte que suena | Sí | — |
| SAVE + KNOB 1 | Cambia de proyecto **al soltar** SAVE | ★ Peligroso y oculto | Solo en SAVE > PROJECTS |
| LFO + KNOB 1-4 | M1-M4 (y latch al soltar) | Más o menos (las macros se ven en HOME) | — |
| LFO + F3 / G3 | MACRO DICE / CLEAR | Sí, mal señalado | Pie correcto |

### Combos contextuales (por página)

| Combo | Qué hace | ¿Tiene sentido? | Sugerencia |
|---|---|---|---|
| Paso sostenido (STEP) + KNOB | Parameter lock | Sí | — |
| Paso sostenido + EDIT | Borra los locks del paso | ★ | Ya está SEQ+G5; sacarlo |
| Teclas de paso + SELECT / paso + REC | COPY / PASTE de pasos | ★ | — |
| Acorde sostenido en grabación por pasos + ARP | Lo arpegia | ★ | — |
| OCT+ / OCT- en EUCLID, MODES | Variación nueva / la anterior | Sí (OCT+ = hacer) | — |
| OCT+ / OCT- en MUTATE | Mutar ahora / volver al ancla | Sí | — |
| OCT+ / OCT- en STRETCH | Cambia el **modo de NUDGE** (secuencia / micro / paso) | ★ Arbitrario | Que sea una perilla en la página |
| OCT+ / OCT- en SCENE | OCT+ dado de roles, OCT- **todo KEEP** | ★ OCT- borra en vez de volver | OCT- = volver; "ALL KEEP" como acción explícita |
| OCT+ / OCT- en PRESETS | Quedarse con el sonido / el anterior | Sí | — |
| OCT+ sostenido en TOOLS | Hace la acción | Sí, por seguridad | — |
| OCT+ / OCT- en ARRANGE, TIMELINE | Agrega fila / borra la fila | Sí | — |
| OCT- en CLIPS | El track para | Más o menos | — |
| OCT- en PARTS | Vuelve a la canción | Sí | — |

---

## 3. Duplicados que se comportan distinto

| Acción | Caminos | Diferencia |
|---|---|---|
| **UNDO** | GLO + G4 · REC + A3 · REC sostenido y soltado | Tres caminos. REC+A3 además desarma la grabación (`song.rec = 0`) y los otros no. Dejar uno: GLO + G4 (o un UNDO fijo en una sola capa) |
| **Borrar la secuencia** | SEQ + F3 (CLEAR, sin diálogo) · REC + F3 (CLEAR, sin diálogo, también la automatización y el acorde latcheado) · SAVE > TOOLS > CLEAR SEQ (con OCT+ sostenido) · SEQ + REC (todos los tracks, con diálogo) | Cuatro caminos, cada uno borra cosas distintas y solo algunos preguntan |
| **Borrar la automatización** | SEQ + F4 CLR AUTO (también los locks) · REC + botón de sección (solo esa parte) · FX + G5 (lane de punch-in) | Alcances distintos y ninguno lo muestra antes |
| **Borrar locks de un paso** | SEQ + G5 (paso y locks) · paso sostenido + EDIT (solo los locks) | Parecidos, distinto alcance |
| **INIT del sonido** | EDIT + F3 · SAVE > TOOLS > INIT SOUND | Mismo diálogo; dos lugares |
| **Tempo** | GLO + SELECT · GLO + F4 TAP · GLO > GLOBAL BPM · **GLO > MIXER KNOB 4** | Cuatro caminos. El de MIXER es ★: una página de mezcla con el tempo en la última perilla (y el comentario de PAGES todavía dice "DJ FILTER") |
| **DJ FILTER (G_DJF)** | HOME > SCOPE KNOB 1 "FILTER" · GLO > LIVE > KEYS KNOB 4 (se va con KEYS) | Cuando KEYS desaparezca queda solo en HOME, y conviene que el MANUAL (§9 "DJ FILTER… GLO › LIVE › KEYS") lo diga. El "FILTER" de FX sostenido es otro filtro (performance) con el mismo nombre |
| **Delay/Reverb** | FX > BUSES (DMIX RSIZE CDEPTH SLDEP) · FX > DLY/REVERB · HOME > SCOPE SPACE (DMIX y RSIZE juntos) | SPACE mueve dos parámetros a la vez; BUSES y las páginas de módulo editan lo mismo por separado |
| **Envíos del track** | FX > SENDS (DIST CHO DLY REV) · GLO > SENDS (CHO DLY REV) | Los mismos tres envíos en dos secciones (con BUS SENDS, decidido, sigue el duplicado) |
| **MUTE del track** | GLO > MIXER KNOB 3 · GLO + negras 1-6 | La perilla y la tecla hacen lo mismo |
| **DIST del track** | FX > SENDS KNOB 1 · FX > DIST · EDIT > FILTER KNOB 4 "DRV" (en DRUM/EXT es P_DIST) | En los demás motores el DRV de FILTER es otro parámetro (P_FDRV): misma posición, distinto efecto según el motor |
| **CLIP / DUCK del master** | FX > MASTER (CLIP, DUCK) · HOME > SCOPE (DRIVE = CLIP, DUCK) | Mismo parámetro con dos nombres |
| **ROTATE / PITCH** | SEQ > STRETCH ROTATE, SEQ > TRACK PITCH · SCL sostenido KNOB 3-4 | — |
| **REC Q** | SEQ > TRACK KNOB 4 (porcentaje) · REC + B3 (ON/OFF) | Uno es cantidad y el otro interruptor |
| **TRACKS / LOAD** | GLO > SYSTEM · MENU > AUDIO | Iguales; sobra uno |
| **CLICK / COUNT-IN** | MENU > AUDIO · REC + G3 (solo CLICK) | Bien; mismo dato |
| **Mute de batería** | GLO + negras 7-10 (grupos KICK SNARE HAT PERC) · EDIT + negras 1-8 (cada sonido) | Dos sistemas de mute para el mismo kit |
| **Mutes de tracks** | GLO + negras (latch) · FX + negras (momentáneo) | Mismas teclas, comportamiento opuesto |
| **SWING** | GLO > GLOBAL SWING (global) · SEQ > PATTERN SWING (track) · ARP 2 SWING | Tres SWING con el mismo nombre en pantalla |
| **Generar secuencia** | SEQ + SELECT · SEQ > TRACK GEN · EUCLID OCT+ · MUTATE OCT+ | Cuatro generadores; solo el dado pide KEEP/CANCEL |
| **Ir a la canción** | SAVE tap · SAVE + G5 (ARRANGE) · HOME (cuando se mueva) | — |

---

## 4. Nombres que no dicen lo que hacen

| Hoy | Dónde | Qué hace | Nombre sugerido (≤10) |
|---|---|---|---|
| COMP / COMP 2 | SEQ | Tipo de paso y condiciones (EVERY…) | STEP COND / COND 2. **Ojo:** si G.COMP pasa a llamarse COMP (decidido), choca con este |
| TRACK | SEQ (grupo PATTERN) | PITCH CHANCE HUMAN REC Q | FEEL (sacando PITCH y REC Q) |
| RHYTHM | SEQ | SPAN FIT (`seq_fit`) | FIT |
| MODES | SEQ (grupo EUCLID) | Cantidades de los cuatro modos de EUCLID | EUCL MODES |
| STRETCH | SEQ | STRETCH REPEAT NUDGE ROTATE | RESHAPE |
| PLAY / PLAY 2 | EDIT | Voces, glide, prioridad / ALLOC DETUNE VEL IMAGE | VOICES / VOICES 2 |
| SCOPE | HOME | FILTER SPACE DRIVE DUCK del mix (no es un osciloscopio que se maneja) | LIVE MIX |
| SCOPE | MENU > DISPLAY | Osciloscopio en las páginas de EDIT | EDIT SCOPE |
| GENE 1..3 | HOME | El generador de canción (GEN) | GEN 1..3 |
| MACROS (pie de FX) | FX sostenido | FILTER CRUSH THROW DEPTH | FX KNOBS |
| DRIVE | HOME > SCOPE KNOB 3 | El CLIP del master | CLIP |
| PUNCH | FX > MASTER KNOB 2 | Engorda el bus de batería (choca con "PUNCH IN") | DRUM PUNCH |
| REL | FX > MASTER KNOB 4 | Release del ducking | DUCK REL |
| X-MOD | EDIT (DRUM) | LFO del kit sobre MORPH, RAND | KIT LFO |
| SL BUS / SL BUS 2 | FX > SLICER | Qué tracks entran al slicer | SLICER IN |
| SL PITCH | FX > SLICER | Pasos de altura del slicer | SLC PITCH |
| SYSTEM 2 | GLO > SETUP | Estado USB / TRS / Bluetooth | PORTS |
| TOOLS | SAVE | Borrar e inicializar | RESET |
| SOUND (grupo de SAVE) | SAVE | USER + PRESETS (choca con EDIT > SOUND y DRUM SOUND 1-4) | PRESETS |
| SOUND 1..4 | EDIT (DRUM) | Un sonido del kit | KIT SOUND |
| LYRIC STEP | SEQ | La letra por paso | LYRICS |
| ALL TR | SCL KNOB 4 | Transposición global | TRANSPOSE |
| QUANT | SCL KNOB 3 | Las teclas se ajustan a la escala | KEY SNAP |
| MSEQ / MSEQ 2 | LFO | Secuenciador de modulación | STEP MOD |
| PHRASES | (oculta) | Cargador de patrones | PATTERNS |
| HOLD | MENU > CONTROL | Tiempo para que un botón sostenido sea capa | HOLD TIME |
| USB | MENU > SYSTEM | Modo USB: CONSOLE/STEREO/STEMS/IN+OUT/HOST | USB MODE |
| LOAD | MENU / GLO > SYSTEM | Calidad vs CPU (ECO LITE FULL) | CPU LOAD |
| SWING | GLO > GLOBAL | Swing global (hay otros dos) | G.SWING |
| [SAVE] SONG | Encabezado de la capa SAVE | Partes en vivo | [SAVE] PARTS (hasta que se mude a HOME) |

---

## 5. MENU ↔ secciones

**Ítems del MENU que van en una sección**

| Ítem | Hoy | Debería ir | Por qué |
|---|---|---|---|
| BPM LOCK | MENU > CONTROL | HOME > GEN | Solo afecta al dado de GEN |
| CLICK / CLICK LEVEL / COUNT-IN | MENU > AUDIO | Se quedan, y CLICK además en REC (ya está) | Bien así; COUNT-IN podría sumarse a la capa REC |
| SCALE LEDS | MENU > CONTROL | Se queda; pestaña DISPLAY | Es de pantalla/LEDs, no de control |
| OCT BEND | MENU > AUDIO | MENU > CONTROL | Es del teclado |
| TRACKS, LOAD | MENU > AUDIO | MENU > SYSTEM (y sacarlos de GLO > SYSTEM) | Son del aparato |

**Páginas de sección que van al MENU**

| Página / control | Hoy | Debería ir | Por qué |
|---|---|---|---|
| GLO > SYSTEM (TRACKS LOAD ROUTE CPU) | GLO > SETUP | MENU > SYSTEM (TRACKS, LOAD, CPU) y MENU > MIDI (ROUTE) | Son configuración del aparato; ROUTE hoy se guarda con el proyecto |
| GLO > SYSTEM 2 (estado de puertos) | GLO > SETUP | MENU > MIDI | Es diagnóstico de conexión |
| GLO > GLOBAL CLOCK | GLO > SETUP | MENU > MIDI | Fuente de clock = conexión, no mezcla; hoy viaja con el proyecto |
| SAVE > TOOLS > CPU TEST | SAVE | MENU > SYSTEM (al lado de CPU CORES / CPU CLOCK) | No es guardar |

Con eso, GLO > SETUP queda con GLOBAL = BPM SWING TUNE (cosas de la canción) y GLO entero es "mezcla": MIXER (sin el BPM), BUS
SENDS y el BALANCE que viene de FX. El MENU suma una pestaña MIDI (MIDI OUT, MIDI MAP, LEARN, ROUTE, CLOCK, PORTS,
BLUETOOTH, USB MODE), y CONTROL deja de estar lleno.
