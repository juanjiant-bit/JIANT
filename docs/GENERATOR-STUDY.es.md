# El generador de canciones de JIANT FM: estudio y reglas (V10.8.34)

Para Juan. Este documento tiene cuatro partes: qué medí del generador y qué encontré; los principios de música y de producción que se aplican; qué cambió en V10.8.34; y qué queda por etapas, con su costo en flash.

Todo lo que dice «medido» sale de tres programas nuevos que quedaron en el repo (`tests/gen_study.c`, `tests/gen_audit.c`, `tests/gen_stems.c`). No escuché nada: son mediciones sobre las notas, los parámetros y el audio renderizado en la computadora. Lo que afirmo sobre «cómo suena» es inferencia a partir de esos números y hay que validarlo con el oído.

## 1. Resumen

- **El generador ya es un sistema de reglas por género.** Una tabla por género (`GENRE` en `gen_tables.c`: tempo, modos, mapa de batería, kit, tres bajos, tres progresiones, estilo de acordes, voicing, arps, estilo de lead, cantidades, sonidos, reverb, delay, forma, humanización, familia armónica) alimenta todas las etapas en este orden: perfil → tonalidad y modo → progresión y plan de acordes → batería → bajo contra el bombo → acordes con conducción de voces → arp → lead → sonidos → mezcla → forma. No hizo falta reescribirlo de cero: las incoherencias que encontré están en lugares puntuales.
- **Lo que las mediciones muestran como flojo** (24 semillas por género, forma PARTS, energía 50):
  1. **Las secciones casi no cambian de acordes.** Solo el 40 % de las secciones B/C/D tiene un conjunto de acordes distinto al de A; en 10 géneros, el 0 %. La «sección B» era la misma progresión girada, con pistas muteadas.
  2. **No había diálogo entre voces.** Lead y arp alternándose (uno habla, el otro calla): 0,3 % de los medios compases.
  3. **Melodías tímidas.** Rango medio del lead: 6,7 semitones (menos de una quinta). Saltos mayores que una quinta: 3,6 %. Motivo que vuelve con el mismo ritmo: 59 % de los compases (bien), pero el contorno casi siempre es por grado conjunto y sin un rasgo propio.
  4. **Mezcla muy despareja entre géneros.** Con todas las partes prendidas, el lead queda entre 1,9 y 22,8 dB por debajo de la batería según el género (promedio 15,5); el arp, entre 1,2 y 28,9 dB. En varios géneros la melodía casi no se oye (ver sección 5).
  5. **Adornos que caricaturizan.** Con complejidad 50, algo más de la mitad de las pistas melódicas recibía un componente de paso al final de frase; en el lead, uno de cada tres era BEND (un «boing» de afinación), y en el arp, uno de cada cuatro era CLIMB.
- **Lo que hace V10.8.34** (sección 4): progresiones nuevas por familia, la sección de contraste con progresión propia, registro propio por sección, motivo con un salto «firma», desplazamiento rítmico como variación, más pregunta–respuesta dentro del lead, diálogo lead–arp, y fuera BEND y CLIMB.
- **Lo que no entró** (sección 6): la corrección de niveles y de parámetros de síntesis por género. Está auditada con números, pero cambiarla sin escuchar sería a ciegas, y la mezcla actual la ajustaste vos a oído en varios géneros. Queda como etapa 2, casi sin costo de flash.
- **Flash:** V10.8.34 usa 300 B de la imagen (quedan 244 B) y 236 B del segmento Bluetooth, donde vive parte del generador (quedan 176 B).

## 2. Mediciones, antes y después

Forma PARTS, energía 50, complejidad 50, 24 semillas por género. «Antes» es V10.8.33.

| Género | progresiones distintas (de 24 semillas) | secciones con otros acordes % | rango del lead (semitonos) | saltos de más de una 5ª % | diálogo lead/2ª voz % |
|---|---|---|---|---|---|
| Minimal Techno | 1 → 1 | 61.1 → 61.1 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |
| Hard Techno | 13 → 13 | 59.7 → 61.1 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |
| Hard Bass | 18 → 16 | 62.5 → 65.3 | 8.2 → 8.2 | 5.0 → 3.4 | 8.3 → 7.7 |
| House | 15 → 12 | 54.2 → 62.5 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |
| Deep House | 14 → 13 | 52.8 → 52.8 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |
| Tech House | 16 → 16 | 58.3 → 58.3 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |
| Tribal | 16 → 17 | 58.3 → 58.3 | 0.0 → 0.0 | 0.0 → 0.0 | 0.0 → 0.0 |
| Cumbia | 7 → 7 | 52.1 → 52.1 | 8.7 → 11.0 | 0.0 → 5.2 | 0.0 → 6.9 |
| Reggaeton | 4 → 4 | 0.0 → 0.0 | 8.1 → 10.2 | 0.8 → 4.6 | 0.0 → 58.3 |
| Dembow | 3 → 3 | 87.5 → 87.5 | 3.8 → 3.8 | 2.3 → 2.3 | 0.0 → 24.4 |
| Electro | 22 → 24 | 58.3 → 62.5 | 15.7 → 16.2 | 58.7 → 55.9 | 0.0 → 0.0 |
| Breakbeat | 14 → 14 | 97.9 → 97.9 | 5.5 → 5.3 | 1.5 → 1.3 | 0.0 → 24.4 |
| Drum & Bass | 15 → 14 | 30.6 → 33.3 | 7.1 → 6.6 | 0.0 → 1.1 | 0.0 → 14.3 |
| Idm Glitch | 18 → 16 | 31.9 → 29.2 | 10.1 → 11.6 | 1.1 → 6.4 | 0.0 → 11.3 |
| Idm Ambient | 19 → 17 | 0.0 → 30.6 | 7.5 → 7.8 | 2.3 → 0.8 | 0.0 → 7.7 |
| Idm Weird | 21 → 20 | 31.9 → 29.2 | 8.2 → 10.2 | 1.1 → 5.3 | 0.0 → 15.0 |
| Kraut | 13 → 14 | 81.9 → 81.9 | 9.2 → 8.8 | 0.0 → 0.0 | 0.0 → 0.0 |
| Ebm | 8 → 10 | 0.0 → 0.0 | 4.2 → 5.0 | 2.7 → 5.4 | 0.0 → 0.0 |
| Neo Classic | 22 → 24 | 0.0 → 27.8 | 9.0 → 7.7 | 0.0 → 0.0 | 0.0 → 0.0 |
| Fuga | 17 → 16 | 0.0 → 20.8 | 10.5 → 12.0 | 1.5 → 6.6 | 0.0 → 24.4 |
| Electro Metal | 11 → 13 | 70.8 → 70.8 | 7.0 → 7.0 | 5.8 → 4.6 | 0.0 → 21.1 |
| Techno Rock | 17 → 16 | 58.3 → 70.8 | 7.2 → 8.2 | 2.8 → 5.8 | 0.0 → 24.4 |
| Dream Pop | 17 → 16 | 0.0 → 26.4 | 6.8 → 7.2 | 1.7 → 1.3 | 0.0 → 11.9 |
| Phonk | 13 → 16 | 0.0 → 0.0 | 8.1 → 8.7 | 6.0 → 6.4 | 0.0 → 0.0 |
| Baile Funk | 13 → 14 | 0.0 → 0.0 | 10.8 → 10.2 | 4.4 → 4.4 | 0.0 → 0.0 |
| Jazz | 18 → 15 | 95.8 → 95.8 | 8.9 → 11.3 | 0.9 → 5.6 | 0.0 → 0.0 |
| Hip Hop | 18 → 20 | 41.7 → 45.8 | 9.8 → 11.6 | 0.8 → 4.2 | 0.0 → 0.6 |
| Dubstep | 12 → 10 | 0.0 → 0.0 | 11.8 → 10.9 | 5.5 → 5.6 | 0.0 → 0.0 |
| Shoegaze | 18 → 14 | 0.0 → 26.4 | 6.8 → 6.7 | 0.0 → 0.0 | 0.0 → 20.8 |
| Metalcore | 17 → 14 | 64.6 → 64.6 | 6.7 → 6.8 | 5.0 → 2.7 | 0.0 → 19.6 |
| Explore | 24 → 24 | 34.7 → 36.1 | 7.5 → 8.1 | 0.7 → 3.1 | 2.5 → 2.5 |
| **Promedio** | 14.6 → 14.3 | 40.2 → 45.5 | 6.7 → 7.1 | 3.6 → 4.6 | 0.3 → 9.5 |

Con todas las partes prendidas (BASS, CHORDS, ARP y LEAD en 60), los promedios pasan de: secciones con otros acordes 47.0 → 51.9 %; rango del lead 7.5 → 8.0 semitonos; saltos 3.6 → 4.8 %; diálogo 0.0 → 13.9 %.

Cómo leerla:
- **Progresiones distintas** no sube (14,6 → 14,3): el generador ya escribía progresiones con una gramática por familia en el 65 % de las piezas. Lo que cambia es *cuáles* aparecen: una de cada cuatro piezas toma ahora una de las doce progresiones «de disco» nuevas. MINIMAL TECHNO, REGGAETON, DEMBOW y CUMBIA siguen con pocas a propósito: sus reglas son las de tus referencias.
- **Secciones con otros acordes** sube poco en promedio (40 → 45 %) porque solo cambia una sección por canción (la de contraste) y los géneros de loop modal (house, techno, EBM, phonk, dubstep) conservan sus acordes a propósito: en esa música el break no cambia de armonía.
- **Diálogo** pasa de casi nada a 9,5 % (13,9 % con todo prendido). Es bajo en términos absolutos porque en muchos géneros el arp o el lead están apagados por defecto.
- El **rango** y los **saltos** suben poco: el salto firma es uno por motivo y después el motivo se adapta a cada acorde.

En resumen: los cambios de V10.8.34 se miden, pero son moderados. No esperes otra máquina; esperá piezas con una parte B distinta, un motivo con más carácter y menos adornos de caricatura.

## 3. Principios que aplica el generador

Cada principio va con la regla del generador que lo implementa. «Ya estaba» quiere decir que la regla existía antes de esta versión.

### Armonía
- **Función y cadencia.** Una progresión es tónica → predominante → dominante → tónica, o un loop modal sin dominante. *Regla:* la progresión se escribe en grados del modo; el último acorde lleva al primero; en menor, el V se hace mayor antes del i si el género lo permite (ya estaba).
- **Intercambio modal y dominantes secundarias.** Un iv menor en tono mayor, un bVII, un V/ii. *Regla:* colores por género (`HX_BOR`, `HX_SEC`, `HX_RAISE`) (ya estaba). *Nuevo:* I–bVII–IV–I y dos turnarounds de jazz que vuelven al ii por su dominante.
- **Progresiones por estilo.** Pop: I–V–vi–IV, vi–IV–I–V. Modal: i–iv–i–v, i–III–VII–iv. Jazz/neo-soul: ciclo de cuartas, ii–V–I. Cine: i–VI–III–V. IDM: pedal, movimientos de segunda, sin cadencia. *Nuevo:* doce progresiones, dos por familia (tabla `PROGF`).
- **Conducción de voces.** Notas comunes quietas, el resto al vecino más cercano, nada de barro debajo de G3. *Regla:* `gvoice` (ya estaba).
- **Contraste armónico entre secciones.** El puente arranca en el subdominante o el relativo; el último estribillo sube un tono. *Antes:* la misma progresión girada. *Nuevo:* la sección de contraste escribe otra progresión con la gramática de la familia (misma cantidad de acordes y mismas calidades que A), desde su predominante, en 8 de cada 10 canciones que no son de loop modal ni latinas.

### Melodía
- **Motivo e identidad.** Una melodía se reconoce por una célula rítmica y un contorno. *Regla:* motivo de un compás sobre una de 16 células (antes 12; las cuatro nuevas son sincopadas).
- **Desarrollo.** Repetir, secuenciar, invertir, truncar, aumentar, retrogradar, desplazar. *Regla:* plantillas de frase (período, sentencia, AABA, gancho, pregunta–respuesta, espacio) y un operador para el compás variado (ya estaba). *Nuevo:* sexto operador, desplazamiento de una corchea.
- **Contorno con un rasgo.** Grado conjunto como base y un salto que se compensa volviendo por grado. *Nuevo:* cada motivo tiene un salto «firma» de quinta o sexta, en un lugar que decide la semilla.
- **Notas del acorde en los tiempos fuertes, cadencia al final de frase** (ya estaba).
- **Pregunta y respuesta.** Dos compases preguntan (terminan abiertos), dos responden (cierran). *Nuevo:* la plantilla pregunta–respuesta sale una de cada cuatro veces más, y las respuestas van una tercera y una cuarta arriba de la pregunta.
- **Diálogo entre voces.** *Nuevo:* en la mitad de las piezas con lead y arp, el arp calla en los medios compases donde el lead toca dos notas o más, y contesta en los huecos. No se aplica si el lead no deja huecos.

### Ritmo y groove
- **El bajo se escribe contra el bombo**: encima (808), en los huecos (minimal), contestando (house). *Regla:* unos 50 arquetipos de bajo y una regla de bombo por género (ya estaba).
- **Dinámica:** acentos, ghost notes, hats con niveles, fills solo al final de frases de 4 u 8 compases (ya estaba).
- **Contención.** Un adorno por frase, donde los discos lo ponen. *Nuevo:* fuera el BEND sobre el lead y el CLIMB sobre el arp.

### Forma
- **Arco de energía:** intro flaca, capas, build, drop, break, outro (ya estaba, por filas).
- **Secciones contrastadas:** otra progresión (nuevo), otro registro (nuevo: en PARTS, B una tercera abajo y C una cuarta arriba para arp y lead), otra densidad, otras pistas (ya estaba).

### Personalidad
Una semilla decide ahora, juntos: la célula rítmica (1 de 16), el tipo de contorno (4), el lugar y tamaño del salto firma (3 × 2), el operador de variación (6), la plantilla de frase (6), si hay diálogo, y la progresión propia de la sección B. Eso es lo que hace que dos piezas del mismo género no se parezcan. No agregué todavía una «paleta de sonido» por semilla: ver etapa 3.

## 4. Qué cambió en V10.8.34

| Prioridad tuya | Cambio | Dónde |
|---|---|---|
| Más progresiones | Doce progresiones nuevas, dos por familia armónica; una de cada cuatro piezas las usa si el modo las admite | `gen_tables.c` `PROGF`, `gen.c` `gen_pick` |
| Partes distintas | La sección de contraste con progresión propia; registro propio por sección | `gen.c` `gen_parts` |
| Melodías | Salto firma, cuatro células sincopadas, operador de desplazamiento, rango del motivo un grado más amplio | `gen.c` `gen_lead` |
| Pregunta y respuesta | Plantilla pregunta–respuesta más frecuente, respuestas más arriba; diálogo lead–arp | `gen.c` `gen_lead`, `gen_post` |
| Personalidad | La identidad por semilla descrita arriba; fuera BEND y CLIMB | `gen.c` `gen_post` |

No cambié: MINIMAL TECHNO, EBM, las latinas y MY LOOP en lo armónico (son tus referencias), EXPLORE, la batería, el bajo, los sonidos ni la mezcla.

Las piezas de una misma semilla cambian respecto de V10.8.33 (cambió el orden de los sorteos).

## 5. Producción: cómo deberían quedar los parámetros, y cómo quedan

### 5.1 Práctica de DAW por rol

| Rol | Síntesis | Envolvente | Filtro | Espacio | Nivel (RMS respecto del bus de batería) |
|---|---|---|---|---|---|
| Bombo / kit | bombo afinado a la tónica o la quinta; caja y hats cortos | decay corto en géneros rápidos | — | reverb casi nula en el bombo; algo en caja y clap | referencia: 0 dB |
| Bajo | mono, sin unison ancho, sub limpio debajo de 100 Hz | ataque 0–5 ms; release corto para que no pise el bombo siguiente | cutoff bajo con envolvente para el ataque | sin reverb ni delay | −2 a −5 dB (808 y dub: 0 a +3) |
| Acordes / pad | unison y detune moderados | pad: ataque y release largos; stab: decay corto, sustain 0 | pasa-bajos para dejar lugar al lead; pasa-altos implícito (no tocar debajo de G3) | reverb alta, delay bajo | −8 a −12 dB |
| Lead | mono o poco polifónico, algo de glide o vibrato tardío | ataque corto, sustain medio | brillante pero sin tapar hats | delay más que reverb | −6 a −9 dB |
| Arp / pluck | corto | decay corto, sustain 0 | envolvente de filtro marcada | delay sincronizado (1/8 con puntillo) | −9 a −13 dB |
| Textura | ruido, drone | muy lento | oscuro | mucha reverb | −15 a −20 dB |

Además: LFO sincronizados al tempo (wobble, filtro del stab); distorsión antes en el bajo que en el pad; chorus en pads y guitarras, nunca en el sub; sidechain del bombo sobre bajo y pad en géneros de club.

### 5.2 Lo que deja el generador hoy (medido)

Niveles de cada rol solo, respecto de la batería, con todas las partes prendidas (4 semillas, 6 s cada render). El nivelador del master está activo, así que las diferencias reales son algo mayores que las de la tabla. NEO CLASSIC y FUGA no llevan batería: sus valores son niveles absolutos, no comparables con el resto.

| Género | batería (dB) | bajo | acordes | lead | arp | textura |
|---|---|---|---|---|---|---|
| Minimal Techno | -21.8 | 10.2 | -6.6 | -61.5 | -1.2 | – |
| Hard Techno | -13.3 | -4.6 | -11.5 | -12.7 | -11.8 | -15.6 |
| Hard Bass | -12.0 | -8.2 | -10.9 | -8.4 | -22.2 | -17.6 |
| House | -15.7 | -0.9 | -7.3 | -11.2 | -12.6 | – |
| Deep House | -13.0 | -4.6 | -18.0 | -10.1 | -10.3 | – |
| Tech House | -16.4 | -5.2 | -9.4 | -47.7 | -10.0 | – |
| Tribal | -18.7 | -4.5 | -6.1 | -46.0 | -5.4 | -10.6 |
| Cumbia | -16.6 | 2.3 | -14.9 | -8.7 | -20.8 | -29.9 |
| Reggaeton | -13.9 | 0.7 | -11.2 | -19.1 | -17.4 | -5.6 |
| Dembow | -16.5 | 4.8 | -13.0 | -5.3 | -26.0 | -20.6 |
| Electro | -13.6 | -5.8 | -10.3 | -4.3 | -9.5 | -10.3 |
| Breakbeat | -16.2 | -0.7 | -14.0 | -1.9 | -9.4 | -9.6 |
| Drum & Bass | -15.8 | -1.7 | -5.3 | -9.5 | -15.5 | -7.0 |
| Idm Glitch | -22.2 | 0.6 | -11.6 | -3.7 | -5.2 | -3.9 |
| Idm Ambient | -19.3 | -3.1 | -0.6 | -4.6 | -5.0 | -11.6 |
| Idm Weird | -23.6 | 2.5 | -17.4 | -8.5 | -2.7 | -4.3 |
| Kraut | -16.4 | 0.6 | -3.8 | -3.8 | -8.8 | -10.8 |
| Ebm | -14.6 | -3.7 | -11.2 | -19.2 | -10.0 | -34.0 |
| Neo Classic | 0.0 | -18.1 | -25.2 | -28.5 | -25.9 | -41.6 |
| Fuga | 0.0 | -20.1 | -14.2 | -31.7 | -30.7 | -30.8 |
| Electro Metal | -13.3 | -3.5 | -7.1 | -5.0 | -20.9 | -34.1 |
| Techno Rock | -17.1 | 1.4 | -6.9 | -3.0 | -16.3 | -25.1 |
| Dream Pop | -18.8 | 1.9 | -9.0 | -22.8 | -7.8 | -16.8 |
| Phonk | -13.9 | 1.9 | -5.6 | -14.5 | -18.4 | -8.6 |
| Baile Funk | -15.5 | -4.5 | -15.5 | -14.2 | -26.4 | -7.3 |
| Jazz | -18.1 | -1.7 | -15.1 | -7.8 | -16.4 | -36.4 |
| Hip Hop | -18.8 | 3.4 | -7.9 | -7.0 | -10.5 | -11.9 |
| Dubstep | -22.3 | -0.5 | -12.6 | -17.2 | -28.9 | -29.3 |
| Shoegaze | -15.4 | 4.5 | -7.6 | -16.5 | -7.6 | -17.7 |
| Metalcore | -10.7 | -3.8 | -7.4 | -10.5 | -20.2 | -42.6 |
| **Promedio** | | -2.0 | -10.6 | -15.5 | -14.5 | -19.0 |

Lo que se ve:
- **El bajo** está bien en el promedio (−2 dB) y donde se aparta es a propósito (minimal +10, dembow y hip hop +3/+5 por el 808; hard bass −8 es el único que queda corto para un género cuyo protagonista es el bajo).
- **Los acordes** promedian −10,6 dB, dentro del objetivo, pero con extremos: deep house −18, IDM weird −17, baile funk −15,5 (correcto, casi no lleva acordes), jazz −15 (bajo para un Rhodes que acompaña), IDM ambient −0,6 (correcto: el pad es el protagonista).
- **El lead** promedia −15,5 dB: unos 7 dB por debajo de la práctica. Los peores: dream pop −22,8, EBM −19,2, reggaeton −19,1, dubstep −17,2, shoegaze −16,5 (este a propósito), baile funk y phonk −14. Esto viene de un pedido tuyo anterior («los leads se comen la batería») y de que cada receta de sonido tiene su propia sonoridad. Es la causa más probable de que las piezas parezcan «sin melodía» en esos géneros.
- **El arp** va de −1,2 a −28,9 dB. Por debajo de −20 (inaudible en la mezcla): hard bass, cumbia, dembow, electro metal, baile funk, dubstep, metalcore.
- **La textura** por debajo de −30 en cumbia, EBM, electro metal, jazz y metalcore: no aporta; se puede apagar en esos géneros y ahorrar CPU.

Parámetros de síntesis (promedios de 16 semillas; el detalle por género está en la sección 7):
- **Bajo:** ATK 3–4, REV 1–3, DLY 0, sin chorus salvo hard bass (24). Correcto. REL 22–40: bien para club.
- **Pad:** REV 45–67, DLY 7–16, CHOR 19–31, FCUT 62–77 (ya bajado 15 %). Correcto para pads; para stabs de house y techno, REV 45–55 es mucho: un stab con tanta reverb embarra el contratiempo. Objetivo: 20–35.
- **Lead:** REV 31–39 y DLY 28–36 a la vez. La práctica es elegir uno: delay para ganchos rítmicos, reverb para notas largas.
- **Arp:** REL 32–53 en house, tech house, tribal y breakbeat: largo para un pluck; con delay 35–42 se superpone consigo mismo. Objetivo: REL 15–25.
- **Batería:** REV 6–10, DLY 3–4, bien. En `gen_kit` no audité la afinación del bombo respecto de la tónica: queda para la etapa 3.

### 5.3 Correcciones propuestas (etapa 2, sin aplicar todavía)

Todas son cambios de valores en tablas o constantes que ya existen (`GMIX`, `gen_mix`), así que cuestan entre 0 y unos 60 B. No las apliqué porque cambian el balance que ajustaste a oído y `tests/gen_test.c` tiene un control de mezcla que hay que revisar a la par.

1. Subir el lead 3 a 6 dB en dream pop, EBM, reggaeton y dubstep (tabla `GMIX`).
2. Subir el arp 6 dB o apagarlo por defecto donde queda debajo de −20 dB.
3. Reverb de stabs (acordes cortos) a la mitad en los géneros de club.
4. Lead: delay o reverb, no los dos altos, según el estilo de lead del género.
5. Release del arp más corto en los géneros de club.
6. Textura apagada donde queda debajo de −30 dB.
7. Sidechain: usar LIFE › PUMP en los géneros de club en lugar de solo bajar la velocidad del bajo bajo el bombo (hay que confirmar que el generador puede fijar PUMP por proyecto; estimo 30–60 B).

## 6. Etapas que quedan y su costo

Quedan 244 B en la imagen y 176 B en el segmento. Para lo que sigue hay que cambiar algo por algo.

| Etapa | Qué | Costo estimado | Riesgo |
|---|---|---|---|
| 2 | Mezcla y envíos por género (5.3), validada a oído | 0–60 B | cambia el balance actual |
| 3 | Recetas de sonido: ADSR, filtro y afinación del kit por rol y género; paleta por semilla | 0 B si son valores de `RECIPE`; ~150 B si hay reglas nuevas | medio |
| 4 | Verso / estribillo / puente de verdad en los géneros pop (hoy A, B y C comparten el motivo): estribillo con el gancho más agudo y más denso, verso más bajo | 300–500 B | alto: toca la forma |
| 5 | Segunda voz real (contramelodía o terceras) cuando el arp está apagado | ~250 B | medio |
| 6 | Batería: hats menos aleatorios en los géneros lentos, variación de 2 y 4 compases | ~150 B | medio |
| 7 | EXPLORE: que una de cada dos piezas use las reglas nuevas de sección y diálogo | ~40 B | bajo |

Lo que se puede sacar para pagar eso lo decidís vos; las opciones grandes son el espejo de pantalla (~1 KB) y más textos de ayuda (quedan pocos que no estén cubiertos por tests).

## 7. Género por género

Para cada género: la práctica del estilo, lo que tiene la tabla del generador y lo que medí. Los nombres en `código` son los de `gen_tables.c`. Los parámetros van en la escala 0–127 del equipo.

### Minimal Techno
- **Referencias:** Hawtin, Hood, Villalobos.
- **Tempo y swing que usa GEN:** 126–130 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** 4x4 seco, hats al contratiempo, clicks; el groove nace de microvariaciones, no de fills.
- **Bajo:** sub de una o dos notas en los huecos del bombo, nunca encima.
- **Armonía:** un solo acorde/stab disonante; la armonía es el filtro que se mueve.
- **Melodía:** sin lead: el gancho es el stab y su automatización.
- **Forma:** intro de bombo, capas de a una, break corto.
- **Paleta:** kit CLICK, sub senoidal, stab corto y oscuro, reverb chica.
- **Mezcla de referencia:** bombo y sub al frente; el resto 6–10 dB abajo.
- **En la tabla del generador:** mapa `DM_TECHNO`, kit `KIT_CLICK`, bajos `BA_MT1, BA_MT2, BA_MT3`, progresiones `PR_MSTAB, PR_MSTAB, PR_MSTAB`, lead `LD_NONE`, forma `FM_CLUB`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 10.2, acordes -6.6, lead -61.5, arp -1.2, textura –.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 115, FCUT 127, ATK 4, REL 26, REV 2, DLY 0, DIST 0, CHOR 0; pad (ANALOG7 PHASE5 WEST4): LEVEL 105, FCUT 77, ATK 4, REL 34, REV 46, DLY 10, DIST 7, CHOR 30; lead (ANALOG11 MATH5): LEVEL 84, FCUT 91, ATK 11, REL 45, REV 37, DLY 35, DIST 12, CHOR 11; arp (ANALOG7 WEST7 FM62): LEVEL 116, FCUT 80, ATK 4, REL 48, REV 27, DLY 32, DIST 0, CHOR 9.

### Hard Techno
- **Referencias:** I Hate Models, VTSS.
- **Tempo y swing que usa GEN:** 148–157 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** 4x4 a 145–160, kit industrial saturado, hats en semicorcheas.
- **Bajo:** rumble: semicorcheas entre bombos, filtradas y con reverb corta.
- **Armonía:** frigio o menor, un acorde o i–bII; stabs rave.
- **Melodía:** ostinato de hoover, 2–4 notas.
- **Forma:** club: intro, build con redoble, drop, break.
- **Paleta:** kit INDUS con drive, hoover, stabs.
- **Mezcla de referencia:** el bombo manda (pico), bajo 4–6 dB abajo, stabs atrás.
- **En la tabla del generador:** mapa `DM_HARD`, kit `KIT_INDUS`, bajos `BA_PSY, BA_ROLL16, BA_OCTBNC`, progresiones `PR_ONE, PR_PHRY, PR_I_VII`, lead `LD_OSTINATO`, forma `FM_CLUB`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -4.6, acordes -11.5, lead -12.7, arp -11.8, textura -15.6.
- **Parámetros que deja (promedios):** bass (ANALOG15 MATH1): LEVEL 127, FCUT 123, ATK 3, REL 25, REV 1, DLY 0, DIST 34, CHOR 0; pad (ANALOG5 PHASE5 WEST6): LEVEL 115, FCUT 71, ATK 2, REL 25, REV 46, DLY 7, DIST 6, CHOR 31; lead (ANALOG12 MATH4): LEVEL 84, FCUT 96, ATK 7, REL 32, REV 31, DLY 32, DIST 13, CHOR 50; arp (ANALOG9 WEST7): LEVEL 125, FCUT 88, ATK 4, REL 35, REV 28, DLY 29, DIST 0, CHOR 9.

### Hard Bass
- **Referencias:** XS Project.
- **Tempo y swing que usa GEN:** 149–157 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** 4x4 duro, clap en 2 y 4.
- **Bajo:** bajo «donk» en cada contratiempo, bombeado por el bombo.
- **Armonía:** menor, un acorde por compás (i–VI–III–VII).
- **Melodía:** riff simple de hoover, coros gritados.
- **Forma:** club.
- **Paleta:** reese/donk, hoover, voz.
- **Mezcla de referencia:** bajo al nivel del bombo, con sidechain.
- **En la tabla del generador:** mapa `DM_HBASS`, kit `KIT_INDUS`, bajos `BA_OFFBEAT, BA_HBASS, BA_OFFOCT`, progresiones `PR_AEOL, PR_TRANCE, PR_I_VI`, lead `LD_OSTINATO`, forma `FM_CLUB`, armonía `HF_POP | HX_RAISE`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -8.2, acordes -10.9, lead -8.4, arp -22.2, textura -17.6.
- **Parámetros que deja (promedios):** bass (ANALOG13 MATH3): LEVEL 127, FCUT 116, ATK 4, REL 40, REV 2, DLY 0, DIST 24, CHOR 24; pad (ANALOG7 PHASE7 WEST2): LEVEL 122, FCUT 77, ATK 4, REL 34, REV 45, DLY 9, DIST 4, CHOR 29; lead (ANALOG12 MATH4): LEVEL 105, FCUT 96, ATK 7, REL 39, REV 32, DLY 28, DIST 12, CHOR 44; arp (ANALOG7 WEST9): LEVEL 105, FCUT 80, ATK 3, REL 45, REV 27, DLY 36, DIST 0, CHOR 8.

### House
- **Referencias:** Knuckles, Jefferson, Kerri Chandler.
- **Tempo y swing que usa GEN:** 122–128 BPM medidos (16 semillas), swing 30.
- **Groove y batería:** 909, hat abierto al contratiempo, clap 2 y 4, swing ~56 %.
- **Bajo:** bajo que contesta al bombo (contratiempo, octavas).
- **Armonía:** séptimas menores: i7–iv7, i7–VImaj7; acordes en plano paralelo.
- **Melodía:** gancho de órgano/piano, corto y repetido.
- **Forma:** club: intro DJ, capas, break, drop.
- **Paleta:** 909, bajo FM, piano/órgano M1, pluck.
- **Mezcla de referencia:** bombo al frente, bajo −2/−4 dB, stabs −6/−8.
- **En la tabla del generador:** mapa `DM_HOUSE`, kit `KIT_909`, bajos `BA_OFFBEAT, BA_BOUNCE, BA_OFFOCT`, progresiones `PR_I_IV, PR_I_VI, PR_DEEP4`, lead `LD_HOOK`, forma `FM_CLUB`, armonía `HF_JAZZY`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -0.9, acordes -7.3, lead -11.2, arp -12.6, textura –.
- **Parámetros que deja (promedios):** bass (FM616): LEVEL 124, FCUT 106, ATK 4, REL 22, REV 3, DLY 0, DIST 7, CHOR 0; pad (ANALOG4 PHASE7 STRING5): LEVEL 107, FCUT 70, ATK 3, REL 30, REV 55, DLY 12, DIST 0, CHOR 19; lead (PHASE4 WHEEL12): LEVEL 86, FCUT 60, ATK 3, REL 26, REV 39, DLY 36, DIST 0, CHOR 9; arp (ANALOG7 WEST9): LEVEL 124, FCUT 82, ATK 4, REL 31, REV 32, DLY 42, DIST 0, CHOR 8.

### Deep House
- **Referencias:** Larry Heard, Chez Damier.
- **Tempo y swing que usa GEN:** 120–125 BPM medidos (16 semillas), swing 35.
- **Groove y batería:** 808 suave, swing ~57 %, shaker.
- **Bajo:** sub que camina, notas largas.
- **Armonía:** 7ª y 9ª: i9–iv9, ii–V–I, neo-soul.
- **Melodía:** notas largas, guía de terceras y séptimas.
- **Forma:** club suave.
- **Paleta:** Rhodes con delay, sub, pads.
- **Mezcla de referencia:** todo más bajo y cálido; pad −8/−10.
- **En la tabla del generador:** mapa `DM_DEEP`, kit `KIT_808`, bajos `BA_DEEPWALK, BA_OFFBEAT, BA_UKG`, progresiones `PR_I_IV, PR_JAZZ, PR_NSOUL`, lead `LD_SPARSE`, forma `FM_CLUB`, armonía `HF_JAZZY`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -4.6, acordes -18.0, lead -10.1, arp -10.3, textura –.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 106, FCUT 127, ATK 3, REL 24, REV 3, DLY 0, DIST 0, CHOR 0; pad (MATH16): LEVEL 102, FCUT 62, ATK 4, REL 47, REV 67, DLY 16, DIST 0, CHOR 19; lead (ANALOG6 MATH10): LEVEL 90, FCUT 79, ATK 5, REL 47, REV 56, DLY 40, DIST 0, CHOR 8; arp (ANALOG7 PHASE2 STRING7): LEVEL 118, FCUT 82, ATK 4, REL 53, REV 30, DLY 41, DIST 0, CHOR 18.

### Tech House
- **Referencias:** Green Velvet, Solardo.
- **Tempo y swing que usa GEN:** 124–130 BPM medidos (16 semillas), swing 22.
- **Groove y batería:** 909 que no afloja, hats shuffle, rim sincopado.
- **Bajo:** bajo rodante fuera del bombo, 1–2 notas.
- **Armonía:** un acorde; un stab con filtro que se mueve.
- **Melodía:** sin lead.
- **Forma:** club.
- **Paleta:** 909, bajo corto con drive leve, dub chord.
- **Mezcla de referencia:** bajo casi al nivel del bombo.
- **En la tabla del generador:** mapa `DM_TECHHOUSE`, kit `KIT_909`, bajos `BA_VILLA, BA_OFFOCT, BA_UKG`, progresiones `PR_ONE, PR_I_VII, PR_ONE`, lead `LD_NONE`, forma `FM_CLUB`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -5.2, acordes -9.4, lead -47.7, arp -10.0, textura –.
- **Parámetros que deja (promedios):** bass (ANALOG14 MATH2): LEVEL 126, FCUT 118, ATK 4, REL 23, REV 1, DLY 0, DIST 34, CHOR 0; pad (ANALOG6 MATH2 WHEEL8): LEVEL 119, FCUT 71, ATK 3, REL 43, REV 47, DLY 14, DIST 0, CHOR 34; lead (ANALOG11 MATH5): LEVEL 84, FCUT 93, ATK 13, REL 44, REV 34, DLY 33, DIST 10, CHOR 12; arp (ANALOG7 WEST5 FM64): LEVEL 119, FCUT 81, ATK 3, REL 48, REV 27, DLY 35, DIST 0, CHOR 10.

### Tribal
- **Referencias:** Tenaglia, Ben Sims.
- **Tempo y swing que usa GEN:** 125–131 BPM medidos (16 semillas), swing 14.
- **Groove y batería:** 4x4 bajo toms, congas, shaker, cencerro.
- **Bajo:** bajo rodante.
- **Armonía:** un acorde.
- **Melodía:** canto/chant, casi sin melodía.
- **Forma:** club.
- **Paleta:** 909 + percusión, coro.
- **Mezcla de referencia:** percusión arriba, armonía mínima.
- **En la tabla del generador:** mapa `DM_TRIBAL`, kit `KIT_909`, bajos `BA_VILLA, BA_OFFBEAT, BA_JUNGLE`, progresiones `PR_ONE, PR_I_VII, PR_ONE`, lead `LD_NONE`, forma `FM_CLUB`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -4.5, acordes -6.1, lead -46.0, arp -5.4, textura -10.6.
- **Parámetros que deja (promedios):** bass (ANALOG15 MATH1): LEVEL 127, FCUT 123, ATK 4, REL 24, REV 2, DLY 0, DIST 33, CHOR 0; pad (ANALOG7 PHASE6 WEST3): LEVEL 117, FCUT 75, ATK 4, REL 33, REV 50, DLY 14, DIST 9, CHOR 28; lead (ANALOG13 MATH3): LEVEL 81, FCUT 98, ATK 12, REL 42, REV 34, DLY 34, DIST 17, CHOR 11; arp (ANALOG7 WEST9): LEVEL 123, FCUT 79, ATK 4, REL 44, REV 36, DLY 38, DIST 6, CHOR 8.

### Cumbia
- **Referencias:** sonidera, villera, digital.
- **Tempo y swing que usa GEN:** 82–98 BPM medidos (16 semillas), swing 10.
- **Groove y batería:** güiro en cada tiempo, llamador en 2 y 4, bombo 1 y 3.
- **Bajo:** bajo sincopado tónica–quinta.
- **Armonía:** I–IV–V, i–iv–V; teclado en el «y».
- **Melodía:** gancho de órgano/acordeón, en terceras.
- **Forma:** partes A B A C.
- **Paleta:** órgano, bajo FM, güiro.
- **Mezcla de referencia:** bajo y güiro al frente, gancho claro.
- **En la tabla del generador:** mapa `DM_CUMBIA`, kit `KIT_909`, bajos `BA_CUMBIA, BA_CUMB3, BA_CUMBIA`, progresiones `PR_CYCLE, PR_I_V, PR_I_IV`, lead `LD_HOOK`, forma `FM_PARTS`, armonía `HF_POP | HX_RAISE`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 2.3, acordes -14.9, lead -8.7, arp -20.8, textura -29.9.
- **Parámetros que deja (promedios):** bass (ANALOG4 PHASE3 FM69): LEVEL 107, FCUT 111, ATK 3, REL 24, REV 2, DLY 0, DIST 22, CHOR 0; pad (PHASE5 WHEEL11): LEVEL 98, FCUT 63, ATK 4, REL 20, REV 54, DLY 56, DIST 16, CHOR 36; lead (ANALOG3 PHASE3 WHEEL10): LEVEL 85, FCUT 67, ATK 5, REL 27, REV 38, DLY 34, DIST 14, CHOR 9; arp (ANALOG7 WEST9): LEVEL 102, FCUT 80, ATK 3, REL 21, REV 31, DLY 37, DIST 10, CHOR 38.

### Reggaeton
- **Referencias:** clásico y romántico.
- **Tempo y swing que usa GEN:** 88–97 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** dembow 3+3+2, bombo en negras, caja en 4ª, 7ª, 12ª y 15ª semicorchea.
- **Bajo:** 808 con el bombo o en tresillo.
- **Armonía:** menor: i–VI–III–VII, i–iv.
- **Melodía:** gancho corto de pluck/marimba.
- **Forma:** partes con pre-coro.
- **Paleta:** 808, pluck, pad.
- **Mezcla de referencia:** batería y 808 arriba, pad abajo del arp.
- **En la tabla del generador:** mapa `DM_REGG`, kit `KIT_808`, bajos `BA_REGG, BA_RGTRES, BA_REGG`, progresiones `PR_AEOL, PR_I_VI, PR_EPIC`, lead `LD_HOOK`, forma `FM_PARTS`, armonía `HF_POP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 0.7, acordes -11.2, lead -19.1, arp -17.4, textura -5.6.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 105, FCUT 127, ATK 4, REL 38, REV 2, DLY 0, DIST 37, CHOR 0; pad (ANALOG9 PHASE2 MATH5): LEVEL 80, FCUT 81, ATK 74, REL 47, REV 48, DLY 10, DIST 4, CHOR 43; lead (ANALOG9 WEST7): LEVEL 100, FCUT 89, ATK 4, REL 21, REV 34, DLY 33, DIST 15, CHOR 7; arp (ANALOG3 VOICE6 STRING5 WEST2): LEVEL 123, FCUT 73, ATK 3, REL 21, REV 28, DLY 31, DIST 7, CHOR 6.

### Dembow
- **Referencias:** dembow dominicano.
- **Tempo y swing que usa GEN:** 116–131 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** riddim rápido 115–125, hats en semicorcheas.
- **Bajo:** 808 corto.
- **Armonía:** 1–2 acordes, frigio.
- **Melodía:** chants en 3-6-11-14, riff de marimba.
- **Forma:** partes.
- **Paleta:** 808, marimba, voces.
- **Mezcla de referencia:** riff y chants arriba.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 4.8, acordes -13.0, lead -5.3, arp -26.0, textura -20.6.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 113, FCUT 127, ATK 3, REL 40, REV 2, DLY 0, DIST 40, CHOR 0; pad (ANALOG4 PHASE7 WEST5): LEVEL 106, FCUT 66, ATK 5, REL 21, REV 43, DLY 9, DIST 7, CHOR 24; lead (STRING16): LEVEL 121, FCUT 66, ATK 2, REL 20, REV 31, DLY 29, DIST 16, CHOR 7; arp (VOICE16): LEVEL 126, FCUT 67, ATK 4, REL 20, REV 25, DLY 25, DIST 7, CHOR 0.

### Electro
- **Referencias:** Anthony Rother, Drexciya.
- **Tempo y swing que usa GEN:** 128–134 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** 808, bombo sincopado (no 4x4), caja en 2 y 4.
- **Bajo:** bajo secuenciado, interlock con el bombo.
- **Armonía:** menor/frigio: i–VII–VI–VII.
- **Melodía:** arpegio de octavas, vocoder.
- **Forma:** club.
- **Paleta:** 808, chip, lead cuadrado.
- **Mezcla de referencia:** seco, poca reverb.
- **En la tabla del generador:** mapa `DM_ELECTRO`, kit `KIT_808`, bajos `BA_ROTHER, BA_SYNTHW, BA_ANGULAR`, progresiones `PR_ELEC, PR_I_VII, PR_ELEC`, lead `LD_ELEC`, forma `FM_CLUB`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -5.8, acordes -10.3, lead -4.3, arp -9.5, textura -10.3.
- **Parámetros que deja (promedios):** bass (ANALOG15 WEST1): LEVEL 124, FCUT 124, ATK 5, REL 24, REV 2, DLY 0, DIST 11, CHOR 16; pad (ANALOG7 PHASE8 WEST1): LEVEL 112, FCUT 76, ATK 3, REL 33, REV 43, DLY 11, DIST 8, CHOR 27; lead (ANALOG13 MATH3): LEVEL 96, FCUT 98, ATK 12, REL 43, REV 30, DLY 32, DIST 5, CHOR 10; arp (ANALOG4 MATH12): LEVEL 118, FCUT 85, ATK 4, REL 34, REV 23, DLY 28, DIST 0, CHOR 0.

### Breakbeat
- **Referencias:** Chemical Brothers, big beat.
- **Tempo y swing que usa GEN:** 127–137 BPM medidos (16 semillas), swing 6.
- **Groove y batería:** break sincopado con ghost notes.
- **Bajo:** ácido o jungle, interlock.
- **Armonía:** un acorde o i–iv.
- **Melodía:** ostinato.
- **Forma:** partes.
- **Paleta:** kit BREAKS, acid.
- **Mezcla de referencia:** batería y bajo parejos.
- **En la tabla del generador:** mapa `DM_BREAKS`, kit `KIT_BREAKS`, bajos `BA_ACID, BA_JUNGLE, BA_ACID`, progresiones `PR_ONE, PR_I_IV, PR_I_VI`, lead `LD_OSTINATO`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -0.7, acordes -14.0, lead -1.9, arp -9.4, textura -9.6.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 114, FCUT 127, ATK 3, REL 24, REV 2, DLY 0, DIST 26, CHOR 0; pad (ANALOG10 PHASE4 WEST2): LEVEL 103, FCUT 85, ATK 4, REL 30, REV 53, DLY 15, DIST 4, CHOR 30; lead (ANALOG9 MATH7): LEVEL 97, FCUT 90, ATK 9, REL 36, REV 38, DLY 39, DIST 14, CHOR 46; arp (ANALOG10 WEST6): LEVEL 124, FCUT 90, ATK 3, REL 44, REV 33, DLY 40, DIST 0, CHOR 14.

### Drum & Bass
- **Referencias:** liquid, neuro.
- **Tempo y swing que usa GEN:** 172–176 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** 170–176, bombo en 1 y 3.5, caja en 2 y 4.
- **Bajo:** reese o sub largo, medio tiempo.
- **Armonía:** menor con 7ª y 9ª.
- **Melodía:** notas largas, poco.
- **Forma:** partes: intro, drop, break, drop.
- **Paleta:** reese, pads, break.
- **Mezcla de referencia:** bajo al nivel de la batería.
- **En la tabla del generador:** mapa `DM_DNB`, kit `KIT_BREAKS`, bajos `BA_DNB, BA_JUNGLE, BA_WOBBLE`, progresiones `PR_I_VI, PR_DEEP4, PR_MINOR4`, lead `LD_SPARSE`, forma `FM_CLUB`, armonía `HF_JAZZY`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -1.7, acordes -5.3, lead -9.5, arp -15.5, textura -7.0.
- **Parámetros que deja (promedios):** bass (ANALOG14 MATH2): LEVEL 110, FCUT 119, ATK 4, REL 38, REV 2, DLY 0, DIST 24, CHOR 29; pad (ANALOG8 PHASE4 MATH4): LEVEL 92, FCUT 78, ATK 81, REL 50, REV 49, DLY 10, DIST 0, CHOR 42; lead (ANALOG11 MATH5): LEVEL 98, FCUT 92, ATK 10, REL 43, REV 41, DLY 32, DIST 6, CHOR 13; arp (ANALOG6 WEST10): LEVEL 119, FCUT 82, ATK 4, REL 21, REV 35, DLY 36, DIST 0, CHOR 10.

### Idm Glitch
- **Referencias:** Aphex, Autechre, Squarepusher.
- **Tempo y swing que usa GEN:** 140–159 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** break programado y luego editado: cortes, retrigs.
- **Bajo:** ácido/angular.
- **Armonía:** ciclos menores con 7ª, sin cadencia fuerte.
- **Melodía:** motivo de campanas, desarrollado.
- **Forma:** arco.
- **Paleta:** kit CLICK, FM, campanas.
- **Mezcla de referencia:** batería más baja, melodía clara.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 0.6, acordes -11.6, lead -3.7, arp -5.2, textura -3.9.
- **Parámetros que deja (promedios):** bass (FM616): LEVEL 102, FCUT 109, ATK 4, REL 22, REV 2, DLY 0, DIST 8, CHOR 0; pad (MATH16): LEVEL 96, FCUT 67, ATK 4, REL 21, REV 64, DLY 15, DIST 0, CHOR 15; lead (WEST16): LEVEL 111, FCUT 74, ATK 4, REL 23, REV 38, DLY 41, DIST 0, CHOR 8; arp (WEST16): LEVEL 105, FCUT 74, ATK 4, REL 23, REV 34, DLY 41, DIST 0, CHOR 7.

### Idm Ambient
- **Referencias:** Boards of Canada.
- **Tempo y swing que usa GEN:** 88–96 BPM medidos (16 semillas), swing 30.
- **Groove y batería:** hip hop lento, swing, kit blando.
- **Bajo:** pedal o quintas.
- **Armonía:** lidio/dórico, acordes sus y maj7, sin dominante.
- **Melodía:** notas largas, nostálgico.
- **Forma:** arco lento.
- **Paleta:** pads desafinados (wow), drones.
- **Mezcla de referencia:** pad al frente, batería atrás.
- **En la tabla del generador:** mapa `DM_BOC`, kit `KIT_SOFT`, bajos `BA_PEDAL, BA_FIFTHS, BA_DUST`, progresiones `PR_IDM1, PR_IDM2, PR_SUS`, lead `LD_SPARSE`, forma `FM_ARC`, armonía `HF_COLOR`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -3.1, acordes -0.6, lead -4.6, arp -5.0, textura -11.6.
- **Parámetros que deja (promedios):** bass (ANALOG14 MATH2): LEVEL 87, FCUT 118, ATK 3, REL 22, REV 4, DLY 0, DIST 22, CHOR 0; pad (ANALOG9 PHASE5 MATH2): LEVEL 88, FCUT 82, ATK 77, REL 49, REV 75, DLY 16, DIST 15, CHOR 38; lead (ANALOG8 MATH8): LEVEL 96, FCUT 87, ATK 4, REL 23, REV 59, DLY 58, DIST 12, CHOR 6; arp (WEST16): LEVEL 114, FCUT 71, ATK 3, REL 23, REV 40, DLY 53, DIST 10, CHOR 39.

### Idm Weird
- **Referencias:** Plaid, µ-Ziq.
- **Tempo y swing que usa GEN:** 105–125 BPM medidos (16 semillas), swing 12.
- **Groove y batería:** metros 3:4, bombos en semicorcheas.
- **Bajo:** angular.
- **Armonía:** lidio/mixolidio, movimientos de segunda.
- **Melodía:** motivo con cromatismo.
- **Forma:** arco.
- **Paleta:** chip, campanas, lo-fi.
- **Mezcla de referencia:** libre.
- **En la tabla del generador:** mapa `DM_BREAKS`, kit `KIT_CLICK`, bajos `BA_ANGULAR, BA_DUST, BA_ACID`, progresiones `PR_I_II, PR_IDM1, PR_DEEPVI`, lead `LD_MOTIF`, forma `FM_ARC`, armonía `HF_COLOR`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 2.5, acordes -17.4, lead -8.5, arp -2.7, textura -4.3.
- **Parámetros que deja (promedios):** bass (PHASE2 FM614): LEVEL 100, FCUT 104, ATK 3, REL 23, REV 1, DLY 0, DIST 15, CHOR 0; pad (MATH16): LEVEL 86, FCUT 66, ATK 4, REL 21, REV 62, DLY 11, DIST 22, CHOR 17; lead (WEST16): LEVEL 102, FCUT 69, ATK 3, REL 23, REV 45, DLY 47, DIST 15, CHOR 7; arp (ANALOG10 MATH6): LEVEL 101, FCUT 97, ATK 2, REL 28, REV 36, DLY 40, DIST 16, CHOR 0.

### Kraut
- **Referencias:** Neu!, Kraftwerk, Can.
- **Tempo y swing que usa GEN:** 122–133 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** motorik: bombo en negras, caja 2 y 4, sin fills.
- **Bajo:** bajo en corcheas.
- **Armonía:** un acorde o I–bVII.
- **Melodía:** línea por grados (Pärt).
- **Forma:** arco que crece.
- **Paleta:** guitarra, drone.
- **Mezcla de referencia:** hipnótico, parejo.
- **En la tabla del generador:** mapa `DM_MOTORIK`, kit `KIT_SOFT`, bajos `BA_MOTORIK, BA_ROCK8, BA_PUMP8`, progresiones `PR_ONE, PR_PROG2, PR_I_VII`, lead `LD_TINTI`, forma `FM_ARC`, armonía `HF_LOOP | HX_BOR`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 0.6, acordes -3.8, lead -3.8, arp -8.8, textura -10.8.
- **Parámetros que deja (promedios):** bass (ANALOG15 STRING1): LEVEL 112, FCUT 124, ATK 4, REL 27, REV 2, DLY 0, DIST 26, CHOR 14; pad (PHASE10 STRING6): LEVEL 111, FCUT 57, ATK 5, REL 23, REV 61, DLY 11, DIST 17, CHOR 39; lead (ANALOG13 MATH3): LEVEL 96, FCUT 100, ATK 12, REL 43, REV 39, DLY 42, DIST 16, CHOR 9; arp (ANALOG8 WEST8): LEVEL 122, FCUT 85, ATK 3, REL 21, REV 34, DLY 42, DIST 10, CHOR 38.

### Ebm
- **Referencias:** Nitzer Ebb, DAF, Front 242.
- **Tempo y swing que usa GEN:** 114–155 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** 4x4 seco, caja dura.
- **Bajo:** secuencia de semicorcheas en un acorde.
- **Armonía:** frigio/menor, un acorde.
- **Melodía:** gritos, dos notas.
- **Forma:** partes.
- **Paleta:** MS-20, kit INDUS.
- **Mezcla de referencia:** bajo y bombo arriba, poca reverb.
- **En la tabla del generador:** mapa `DM_EBM`, kit `KIT_INDUS`, bajos `BA_NITZER1, BA_DAF, BA_GALLOP`, progresiones `PR_ONE, PR_PHRY, PR_I_VI`, lead `LD_EBM`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -3.7, acordes -11.2, lead -19.2, arp -10.0, textura -34.0.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 122, FCUT 127, ATK 4, REL 18, REV 1, DLY 0, DIST 36, CHOR 0; pad (ANALOG5 PHASE7 WEST4): LEVEL 111, FCUT 68, ATK 3, REL 31, REV 34, DLY 8, DIST 7, CHOR 29; lead (ANALOG6 VOICE10): LEVEL 125, FCUT 84, ATK 3, REL 20, REV 23, DLY 31, DIST 11, CHOR 0; arp (ANALOG9 WEST7): LEVEL 125, FCUT 85, ATK 4, REL 41, REV 19, DLY 23, DIST 0, CHOR 11.

### Neo Classic
- **Referencias:** Nils Frahm, Ólafur Arnalds.
- **Tempo y swing que usa GEN:** 80–103 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** sin batería.
- **Bajo:** arpegio de piano, pedal.
- **Armonía:** funcional con dominantes secundarias y préstamos.
- **Melodía:** línea por grados.
- **Forma:** arco.
- **Paleta:** piano, cuerdas.
- **Mezcla de referencia:** mucho espacio y reverb.
- **En la tabla del generador:** mapa `DM_NONE`, kit `KIT_SOFT`, bajos `BA_NEOARP, BA_PEDAL, BA_FIFTHS`, progresiones `PR_50S, PR_NEOCL, PR_CYCLE`, lead `LD_TINTI`, forma `FM_ARC`, armonía `HF_CINE | HX_RAISE | HX_SEC | HX_BOR`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -18.1, acordes -25.2, lead -28.5, arp -25.9, textura -41.6.
- **Parámetros que deja (promedios):** bass (PHASE2 STRING11 FM63): LEVEL 117, FCUT 93, ATK 3, REL 21, REV 3, DLY 0, DIST 0, CHOR 7; pad (ANALOG14 STRING2): LEVEL 91, FCUT 100, ATK 56, REL 35, REV 81, DLY 12, DIST 0, CHOR 55; lead (PHASE5 STRING11): LEVEL 95, FCUT 61, ATK 4, REL 22, REV 53, DLY 56, DIST 0, CHOR 11; arp (PHASE5 STRING11): LEVEL 102, FCUT 59, ATK 3, REL 22, REV 41, DLY 53, DIST 0, CHOR 8.

### Fuga
- **Referencias:** Bach.
- **Tempo y swing que usa GEN:** 76–91 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** sin batería.
- **Bajo:** voz de bajo imitativa.
- **Armonía:** i–iv–V–i, cadencia andaluza.
- **Melodía:** sujeto y respuesta tonal.
- **Forma:** entradas de voces.
- **Paleta:** clave, órgano.
- **Mezcla de referencia:** voces parejas.
- **En la tabla del generador:** mapa `DM_NONE`, kit `KIT_SOFT`, bajos `BA_FIFTHS, BA_NEOARP, BA_PEDAL`, progresiones `PR_CYCLE, PR_I_V, PR_ANDAL`, lead `LD_MOTIF`, forma `FM_ARC`, armonía `HF_CINE | HX_RAISE`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -20.1, acordes -14.2, lead -31.7, arp -30.7, textura -30.8.
- **Parámetros que deja (promedios):** bass (PHASE3 STRING13): LEVEL 125, FCUT 112, ATK 4, REL 24, REV 2, DLY 0, DIST 0, CHOR 6; pad (PHASE4 WHEEL12): LEVEL 111, FCUT 65, ATK 16, REL 35, REV 58, DLY 12, DIST 0, CHOR 12; lead (PHASE5 STRING11): LEVEL 102, FCUT 82, ATK 5, REL 24, REV 39, DLY 43, DIST 0, CHOR 8; arp (PHASE6 STRING10): LEVEL 108, FCUT 82, ATK 3, REL 23, REV 34, DLY 43, DIST 0, CHOR 6.

### Electro Metal
- **Referencias:** Rammstein, NIN.
- **Tempo y swing que usa GEN:** 108–131 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** doble bombo, caja en 3.
- **Bajo:** power chords palm mute, en corcheas/semicorcheas con el bombo.
- **Armonía:** frigio, i–bII, i–VI.
- **Melodía:** ostinato.
- **Forma:** partes: riff, verso, estribillo.
- **Paleta:** guitarra distorsionada, kit INDUS.
- **Mezcla de referencia:** guitarra y batería parejas.
- **En la tabla del generador:** mapa `DM_METAL`, kit `KIT_INDUS`, bajos `BA_PUNK16, BA_GALLOP, BA_ROCK8`, progresiones `PR_ONE, PR_PHRY, PR_I_VI`, lead `LD_OSTINATO`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -3.5, acordes -7.1, lead -5.0, arp -20.9, textura -34.1.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 119, FCUT 127, ATK 4, REL 15, REV 1, DLY 0, DIST 48, CHOR 0; pad (ANALOG3 PHASE2 STRING11): LEVEL 89, FCUT 65, ATK 4, REL 19, REV 38, DLY 10, DIST 88, CHOR 8; lead (ANALOG12 MATH4): LEVEL 76, FCUT 94, ATK 10, REL 43, REV 30, DLY 27, DIST 62, CHOR 13; arp (ANALOG9 PHASE2 WEST5): LEVEL 81, FCUT 85, ATK 4, REL 23, REV 23, DLY 29, DIST 38, CHOR 25.

### Techno Rock
- **Referencias:** Prodigy, synth 80s.
- **Tempo y swing que usa GEN:** 104–137 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** break o 4x4 con toms.
- **Bajo:** bajo en semicorcheas.
- **Armonía:** un acorde, i–VII.
- **Melodía:** ostinato/riff.
- **Forma:** partes.
- **Paleta:** hoover, guitarra.
- **Mezcla de referencia:** batería arriba.
- **En la tabla del generador:** mapa `DM_TROCK`, kit `KIT_BREAKS`, bajos `BA_SEQ16, BA_ROLL16, BA_PUNK16`, progresiones `PR_ONE, PR_I_VII, PR_PHRY`, lead `LD_OSTINATO`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 1.4, acordes -6.9, lead -3.0, arp -16.3, textura -25.1.
- **Parámetros que deja (promedios):** bass (ANALOG14 STRING1 WEST1): LEVEL 109, FCUT 120, ATK 4, REL 24, REV 2, DLY 0, DIST 51, CHOR 8; pad (ANALOG3 PHASE5 MATH2 STRING6): LEVEL 82, FCUT 67, ATK 34, REL 30, REV 54, DLY 10, DIST 88, CHOR 21; lead (ANALOG10 MATH6): LEVEL 75, FCUT 93, ATK 10, REL 31, REV 40, DLY 39, DIST 63, CHOR 36; arp (ANALOG8 WEST8): LEVEL 92, FCUT 86, ATK 3, REL 23, REV 31, DLY 36, DIST 42, CHOR 5.

### Dream Pop
- **Referencias:** Cocteau Twins, Beach House.
- **Tempo y swing que usa GEN:** 75–95 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** lento, bombo 1 y 3, caja 3.
- **Bajo:** tónica–quinta, notas largas.
- **Armonía:** mayor/lidio con add9 y maj7; IV menor prestado.
- **Melodía:** voz larga, grados conjuntos.
- **Forma:** arco.
- **Paleta:** pads, guitarra con chorus, voz.
- **Mezcla de referencia:** pared de reverb; voz adentro.
- **En la tabla del generador:** mapa `DM_DREAM`, kit `KIT_SOFT`, bajos `BA_FIFTHS, BA_ROCK8, BA_NEOARP`, progresiones `PR_DREAM, PR_DREAM2, PR_PROG1`, lead `LD_SPARSE`, forma `FM_ARC`, armonía `HF_COLOR`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 1.9, acordes -9.0, lead -22.8, arp -7.8, textura -16.8.
- **Parámetros que deja (promedios):** bass (ANALOG13 MATH3): LEVEL 99, FCUT 114, ATK 3, REL 25, REV 4, DLY 0, DIST 22, CHOR 0; pad (ANALOG4 PHASE5 MATH7): LEVEL 78, FCUT 62, ATK 85, REL 42, REV 82, DLY 17, DIST 15, CHOR 38; lead (MATH7 VOICE9): LEVEL 72, FCUT 65, ATK 12, REL 43, REV 61, DLY 55, DIST 13, CHOR 21; arp (PHASE13 STRING3): LEVEL 95, FCUT 56, ATK 4, REL 22, REV 47, DLY 60, DIST 14, CHOR 37.

### Phonk
- **Referencias:** drift phonk, Memphis.
- **Tempo y swing que usa GEN:** 128–150 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** trap o brasileño, cowbell.
- **Bajo:** 808 largo con glide.
- **Armonía:** frigio, 1–2 acordes.
- **Melodía:** riff de cowbell.
- **Forma:** partes.
- **Paleta:** 808, cowbell, chants.
- **Mezcla de referencia:** 808 y cowbell arriba, todo saturado.
- **En la tabla del generador:** mapa `DM_PHONK`, kit `KIT_808`, bajos `BA_WOBBLE, BA_PH808, BA_DNB`, progresiones `PR_ONE, PR_I_VI, PR_PHRY`, lead `LD_RIFF`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 1.9, acordes -5.6, lead -14.5, arp -18.4, textura -8.6.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 108, FCUT 127, ATK 4, REL 40, REV 1, DLY 0, DIST 41, CHOR 0; pad (ANALOG7 PHASE5 MATH4): LEVEL 90, FCUT 75, ATK 83, REL 49, REV 51, DLY 9, DIST 0, CHOR 38; lead (ANALOG16): LEVEL 120, FCUT 107, ATK 4, REL 19, REV 35, DLY 37, DIST 16, CHOR 0; arp (VOICE16): LEVEL 127, FCUT 67, ATK 4, REL 20, REV 27, DLY 33, DIST 5, CHOR 0.

### Baile Funk
- **Referencias:** funk carioca 150.
- **Tempo y swing que usa GEN:** 129–150 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** tamborzão: tom en 3-6-12.
- **Bajo:** 808 corto con glide.
- **Armonía:** sin acordes.
- **Melodía:** riff/flauta.
- **Forma:** partes.
- **Paleta:** 808, cencerros.
- **Mezcla de referencia:** percusión y 808.
- **En la tabla del generador:** mapa `DM_BAILE`, kit `KIT_909`, bajos `BA_TAMBOR, BA_MANDEL, BA_FUNK3`, progresiones `PR_ONE, PR_I_VII, PR_I_IV`, lead `LD_RIFF`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -4.5, acordes -15.5, lead -14.2, arp -26.4, textura -7.3.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 99, FCUT 127, ATK 4, REL 39, REV 1, DLY 0, DIST 70, CHOR 0; pad (ANALOG7 PHASE5 WEST4): LEVEL 99, FCUT 75, ATK 3, REL 29, REV 37, DLY 8, DIST 18, CHOR 30; lead (ANALOG11 WHEEL2 ANIMAL3): LEVEL 107, FCUT 101, ATK 5, REL 24, REV 27, DLY 28, DIST 42, CHOR 0; arp (VOICE16): LEVEL 126, FCUT 65, ATK 3, REL 23, REV 22, DLY 25, DIST 10, CHOR 0.

### Jazz
- **Referencias:** trío/cuarteto.
- **Tempo y swing que usa GEN:** 92–98 BPM medidos (16 semillas), swing 66.
- **Groove y batería:** ride swing, hi-hat en 2 y 4, comping.
- **Bajo:** walking bass en negras.
- **Armonía:** ii–V–I, turnarounds, dominantes secundarias.
- **Melodía:** motivo desarrollado, aproximaciones cromáticas.
- **Forma:** AABA / chorus.
- **Paleta:** contrabajo, Rhodes/piano.
- **Mezcla de referencia:** acústico, parejo, poca compresión.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -1.7, acordes -15.1, lead -7.8, arp -16.4, textura -36.4.
- **Parámetros que deja (promedios):** bass (ANALOG16): LEVEL 118, FCUT 127, ATK 3, REL 32, REV 2, DLY 0, DIST 0, CHOR 0; pad (MATH16): LEVEL 97, FCUT 68, ATK 4, REL 24, REV 46, DLY 11, DIST 0, CHOR 22; lead (ANALOG8 MATH8): LEVEL 90, FCUT 90, ATK 3, REL 24, REV 35, DLY 34, DIST 0, CHOR 7; arp (PHASE4 STRING12): LEVEL 98, FCUT 70, ATK 4, REL 23, REV 28, DLY 31, DIST 0, CHOR 9.

### Hip Hop
- **Referencias:** trap, drill, boom bap, lo-fi.
- **Tempo y swing que usa GEN:** 76–145 BPM medidos (16 semillas), swing 45.
- **Groove y batería:** trap: hats en rolls, caja en 3; boom bap: swing.
- **Bajo:** 808 con el bombo.
- **Armonía:** menor: i–VI, neo-soul.
- **Melodía:** gancho corto (bell/pluck).
- **Forma:** verso / estribillo.
- **Paleta:** 808, pads, bells.
- **Mezcla de referencia:** 808 y batería arriba.
- **En la tabla del generador:** mapa `DM_TRAP`, kit `KIT_808`, bajos `BA_WOBBLE, BA_PH808, BA_JUNGLE`, progresiones `PR_NSOUL2, PR_MINOR4, PR_NSOUL`, lead `LD_HOOK`, forma `FM_PARTS`, armonía `HF_POP | HX_RAISE`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 3.4, acordes -7.9, lead -7.0, arp -10.5, textura -11.9.
- **Parámetros que deja (promedios):** bass (ANALOG14 MATH2): LEVEL 93, FCUT 119, ATK 4, REL 32, REV 1, DLY 0, DIST 28, CHOR 0; pad (ANALOG4 PHASE1 MATH9 STRING2): LEVEL 87, FCUT 73, ATK 28, REL 31, REV 55, DLY 12, DIST 21, CHOR 31; lead (MATH2 WEST14): LEVEL 112, FCUT 80, ATK 5, REL 23, REV 36, DLY 37, DIST 17, CHOR 10; arp (ANALOG10 WEST6): LEVEL 115, FCUT 90, ATK 4, REL 21, REV 30, DLY 37, DIST 18, CHOR 7.

### Dubstep
- **Referencias:** Skream, Skrillex.
- **Tempo y swing que usa GEN:** 138–150 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** medio tiempo 140: bombo 1, caja 3.
- **Bajo:** wobble: LFO sincronizado al filtro.
- **Armonía:** menor/frigio, un acorde.
- **Melodía:** riff del bajo.
- **Forma:** intro, drop, break, drop.
- **Paleta:** wobble, growl.
- **Mezcla de referencia:** el bajo es el protagonista.
- **En la tabla del generador:** mapa `DM_DUBSTEP`, kit `KIT_BREAKS`, bajos `BA_WOBBLE, BA_DNB, BA_PEDAL`, progresiones `PR_ONE, PR_PHRY, PR_I_VI`, lead `LD_RIFF`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -0.5, acordes -12.6, lead -17.2, arp -28.9, textura -29.3.
- **Parámetros que deja (promedios):** bass (ANALOG8 MATH8): LEVEL 117, FCUT 92, ATK 3, REL 31, REV 1, DLY 0, DIST 70, CHOR 0; pad (ANALOG6 PHASE6 MATH4): LEVEL 83, FCUT 70, ATK 80, REL 67, REV 49, DLY 10, DIST 17, CHOR 38; lead (ANALOG6 MATH4 WHEEL2 ANIMAL4): LEVEL 89, FCUT 91, ATK 6, REL 29, REV 31, DLY 30, DIST 43, CHOR 32; arp (ANALOG11 WEST4 FM61): LEVEL 102, FCUT 92, ATK 4, REL 32, REV 28, DLY 31, DIST 9, CHOR 8.

### Shoegaze
- **Referencias:** My Bloody Valentine, Slowdive.
- **Tempo y swing que usa GEN:** 103–121 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** rock, toms.
- **Bajo:** corcheas en tónica.
- **Armonía:** mayor/lidio, I–IV.
- **Melodía:** voz enterrada.
- **Forma:** arco.
- **Paleta:** pared de guitarras con fuzz y chorus.
- **Mezcla de referencia:** la pared sobre todo; voz atrás.
- **En la tabla del generador:** mapa `DM_ROCK`, kit `KIT_SOFT`, bajos `BA_ROCK8, BA_PEDAL, BA_FIFTHS`, progresiones `PR_DREAM, PR_I_IV, PR_DREAM2`, lead `LD_SPARSE`, forma `FM_ARC`, armonía `HF_COLOR`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo 4.5, acordes -7.6, lead -16.5, arp -7.6, textura -17.7.
- **Parámetros que deja (promedios):** bass (ANALOG14 STRING2): LEVEL 109, FCUT 121, ATK 4, REL 25, REV 3, DLY 0, DIST 30, CHOR 16; pad (ANALOG12 STRING4): LEVEL 78, FCUT 92, ATK 46, REL 46, REV 85, DLY 21, DIST 71, CHOR 76; lead (MATH5 VOICE11): LEVEL 62, FCUT 62, ATK 10, REL 46, REV 66, DLY 63, DIST 49, CHOR 20; arp (PHASE13 STRING3): LEVEL 74, FCUT 56, ATK 6, REL 21, REV 50, DLY 66, DIST 61, CHOR 68.

### Metalcore
- **Referencias:** Architects, BMTH.
- **Tempo y swing que usa GEN:** 137–160 BPM medidos (16 semillas), swing 0.
- **Groove y batería:** doble bombo, breakdown a medio tiempo.
- **Bajo:** con la guitarra.
- **Armonía:** frigio, i–bII, VI–VII–i.
- **Melodía:** riff.
- **Forma:** verso, estribillo, breakdown.
- **Paleta:** guitarra distorsionada.
- **Mezcla de referencia:** guitarra y batería.
- **En la tabla del generador:** mapa `DM_METAL`, kit `KIT_BREAKS`, bajos `BA_GALLOP, BA_PUNK16, BA_ROCK8`, progresiones `PR_PHRY, PR_I_VI, PR_EPIC`, lead `LD_OSTINATO`, forma `FM_PARTS`, armonía `HF_LOOP`.
- **Mezcla medida (dB respecto de la batería, todas las partes prendidas):** bajo -3.8, acordes -7.4, lead -10.5, arp -20.2, textura -42.6.
- **Parámetros que deja (promedios):** bass (ANALOG11 STRING5): LEVEL 107, FCUT 111, ATK 4, REL 27, REV 1, DLY 0, DIST 52, CHOR 14; pad (ANALOG6 PHASE2 STRING8): LEVEL 99, FCUT 76, ATK 4, REL 18, REV 41, DLY 8, DIST 84, CHOR 8; lead (ANALOG10 MATH6): LEVEL 71, FCUT 89, ATK 12, REL 42, REV 30, DLY 28, DIST 65, CHOR 13; arp (ANALOG9 PHASE4 WEST3): LEVEL 79, FCUT 83, ATK 4, REL 21, REV 24, DLY 24, DIST 40, CHOR 28.


### Explore
No es un género: sortea campo por campo (tempo 76–154, cualquier mapa, kit, bajo, modo, timbre) con una o dos rarezas por pieza. Es donde abre la pantalla GEN, así que es lo primero que se oye. En V10.8.34 no lo toqué: solo hereda el salto firma y las células nuevas del lead. Si las piezas «cómicas» que escuchaste salieron de EXPLORE, la etapa 7 es la que corresponde.

## 8. Apéndice: tres piezas de V10.8.34

Notación: una línea por compás; `=` nota sostenida, `.` silencio, `/` une las notas de un acorde, `^` acento, `~` slide. En la batería, `X` acento, `x` fuerte, `o` medio, `,` ghost.

### Dream pop, semilla 9, energía 60, PARTS

```
=== DREAM POP  seed 9  energy 60  form PARTS  85 BPM  swing 0
SCALE page: ROOT Bb  SCALE MIX  QUANT SNAP   CHORD page (T3): 7TH, VOICING LEAD, STRUM 0
harmony: COLOR  palette 7f  progression: 4 1 5 6   (* coloured: borrowed, secondary, raised)
chords: Eb(add9)[IV] @0+16 Bb7[I7] @16+16 Fm9[v7] @32+16 Gm7[vi7] @48+16
bar 1:  Eb(add9) (IV)
bar 2:  Bb7 (I7)
bar 3:  Fm9 (v7)
bar 4:  Gm7 (vi7)
T1 DRUM DRUM ACOUSTIC  LEN 64
  BD   X.......X.x.....|X.......X.x.....|X.......X.x.....|X.......X.x.....
  SD   ....X.......X...|....X.......X...|....X.......X...|....X.......X...
  CH   xxx..xxxx..xxxx.|xx.xxxxxx.x.xxxx|xxxxxxxx.xxxxxx.|.xxxxxx.x.xxxxxx
  OH   ......x.........|......x.......x.|......x.........|..............x.
T2 BASS ANALOG SUB BASS  LEN 64
       | Eb2^ . Eb2 . Eb2 . . Eb2 Eb2^ Eb2~ Eb2 . Eb2 . C2 Eb2
       | Bb1^ . Bb1 . Bb1 . . Bb1 Bb1^ Bb1 Bb1 . Bb1 . Eb2 Bb1
       | F2^ . F2 . F2 . . F2 F2^ F2 F2 . F2 . F2 F2
       | G2^ . G2 . G2 . . G2 G2^ G2~ G2 . G2 . F2 G2
T3 PAD  MATH MATH PAD  LEN 64
       | G3/Eb4/F4/Bb4 = = = = = = = = = = = = = = =
       | Ab3/D4/F4/Bb4 = = = = = = = = = = = = = = =
       | Ab3/F4/G4/Eb5 = = = = = = = = = = = = = = =
       | Bb3/F4/G4/D5 = = = = = = = = = = = = = = =
T4 ARP  PHASE HARP  LEN 64
       | Eb3 . Bb3 . F4 . . . . . . . . . . .
       | F3 . Bb3 . D4 . Ab4 . D4 . Bb3 . F3 . Bb3 .
       | F3 . Ab3 . Eb4 . . . . . . . . . . .
       | F3 . Bb3 . D4 . G4 . . . . . . . . .
T5 LEAD VOICE VOX LEAD  LEN 64
       | Eb4 = = = = = = = Ab4 = G4 = F4 = = =
       | Ab4 = = = = = = = = = = = = . . .
       | C5 = = = = = = = Bb4 = Ab4 = = = = =
       | G4 = = = = = = = D4 = Eb4 = F4 = = =
T6 TEX  ANALOG DARK PAD  LEN 64
       | Eb4 = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
       | G4 = = = = = = = = = = = = = = =
song: A x4 A x4 A x4 A x4 A x4 A x4 A x4 A x4 D x4 A x12 A x4 A x4
  row  1  A x4   plays: DRUM<B BASS PAD ARP
  row  2  A x4   plays: DRUM<B BASS PAD
  row  3  A x4   plays: DRUM BASS PAD ARP
  row  4  A x4   plays: DRUM<B BASS PAD ARP
  row  5  A x4   plays: DRUM BASS PAD LEAD
  row  6  A x4   plays: DRUM<B BASS PAD LEAD
  row  7  A x4   plays: DRUM BASS PAD ARP LEAD
  row  8  A x4   plays: DRUM<B BASS PAD ARP LEAD
  row  9  D x4   plays: PAD LEAD TEX
  row 10  A x12  plays: DRUM BASS PAD ARP LEAD
  row 11  A x4   plays: DRUM<B BASS PAD ARP LEAD
  row 12  A x4   plays: DRUM<B BASS PAD
```

En el arp (T4) se ve el diálogo: toca al principio de cada compás y calla en la segunda mitad, donde el lead (T5) mueve tres notas.

### Jazz, semilla 5, energía 60, PARTS

```
=== JAZZ  seed 5  energy 60  form PARTS  103 BPM  swing 65
SCALE page: ROOT D  SCALE MAJ  QUANT SNAP   CHORD page (T3): 7TH, VOICING LEAD, STRUM 6
harmony: JAZZ RAISE SEC APPR  palette 7f  progression: 1 4 2 1   (* coloured: borrowed, secondary, raised)
chords: Dmaj7[I7] @0+16 Gmaj7[IV7] @16+8 B7[VI7*] @24+8 Em7[ii7] @32+16 Em7[ii7] @48+8 A7[V7] @56+8
bar 1:  Dmaj7 (I7)
bar 2:  Gmaj7 (IV7)  B7 (VI7*)
bar 3:  Em7 (ii7)
bar 4:  Em7 (ii7)  A7 (V7)
T1 DRUM DRUM ACOUSTIC  LEN 64
  BD   x.x.x.x.x.xox.x.|x.x.x.x.x.x,x.x.|x.x.x.x.x.x,x.x.|x.x.x.x.x.x,x.x.
  SD   .............,.o|...........,...o|...........,.,..|...........,...o
  CH   ..x...x...x...x.|..x...x...x...x.|..x...x...x...x.|..x...x...x...x.
  OH   x.Xox.Xox.Xox.Xo|x.Xox.Xox.X,x.Xo|x.Xox.Xox.X,x.Xo|x.Xxx.Xox.X,x.Xo
T2 BASS ANALOG SUB BASS  LEN 64
       | D2 = D3 = B2 = A2 = F#2 = A2 = B2 = A1 =
       | G1 = G2 = E2 = D2 = B1 = F#2 = F#2 = F#1 =
       | E1 = E2 = D2 = B1 = G1 = B1 = C#2 = F#1 =
       | E1 = E2 = D2 = B1 = A1 = E2 = F#2 = C#2 =
T3 PAD  MATH MATH KEYS  LEN 64
       | A3/C#4/E4/F#4 . . A3/C#4/E4/F#4 . . . A3/C#4/E4/F#4 A3/C#4/E4/F#4 . . A3/C#4/E4/F#4 . . . .
       | A3/B3/D4/F#4 . . A3/B3/D4/F#4 . . . A3/B3/D4/F#4 A3/B3/D#4/F#4 . . A3/B3/D#4/F#4 . . . .
       | G3/B3/D4/F#4 . . G3/B3/D4/F#4 . . . G3/B3/D4/F#4 G3/B3/D4/F#4 . . G3/B3/D4/F#4 . . . .
       | G3/B3/D4/F#4 . . G3/B3/D4/F#4 . . . G3/B3/D4/F#4 G3/B3/C#4/E4 . G3/B3/C#4/E4 G3/B3/C#4/E4 . . . .
T4 LEAD ANALOG ORGAN SQR  LEN 64
       | . . D4 . C#4 = = . A4 . G4 = = = = =
       | = = = . F#4 = = . F#4 = = = = = = =
       | = . C#4 . G4 = = . F#4 . F#4 = = = = =
       | = = = . G4 = = . A4 = = = = = = .
T5 ARP  off
T6 TEX  WEST DUST DRONE  LEN 64
       | D3 = = = = = = = = = = = = = = =
       | . . . . . . . . . . . . . . . .
       | E3 = = = = = = = = = = = = = = =
       | E3 = = = = = = = = = = = = = = =
song: A x4 A x4 B x8 A x4 A x4 C x4 C x4
  row  1  A x4   plays: DRUM BASS PAD LEAD TEX
  row  2  A x4   plays: DRUM<B BASS PAD LEAD TEX
  row  3  B x8   plays: PAD LEAD TEX
  row  4  A x4   plays: DRUM BASS PAD LEAD TEX
```

### IDM ambient, semilla 3, energía 60, LOOP

```
=== IDM AMBIENT  seed 3  energy 60  form LOOP  97 BPM  swing 30
SCALE page: ROOT Gb  SCALE MAJ  QUANT SNAP   CHORD page (T3): 7TH, VOICING LEAD, STRUM 0
harmony: COLOR  palette 7f  progression: 1 4   (* coloured: borrowed, secondary, raised)
chords: Gbmaj7[I7] @0+32 Bmaj7[IV7] @32+32
bar 1:  Gbmaj7 (I7)
bar 2:
bar 3:  Bmaj7 (IV7)
bar 4:
T1 DRUM DRUM ACOUSTIC  LEN 64
  BD   X......o..x..,..|X......o..x..,..|X......o..x..,..|.X.....o..x..o..
  SD   ....X..o....X...|....X..o....X...|....X..o....X...|....X..o....xoxx
  CH   ..o...o...x...o.|..o...o...x...o.|..o...o...x...o.|..........x...x.
T2 BASS ANALOG SUB BASS  LEN 64
       | Gb2 = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
T3 PAD  MATH MATH PAD  LEN 64
       | Gb3/Db4/F4/Bb4 = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
       | Gb3/B3/Eb4/Bb4 = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
T4 ARP  WEST GAMELAN  LEN 64
       | Gb3 . . . Db4 . . . Db4 . . . Gb3 . . .
       | Db4 . . . Db4 . Bb3 . Gb3 . . . Db4 . . .
       | Gb3 . . . B3 . . . B3 . . . Gb3 . . .
       | B3 . . . B3 . Bb3 . Gb3 . . . B3 . . .
T5 LEAD MATH MATH BELL  LEN 64
       | Bb4 = = = = = = = = = = = = . . .
       | . . . . . . . . . . . . . . . .
       | B4 = = = = = = = = = = = = . . .
       | Bb4 = = = = = = = Bb4 = = = = = = =
T6 TEX  ANALOG CELLO  LEN 64
       | Gb3 = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
       | = = = = = = = = = = = = = = = =
       | Gb4 = = = = = = = = = = = = = = =
```

## 9. Cómo repetir las mediciones

```
python3 -c "import sys;sys.path.insert(0,'tools');import build;build.generate()"
cc -O1 -w -Wno-implicit-function-declaration -Ibuild/gen -Ifirmware/src -Itests -o build/host/gen_study tests/gen_study.c -lm
build/host/gen_study 24            # composición (GEN_ALL=1: todas las partes prendidas)
cc ... -o build/host/gen_audit tests/gen_audit.c -lm && build/host/gen_audit 16    # parámetros por rol
cc -O2 ... -o build/host/gen_stems tests/gen_stems.c -lm && build/host/gen_stems 4 # niveles por rol
build/host/gen_dump GÉNERO SEMILLA ENERGÍA FORMA                                   # una pieza como partitura
```

## 10. V10.8.36: etapas 7, 2 y 3 (aplicadas)

Mismas herramientas, mismas semillas. Nada fue juzgado de oído: la mezcla la tenés que validar vos en la beta.

**Etapa 2, mezcla y envíos** (tabla `GMIX` y `gen_mix`):

| Género | Rol | Antes (dB bajo la batería) | Después |
|---|---|---|---|
| Reggaeton | lead | −19,1 | −14,4 |
| Dream pop | lead | −22,8 | −18,4 |
| Dubstep | lead | −17,2 | −15,8 |
| Dubstep | arp | −28,9 | −23,2 |
| Hard bass | arp | −22,2 | −16,8 |
| Cumbia | arp | −20,8 | −16,5 |
| Electro metal | arp | −20,8 | −16,5 |
| Metalcore | arp | −20,2 | −15,7 |

- El lead elige: si tenía reverb y delay altos a la vez, se le baja a la mitad uno de los dos (el reverb en los ganchos y riffs, el delay en las líneas largas).
- En los géneros de club: el reverb de los stabs a la mitad (house: 55 → 27), el release del arp corto (house: 31 → 20) y el DUCK del bombo sobre el resto de 14–30 a 30–50.
- No cambió: EBM (su lead ya estaba al tope del nivel: 125 de 127; para subirlo hay que tocar el sonido), dembow y baile funk (arp al tope), y la textura (apagarla en cumbia y jazz cambiaba los sonidos de las demás pistas, así que quedó como estaba).

**Etapa 3, sonido** (parcial): el arp de club como pluck, el stab seco, y un color propio por pieza: las tres partes melódicas juntas un poco más oscuras, como estaban, o un poco más brillantes (corte al 79, 85 o 91 %), según la tonalidad y la cantidad de acordes. Falta: afinación del bombo respecto de la tónica y recetas de ADSR por género.

**Etapa 7, EXPLORE:** toma el diálogo lead/arp (2,5 → 7,5 % de los medios compases) y la progresión propia de la sección de contraste también cuando la pieza tiene un solo acorde. La medida de secciones con otros acordes no se movió (36,1 %).
